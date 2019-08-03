/*7月30日更新日志：把UE主叫与主叫的定时器写好了，下一步完善被叫的逻辑和定时器*/
/*完成情况：把UE的被叫完成了，没用完成测试，明天完成UE被叫的测试以及PCC服务器对于被叫的测试*/
/*7月31日更新日志：把6.2.2.3之前写完了（不包含）*/
#include "mainwindow.h"
#include "ui_mainwindow.h"

using namespace std;

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    //在此初始化
    ui->start->setDisabled(false);
    ui->DeReigster->setDisabled(true);
    ui->call->setDisabled(true);
    ui->disconnect->setDisabled(true);
    ui->connect->setDisabled(true);

    ui->DeReigster->setVisible(false);
    ui->call->setVisible(false);
    ui->disconnect->setVisible(false);
    ui->connect->setVisible(false);

    registerstate = UNREGISTERED;//初始化成未注册的
    regRecvPort = 10002;//本机接收注册信息的绑定端口
    regsendPort = 50001;//发送注册信息的目的端口
    Resendcnt = 0;//设定重发次数初始化为0,当加到2的时候还没有回复，则注册失败
    Resend_au_cnt = 0; //设定重发鉴权注册次数，初始化为0,当加到2的时候还没有回复，则注册失败
    Resend_DeReg_cnt = 0; //设定注销重发次数，初始化为0,当加到2的时候还没有回复，则注册失败
    CallConnectcnt = 0;//设定call connect册次数，初始化为0
    CallDisconnectcnt = 0;//设定call connect册次数，初始化为0

    Ancaddr.setAddress("162.105.85.235");//设置Anc的IP
    localip = getlocalIP();//获得本机的IP
    qDebug()<<localip;

    /*初始化注册信令*/
    QString QIMSIstr = "460001357924680";
    init_regMsg(QIMSIstr);//初始化第一个注册信息
    init_voiceDeRegisterRsp();//初始化 DeRegister Rsp
    init_voiceDeRegisterReq();//初始化 DeRegister Req

    //init_sc2();//初始化sc2头

    callstate = U0;
    /*初始化呼叫信令*/
    string calledBCDNumber = "15650709603";
    init_callSetup(calledBCDNumber);

    init_callSetupAck();
    init_callAlerting();
    init_callConnect();
    init_callConnectAck();
    int cause = 27;
    init_callDisconnect(cause);//UE正常呼叫释放
    init_callReleaseRsp(cause);


    regUdpSocket = new QUdpSocket(this);
    sendSocket = new QUdpSocket(this);
    bool bindflag=  regUdpSocket->bind(QHostAddress::Any,regRecvPort);//注册消息接收端口

    if(!bindflag){
        QMessageBox box;
        box.setText(tr("初始化绑定错误！"));
        box.exec();
    }
    else{//绑定回调函数
        connect(regUdpSocket,SIGNAL(readyRead()),this,SLOT(recvRegInfo()));
    }

    regtimer = new QTimer();//注册定时器
    connect(regtimer,SIGNAL(timeout()),this,SLOT(proc_timeout()));

    calltimerT9005 = new QTimer();
    connect(calltimerT9005,SIGNAL(timeout()),this,SLOT(call_timeoutT9005()));
    calltimerT9006 = new QTimer();
    connect(calltimerT9006,SIGNAL(timeout()),this,SLOT(call_timeoutT9006()));
    calltimerT9007 = new QTimer();
    connect(calltimerT9007,SIGNAL(timeout()),this,SLOT(call_timeoutT9007()));
    calltimerT9009 = new QTimer();
    connect(calltimerT9009,SIGNAL(timeout()),this,SLOT(call_timeoutT9009()));
    calltimerT9014 = new QTimer();
    connect(calltimerT9014,SIGNAL(timeout()),this,SLOT(call_timeoutT9014()));

}

MainWindow::~MainWindow()
{
    delete ui;
    delete sendSocket;
    delete regUdpSocket;
}

void MainWindow::on_start_clicked()
{

    qDebug()<<"start button clicked";
    ui->start->setText("正在注册");
    ui->start->setDisabled(true);
    ui->start->setVisible(false);
    //这里开始的是第一次计时，等待的是PCC回复的authrization command
    regtimer->start(5000);
    int num=sendSocket->writeDatagram((char*)regMsg,sizeof(regMsg),Ancaddr,regsendPort);//num返回成功发送的字节数量
    qDebug()<<"发送初始注册消息，长度为 "<<num<<" 字节";


}
void MainWindow::recvRegInfo(){
    while(regUdpSocket->hasPendingDatagrams()){
        QByteArray datagram;
        datagram.resize(regUdpSocket->pendingDatagramSize());
        QHostAddress senderIP;
        quint16 senderPort;//
        regUdpSocket->readDatagram(datagram.data(),datagram.size(),&senderIP,&senderPort);//发送方的IP和port

        if(regtimer->isActive()) regtimer->stop();

        int recvlen = 0;//sci头的第7 8字节是净负荷的长度,注意大端
        recvlen = recvlen | datagram[6];
        recvlen = recvlen << 8;
        recvlen = recvlen | datagram[7];
        qDebug()<<"收到的净负荷长度是： "<<recvlen;

        char *recvbuf = new char[recvlen];
        char* str = datagram.data();
        memcpy(recvbuf,str+8,recvlen);

        char judge = *(recvbuf+2);
        /*这里面涉及到接收到包之后复制的地方还需要改，不是说加了sc2头之后仅仅吧索引从2变成10就可以了*/
        if(judge == 0x02 && registerstate == UNREGISTERED){//说明是authorization command
            qDebug()<<"收到authorization command！";
            registerstate = AUTH_PROC;

            //首先截取9到16字节的内容作为鉴权参数nonce
            QByteArray word = datagram.mid(8,8);
            //计算MD5
            QByteArray str = QCryptographicHash::hash(word,QCryptographicHash::Md5);

            //16byte长度，或者可以理解成32位BCD码组成
            memcpy(regMsg_au + 21, (char*)&str, 16);

            //再次发送带有鉴权的注册消息
            regtimer->start(5000);
            int num=sendSocket->writeDatagram((char*)regMsg_au,sizeof(regMsg_au),Ancaddr,regsendPort);//num返回成功发送的字节数量
            qDebug()<<"发送带有鉴权的注册消息，长度为 "<<num<<" 字节";
        }
        else if(judge == 0x03 && registerstate == AUTH_PROC){//说明是voice register rsp
            ui->start->setText("注册成功");
            qDebug()<<"收到 register rsp,注册成功！";
            registerstate = REGISTERED;//标识注册成功
            callstate = U0;
            ui->start->setDisabled(true);
            ui->DeReigster->setDisabled(false);
            ui->call->setDisabled(false);

            ui->start->setVisible(false);
            ui->DeReigster->setVisible(true);
            ui->call->setVisible(true);
        }
        else if(judge == 0x04  && registerstate == REGISTERED){//收到从PCC端来的voice DeRegister Req 应该终止业务
            ui->start->setText("开机注册");
            qDebug()<<"收到PCC 发送的voice DeRegister Req,需要重新注册！";

            int num=sendSocket->writeDatagram((char*)voiceDeRegisterRsp,sizeof(voiceDeRegisterRsp),Ancaddr,regsendPort);//num返回成功发送的字节数量
            qDebug()<<"发送voice DeRegister Rsp，长度为 "<<num<<" 字节";

            /*释放呼叫资源*/
            ReleaseRegResources();
        }
        else if(judge == 0x05  && registerstate == REGISTERED){//收到从PCC端来的voice DeRegister Rsp 可以终止业务
            ui->start->setText("开机注册");
            qDebug()<<"收到PCC 发送的voice DeRegister Rsp回应，注销成功！";

            /*释放呼叫资源*/
            ReleaseRegResources();

        }
        else if(judge == 0x06 && callstate == U0 && registerstate == REGISTERED){//这是被叫的状态转移
            /*发送call setup ack, call alerting, call connect*/
            qDebug()<<"收到call setup";

            callstate = U6;//呼叫发现态

            /*这里可能要修改，因为接收到的也需要加上sc2头*/
            memcpy(callSetupAck+3,recvbuf+3,4);
            memcpy(callAllerting+3,recvbuf+3,4);
            memcpy(callConnect+3,recvbuf+3,4);
            memcpy(callDisconnect+3,recvbuf+3,4);
            memcpy(callReleaseRsp+3,recvbuf+3,4);


            int num=sendSocket->writeDatagram((char*)callSetupAck,sizeof(callSetupAck),Ancaddr,regsendPort);
            qDebug()<<"发送call setup ack，长度为 "<<num<<" 字节";
            callstate = U9;//呼叫确认态

            num=sendSocket->writeDatagram((char*)callAllerting,sizeof(callAllerting),Ancaddr,regsendPort);
            qDebug()<<"发送call alerting，长度为 "<<num<<" 字节";
            callstate = U7;//呼叫接受态

            /*拒绝和接听都是可以的*/
            ui->call->setDisabled(true);
            ui->disconnect->setText("拒绝接听");
            ui->disconnect->setDisabled(false);
            ui->connect->setDisabled(false);

            ui->disconnect->setVisible(true);
            ui->connect->setVisible(true);

        }
        else if(judge == 0x07 && callstate == U1){
            qDebug()<<"收到call setup ack!";

            callstate = U3;
            calltimerT9005 ->stop();
            /*理论上这里受到的从PCC发过来的call setup ack里面有PCC分配的call ID信息*/
            /*主叫只需要发送call connect ack所以只初始化这个*/

            memcpy(callConnectAck+3,recvbuf+3,4);
            memcpy(callDisconnect+3,recvbuf+3,4);
            memcpy(callReleaseRsp+3,recvbuf+3,4);

            //这个定时器用于等待call alerting
            calltimerT9006 ->start(5000);

        }
        else if(judge == 0x08 && callstate == U3){
            qDebug()<<"收到call alerting!";
            callstate = U4;
            calltimerT9006 -> stop();
            if(calltimerT9005->isActive()) calltimerT9005->stop();

            ui->disconnect->setDisabled(false);//这个时候如果在响铃的时候也是可以结束呼叫的
            ui->disconnect->setVisible(true);

            calltimerT9007->start(30000);
        }
        else if(judge == 0x09 && (callstate == U4 || callstate == U3)){
            qDebug()<<"收到call connect";
            callstate = U10;
            calltimerT9007->stop();
            if(calltimerT9005->isActive()) calltimerT9005->stop();
            if(calltimerT9006->isActive()) calltimerT9006->stop();
            int num=sendSocket->writeDatagram((char*)callConnectAck,sizeof(callConnectAck),Ancaddr,regsendPort);//num返回成功发送的字节数量
            qDebug()<<"主叫建立成功！ 发送call connect ack，长度为 "<<num<<" 字节";

            ui->call->setDisabled(true);
            ui->disconnect->setVisible(true);

        }
        else if(judge == 0x0a && callstate == U8){
            qDebug()<<"收到call connect ack,通话建立成功";
            callstate = U10;
            calltimerT9014->stop();

            CallConnectcnt = 0;
            CallDisconnectcnt = 0;
            ui->call->setDisabled(true);
            ui->disconnect->setText("结束通话");
            ui->disconnect->setDisabled(false);
            ui->disconnect->setVisible(true);

            /*就可以开启语音发送线程*/

        }
        else if(judge == 0x0c){//收到Call Release Req ;U3,4,7,8,9,10状态都需要变成U0空闲态
            int cause = datagram[7];
            qDebug()<<"收到call Release Req,原因索引是： "<<cause<<" ,释放资源";

            /*P15页UE取消呼叫或者是P16页被叫UE没接听或者PCC拒绝呼叫，则不需要杀掉语音进程，否则P15页UE挂机则需要杀掉语音进程，根据呼叫状态区分即可*/
            if(callstate == U10){
                //杀掉语音进程
            }

            calltimerT9009->stop();

            /*释放资源*/
            ReleaseCallResources();

            int num=sendSocket->writeDatagram((char*)callReleaseRsp,sizeof(callReleaseRsp),Ancaddr,regsendPort);//发送callReleaseRsp
            qDebug()<<"发送call Release Rsp: "<<num<<" 字节";
            qDebug()<<"-------结束呼叫--------";

            ui->disconnect->setText("结束通话");

        }
        //char * strJudge=datagram.data();//把QByteArray转换成char *
    }
}
quint32 MainWindow::getlocalIP(){
    QList<QHostAddress> list =QNetworkInterface::allAddresses();
    QString localip;
    foreach (QHostAddress address, list)
    {
           if(address.protocol() ==QAbstractSocket::IPv4Protocol){
               localip = address.toString();
               QStringList ipsection = localip.split('.');
               if(ipsection[0] == "162" && ipsection[1] == "105"){//获得的是本机的公网ip
                   bool bOk = false;
                   quint32 nIPV4 = address.toIPv4Address(&bOk);
                   return nIPV4;
               }
           }
    }
}

/*以下都是初始化信令部分*/
void MainWindow::init_regMsg(QString QIMSIstr){

        unsigned char protocolVersion = 0x00;
        regMsg[0] = protocolVersion; //0x00是测试版本，0x01是初始版本，我们先用测试版本


        //第2字节，在第一次注册的时候，长度是21
        unsigned char msgLength = 0x15;
        regMsg[1] = msgLength;

        //第3字节，消息类型
        int msgType = 0x01;
        regMsg[2] = msgType;

        //第4字节到第8字节，共40比特，作为S-TMSI.
        //s-TMSI = MMEC(8bits) + M-TMSI(32bits)移动用户标识
        //总的来说，这是一个区分不同UE的随机数，我先把它设置为全0吧

        unsigned char MMEC = 0x00;
        regMsg[3] = MMEC;
        int MTMSI = 0x00000000;
        unsigned char *p = (unsigned char *)&MTMSI;
        regMsg[4] = *p;//0x00注意大端
        regMsg[5] = *(p + 1);//00
        regMsg[6] = *(p + 2);//00
        regMsg[7] = *(p + 3);//00

        //第9字节，reg_type
        unsigned char REG_TYPE = 0x00;//开机注册是00，周期注册是0x01，注销是03
        regMsg[8] = REG_TYPE;

        //第10到17字节是IMSI,8个字节的用户标识,unsigned long long int 正好是8字节
        //unsigned long long int IMSInum = 460001357924680;
        //memcpy(regMsg+9,&IMSInum,sizeof(unsigned long long int));

        //用BCD码字来存储，不用上面的long long int
        //QString QIMSIstr = "460001357924680";
        init_IMSI(QIMSIstr);

        //第18到21字节是IPAddr
        uint32_t IPnum = htonl(localip);//转换成大端模式
        memcpy(regMsg+17,(char*) &IPnum, 4);

        memcpy(regMsg_au,regMsg,21);

        //加上sc2头.现在不需要了
        /*
        short len = htons(sizeof(regMsg));
        memcpy(SC2_header+6,(char*)&len,2);memcpy(sc2_regMsg,SC2_header,sizeof(SC2_header)); memcpy(sc2_regMsg+sizeof(SC2_header), regMsg, sizeof(regMsg));

        len = htons(sizeof(regMsg_au));
        memcpy(SC2_header+6,(char*)&len,2);memcpy(sc2_regMsg_au,SC2_header,sizeof(SC2_header)); memcpy(sc2_regMsg_au+sizeof(SC2_header), regMsg_au, sizeof(regMsg_au));
        */
}

void MainWindow::init_IMSI(QString &QIMSIstr){//这个函数实现大端存储
    int len = QIMSIstr.size();
    if(len !=15){
        QMessageBox box;
        box.setText(tr("IMSI长度需要为15位！"));
        box.exec();
        return;
    }
    //"460001357924680F"最高位补了一个F(1111)
    //IMSI最低位对应的是regMsg[9],最高为对应的是regMsg[16],一个字节存放两位数字
    //注意我在IMSI最高位补了一个0变成16位,这是为了代码整洁性，但是最高位是0不会影响结果
    //对于一个char里面的两个数字，同样一个char里面低位存放低位数字。高4位存放高位数字

    std::string IMSIstr = QIMSIstr.toStdString() + '?';
    //printf(IMSIstr.c_str());
    unsigned char IMSI[8];
    memset(IMSI,0,8);
    for(int i=0;i<=7;i++){//注意大端
        int index1 = 2*i; //低位数字存在一个字节里面的低4位
        int index2 = 2*i+1;	//高位数字
        int num1 = int(IMSIstr[index1]- '0');
        unsigned char num1c = num1;
        IMSI[i] = IMSI[i] | num1c;
        int num2 = int(IMSIstr[index2] -'0');
        unsigned char num2c = (num2<<4);
        IMSI[i] = IMSI[i] | num2c;
    }
    memcpy(regMsg+9,IMSI,8);

}

void MainWindow::init_voiceDeRegisterReq(){//初始化DeRegisterReq
    voiceDeRegisterReq[0] = 0x00;
    voiceDeRegisterReq[1] = 0x0a;//message length == 10byte
    voiceDeRegisterReq[2] = 0x04;//message type;
    //略去STMSI部分
    voiceDeRegisterReq[8] = 0x03;//cause 0x03表示的是UE 侧的注销请求
    voiceDeRegisterReq[9] = 0x10;//UE关机注销

    /*
    short len = htons(sizeof(voiceDeRegisterReq));
    memcpy(SC2_header+6,(char*)&len,2);memcpy(sc2_voiceDeRegisterReq,SC2_header,sizeof(SC2_header)); memcpy(sc2_voiceDeRegisterReq+sizeof(SC2_header), voiceDeRegisterReq, sizeof(voiceDeRegisterReq));
    */
}

void MainWindow::init_voiceDeRegisterRsp(){
    memset(voiceDeRegisterRsp,0,sizeof(voiceDeRegisterRsp));
    voiceDeRegisterRsp[0] = 0x00;
    voiceDeRegisterRsp[1] = 0x08;//message length
    voiceDeRegisterRsp[2] = 0x05;//message type
    //后面5个字节的STMSI目前就是0，是和前面RegMsg是一样的
    /*
    short len = htons(sizeof(voiceDeRegisterRsp));
    memcpy(SC2_header+6,(char*)&len,2);memcpy(sc2_voiceDeRegisterRsp,SC2_header,sizeof(SC2_header)); memcpy(sc2_voiceDeRegisterRsp+sizeof(SC2_header), voiceDeRegisterRsp, sizeof(voiceDeRegisterRsp));
    */
}

void MainWindow::init_sc2(){//这个函数不需要了
    memset(SC2_header,0,sizeof(SC2_header));
    //前5个字节是UEID
    SC2_header[5] = 0x00;//信令方向00为上行
}

void MainWindow::init_callSetup(string calledBCDNumber){
    if(calledBCDNumber.size()!=11){
        qDebug()<<"电话号码长度有误！";
        return;
    }
    callSetup[0] = 0x00;//Protocol version
    callSetup[1] = 0x15;//Message length == 21
    callSetup[2] = 0x06;//Message type
    //call ID 4个字节填全F
    memset(callSetup+3, 255,4);
    //STMSI
    memset(callSetup+7,0,5);
    //call type
    callSetup[12] = 0x01;
    //init called Party BCD Number
    callSetup[13] = 0x03;// tag
    callSetup[14] = 0x08;// length of called BCD number

    /*init BCD number*/
    calledBCDNumber += '?';//为了最后可以补一个1111
    unsigned char nums[6];
    memset(nums,0,6);
    for(int i=0;i<=5;i++){//注意大端
        int index1 = 2*i; //低位数字存在一个字节里面的低4位
        int index2 = 2*i+1;	//高位数字
        int num1 = int(calledBCDNumber[index1]- '0');
        unsigned char num1c = num1;
        nums[i] = nums[i] | num1c;
        int num2 = int(calledBCDNumber [index2] -'0');
        unsigned char num2c = (num2<<4);
        nums[i] = nums[i] | num2c;
    }
    memcpy(callSetup+15,nums,6);
    /*
    short len = htons(sizeof(callSetup));
    memcpy(SC2_header+6,(char*)&len,2);memcpy(sc2_callSetup,SC2_header,sizeof(SC2_header)); memcpy(sc2_callSetup+sizeof(SC2_header), callSetup, sizeof(callSetup));
    */
}

void MainWindow::init_callSetupAck(){
    callSetupAck[0] = 0x00;//protocol version
    callSetupAck[1] = 0x07;//message length
    callSetupAck[2] = 0x07;//message type
    //后面的需要memcpy以下从PCC发送来的call ID
    /*
    short len = htons(sizeof(callSetupAck));
    memcpy(SC2_header+6,(char*)&len,2);memcpy(sc2_callSetupAck,SC2_header,sizeof(SC2_header)); memcpy(sc2_callSetupAck+sizeof(SC2_header), callSetupAck, sizeof(callSetupAck));
    */
}

void MainWindow::init_callAlerting(){
    callAllerting[0] = 0x00;//protocol version
    callAllerting[1] = 0x07;//message length
    callAllerting[2] = 0x08;//message type
    //后面的需要memcpy以下从PCC发送来的call ID

    /*
    short len = htons(sizeof(callAllerting));
    memcpy(SC2_header+6,(char*)&len,2);memcpy(sc2_callAllerting,SC2_header,sizeof(SC2_header)); memcpy(sc2_callAllerting+sizeof(SC2_header), callAllerting, sizeof(callAllerting));
    */
}

void MainWindow::init_callConnect(){
    callConnect[0] = 0x00;//protocol version
    callConnect[1] = 0x08;//message length
    callConnect[2] = 0x09;//message type
    //后面的需要memcpy以下从PCC发送来的call ID
    callConnect[7] = 0x01;//call type

    /*
    short len = htons(sizeof(callConnect));
    memcpy(SC2_header+6,(char*)&len,2);memcpy(sc2_callConnect,SC2_header,sizeof(SC2_header)); memcpy(sc2_callConnect+sizeof(SC2_header), callConnect, sizeof(callConnect));
    */
}

void MainWindow::init_callConnectAck(){
    callConnectAck[0] = 0x00;//protocol version
    callConnectAck[1] = 0x07;//message length
    callConnectAck[2] = 0x0a;//message type
    //后面的需要memcpy以下从PCC发送来的call ID

    /*
    short len = htons(sizeof(callConnectAck));
    memcpy(SC2_header+6,(char*)&len,2);memcpy(sc2_callConnectAck,SC2_header,sizeof(SC2_header)); memcpy(sc2_callConnectAck+sizeof(SC2_header), callConnectAck, sizeof(callConnectAck));
    */
}

void MainWindow::init_callDisconnect(int cause){

    callDisconnect[0] = 0x00;//protocol version
    callDisconnect[1] = 0x07;//message length
    callDisconnect[2] = 0x0b;//message type
    //后面的需要memcpy以下从PCC发送来的call ID

    callDisconnect[8] = char(cause);//casue

    /*
    short len = htons(sizeof(callDisconnect));
    memcpy(SC2_header+6,(char*)&len,2);memcpy(sc2_callDisconnect,SC2_header,sizeof(SC2_header)); memcpy(sc2_callDisconnect+sizeof(SC2_header), callDisconnect, sizeof(callDisconnect));
    */
}

void MainWindow::init_callReleaseRsp(int cause){

    callReleaseRsp[0] = 0x00;//protocol version
    callReleaseRsp[1] = 0x08;//message length
    callReleaseRsp[2] = 0x0d;//message type
    //后面的需要memcpy以下从PCC发送来的call ID

    callReleaseRsp[8] = char(cause);//casue

    /*
    short len = htons(sizeof(callReleaseRsp));
    memcpy(SC2_header+6,(char*)&len,2);memcpy(sc2_callReleaseRsp,SC2_header,sizeof(SC2_header)); memcpy(sc2_callReleaseRsp+sizeof(SC2_header), callReleaseRsp, sizeof(callReleaseRsp));
    */
}
/*上面都是初始化信令*/
void MainWindow::proc_timeout(){

    regtimer->stop();

    if(registerstate == UNREGISTERED){//仍然是第一次注册都没收到回复的状态
        if(Resendcnt <=1){
            Resendcnt++;
            regtimer->start(5000);
            int num=sendSocket->writeDatagram((char*)regMsg,sizeof(regMsg),Ancaddr,regsendPort);//num返回成功发送的字节数量

            qDebug()<<"第 "<<Resendcnt<<" 次数重发 初始 注册消息，长度为 "<<num<<" 字节";
        }
        else{
            ui->start->setText("开机注册");
            qDebug()<<"第一次注册没用收到authorization command,请点击开机注册按钮重试！";

            ReleaseRegResources();
        }
    }
    else if(registerstate == AUTH_PROC){//这说明是鉴权注册超时了
        if(Resend_au_cnt <=1){
            Resend_au_cnt++;
            regtimer->start(5000);
            int num=sendSocket->writeDatagram((char*)regMsg,sizeof(regMsg),Ancaddr,regsendPort);//num返回成功发送的字节数量
            qDebug()<<"第 "<<Resend_au_cnt<<" 次数重发 鉴权 注册消息，长度为 "<<num<<" 字节";
        }
        else{
            ui->start->setText("开机注册");
            qDebug()<<"鉴权注册没有收到 voice register rsp, 请点击开机注册按钮重试！";

            ReleaseRegResources();
        }
    }
    else if(registerstate == REGISTERED){//UE注销超时
        if(Resend_DeReg_cnt <=1){
            Resend_DeReg_cnt++;
            regtimer->start(5000);
            int num=sendSocket->writeDatagram((char*)voiceDeRegisterReq,sizeof(voiceDeRegisterReq),Ancaddr,regsendPort);//num返回成功发送的字节数量
            qDebug()<<"第 "<<Resend_DeReg_cnt<<" 次数重发 注销 消息，长度为 "<<num<<" 字节";
        }
        else{

            ReleaseRegResources();
            ui->start->setText("开机注册");
            qDebug()<<"UE注销超时，自动注销！";

        }


    }

}

void MainWindow::on_DeReigster_clicked()
{
    if(registerstate != REGISTERED){
        qDebug()<<"请先进行注册";
    }
    ui->DeReigster->setDisabled(true);
    regtimer->start(5000);
    int num=sendSocket->writeDatagram((char*)voiceDeRegisterReq,sizeof(voiceDeRegisterReq),Ancaddr,regsendPort);//num返回成功发送的字节数量
    qDebug()<<"UE请求注销,长度: "<<num;
}


void MainWindow::on_call_clicked()//呼叫按钮
{
    if(registerstate != REGISTERED){
        qDebug()<<"请先注册！";
        return;
    }
    if(callstate !=U0){
        qDebug()<<"已经在通话";
        return;
    }
    /*以下流程是主叫建立流程。我首先忽略掉被叫建立的过程，模拟的是全程顺利的过程，呼叫成功之后应该开启语音发送线程*/
    ui->call->setDisabled(true);
    callstate = U1;
    //发送呼叫建立信令
    calltimerT9005 -> start(5000);
    //UE -> PCC
    int num=sendSocket->writeDatagram((char*)callSetup,sizeof(callSetup),Ancaddr,regsendPort);//num返回成功发送的字节数量
    qDebug()<<"UE sends call setup,长度: "<<num;
    ui->call->setDisabled(true);

}

void MainWindow::call_timeoutT9005(){//call setup超时处理

    calltimerT9005 ->stop();
    if(calltimerT9006->isActive()) calltimerT9006->stop();
    if(calltimerT9007->isActive()) calltimerT9007->stop();
    callstate = U0;
    CallConnectcnt = 0;
    CallDisconnectcnt = 0;
    qDebug()<<"T9005 超时， 请重新呼叫";
    ui->call->setDisabled(false);

}
void MainWindow::call_timeoutT9006(){//呼叫call setup ack超时处理

    calltimerT9006 ->stop();
    if(calltimerT9005->isActive()) calltimerT9005->stop();
    if(calltimerT9007->isActive()) calltimerT9007->stop();
    callstate = U0;
    CallConnectcnt = 0;
    CallDisconnectcnt = 0;
    qDebug()<<"T9006 超时， 请重新呼叫";
    ui->call->setDisabled(false);

}
void MainWindow::call_timeoutT9007(){//call connect超时处理

    calltimerT9007 ->stop();
    if(calltimerT9005->isActive()) calltimerT9005->stop();
    if(calltimerT9006->isActive()) calltimerT9006->stop();
    callstate = U0;
    CallConnectcnt = 0;
    CallDisconnectcnt = 0;
    qDebug()<<"T9007 超时， 请重新呼叫";
    ui->call->setDisabled(false);

}
void MainWindow::call_timeoutT9009(){//call disconnect超时处理,只重发一次

    calltimerT9009 -> stop();

    if(CallDisconnectcnt <=0){
        CallDisconnectcnt++;
        calltimerT9009->start(5000);
        int num=sendSocket->writeDatagram((char*)callDisconnect,sizeof(callDisconnect),Ancaddr,regsendPort);

        qDebug()<<"第 "<<CallDisconnectcnt<<" 次数重发 call disconnect消息，长度为 "<<num<<" 字节";
    }
    else{
        qDebug()<<"call disconnect 重发超过一次,清空呼叫资源";
        ReleaseCallResources();
    }

}
void MainWindow::call_timeoutT9014(){//call connect超时处理

    calltimerT9014 -> stop();

    if(CallConnectcnt <=1){
        CallConnectcnt++;
        calltimerT9014->start(5000);
        int num=sendSocket->writeDatagram((char*)callConnect,sizeof(callConnect),Ancaddr,regsendPort);

        qDebug()<<"第 "<<CallConnectcnt<<" 次数重发 call connect消息，长度为 "<<num<<" 字节";
    }
    else{
        CallConnectcnt = 0;
        CallDisconnectcnt = 0;
        qDebug()<<"call connect 重发超过两次";
        int cause = 6;
        memcpy(callDisconnect+7,(char*)&cause,1);

        /*call connect2次超时之后重新发送call disconnect*/
        calltimerT9009->start(5000);
        callstate = U19;//进入资源释放态
        sendSocket->writeDatagram((char*)callDisconnect,sizeof(callDisconnect),Ancaddr,regsendPort);
        qDebug()<<"call connect 2次超时之后重新发送call disconnect！";
    }
}
void MainWindow::ReleaseRegResources(){//释放注册资源

    if(regtimer->isActive()) regtimer->stop();

    registerstate = UNREGISTERED;
    callstate = U0;
    Resendcnt = 0;
    Resend_au_cnt = 0;
    Resend_DeReg_cnt = 0;
    CallConnectcnt = 0;
    CallDisconnectcnt = 0;
    ui->start->setDisabled(false);
    ui->DeReigster->setDisabled(true);
    ui->call->setDisabled(true);
    ui->disconnect->setDisabled(true);
    ui->connect->setDisabled(true);

    ui->start->setVisible(true);
    ui->DeReigster->setVisible(false);
    ui->call->setVisible(false);
    ui->disconnect->setVisible(false);
    ui->connect->setVisible(false);

}
void MainWindow::ReleaseCallResources(){//释放呼叫资源

    callstate = U0;
    CallConnectcnt = 0;
    CallDisconnectcnt = 0;

    if(calltimerT9005->isActive()) calltimerT9005->stop();
    if(calltimerT9006->isActive()) calltimerT9006->stop();
    if(calltimerT9007->isActive()) calltimerT9007->stop();
    if(calltimerT9009->isActive()) calltimerT9009->stop();
    if(calltimerT9014->isActive()) calltimerT9014->stop();

    ui->call->setDisabled(false);
    ui->disconnect->setDisabled(true);
    ui->connect->setDisabled(true);

    ui->disconnect->setVisible(false);
    ui->connect->setVisible(false);

}

void MainWindow::on_disconnect_clicked()//主叫或者被叫的结束通话按钮
{

    ui->disconnect->setDisabled(true);

    if(callstate == U4){//这种情况是在响铃的时候主叫点击了disconnect按钮
        //启动T9009定时器，关闭T9007定时器
        CallDisconnectcnt = 0;
        calltimerT9007->stop();
        calltimerT9009->start(5000);

        //进入资源释放态
        callstate = U19;
        sendSocket->writeDatagram((char*)callDisconnect,sizeof(callDisconnect),Ancaddr,regsendPort);
        qDebug()<<"主叫在响铃期间停止呼叫，发送call Disconnect!";

    }
    else if(callstate == U10){//在通话中停止呼叫
        //启动T9009定时器，关闭T9007定时器
        CallDisconnectcnt = 0;
        calltimerT9007->stop();
        calltimerT9009->start(5000);

        /*终止语音线程，并且还有填写disconnect 的cause是 用户挂机*/
        //进入资源释放态
        callstate = U19;
        int cause = 27;//原因是1B 呼叫正常释放
        memcpy(callDisconnect+7,(char*)&cause,1);
        sendSocket->writeDatagram((char*)callDisconnect,sizeof(callDisconnect),Ancaddr,regsendPort);
        qDebug()<<"UE 正常用户挂机，发送call Disconnect!";
    }
    else if(callstate == U7){//被叫在响铃的时候拒绝接听

        //启动T9009定时器，关闭T9007定时器
        CallDisconnectcnt = 0;
        if(calltimerT9007->isActive()) calltimerT9007->stop();
        calltimerT9009->start(5000);

        //进入资源释放态
        callstate = U19;
        int cause = 26;//原因是1A 被叫用户拒绝接听
        memcpy(callDisconnect+7,(char*)&cause,1);
        sendSocket->writeDatagram((char*)callDisconnect,sizeof(callDisconnect),Ancaddr,regsendPort);
        qDebug()<<"被叫在响铃期间拒绝接听，释放资源";

    }

}

void MainWindow::on_connect_clicked()//UE被叫的时候点击接听按钮
{
    ui->connect->setDisabled(true);
    calltimerT9014->start(5000);
    callstate = U8;//呼叫连接请求态
    int num=sendSocket->writeDatagram((char*)callConnect,sizeof(callConnect),Ancaddr,regsendPort);
    qDebug()<<"UE被叫接听，发送call connect，长度为 "<<num<<" 字节";
}

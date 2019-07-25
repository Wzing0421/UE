#include "mainwindow.h"
#include "ui_mainwindow.h"

using namespace std;

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    //在此初始化
    ui->DeReigster->setDisabled(true);
    ui->start->setDisabled(false);

    registerstate = UNREGISTERED;//初始化成未注册的
    regRecvPort = 10002;//本机接收注册信息的绑定端口
    regsendPort = 5000;//发送注册信息的目的端口
    Resendcnt = 0;//设定重发次数初始化为0,当加到2的时候还没有回复，则注册失败
    Resend_au_cnt = 0; //设定重发鉴权注册次数，初始化为0,当加到2的时候还没有回复，则注册失败

    PCCaddr.setAddress("162.105.85.198");//设置PCC的IP
    localip = getlocalIP();//获得本机的IP
    qDebug()<<localip;

    init_regMsg();//初始化第一个注册信息

    init_voiceDeRegisterRsp();//初始化 DeRegister Rsp
    init_voiceDeRegisterReq();//初始化 DeRegister Req
    init_sc2();//初始化sc2头

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

    timer = new QTimer();
    connect(timer,SIGNAL(timeout()),this,SLOT(proc_timeout()));

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
    //这里开始的是第一次计时，等待的是PCC回复的authrization command
    timer->start(5000);
    int num=sendSocket->writeDatagram((char*)sc2_regMsg,sizeof(sc2_regMsg),PCCaddr,regsendPort);//num返回成功发送的字节数量
    qDebug()<<"发送初始注册消息，长度为 "<<num<<" 字节";


}
void MainWindow::recvRegInfo(){
    while(regUdpSocket->hasPendingDatagrams()){
        QByteArray datagram;
        datagram.resize(regUdpSocket->pendingDatagramSize());
        QHostAddress senderIP;
        quint16 senderPort;//
        regUdpSocket->readDatagram(datagram.data(),datagram.size(),&senderIP,&senderPort);//发送方的IP和port

        if(timer->isActive()) timer->stop();

        char judge = datagram[2];
        if(judge == 0x02 && registerstate == UNREGISTERED){//说明是authorization command
            qDebug()<<"收到authorization command！";
            registerstate = AUTH_PROC;

            //首先截取9到16字节的内容作为鉴权参数nonce
            QByteArray word = datagram.mid(8,8);
            //计算MD5
            QByteArray str = QCryptographicHash::hash(word,QCryptographicHash::Md5);

            //16byte长度，或者可以理解成32位BCD码组成
            memcpy(sc2_regMsg_au + 21 + sizeof(SC2_header), (char*)&str, 16);

            //再次发送带有鉴权的注册消息
            timer->start(5000);
            int num=sendSocket->writeDatagram((char*)sc2_regMsg_au,sizeof(sc2_regMsg_au),PCCaddr,regsendPort);//num返回成功发送的字节数量
            qDebug()<<"发送带有鉴权的注册消息，长度为 "<<num<<" 字节";
        }
        else if(judge == 0x03 && registerstate == AUTH_PROC){//说明是voice register rsp
            ui->start->setText("注册成功");
            qDebug()<<"收到 register rsp,注册成功！";
            registerstate = REGISTERED;//标识注册成功
            ui->start->setDisabled(true);
            ui->DeReigster->setDisabled(false);
        }
        else if(judge == 0x04  && registerstate == REGISTERED){//收到从PCC端来的voice DeRegister Req 应该终止业务
            ui->start->setText("开机注册");
            qDebug()<<"收到PCC 发送的voice DeRegister Req,需要重新注册！";
            registerstate = UNREGISTERED;//需要重新注册
            int num=sendSocket->writeDatagram((char*)sc2_voiceDeRegisterRsp,sizeof(sc2_voiceDeRegisterRsp),PCCaddr,regsendPort);//num返回成功发送的字节数量
            qDebug()<<"发送voice DeRegister Rsp，长度为 "<<num<<" 字节";
            ui->start->setDisabled(false);
            ui->DeReigster->setDisabled(true);

        }
        else if(judge == 0x05  && registerstate == REGISTERED){//收到从PCC端来的voice DeRegister Rsp 可以终止业务
            ui->start->setText("开机注册");
            qDebug()<<"收到PCC 发送的voice DeRegister Rsp回应，注销成功！";
            registerstate = UNREGISTERED;
            ui->start->setDisabled(false);
            ui->DeReigster->setDisabled(true);
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

void MainWindow::init_regMsg(){

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
        QString QIMSIstr = "460001357924680";
        init_IMSI(QIMSIstr);

        //第18到21字节是IPAddr
        uint32_t IPnum = htonl(localip);//转换成大端模式
        memcpy(regMsg+17,(char*) &IPnum, 4);

        //加上sc2头
        short len = sizeof(regMsg);
        memcpy(SC2_header+6,(char*)&len,2);memcpy(sc2_regMsg,SC2_header,sizeof(SC2_header)); memcpy(sc2_regMsg+sizeof(SC2_header), regMsg, sizeof(regMsg));

        memcpy(regMsg_au,regMsg,21);
        len = sizeof(regMsg_au);
        memcpy(SC2_header+6,(char*)&len,2);memcpy(sc2_regMsg_au,SC2_header,sizeof(SC2_header)); memcpy(sc2_regMsg_au+sizeof(SC2_header), regMsg_au, sizeof(regMsg_au));

        /*
        if (regMsg[0] == 0x00) qDebug() << "yes0";
        if (regMsg[1] == 0x15) qDebug()<< "yes1" ;
        if (regMsg[2] == 0x01) qDebug()<< "yes2" ;
        if (regMsg[3] == 0x00) qDebug()<< "yes3" ;
        if (regMsg[4] == 0x00) qDebug()<< "yes4" ;
        if (regMsg[5] == 0x00) qDebug()<< "yes5" ;
        if (regMsg[6] == 0x00) qDebug()<< "yes6" ;
        if (regMsg[7] == 0x00) qDebug() << "yes7" ;

        //for(int i=0;i<8;i++) qDebug()<<((regMsg[17]>>i) &(0x01));
        if (int(regMsg[17]) == 198) qDebug()<< "yes17" ;
        if (int(regMsg[18]) == 85) qDebug()<< "yes18" ;
        if (int(regMsg[19]) == 105) qDebug()<< "yes19" ;
        if (int(regMsg[20]) == 162) qDebug() << "yes20" ;
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
    //"0460001357924680"最高位补了一个0
    //IMSI最低位对应的是regMsg[9],最高为对应的是regMsg[16],一个字节存放两位数字
    //注意我在IMSI最高位补了一个0变成16位,这是为了代码整洁性，但是最高位是0不会影响结果
    //对于一个char里面的两个数字，同样一个char里面低位存放低位数字。高4位存放高位数字

    std::string IMSIstr = '0' + QIMSIstr.toStdString();
    //printf(IMSIstr.c_str());
    unsigned char IMSI[8];
    memset(IMSI,0,8);
    for(int i=0;i<=7;i++){//注意大端
        int index1 = 2*i; //低位数字
        int index2 = 2*i+1;	//高位数字
        int num1 = int(IMSIstr[index1]- '0');
        unsigned char num1c = num1;
        IMSI[i] = IMSI[i] | (num1c<<4);
        int num2 = int(IMSIstr[index2] -'0');
        unsigned char num2c = num2;
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

    short len = sizeof(voiceDeRegisterReq);
    memcpy(SC2_header+6,(char*)&len,2);memcpy(sc2_voiceDeRegisterReq,SC2_header,sizeof(SC2_header)); memcpy(sc2_voiceDeRegisterReq+sizeof(SC2_header), voiceDeRegisterReq, sizeof(voiceDeRegisterReq));
}

void MainWindow::init_voiceDeRegisterRsp(){
    memset(voiceDeRegisterRsp,0,sizeof(voiceDeRegisterRsp));
    voiceDeRegisterRsp[0] = 0x00;
    voiceDeRegisterRsp[1] = 0x08;//message length
    voiceDeRegisterRsp[2] = 0x05;//message type
    //后面5个字节的STMSI目前就是0，是和前面RegMsg是一样的

    short len = sizeof(voiceDeRegisterRsp);
    memcpy(SC2_header+6,(char*)&len,2);memcpy(sc2_voiceDeRegisterRsp,SC2_header,sizeof(SC2_header)); memcpy(sc2_voiceDeRegisterRsp+sizeof(SC2_header), voiceDeRegisterRsp, sizeof(voiceDeRegisterRsp));
}

void MainWindow::init_sc2(){
    memset(SC2_header,0,sizeof(SC2_header));
    //前5个字节是UEID
    SC2_header[5] = 0x00;//信令方向00为上行
}

void MainWindow::proc_timeout(){

    timer->stop();

    if(registerstate == UNREGISTERED){//仍然是第一次注册都没收到回复的状态
        if(Resendcnt <=1){
            Resendcnt++;
            timer->start(5000);
            int num=sendSocket->writeDatagram((char*)sc2_regMsg,sizeof(sc2_regMsg),PCCaddr,regsendPort);//num返回成功发送的字节数量

            qDebug()<<"第 "<<Resendcnt<<" 次数重发 初始 注册消息，长度为 "<<num<<" 字节";
        }
        else{
            //状态清零，计数器也清零
            Resendcnt = 0;
            Resend_au_cnt = 0;
            registerstate = UNREGISTERED;
            ui->start->setText("开机注册");
            qDebug()<<"第一次注册没用收到authorization command,请点击开机注册按钮重试！";
            ui->start->setDisabled(false);
            ui->DeReigster->setDisabled(true);
        }
    }
    else if(registerstate == AUTH_PROC){//这说明是鉴权注册超时了
        if(Resend_au_cnt <=1){
            Resend_au_cnt++;
            timer->start(5000);
            int num=sendSocket->writeDatagram((char*)sc2_regMsg,sizeof(sc2_regMsg),PCCaddr,regsendPort);//num返回成功发送的字节数量
            //sendSocket->waitForReadyRead();
            qDebug()<<"第 "<<Resendcnt<<" 次数重发 鉴权 注册消息，长度为 "<<num<<" 字节";
        }
        else{
            Resendcnt = 0;
            Resend_au_cnt = 0;
            registerstate = UNREGISTERED;
            ui->start->setText("开机注册");
            qDebug()<<"鉴权注册没有收到 voice register rsp, 请点击开机注册按钮重试！";
            ui->start->setDisabled(false);
            ui->DeReigster->setDisabled(true);
        }
    }
    else if(registerstate == REGISTERED){//UE注销超时
        registerstate = UNREGISTERED;
        ui->start->setText("开机注册");
        qDebug()<<"UE注销超时，自动注销！";
        ui->start->setDisabled(false);
        ui->DeReigster->setDisabled(true);

    }

}

void MainWindow::on_DeReigster_clicked()
{
    if(registerstate != REGISTERED){
        qDebug()<<"请先进行注册";
    }
    ui->DeReigster->setDisabled(true);
    timer->start(5000);
    int num=sendSocket->writeDatagram((char*)sc2_voiceDeRegisterReq,sizeof(sc2_voiceDeRegisterReq),PCCaddr,regsendPort);//num返回成功发送的字节数量
    //sendSocket->waitForReadyRead();
    qDebug()<<"UE请求注销,长度: "<<num;
}

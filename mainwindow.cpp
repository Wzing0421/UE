#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    //在此初始化
    registerstate = 0;//初始化成未注册的
    regRecvPort = 10002;//本机接收注册信息的绑定端口
    regsendPort = 5000;//发送注册信息的目的端口
    Resendcnt = 0;//设定重发次数初始化为0,当加到2的时候还没有回复，则注册失败
    Resend_au_cnt = 0; //设定重发鉴权注册次数，初始化为0,当加到2的时候还没有回复，则注册失败

    PCCaddr.setAddress("162.105.85.198");//设置PCC的IP
    localip = getlocalIP();//获得本机的IP
    qDebug()<<localip;

    init_regMsg();//初始化第一个注册信息

    //这个鉴权信息初始化是我随便编的
    memcpy(regMsg_au,regMsg,21);
    regMsg_au[21] = 0x11;
    regMsg_au[22] = 0x22;


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
    //这里开始的是第一次计时，等待的是PCC回复的authrization command
    timer->start(5000);
    int num=sendSocket->writeDatagram((char*)regMsg,sizeof(regMsg),PCCaddr,regsendPort);//num返回成功发送的字节数量
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
        if(judge == 0x02 && registerstate == 0){//说明是authorization command
            qDebug()<<"收到authorization command！";
            registerstate = 1;

            //再次发送带有鉴权的注册消息
            timer->start(5000);

            int num=sendSocket->writeDatagram((char*)regMsg_au,sizeof(regMsg_au),PCCaddr,regsendPort);//num返回成功发送的字节数量
            qDebug()<<"发送带有鉴权的注册消息，长度为 "<<num<<" 字节";
        }
        else if(judge == 0x03 && registerstate == 1){//说明是voice register rsp
            ui->start->setText("注册成功");
            qDebug()<<"收到 register rsp,注册成功！";
            registerstate = 2;//标识注册成功

        }
        //char * strJudge=datagram.data();//把QByteArray转换成char *
    }
}
QString MainWindow::getlocalIP(){
    QList<QHostAddress> list =QNetworkInterface::allAddresses();
    QString localip;
    foreach (QHostAddress address, list)
    {
           if(address.protocol() ==QAbstractSocket::IPv4Protocol){
               localip = address.toString();
               QStringList ipsection = localip.split('.');
               if(ipsection[0] == "162" && ipsection[1] == "105"){//获得的是本机的公网ip
                   return localip;
               }
           }
    }
}

void MainWindow::init_regMsg(){

        unsigned char protocolVersion = 0x01;
        regMsg[0] = protocolVersion; //0x01表示协议版本


        //第2字节，在第一次注册的时候，长度是21
        unsigned char msgLength = 0x15;
        regMsg[1] = msgLength;

        //第3字节，消息类型
        int msgType = 0x01;
        regMsg[2] = msgType;

        //第4字节到第8字节，共40比特，作为S-TMSI.
        //s-TMSI = MMEC(8bits) + M-TMSI(32bits)移动用户标识
        unsigned char MMEC = 0x00;
        regMsg[3] = MMEC;
        int MTMSI = 0x00000000;
        unsigned char *p = (unsigned char *)&MTMSI;
        regMsg[4] = *p;//0x00注意小端
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
        QStringList ipsection = localip.split('.');
        bool ok;
        //注意是小端存储，162.105.85.98中98要存在低位
        regMsg[17] = ipsection[3].toInt(&ok,10); //右边的返回值是一个int
        regMsg[18] = ipsection[2].toInt(&ok,10);
        regMsg[19] = ipsection[1].toInt(&ok,10);
        regMsg[20] = ipsection[0].toInt(&ok,10);

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

void MainWindow::init_IMSI(QString &QIMSIstr){//这个函数没用了，我换成一个unsigned long long int了
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
    for(int i=7;i>=0;i--){//注意小端
        int index1 = 2*i+1; //低位数字
        int index2 = 2*i;	//高位数字
        int num1 = int(IMSIstr[index1]- '0');
        unsigned char num1c = num1;
        IMSI[7-i] = IMSI[7-i] | num1c;
        int num2 = int(IMSIstr[index2] -'0');
        unsigned char num2c = num2;
        IMSI[7-i] = IMSI[7-i] | (num2c<<4);
    }
    memcpy(regMsg+9,IMSI,8);

}
void MainWindow::proc_timeout(){

    timer->stop();

    if(registerstate == 0){//仍然是第一次注册都没收到回复的状态
        if(Resendcnt <=1){
            Resendcnt++;
            timer->start(5000);
            int num=sendSocket->writeDatagram((char*)regMsg,sizeof(regMsg),PCCaddr,regsendPort);//num返回成功发送的字节数量
            //sendSocket->waitForReadyRead();
            qDebug()<<"第 "<<Resendcnt<<" 次数重发 初始 注册消息，长度为 "<<num<<" 字节";
        }
        else{
            //状态清零，计数器也清零
            Resendcnt = 0;
            Resend_au_cnt = 0;
            registerstate = 0;
            ui->start->setText("开机注册");
            qDebug()<<"第一次注册没用收到authorization command,请点击开机注册按钮重试！";
        }
    }
    else if(registerstate == 1){//这说明是鉴权注册超时了
        if(Resend_au_cnt <=1){
            Resend_au_cnt++;
            timer->start(5000);
            int num=sendSocket->writeDatagram((char*)regMsg,sizeof(regMsg),PCCaddr,regsendPort);//num返回成功发送的字节数量
            //sendSocket->waitForReadyRead();
            qDebug()<<"第 "<<Resendcnt<<" 次数重发 鉴权 注册消息，长度为 "<<num<<" 字节";
        }
        else{
            Resendcnt = 0;
            Resend_au_cnt = 0;
            registerstate = 0;
            ui->start->setText("开机注册");
            qDebug()<<"鉴权注册没有收到 voice register rsp, 请点击开机注册按钮重试！";
        }
    }

}

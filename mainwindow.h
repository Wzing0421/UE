#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QDialog>
#include <QString>
#include <QtNetwork/QUdpSocket>
#include <QHostAddress>
#include <QNetworkInterface>
#include <QMessageBox>
#include <QCryptographicHash>
#include <QDebug>
#include <QTimer>
#include <QtCore/QThread>

#include <iostream>
#include <arpa/inet.h>
#include <string>
#include <cstring>
using namespace std;

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();


    QUdpSocket *regUdpSocket;//接收注册信息用的socket
    QUdpSocket *sendSocket;//用于发送注册信息的socket
    quint16 regRecvPort;//接收注册信息用的端口
    quint16 regsendPort;//向PCC发送注册信息的目的端口
    QHostAddress PCCaddr;//PCC的IP地址
    quint32 localip; //本机IP地址

    QTimer *regtimer;
    int Resendcnt; //用于记录register req的重发次数
    int Resend_au_cnt;//用于记录鉴权注册重发的次数

    //注意都是大端存储,信号以及加上sc2头的信号
    unsigned char regMsg[21],sc2_regMsg[29];//第一次注册所用的注册信息
    unsigned char regMsg_au[37],sc2_regMsg_au[45];//第二次鉴权注册用的注册信息。添加了16字节的MD5用户信息摘要
    unsigned char voiceDeRegisterRsp[8],sc2_voiceDeRegisterRsp[16];//UE对于PCC端的停止注册的回复
    unsigned char voiceDeRegisterReq[10],sc2_voiceDeRegisterReq[18];//UE对于PCC端的停止注册的请求
    unsigned char SC2_header[8];//8字节的SC2接口的头，实际上是ANC加上的，只不过为了简便在这里面就加上了

    enum REG_STATE{//UE注册的状态类型，未注册，鉴权注册过程以及注册成功
        UNREGISTERED, AUTH_PROC, REGISTERED
    };
    REG_STATE registerstate;

    quint32 getlocalIP();//获得本机本地IP

    void init_regMsg(QString QIMSIstr);//用于第一次初始化注册信息
    void init_IMSI(QString &IMSIstr);//初始化8字节的IMSI信息
    void init_voiceDeRegisterRsp();
    void init_voiceDeRegisterReq();
    void init_sc2();//初始化sc2的头


    /*呼叫状态机的状态,我在这里面加了一个U状态，表示的是还没有注册成功的时候的呼叫状态，这个状态不同于U0空闲态*/
    enum CALL_STATE{
       U0,U1,U2,U3,U4,U5,U6,U7,U8,U9,U10,U19
    };
    CALL_STATE callstate;

    QTimer *calltimerT9005;
    QTimer *calltimerT9006;
    QTimer *calltimerT9007;
    QTimer *calltimerT9009;
    QTimer *calltimerT9014;

    /*呼叫状态的变量*/
    unsigned char callSetup[21],sc2_callSetup[29];
    unsigned char callSetupAck[7],sc2_callSetupAck[15];
    unsigned char callAllerting[7],sc2_callAllerting[15];
    unsigned char callConnect[8],sc2_callConnect[16];
    unsigned char callConnectAck[8],sc2_callConnectAck[16];
    unsigned char callDisconnect[7],sc2_callDisconnect[15];
    unsigned char callReleaseRsp[8],sc2_callReleaseRsp[16];

    int CallConnectcnt; //用于记录call connect的重发次数
    int CallDisconnectcnt;
    /*以下是呼叫过程的初始化*/
    void init_callSetup(string calledBCDNumber);
    void init_callSetupAck();
    void init_callAlerting();
    void init_callConnect();
    void init_callConnectAck();
    void init_callDisconnect(int cause);
    void init_callReleaseRsp(int cause);


private slots:

    void on_start_clicked();

    void recvRegInfo();//接收注册消息的回调函数

    void proc_timeout();//注册时候的超时处理，设置T9001=5s
    void call_timeoutT9005();
    void call_timeoutT9006();
    void call_timeoutT9007();
    void call_timeoutT9009();
    void call_timeoutT9014();

    void on_DeReigster_clicked();

    void on_call_clicked();


private:
    Ui::MainWindow *ui;
};

#endif // MAINWINDOW_H

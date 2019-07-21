#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QDialog>
#include <QString>
#include <QtNetwork/QUdpSocket>
#include <QHostAddress>
#include <QNetworkInterface>
#include <QMessageBox>
#include <QDebug>
#include <QTimer>

#include <string>
#include <cstring>
namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

    int registerstate;//表示注册状态；0表示未注册，1表示注册鉴权过程，2表示已经注册
    QUdpSocket *regUdpSocket;//接收注册信息用的socket
    QUdpSocket *sendSocket;//用于发送注册信息的socket
    quint16 regRecvPort;//接收注册信息用的端口
    quint16 regsendPort;//向PCC发送注册信息的目的端口
    QHostAddress PCCaddr;//PCC的IP地址
    QString localip; //本机IP地址

    QTimer *timer;
    int Resendcnt; //用于记录register req的重发次数
    int Resend_au_cnt;//用于记录鉴权注册重发的次数
    unsigned char regMsg[21];//第一次注册所用的注册信息
    unsigned char regMsg_au[23];//第二次鉴权注册用的注册信息

    QString getlocalIP();//获得本机本地IP

    void init_regMsg();//用于第一次初始化注册信息

    void init_IMSI(QString &IMSIstr);//初始化8字节的IMSI信息

private slots:

    void on_start_clicked();

    void recvRegInfo();//接收注册消息的回调函数

    void proc_timeout();//注册时候的超时处理，设置T9001=5s

private:
    Ui::MainWindow *ui;
};

#endif // MAINWINDOW_H

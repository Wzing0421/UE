#ifndef AUDIOSENDTHREAD_H
#define AUDIOSENDTHREAD_H
//这是发送线程

#include <QObject>
#include <QThread>
#include <QDebug>

#include <QAudio>
#include <QAudioFormat>
#include <QAudioInput>
#include <QAudioOutput>
#include <QIODevice>

#include <QtNetwork/QUdpSocket>
#include <QHostAddress>

#include <arpa/inet.h>

#include "config.h"

class audiosendthread : public QThread
{
    Q_OBJECT
public:
    explicit audiosendthread(QObject *parent = nullptr);
    ~audiosendthread();

    QUdpSocket *udpSocket;
    QHostAddress destaddr;//multimedia addr
    //quint16 destport;//multimedia port

    QUdpSocket *Socket2_4;//for 2.4ksocket
    QHostAddress tmpaddr;
    quint16 tmpport;

    QAudioInput *input;
    QIODevice *inputDevice;
    QAudioFormat format;

    unsigned short SN = 0;//表示语音帧头的序列号
    struct video{
        int lens;
        char data[960];
    };

    char sc2_2[12];//sc2_2的头
    char au_data[960];//存储960字节的语音帧

    void setaudioformat(int samplerate, int channelcount, int samplesize);
    void mystart();
    void mystop();

public slots:
    void onReadyRead();

    void readyReadSlot();//udpsocket接收到压缩的数据调用的回调函数，用于加上头并发送到媒体网关

};

#endif // AUDIOSENDTHREAD_H

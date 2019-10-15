#ifndef AUDIOSENDTHREAD_H
#define AUDIOSENDTHREAD_H
//这是发送线程
#include <iostream>
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

#include <QMutex>
#include <QMutexLocker>

#include "config.h"

using namespace std;

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

    QMutex m_Mutex;
    unsigned int SN = 0;//表示语音帧头的序列号,这是我和swh的第一次语音接口，总的头的长度是4字节，只有一个SN号
    int m_CurrentPlayIndex = 0; //每次攒够960就发一包，这个用于记录当前缓冲区还剩多少字节
    QByteArray m_PCMDataBuffer;

    struct video{
        int lens;
        char data[960];
    };

    char sc2_2[12];//sc2_2的头
    char au_data[FRAME_LEN_60ms];//存储960字节的语音帧

    void setaudioformat(int samplerate, int channelcount, int samplesize);
    void mystart();
    void mystop();
    void init_sc2_2();//初始化sc2_2的头

public slots:
    void onReadyRead();

    void readyReadSlot();//udpsocket接收到压缩的数据调用的回调函数，用于加上头并发送到媒体网关

};

#endif // AUDIOSENDTHREAD_H

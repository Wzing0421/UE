#ifndef AUDIO_PLAY_THREAD_H
#define AUDIO_PLAY_THREAD_H
//这是接收线程
#include <QThread>
#include <QObject>
#include <QAudioFormat>
#include <QAudioOutput>
#include <QMutex>
#include <QMutexLocker>
#include <QByteArray>

#include <QtNetwork/QUdpSocket>
#include <QHostAddress>
#include <QNetworkInterface>
#include <QDebug>

#include "config.h"
class AudioPlayThread : public QThread
{
    Q_OBJECT

public:
    AudioPlayThread(QObject *parent = nullptr);
    ~AudioPlayThread();

    // ----------- 添加数据相关 ----------------------------------------
    // 设置当前的PCM Buffer
    void setCurrentBuffer(QByteArray buffer);
    // 添加数据
    void addAudioBuffer(char* pData, int len);
    // 清空当前的数据
    void cleanAllAudioBuffer(void);
    // ------------- End ----------------------------------------------

    // 设置当前的采样率、采样位数、通道数目
    void setCurrentSampleInfo(int sampleRate, int sampleSize, int channelCount);

    virtual void run(void) override;//多线程重载运行函数run

    // 设置音量
    void setCurrentVolumn(qreal volumn);

    void stop();//停止

private:
    QAudioOutput *m_OutPut = nullptr;
    QIODevice *m_AudioIo = nullptr;
    QAudioFormat nFormat;

    QByteArray m_PCMDataBuffer;
    int m_CurrentPlayIndex = 0;

    QMutex m_Mutex;
    // 播放状态
    volatile bool m_IsPlaying = true;

    //for Audio
    QUdpSocket *udpsocket;

    struct video{
        int lens;
        char data[960];
    };

    char au_data[960];

private slots:
    void readyReadSlot();

};

#endif


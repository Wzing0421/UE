#include "audioplaythread.h"

AudioPlayThread::AudioPlayThread(QObject *parent)
    :QThread(parent)
{
    m_PCMDataBuffer.clear();

    udpsocket = new QUdpSocket(this);
    //这个端口用于绑定被叫输出到输出音频设备的端口
    udpsocket->bind(QHostAddress::Any,local_audio_port);
    //connect(udpsocket,SIGNAL(readyRead()),this,SLOT(readyReadSlot()));//收到网络数据报就开始往outputDevice写入，进行播放
}

AudioPlayThread::~AudioPlayThread()
{
    delete udpsocket;
    delete m_OutPut;
    delete m_AudioIo;

}

void AudioPlayThread::setCurrentVolumn(qreal volumn){
    m_OutPut->setVolume(volumn);
}

void AudioPlayThread::setCurrentSampleInfo(int sampleRate, int sampleSize, int channelCount)
{
    QMutexLocker locker(&m_Mutex);

    // Format
    nFormat.setSampleRate(sampleRate);
    nFormat.setSampleSize(sampleSize);
    nFormat.setChannelCount(channelCount);
    nFormat.setCodec("audio/pcm");
    nFormat.setSampleType(QAudioFormat::SignedInt);
    nFormat.setByteOrder(QAudioFormat::LittleEndian);

    if (m_OutPut != nullptr) delete m_OutPut;

    m_OutPut = new QAudioOutput(nFormat);
    m_AudioIo = m_OutPut->start();//原来放在setrcurrentinfo里面的

}

void AudioPlayThread::run(void)
{
    m_IsPlaying = true;
    qDebug()<<"audio receiver starts!";

    connect(udpsocket,&QUdpSocket::readyRead,this,&AudioPlayThread::readyReadSlot);//收到网络数据报就开始往outputDevice写入，进行播放

    while (1)
    {
        QMutexLocker locker(&m_Mutex);
        if (!m_IsPlaying)//用于终止线程
        {
            break;
        }

        if(m_PCMDataBuffer.size() < m_CurrentPlayIndex + FRAME_LEN_60ms){//缓冲区不够播放60ms音频
            //QThread::msleep(10);
            continue;
        }
        else{
            //拷贝960字节的数据
            char *writeData = new char[FRAME_LEN_60ms];
            memcpy(writeData,&m_PCMDataBuffer.data()[m_CurrentPlayIndex], FRAME_LEN_60ms);
            // 写入音频数据
            m_AudioIo->write(writeData, FRAME_LEN_60ms);
            m_CurrentPlayIndex += FRAME_LEN_60ms;
            //qDebug()<<m_CurrentPlayIndex;
            delete []writeData;

            //如果长度超过了MAX_AUDIO_LEN就从
            if(m_CurrentPlayIndex > MAX_AUDIO_LEN){
                m_PCMDataBuffer = m_PCMDataBuffer.right(m_PCMDataBuffer.size()-MAX_AUDIO_LEN);
                m_CurrentPlayIndex -= MAX_AUDIO_LEN;
            }
        }
    }
    disconnect(udpsocket,&QUdpSocket::readyRead,this,&AudioPlayThread::readyReadSlot);
    //m_OutPut->stop();//这个地方也是新加上的
    cleanAllAudioBuffer();
    qDebug()<<"audio receiver stops!";
}

// 添加数据
void AudioPlayThread::addAudioBuffer(char* pData, int len)
{
    QMutexLocker locker(&m_Mutex);

    m_PCMDataBuffer.append(pData, len);
    //m_IsPlaying = true;
}

void AudioPlayThread::cleanAllAudioBuffer(void)
{
    QMutexLocker locker(&m_Mutex);
    m_CurrentPlayIndex = 0;
    m_PCMDataBuffer.clear();
    m_IsPlaying = false;
}

void AudioPlayThread::readyReadSlot(){
    while(udpsocket->hasPendingDatagrams()){
            QHostAddress senderip;
            quint16 senderport;

            /*
            //注意这里把头的长度从2改成12了
            char recvbuf[FRAME_LEN_60ms+12];
            memset(recvbuf,0,sizeof(recvbuf));
            int num = udpsocket->readDatagram(recvbuf,FRAME_LEN_60ms+12,&senderip,&senderport);
            qDebug()<<num;
            //outputDevice->write(vp.data,vp.lens);
            addAudioBuffer(recvbuf+12, FRAME_LEN_60ms);
            */

            //注意这里把头的长度从2改成12了
            char recvbuf[FRAME_LEN_60ms];
            memset(recvbuf,0,sizeof(recvbuf));
            int num = udpsocket->readDatagram(recvbuf,FRAME_LEN_60ms,&senderip,&senderport);
            //qDebug()<<num;
            //outputDevice->write(vp.data,vp.lens);
            //for(int i = 0; i< FRAME_LEN_60ms; i++) printf("%d ",int(recvbuf[i]));
            //printf("\n\n");
            addAudioBuffer(recvbuf, FRAME_LEN_60ms);
    }
}

void AudioPlayThread::stop(){
    QMutexLocker locker(&m_Mutex);
    m_IsPlaying = false;
}


#include "audiosendthread.h"

/*这个audio sned实现了两个功能：鉴于虚拟UE不能产生2.4k的语音帧，所以先产生60ms的128k语音帧给媒体网关的端口
然后压缩成2.4k的语音帧之后，再真正的加上头发送给媒体网关
*/

audiosendthread::audiosendthread(QObject *parent)
    : QThread(parent)
{
    //首先用Socket2_4将128k语音包送到媒体网关的一个端口进行压缩，Socket2_4只负责发送，udpSocket负责接收并发送
    udpSocket = new QUdpSocket(this);
    destaddr.setAddress(mediaGW_addr2);
    //destport = 20000;

    //用于2.4k压缩的目的地址和端口
    Socket2_4 = new QUdpSocket(this);
    tmpaddr.setAddress(mediaGW_addr1);
    //tmpport = 30000;

    SN = 0;//序列号初始化成0

    /*初始化sc2_2接口的头,这里写的很简陋，需要再看一下*/
    sc2_2[0]=0x00;
    memset(sc2_2+1,0,5);//5个字节的callid
    short len24k = 18;
    short len24k_net = htons(len24k);
    memcpy(sc2_2+10,&len24k_net,2);
}
audiosendthread::~audiosendthread(){
    delete udpSocket;
    delete Socket2_4;
    delete input;
    delete inputDevice;
}

void audiosendthread::setaudioformat(int samplerate, int channelcount, int samplesize){
    format.setSampleRate(samplerate);
    format.setChannelCount(channelcount);
    format.setSampleSize(samplesize);
    format.setCodec("audio/pcm");
    format.setSampleType(QAudioFormat::SignedInt);
    format.setByteOrder(QAudioFormat::LittleEndian);

    input = new QAudioInput(format, this);

}

void audiosendthread::mystart(){
    if(!udpSocket -> bind(QHostAddress::Any, 10005)){ qDebug()<<"socket bind error";}
    connect(udpSocket,SIGNAL(readyRead()),this,SLOT(readyReadSlot()));

    SN = 0;
    qDebug()<<"audio sender starts!";
    inputDevice = input->start();
    connect(inputDevice,SIGNAL(readyRead()),this,SLOT(onReadyRead()));


}

void audiosendthread::mystop(){
    qDebug()<<"audio sender stops!";
    SN = 0;
    input->stop();
    udpSocket->close();

}

void audiosendthread::onReadyRead(){

    // read audio from input device
    inputDevice -> read(au_data,FRAME_LEN_60ms);
    char* audio = new char[FRAME_LEN_60ms+2];

    unsigned short SN_net = htons(SN);
    memcpy(audio,&SN_net,sizeof(unsigned short));
    memcpy(audio+sizeof(unsigned short),au_data,FRAME_LEN_60ms);
    SN++;

    //发送到媒体网关的2.4k语音压缩端口
    Socket2_4 -> writeDatagram(audio, FRAME_LEN_60ms+2,tmpaddr,mediaGW_port1);
    delete []audio;

}

void audiosendthread::readyReadSlot(){//收到压缩的2.4k的语音包
    while(udpSocket->hasPendingDatagrams()){
            QHostAddress senderip;
            quint16 senderport;

            char recvbuf[962];//2.4k语音包的长度是18字节，加上2个字节长度的头
            udpSocket->readDatagram(recvbuf,sizeof(recvbuf),&senderip,&senderport);
            //加上头再发送出去
            char* sendbuf = new char[FRAME_LEN_60ms+12];//12字节的头，18字节的裸2.4k语音
            memcpy(sendbuf,sc2_2,sizeof(sc2_2));
            memcpy(sendbuf+6,recvbuf,2);
            memcpy(sendbuf+sizeof(sc2_2),recvbuf+2,FRAME_LEN_60ms);
            udpSocket->writeDatagram(sendbuf,FRAME_LEN_60ms+12,destaddr,mediaGW_port2);
            delete []sendbuf;
    }
}

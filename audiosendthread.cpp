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
    init_sc2_2();
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
    //这个函数的功能是实现第一次压缩到2.4k，因为本地硬件没有能力将其压缩只2.4k所以首先发送128k的语音包
    //首先我是在其头上面加序号的，序号等到之后发送真正的编码的时候再加上
    // read audio from input device
    inputDevice -> read(au_data,FRAME_LEN_60ms);
    char* audio = new char[FRAME_LEN_60ms];

    /*
    unsigned int SN_net = htonl(SN);
    memcpy(audio,&SN_net,sizeof(unsigned int));
    memcpy(audio+sizeof(unsigned int),au_data,FRAME_LEN_60ms);
    SN++;
    */
    //发送到媒体网关的2.4k语音压缩端口，长度应该是962字节
    Socket2_4 -> writeDatagram(audio, FRAME_LEN_60ms,tmpaddr,mediaGW_port1);
    delete []audio;

}

void audiosendthread::readyReadSlot(){//收到压缩的2.4k的语音包
    while(udpSocket->hasPendingDatagrams()){
            QHostAddress senderip;
            quint16 senderport;

            char recvbuf[18];//2.4k语音包的长度是18字节
            udpSocket->readDatagram(recvbuf,sizeof(recvbuf),&senderip,&senderport);
            //加上头再发送出去
            char* sendbuf = new char[FRANE_COMPRESS_60ms + sizeof(sc2_2)];//12字节的头，18字节的裸2.4k语音共30字节
            memcpy(sendbuf,sc2_2,sizeof(sc2_2));
            /*处理三个字节长度的SN*/

            unsigned int SN_net = htonl(SN);
            memcpy(sendbuf + 5,(&SN_net)+1,3);

            memcpy(sendbuf + sizeof(sc2_2), recvbuf, FRANE_COMPRESS_60ms);
            udpSocket->writeDatagram(sendbuf,FRANE_COMPRESS_60ms +sizeof(sc2_2), destaddr,mediaGW_port2);
            SN++;
            delete []sendbuf;
    }
}
void audiosendthread::init_sc2_2(){//初始化sc2_2的头
    /*初始化UEID,我只是不想让UEUD变得全是0*/
    sc2_2[0] = 0x00;
    sc2_2[1] = 0x00;
    sc2_2[2] = 0x00;
    sc2_2[3] = 0x01;
    //媒体方向
    sc2_2[4] = 0x00;//上行

    /*SN序号*/
    sc2_2[5] = 0x00;
    sc2_2[6] = 0x00;
    sc2_2[7] = 0x00;

    //length
    short len24k = 18;
    short len24k_net = htons(len24k);
    memcpy(sc2_2+8,&len24k_net,2);

    //Rev
    sc2_2[10] = 0x00;
    sc2_2[11] = 0x00;
}

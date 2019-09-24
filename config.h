#ifndef CONFIG_H
#define CONFIG_H

#include <QString>

#define MAX_AUDIO_LEN 960000 //如果接收缓冲区大于这个数值就剪掉
#define FRAME_LEN_60ms 960 //每一个语音帧长度是960字节
#define local_audio_port 10004//本地绑定接收语音的端口
#define mediaGW_port1 30000 //用于本机采集语音之后首先交给媒体网关进行压缩，这个媒体网关的目的端口
#define mediaGW_port2 20000 //压缩之后的2.4k语音包交给真正的媒体网关的目的端口
#define regRecvPort 10002 //本机接收注册和呼叫信息的绑定端口
#define regsendPort 50001 //向PCC发送注册信息的目的端口
#define periodtime 3600000 //周期注册的时间是3600s

const QString mediaGW_addr1 = "162.105.85.235"; //用于本机采集语音之后首先交给媒体网关进行压缩，这个媒体网关的目的地址
const QString mediaGW_addr2 = "162.105.85.235"; //压缩之后的2.4k语音包交给真正的媒体网关
const QString ANC_addr = "162.105.85.235";//注册和呼叫时候的ANC的目的地址

#endif // CONFIG_H

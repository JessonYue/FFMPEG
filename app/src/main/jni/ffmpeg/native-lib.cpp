#include <jni.h>
#include <string>
//ffmpeg库 注意这里的extern "C"{}作用
extern "C"{
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libswscale/swscale.h"
}


//打印日志
#include <android/log.h>
#define LOGI(FORMAT,...) __android_log_print(ANDROID_LOG_INFO,"jesson",FORMAT,##__VA_ARGS__);
#define LOGE(FORMAT,...) __android_log_print(ANDROID_LOG_ERROR,"jesson",FORMAT,##__VA_ARGS__);



extern "C"
JNIEXPORT void JNICALL
Java_ffmpeg_jesson_com_ffmpeg_FFmpegPlayer_playMyMedia(JNIEnv *env, jobject instance,
                                                       jstring url_) {
    const char *url = env->GetStringUTFChars(url_, 0);

    LOGI("url:%s", url);
    av_register_all();
    AVCodec *c_temp = av_codec_next(NULL);
    while (c_temp != NULL)
    {
        switch (c_temp->type)
        {
            case AVMEDIA_TYPE_VIDEO:
                LOGI("[Video]:%s", c_temp->name);
                break;
            case AVMEDIA_TYPE_AUDIO:
                LOGI("[Audio]:%s", c_temp->name);
                break;
            default:
                LOGI("[Other]:%s", c_temp->name);
                break;
        }
        c_temp = c_temp->next;
    }

    env->ReleaseStringUTFChars(url_, url);
}

extern "C"
JNIEXPORT void JNICALL
Java_ffmpeg_jesson_com_ffmpeg_FFmpegPlayer_decode(JNIEnv *env, jobject instance, jstring input,jstring output) {
    const char *input_url = env->GetStringUTFChars(input, 0);
    const char *output_url = env->GetStringUTFChars(output, 0);

    //1.注册组件
    av_register_all();

    AVFormatContext *avFormatContext = avformat_alloc_context();
    //2.打开视频文件
    int open = avformat_open_input(&avFormatContext,input_url,NULL,NULL);
    if(open!=0){
        LOGE("%s","打开文件失败");
        return;
    }
    //3.获取视频信息
    if(avformat_find_stream_info(avFormatContext,NULL)<0){
        LOGE("%s","获取视频信息");
        return;
    }
    //4.解码 音频？视频？
    int i=0,AVMEDIA_INDEX = 0;
    for (; i <  avFormatContext->nb_streams; ++i) {
           if(avFormatContext->streams[i]->codecpar->codec_type==AVMEDIA_TYPE_VIDEO){
               AVMEDIA_INDEX = i;
               break;
           }
    }
    //获取视频流对应的解码器
    AVCodecContext *avCodecContext = avFormatContext->streams[AVMEDIA_INDEX]->codec;
    AVCodec *avCodec= avcodec_find_decoder(avCodecContext->codec_id);
    if(avCodec == NULL){
        LOGE("%s","视频解码失败");
    }

    //5.打开解码器
    if(avcodec_open2(avCodecContext,avCodec,NULL)<0){
        LOGE("%s","打开解码器失败");
        return;
    }
    //6.读取压缩的视频数据AVPacket AVFrame
    AVPacket *avPacket;
    avPacket = static_cast<AVPacket *>(av_malloc(sizeof(AVPacket)));
    av_init_packet(avPacket);

    AVFrame *avFrame,*yuvFrame;
    avFrame = av_frame_alloc();
    yuvFrame = av_frame_alloc();

    //分配内存
    uint8_t *out_buffer = static_cast<uint8_t *>(av_malloc(avpicture_get_size(AV_PIX_FMT_YUV420P, avCodecContext->width, avCodecContext->height)));
    avpicture_fill(reinterpret_cast<AVPicture *>(yuvFrame), out_buffer, AV_PIX_FMT_YUV420P, avCodecContext->width, avCodecContext->height);

    FILE *yuv = fopen(output_url,"wb");

    struct SwsContext *swsContext = sws_getContext(avCodecContext->width,avCodecContext->height,avCodecContext->pix_fmt,
                   avCodecContext->width,avCodecContext->height,AV_PIX_FMT_YUV420P,
                    SWS_BILINEAR,NULL,NULL,NULL);

    int len =0 ,got_frame=0,frame_count=0;

    while(av_read_frame(avFormatContext,avPacket)>=0){
        //AVPacket 进行解码
        len = avcodec_decode_video2(avCodecContext,avFrame,&got_frame,avPacket);
        // 非0 正在解码
        if(got_frame){
            //frame--->yuv420p
            sws_scale(swsContext,avFrame->data,avFrame->linesize,0,avFrame->height,
                        yuvFrame->data,yuvFrame->linesize);

            int yuv_size = avCodecContext->width * avCodecContext->height;
            fwrite(yuvFrame->data[0], 1, static_cast<size_t>(yuv_size), yuv);
            fwrite(yuvFrame->data[1], 1, static_cast<size_t>(yuv_size / 4), yuv);
            fwrite(yuvFrame->data[2], 1, static_cast<size_t>(yuv_size / 4), yuv);

            LOGE("解码%d帧",frame_count++);
        }
        av_free_packet(avPacket);
    }

    fclose(yuv);

    av_frame_clone(avFrame);
    avcodec_close(avCodecContext);
    avformat_free_context(avFormatContext);

    env->ReleaseStringUTFChars(input, input_url);
    env->ReleaseStringUTFChars(output, output_url);
}
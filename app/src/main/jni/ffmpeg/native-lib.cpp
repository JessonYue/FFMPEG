#include <jni.h>
#include <string>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <android/native_window_jni.h>
#include <android/native_window.h>

//ffmpeg库 注意这里的extern "C"{}作用
extern "C"{
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libswscale/swscale.h" //缩放
#include "libavutil/pixfmt.h"
#include "libyuv.h"
#include "libswresample/swresample.h"  //音频解码
}


//打印日志
#include <android/log.h>
#include <cstdio>

#define LOGI(FORMAT,...) __android_log_print(ANDROID_LOG_INFO,"jesson",FORMAT,##__VA_ARGS__);
#define LOGE(FORMAT,...) __android_log_print(ANDROID_LOG_ERROR,"jesson",FORMAT,##__VA_ARGS__);

#define MAX_AUDIO_FRME_SIZE 48000 * 4

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

/***
 * 开始渲染 Android的原生绘制
 */
extern "C"
JNIEXPORT void JNICALL
Java_ffmpeg_jesson_com_ffmpeg_FFmpegPlayer_beginrender(JNIEnv *env, jobject instance,
                                                       jstring input_, jobject surface) {
    const char* input_cstr = env->GetStringUTFChars(input_,NULL);
    //1.注册组件
    av_register_all();

    //封装格式上下文
    AVFormatContext *pFormatCtx = avformat_alloc_context();

    //2.打开输入视频文件
    if(avformat_open_input(&pFormatCtx,input_cstr,NULL,NULL) != 0){
        LOGE("%s","打开输入视频文件失败");
        return;
    }
    //3.获取视频信息
    if(avformat_find_stream_info(pFormatCtx,NULL) < 0){
        LOGE("%s","获取视频信息失败");
        return;
    }

    //视频解码，需要找到视频对应的AVStream所在pFormatCtx->streams的索引位置
    int video_stream_idx = -1;
    int i = 0;
    for(; i < pFormatCtx->nb_streams;i++){
        //根据类型判断，是否是视频流
        if(pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO){
            video_stream_idx = i;
            break;
        }
    }

    //4.获取视频解码器
    AVCodecContext *pCodeCtx = pFormatCtx->streams[video_stream_idx]->codec;
    AVCodec *pCodec = avcodec_find_decoder(pCodeCtx->codec_id);
    if(pCodec == NULL){
        LOGE("%s","无法解码");
        return;
    }

    //5.打开解码器
    if(avcodec_open2(pCodeCtx,pCodec,NULL) < 0){
        LOGE("%s","解码器无法打开");
        return;
    }

    //编码数据
    AVPacket *packet = (AVPacket *)av_malloc(sizeof(AVPacket));

    //像素数据（解码数据）
    AVFrame *yuv_frame = av_frame_alloc();
    AVFrame *rgb_frame = av_frame_alloc();

    //native绘制
    //窗体
    ANativeWindow* nativeWindow = ANativeWindow_fromSurface(env,surface);
    //绘制时的缓冲区
    ANativeWindow_Buffer outBuffer;

    int len ,got_frame, framecount = 0;
    //6.一阵一阵读取压缩的视频数据AVPacket
    while(av_read_frame(pFormatCtx,packet) >= 0){
        //解码AVPacket->AVFrame
        len = avcodec_decode_video2(pCodeCtx, yuv_frame, &got_frame, packet);

        //Zero if no frame could be decompressed
        //非零，正在解码
        if(got_frame){
            LOGI("解码%d帧",framecount++);
            //lock
            //设置缓冲区的属性（宽、高、像素格式）
            ANativeWindow_setBuffersGeometry(nativeWindow, pCodeCtx->width, pCodeCtx->height,WINDOW_FORMAT_RGBA_8888);
            ANativeWindow_lock(nativeWindow,&outBuffer,NULL);

            //设置rgb_frame的属性（像素格式、宽高）和缓冲区
            //rgb_frame缓冲区与outBuffer.bits是同一块内存
            avpicture_fill((AVPicture *)rgb_frame, static_cast<const uint8_t *>(outBuffer.bits), AV_PIX_FMT_RGBA, pCodeCtx->width, pCodeCtx->height);

            //YUV->RGBA_8888
            libyuv::I420ToARGB(yuv_frame->data[0],yuv_frame->linesize[0],
                       yuv_frame->data[2],yuv_frame->linesize[2],
                       yuv_frame->data[1],yuv_frame->linesize[1],
                       rgb_frame->data[0], rgb_frame->linesize[0],
                       pCodeCtx->width,pCodeCtx->height);

            //unlock
            ANativeWindow_unlockAndPost(nativeWindow);

            usleep(1000 * 16);

        }

        av_free_packet(packet);
    }

    ANativeWindow_release(nativeWindow);
    av_frame_free(&yuv_frame);
    avcodec_close(pCodeCtx);
    avformat_free_context(pFormatCtx);

    env->ReleaseStringUTFChars(input_,input_cstr);
}



extern "C"
JNIEXPORT void JNICALL
/***
 * 音频解码 pcm数据
 */
Java_ffmpeg_jesson_com_ffmpeg_FFmpegPlayer_beginsound(JNIEnv *env, jobject instance, jstring input_,
                                                      jstring output_) {
    const char *input = env->GetStringUTFChars(input_, 0);
    const char *output = env->GetStringUTFChars(output_, 0);

    //注册组件
    av_register_all();
    AVFormatContext *avFormatContext = avformat_alloc_context();

    if(avformat_open_input(&avFormatContext,input,NULL,NULL)!=0){
        LOGI("文件无法打开");
        return;
    }
    if(avformat_find_stream_info(avFormatContext,NULL)<0){
        LOGI("avformat_find_stream_info==>error");
        return;
    }
    //获取音频流信息
    int i=0,audio_stream_id = -1;
    for (; i < avFormatContext->nb_streams; ++i) {
        if(avFormatContext->streams[i]->codec->codec_type==AVMEDIA_TYPE_AUDIO){
            audio_stream_id = i;
            break;
        }
    }
    //获取解码器
    AVCodecContext *avCodecContext = avFormatContext->streams[audio_stream_id]->codec;
    AVCodec *avCodec = avcodec_find_decoder(avCodecContext->codec_id);
    if(avCodec == NULL){
        LOGI("无法获取解码器");
        return;
    }

    if(avcodec_open2(avCodecContext,avCodec,NULL)<0){
        LOGI("无法打开解码器");
        return;
    }

    AVFrame *avFrame = av_frame_alloc();
    //AVPacket *avPacket = av_packet_alloc();
    AVPacket *avPacket = (AVPacket *)av_malloc(sizeof(AVPacket));
    int got_frame = 0;
    SwrContext *swrContext = swr_alloc();
    enum AVSampleFormat  in_sample_fmt = avCodecContext->sample_fmt;

    int out_sample_rate = 44100;
    int in_sample_rate = avCodecContext->sample_rate;
    int index = 0;

    //重新采样设置的参数
    swr_alloc_set_opts(swrContext, AV_CH_LAYOUT_STEREO, AV_SAMPLE_FMT_S16
                      ,out_sample_rate,avCodecContext->channel_layout,
                       in_sample_fmt,in_sample_rate, 0,NULL);
    swr_init(swrContext);

    //输出声道的个数
    int out_channle_nb = av_get_channel_layout_nb_channels(AV_CH_LAYOUT_STEREO);
    //格式转换 16位pcm 44100hz 采样率
    uint8_t *out = static_cast<uint8_t *>(av_malloc(MAX_AUDIO_FRME_SIZE));

    //打印原始音频数据
    LOGI(" bit_rate = %d \r\n", avCodecContext->bit_rate);
    LOGI(" sample_rate = %d \r\n", avCodecContext->sample_rate);
    LOGI(" channels = %d \r\n", avCodecContext->channels);
    LOGI(" code_name = %s \r\n",avCodecContext->codec->name);

    FILE *fp_pcm = fopen(output,"wb");

    while(av_read_frame(avFormatContext,avPacket)>=0){
        //解码
        if(avcodec_decode_audio4(avCodecContext,avFrame,&got_frame,avPacket)<0){
            LOGI("解码完成");
        }
        if(got_frame>0){
            LOGI("解码：%d",index++);
            swr_convert(swrContext, &out, MAX_AUDIO_FRME_SIZE,
                        (const uint8_t**)(avFrame->data), avFrame->nb_samples);
            int out_buffer_size = av_samples_get_buffer_size(NULL,out_channle_nb,avFrame->nb_samples,
            AV_SAMPLE_FMT_S16,1);
            fwrite(out,1,out_buffer_size,fp_pcm);
        }
        av_free_packet(avPacket);
    }

    //释放资源

    fclose(fp_pcm);
    av_frame_free(&avFrame);
    av_free(out);
    swr_free(&swrContext);
    avcodec_close(avCodecContext);
    avformat_close_input(&avFormatContext);

    env->ReleaseStringUTFChars(input_, input);
    env->ReleaseStringUTFChars(output_, output);

}

/**音频播放使用audio_track**/
extern "C"
JNIEXPORT void JNICALL
Java_ffmpeg_jesson_com_ffmpeg_FFmpegPlayer_beginplay(JNIEnv *env, jobject instance, jstring input_,
                                                     jstring output_) {
    const char *input = env->GetStringUTFChars(input_, 0);
    const char *output = env->GetStringUTFChars(output_, 0);
    //注册组件
    av_register_all();
    AVFormatContext *avFormatContext = avformat_alloc_context();

    if(avformat_open_input(&avFormatContext,input,NULL,NULL)!=0){
        LOGI("文件无法打开");
        return;
    }
    if(avformat_find_stream_info(avFormatContext,NULL)<0){
        LOGI("avformat_find_stream_info==>error");
        return;
    }
    //获取音频流信息
    int i=0,audio_stream_id = -1;
    for (; i < avFormatContext->nb_streams; ++i) {
        if(avFormatContext->streams[i]->codec->codec_type==AVMEDIA_TYPE_AUDIO){
            audio_stream_id = i;
            break;
        }
    }
    //获取解码器
    AVCodecContext *avCodecContext = avFormatContext->streams[audio_stream_id]->codec;
    AVCodec *avCodec = avcodec_find_decoder(avCodecContext->codec_id);
    if(avCodec == NULL){
        LOGI("无法获取解码器");
        return;
    }

    if(avcodec_open2(avCodecContext,avCodec,NULL)<0){
        LOGI("无法打开解码器");
        return;
    }

    AVFrame *avFrame = av_frame_alloc();
    //AVPacket *avPacket = av_packet_alloc();
    AVPacket *avPacket = (AVPacket *)av_malloc(sizeof(AVPacket));
    int got_frame = 0;
    SwrContext *swrContext = swr_alloc();
    enum AVSampleFormat  in_sample_fmt = avCodecContext->sample_fmt;

    int out_sample_rate = 44100;
    int in_sample_rate = avCodecContext->sample_rate;
    int index = 0;

    //重新采样设置的参数
    swr_alloc_set_opts(swrContext, AV_CH_LAYOUT_STEREO, AV_SAMPLE_FMT_S16
            ,out_sample_rate,avCodecContext->channel_layout,
                       in_sample_fmt,in_sample_rate, 0,NULL);
    swr_init(swrContext);

    //输出声道的个数
    int out_channle_nb = av_get_channel_layout_nb_channels(AV_CH_LAYOUT_STEREO);
    //格式转换 16位pcm 44100hz 采样率
    uint8_t *out = static_cast<uint8_t *>(av_malloc(MAX_AUDIO_FRME_SIZE));

    //打印原始音频数据
    LOGI(" bit_rate = %d \r\n", avCodecContext->bit_rate);
    LOGI(" sample_rate = %d \r\n", avCodecContext->sample_rate);
    LOGI(" channels = %d \r\n", avCodecContext->channels);
    LOGI(" code_name = %s \r\n",avCodecContext->codec->name);

    //增加jni aduio_track 播放pcm
    jclass player = env->GetObjectClass(instance);
    //构造andiotrack对象
    jmethodID create_audio_track_mid = env->GetMethodID(player,"createAudioTrack","(II)Landroid/media/AudioTrack;");
    jobject audio_track = env->CallObjectMethod(instance,create_audio_track_mid,out_sample_rate,out_channle_nb);

    //调用AudioTrack.play方法
    jclass audio_track_class = env->GetObjectClass(audio_track);
    jmethodID audio_track_play_mid = env->GetMethodID(audio_track_class,"play","()V");
    env->CallVoidMethod(audio_track,audio_track_play_mid);

    //AudioTrack.write
    jmethodID audio_track_write_mid = env->GetMethodID(audio_track_class,"write","([BII)I");

    FILE *fp_pcm = fopen(output,"wb");

    while(av_read_frame(avFormatContext,avPacket)>=0){
        //解码
        if(avcodec_decode_audio4(avCodecContext,avFrame,&got_frame,avPacket)<0){
            LOGI("解码完成");
        }
        if(got_frame>0){
            LOGI("解码：%d",index++);
            swr_convert(swrContext, &out, MAX_AUDIO_FRME_SIZE,
                        (const uint8_t**)(avFrame->data), avFrame->nb_samples);
            int out_buffer_size = av_samples_get_buffer_size(NULL,out_channle_nb,avFrame->nb_samples,
                                                             AV_SAMPLE_FMT_S16,1);
            fwrite(out,1,out_buffer_size,fp_pcm);

            //out_buffer缓冲区数据，转成byte数组
            jbyteArray audio_sample_array = env->NewByteArray(out_buffer_size);
            jbyte* sample_bytep = env->GetByteArrayElements(audio_sample_array,NULL);
            //out_buffer的数据复制到sampe_bytep
            memcpy(sample_bytep,out,out_buffer_size);
            //同步
            env->ReleaseByteArrayElements(audio_sample_array,sample_bytep,0);

            //AudioTrack.write PCM数据
            env->CallIntMethod(audio_track,audio_track_write_mid,
                                  audio_sample_array,0,out_buffer_size);
            //释放局部引用
            env->DeleteLocalRef(audio_sample_array);
            usleep(1000 * 16);
        }
        av_free_packet(avPacket);
    }

    //释放资源

    fclose(fp_pcm);
    av_frame_free(&avFrame);
    av_free(out);
    swr_free(&swrContext);
    avcodec_close(avCodecContext);
    avformat_close_input(&avFormatContext);



    env->ReleaseStringUTFChars(input_, input);
    env->ReleaseStringUTFChars(output_, output);
}
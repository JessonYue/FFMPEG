#include <jni.h>
#include <string>
//ffmpeg库 注意这里的extern "C"{}作用
extern "C"{
#include "libavformat/avformat.h"
}


//打印日志
#include <android/log.h>
#define LOGI(FORMAT,...) __android_log_print(ANDROID_LOG_INFO,"jesson",FORMAT,##__VA_ARGS__);
#define LOGE(FORMAT,...) __android_log_print(ANDROID_LOG_ERROR,"jesson",FORMAT,##__VA_ARGS__);


extern "C" JNIEXPORT jstring JNICALL
Java_ffmpeg_jesson_com_ffmpeg_MainActivity_stringFromJNI(
        JNIEnv *env,
        jobject /* this */) {
    std::string hello = "Hello from C++";
    return env->NewStringUTF(hello.c_str());
}

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
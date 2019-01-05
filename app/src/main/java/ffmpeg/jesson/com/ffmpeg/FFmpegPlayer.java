package ffmpeg.jesson.com.ffmpeg;

import android.view.Surface;

public class FFmpegPlayer {
    static
    {
        System.loadLibrary("avutil-55");
        System.loadLibrary("swresample-2");
        System.loadLibrary("avcodec-57");
        System.loadLibrary("avformat-57");
        System.loadLibrary("swscale-4");
        System.loadLibrary("postproc-54");
        System.loadLibrary("avfilter-6");
        System.loadLibrary("avdevice-57");
        System.loadLibrary("jessonffmpeg");
        System.loadLibrary("yuv_static");
    }
    public native void playMyMedia(String url);
    public native void decode(String input,String output);
    public native void beginrender(String input,Surface surface);
}

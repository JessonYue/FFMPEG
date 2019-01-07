package ffmpeg.jesson.com.ffmpeg;

import android.Manifest;
import android.annotation.SuppressLint;
import android.os.Environment;
import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.view.Surface;
import android.view.View;

import com.tbruyelle.rxpermissions2.RxPermissions;

import java.io.File;


public class MainActivity extends AppCompatActivity {
    FFmpegPlayer fFmpegPlayer;
    final RxPermissions rxPermissions = new RxPermissions(this);
    PlayerView playerview;

    @SuppressLint("CheckResult")
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        playerview = findViewById(R.id.surfaceview);

        String input = Environment.getExternalStorageDirectory().getAbsolutePath()+"/test.mp4";
        String output = Environment.getExternalStorageDirectory().getAbsolutePath()+"/out";

        rxPermissions
                .request(Manifest.permission.READ_EXTERNAL_STORAGE,
                        Manifest.permission.WRITE_EXTERNAL_STORAGE)
                .subscribe(granted -> {
                    if (granted) { // Always true pre-M
                        fFmpegPlayer = new FFmpegPlayer();
                        //fFmpegPlayer.playMyMedia("http://www.qq.com");
                        //fFmpegPlayer.decode(input,output);
                    } else {
                        // Oups permission denied
                    }
                });
    }

    /**开始播放、渲染视频**/
    public void begin(View view) {
        String input = Environment.getExternalStorageDirectory().getAbsolutePath()+"/test.mp4";
        //Surface传入到Native函数中，用于绘制
        Surface surface = playerview.getHolder().getSurface();
        fFmpegPlayer.beginrender(input, surface);
    }

    /**音频解析**/
    public void sound(View view) {
        String input = Environment.getExternalStorageDirectory().getAbsolutePath()+"/test.mp3";
        String output = Environment.getExternalStorageDirectory().getAbsolutePath()+"/out.pcm";
        //fFmpegPlayer.beginsound(input,output);
        fFmpegPlayer.beginplay(input,output);
    }
}

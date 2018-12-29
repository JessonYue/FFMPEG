package ffmpeg.jesson.com.ffmpeg;

import android.Manifest;
import android.annotation.SuppressLint;
import android.os.Environment;
import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.widget.TextView;

import com.tbruyelle.rxpermissions2.RxPermissions;

import java.io.File;

public class MainActivity extends AppCompatActivity {
    FFmpegPlayer fFmpegPlayer;
    final RxPermissions rxPermissions = new RxPermissions(this);

    @SuppressLint("CheckResult")
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        String input = Environment.getExternalStorageDirectory().getAbsolutePath()+"/test.mp4";
        String output = Environment.getExternalStorageDirectory().getAbsolutePath()+"/out";

        rxPermissions
                .request(Manifest.permission.READ_EXTERNAL_STORAGE,
                        Manifest.permission.WRITE_EXTERNAL_STORAGE)
                .subscribe(granted -> {
                    if (granted) { // Always true pre-M
                        fFmpegPlayer = new FFmpegPlayer();
                        //fFmpegPlayer.playMyMedia("http://blog.csdn.net/ywl5320");
                        fFmpegPlayer.decode(input,output);
                    } else {
                        // Oups permission denied
                    }
                });
    }
}

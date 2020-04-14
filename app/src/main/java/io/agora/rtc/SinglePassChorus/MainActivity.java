package io.agora.rtc.SinglePassChorus;

import android.Manifest;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.support.annotation.NonNull;
import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.view.View;
import android.widget.Toast;

import io.agora.rtc.SinglePassChorus.utils.AppUtil;

public class MainActivity extends AppCompatActivity {
    private static final int PERMISSION_REQUEST_A = 101;
    private static final int PERMISSION_REQUEST_B = 102;
    private static final int PERMISSION_REQUEST_CONSUMER = 103;

    private String[] needPermissions = {Manifest.permission.WRITE_EXTERNAL_STORAGE,
            Manifest.permission.RECORD_AUDIO, Manifest.permission.CAMERA};

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
    }

    public void onClickA(View view) {
        if (AppUtil.checkAndRequestAppPermission(this, needPermissions, PERMISSION_REQUEST_A))
            startActivity(new Intent(this, BroadcasterAActivity.class));
    }

    public void onClickB(View view) {
        if (AppUtil.checkAndRequestAppPermission(this, needPermissions, PERMISSION_REQUEST_B))
            startActivity(new Intent(this, BroadcasterBActivity.class));
    }

    public void onClickConsumer(View view) {
        if (AppUtil.checkAndRequestAppPermission(this, needPermissions, PERMISSION_REQUEST_CONSUMER))
            startActivity(new Intent(this, AudienceActivity.class));
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions, @NonNull int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        for (int grantResult : grantResults) {
            if (grantResult != PackageManager.PERMISSION_GRANTED) {
                Toast.makeText(this, "No Permission!", Toast.LENGTH_SHORT).show();
                return;
            }
        }

        switch (requestCode) {
            case PERMISSION_REQUEST_A:
                startActivity(new Intent(this, BroadcasterAActivity.class));
                break;
            case PERMISSION_REQUEST_B:
                startActivity(new Intent(this, BroadcasterBActivity.class));
                break;
            case PERMISSION_REQUEST_CONSUMER:
                startActivity(new Intent(this, AudienceActivity.class));
                break;
        }
    }
}

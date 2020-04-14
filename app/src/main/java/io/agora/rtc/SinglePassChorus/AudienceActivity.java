package io.agora.rtc.SinglePassChorus;

import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.util.Log;
import android.view.SurfaceView;
import android.widget.FrameLayout;
import android.widget.TextView;
import android.widget.Toast;

import io.agora.rtc.Constants;
import io.agora.rtc.IRtcEngineEventHandler;
import io.agora.rtc.RtcEngine;
import io.agora.rtc.plugin.rawdata.VideoRawDataApi;
import io.agora.rtc.video.VideoCanvas;

public class AudienceActivity extends AppCompatActivity {

    private FrameLayout flLocalVideo, flRemoteVideo;

    RtcEngine mRtcEngine;
    String path = "/assets/yiquxiangsi.mp3";
    IRtcEngineEventHandler mRtcEngineEventHandler = new IRtcEngineEventHandler() {
        @Override
        public void onJoinChannelSuccess(String channel, int uid, int elapsed) {
            super.onJoinChannelSuccess(channel, uid, elapsed);
            runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    ((TextView)(AudienceActivity.this.findViewById(R.id.tv_join_status))).setText("join success");
                }
            });
        }

        @Override
        public void onError(int err) {
            super.onError(err);
            Log.e("Chorus", "err:" + err);
        }

        @Override
        public void onUserJoined(final int uid, int elapsed) {
            if (uid == 20000) {
                runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        setupRemoteVideo(uid);
                    }
                });
            }
        }
    };

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_audience);
        flLocalVideo = findViewById(R.id.fl_local_video);
        flRemoteVideo = findViewById(R.id.fl_remote_video);
        initRtcEngine();
    }

    private void initRtcEngine() {
        try {
            mRtcEngine = RtcEngine.create(getApplicationContext(), getString(R.string.agora_app_id), mRtcEngineEventHandler);
        } catch (Exception e) {
            Toast.makeText(this, "初始化失败", Toast.LENGTH_LONG).show();
            e.printStackTrace();
            return;
        }
        mRtcEngine.setParameters("{\"rtc.log_filter\": 65535}");
        mRtcEngine.setChannelProfile(Constants.CHANNEL_PROFILE_LIVE_BROADCASTING);
        mRtcEngine.setClientRole(Constants.CLIENT_ROLE_AUDIENCE);
        mRtcEngine.muteRemoteAudioStream(10000, true);
        mRtcEngine.muteRemoteVideoStream(10000, true);
        mRtcEngine.enableVideo();
        mRtcEngine.joinChannel(null, ConstantsApp.CHANNEL_NAME, "", 0);
    }

    private void setupLocalVideo() {
        SurfaceView localSurfaceView = RtcEngine.CreateRendererView(this);
        mRtcEngine.setupLocalVideo(new VideoCanvas(localSurfaceView, Constants.RENDER_MODE_FIT, 0));
        flLocalVideo.addView(localSurfaceView);
    }

    private void setupRemoteVideo(int uid) {
        if (flRemoteVideo.getChildCount() > 0)
            flRemoteVideo.removeAllViews();

        SurfaceView remoteSurfaceView = RtcEngine.CreateRendererView(this);
        mRtcEngine.setupRemoteVideo(new VideoCanvas(remoteSurfaceView, Constants.RENDER_MODE_FIT, uid));
        flRemoteVideo.addView(remoteSurfaceView);
    }

    @Override
    protected void onDestroy() {
        mRtcEngine.leaveChannel();
        RtcEngine.destroy();
        super.onDestroy();
    }
}

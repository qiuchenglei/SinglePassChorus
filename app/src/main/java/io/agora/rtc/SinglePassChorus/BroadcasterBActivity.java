package io.agora.rtc.SinglePassChorus;

import android.os.Bundle;
import android.support.v7.app.AppCompatActivity;
import android.text.TextUtils;
import android.util.Log;
import android.view.SurfaceView;
import android.widget.FrameLayout;
import android.widget.TextView;
import android.widget.Toast;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.Arrays;
import java.util.LinkedList;

import io.agora.rtc.Constants;
import io.agora.rtc.IAudioFrameObserver;
import io.agora.rtc.IRtcEngineEventHandler;
import io.agora.rtc.RtcEngine;
import io.agora.rtc.plugin.rawdata.VideoRawDataApi;
import io.agora.rtc.video.VideoCanvas;

public class BroadcasterBActivity extends AppCompatActivity {

    private static final int SAMPLE_RATE = 44100;
    private static final int SAMPLE_PER_CALL = 882;
    private static final int CHANNEL = 2;

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
                    ((TextView)(BroadcasterBActivity.this.findViewById(R.id.tv_join_status))).setText("join success");
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
            runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    setupRemoteVideo(uid);
                    VideoRawDataApi.getInstance().updateMixingUid(uid);
                }
            });
        }

        @Override
        public void onUserOffline(final int uid, int reason) {
            super.onUserOffline(uid, reason);

            runOnUiThread(new Runnable() {
                @Override
                public void run() {
                }
            });
        }

    };

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_broadcaster_b);
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
        mRtcEngine.setClientRole(Constants.CLIENT_ROLE_BROADCASTER);

        mRtcEngine.setAudioProfile(Constants.AUDIO_PROFILE_MUSIC_STANDARD_STEREO, Constants.AUDIO_SCENARIO_GAME_STREAMING);
        mRtcEngine.setParameters("{\"che.audio.enable.ns\":false}");
        mRtcEngine.setParameters("{\"che.audio.enable.aec\":false}");
        mRtcEngine.setParameters("{\"che.audio.enable.agc\":false}");

        mRtcEngine.setRecordingAudioFrameParameters(SAMPLE_RATE, CHANNEL, Constants.RAW_AUDIO_FRAME_OP_MODE_READ_WRITE, SAMPLE_PER_CALL);
        mRtcEngine.setPlaybackAudioFrameParameters(SAMPLE_RATE, CHANNEL, Constants.RAW_AUDIO_FRAME_OP_MODE_READ_ONLY, SAMPLE_PER_CALL);
        mRtcEngine.registerAudioFrameObserver(audioFrameObserver);

        mRtcEngine.enableVideo();
        setupLocalVideo();

        mRtcEngine.setVideoProfile(Constants.VIDEO_PROFILE_360P, true);
        VideoRawDataApi.getInstance().registerMixing();
        mRtcEngine.joinChannel(null, ConstantsApp.CHANNEL_NAME, "", 20000);

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

//    private static final int DEVIATION_DEFAULT = 200 / (SAMPLE_PER_CALL * 1000 / SAMPLE_RATE / CHANNEL) ; //即偏移延迟200ms.（测试小米5适合18，荣耀7x适合21，其他机型均在此值上下浮动）

    private int deviation = -1;
    private byte[] silentBuffer;
    private int throwCount;
    private LinkedList<byte[]> playbackBytes = new LinkedList<>();

    private volatile boolean isFinish = false;

    IAudioFrameObserver audioFrameObserver = new IAudioFrameObserver() {
        @Override
        public boolean onRecordFrame(byte[] bytes, int i, int i1, int i2, int i3) {
            if (isFinish)
                return true;
            int playDelay;
            String dealyStr = mRtcEngine.getParameter("che.audio.android_loopback_delay", "-1");
            if (!TextUtils.isEmpty(dealyStr)) {
                playDelay = Integer.parseInt(dealyStr);
            } else {
                playDelay = -1;
            }

            int timePerCall = SAMPLE_PER_CALL * 1000 / SAMPLE_RATE / CHANNEL;
            if (playDelay > 0) {
                int add = deviation % timePerCall < 5 ? 0 : 1;
                deviation = playDelay / timePerCall + add;
                VideoRawDataApi.getInstance().updateMixingDelay(playDelay);
            } else if (deviation < 0) {
                deviation = 200 / timePerCall;    //默认偏移延迟200ms.
            }

            Log.e("AGORA_2222", "playDelay: " + playDelay + ", deviation: " + deviation + ",bytes length:" + bytes.length + "I=" +i+"," + i1 + "," + i2 + "," + i3);

            if (silentBuffer == null || silentBuffer.length != bytes.length) {
                silentBuffer = new byte[bytes.length];
                Arrays.fill(silentBuffer, (byte) 0);
            }

            if (throwCount < deviation) {
                throwCount++;
                mergeArray(bytes, silentBuffer);
            } else {
                if (playbackBytes.isEmpty()) {
                    mergeArray(bytes, silentBuffer);
                } else {
                    alignDeviation();
                    mergeArray(bytes, playbackBytes.pop());
                }
            }
            return true;
    }

        @Override
        public boolean onPlaybackFrame(byte[] bytes, int i, int i1, int i2, int i3) {
            if (isFinish)
                return true;
            playbackBytes.add(bytes);
            return true;
        }
    };


    private void reset() {
        throwCount = 0;
        deviation = -1;
        playbackBytes.clear();
    }

    private void alignDeviation() {
        if (throwCount > deviation) {
            throwCount--;
            if (!playbackBytes.isEmpty()) {
                playbackBytes.pop();
            } else {
                reset();
            }
            alignDeviation();
        }
    }

    public static void mergeArray(byte[] bytes1, byte[] bytes2) {
        if (bytes1.length != bytes2.length)
            return;

        int length = bytes1.length;

        if (length % 2 != 0)
            length = length - 1;

        short[] shorts1 = new short[length / 2];
        short[] shorts2 = new short[length / 2];
        short[] shortsResult = new short[length / 2];
        ByteBuffer.wrap(bytes1).order(ByteOrder.LITTLE_ENDIAN).asShortBuffer().get(shorts1);
        ByteBuffer.wrap(bytes2).order(ByteOrder.LITTLE_ENDIAN).asShortBuffer().get(shorts2);

        for (int i = 0; i < shorts1.length; i++) {
            int resultInt = ((int) shorts1[i]) + ((int) shorts2[i] / 2);
            if (resultInt > 32767) {
                resultInt = 32767;
            } else if (resultInt < -32767) {
                resultInt = -32767;
            }

            shortsResult[i] = (short) (resultInt /*/ 2*/);
        }

        ByteBuffer.wrap(bytes1).order(ByteOrder.LITTLE_ENDIAN).asShortBuffer().put(shortsResult);
    }

    @Override
    protected void onDestroy() {
        isFinish = true;
        mRtcEngine.registerAudioFrameObserver(null);
        VideoRawDataApi.getInstance().unRegisterMixing();
        mRtcEngine.leaveChannel();
        RtcEngine.destroy();
        super.onDestroy();
    }
}

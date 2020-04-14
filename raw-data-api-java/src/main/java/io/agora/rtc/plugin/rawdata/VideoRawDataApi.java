package io.agora.rtc.plugin.rawdata;

public class VideoRawDataApi {
    static {
        System.loadLibrary("apm-plugin-video-raw-data");
    }

    private VideoRawDataApi() {
    }

    private static final VideoRawDataApi rawDataApi = new VideoRawDataApi();

    public static VideoRawDataApi getInstance() {
        return rawDataApi;
    }

    public boolean registerMixing() {
        return registerVideoRawData();
    }

    public void updateMixingUid(int uid) {
        setVideoMixingLayoutsUid(uid);
    }

    public void updateMixingDelay(int delay) {
        setBufferDelay(delay);
    }

    public void unRegisterMixing() {
        unRegisterVideoRawData();
    }

    private native boolean registerVideoRawData();

    private native void setVideoMixingLayoutsUid(int uid);

    private native void setBufferDelay(int delay);

    private native void unRegisterVideoRawData();

}

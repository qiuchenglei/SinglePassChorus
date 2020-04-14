//
// Created by ChengleiQiu on 2019/4/18.
//
#include <jni.h>
#include <android/log.h>
#include <agora/IAgoraMediaEngine.h>

#include <map>
#include <list>
#include <libyuv.h>
#include <agora/IAgoraRtcEngine.h>
#include <sys/time.h>

using namespace std;

int64_t getCurrentTime() {
    struct timeval tv;
    gettimeofday(&tv, NULL);

    int64_t tv_sec = tv.tv_sec;
    return tv_sec * 1000 + tv.tv_usec / 1000;
}

class VideoFrameObserver : public agora::media::IVideoFrameObserver {
public:
    VideoFrameObserver() {
    }

    ~VideoFrameObserver() {
        _listRemoteVideoFrameBuffer.clear();
    }

    void mixVideoFrame(VideoFrame &localVideoFrame) {

        if (_listRemoteVideoFrameBuffer.empty())
            return;

        int64_t localRenderTime = getCurrentTime();

        VideoFrame remoteVideoFrame = _listRemoteVideoFrameBuffer.front();
        if (localRenderTime - remoteVideoFrame.renderTimeMs > _delay) {
            _listRemoteVideoFrameBuffer.pop_front();
        }

        const int plane_w = localVideoFrame.width;
        const int plane_h = localVideoFrame.height;

        const int scale_src_w = remoteVideoFrame.width;
        const int scale_src_h = remoteVideoFrame.height;
        const int scale_dst_w = scale_src_w / 2;
        const int scale_dst_h = scale_src_h / 2;

        const int d_scale_yStride = scale_dst_w;
        const int d_scale_uStride = scale_dst_w / 2;
        const int d_scale_vStride = scale_dst_w / 2;

        int scale_dst_wh = scale_dst_w * scale_dst_h;
        uint8_t *d_scale_ybuf = (uint8_t *) malloc(scale_dst_wh);
        uint8_t *d_scale_ubuf = (uint8_t *) malloc(scale_dst_wh / 4);
        uint8_t *d_scale_vbuf = (uint8_t *) malloc(scale_dst_wh / 4);

        //视频画面的放缩
        libyuv::I420Scale(static_cast<const uint8 *>(remoteVideoFrame.yBuffer),
                          remoteVideoFrame.yStride,
                          static_cast<const uint8 *>(remoteVideoFrame.uBuffer),
                          remoteVideoFrame.uStride,
                          static_cast<const uint8 *>(remoteVideoFrame.vBuffer),
                          remoteVideoFrame.vStride,
                          scale_src_w, scale_src_h,
                          d_scale_ybuf, d_scale_yStride,
                          d_scale_ubuf, d_scale_uStride,
                          d_scale_vbuf, d_scale_vStride,
                          scale_dst_w, scale_dst_h,
                          libyuv::kFilterNone);

        int offset_right = 10;
        int offset_top = 10;

        const int rotate_src_w = scale_dst_w;
        const int rotate_src_h = scale_dst_h;
        const int rotate_dst_w = scale_dst_h;
        const int rotate_dst_h = scale_dst_w;

        const int d_rotate_yStride = scale_dst_h;
        const int d_rotate_uStride = scale_dst_h / 2;
        const int d_rotate_vStride = scale_dst_h / 2;

        int rotate_dst_wh = scale_dst_w * scale_dst_h;
        uint8_t *d_rotate_ybuf = (uint8_t *) malloc(scale_dst_wh);
        uint8_t *d_rotate_ubuf = (uint8_t *) malloc(scale_dst_wh / 4);
        uint8_t *d_rotate_vbuf = (uint8_t *) malloc(scale_dst_wh / 4);

        //视频画面的旋转
        libyuv::I420Rotate(d_scale_ybuf, d_scale_yStride,
                           d_scale_ubuf, d_scale_uStride,
                           d_scale_vbuf, d_scale_vStride,
                           d_rotate_ybuf, d_rotate_yStride,
                           d_rotate_ubuf, d_rotate_uStride,
                           d_rotate_vbuf, d_rotate_vStride,
                           rotate_src_w, rotate_src_h,
                           libyuv::RotationMode::kRotate90);

        //合图的方法处理
        int off_x = plane_w - offset_right - rotate_dst_w;
        int off_y = offset_top;
        int n0ff = 0;
        for (int i = 0; i < rotate_dst_h; i++) {
            n0ff = plane_w * (off_y + i) + off_x;
            memcpy((uint8_t *)localVideoFrame.yBuffer + n0ff, d_rotate_ybuf + rotate_dst_w * i, rotate_dst_w);
        }
        int uv_w = rotate_dst_w / 2;
        int uv_h = rotate_dst_h / 2;
        int uv_plane_w = plane_w / 2;
        int uv_off_y = off_y / 2;
        int uv_off_x = off_x / 2;
        for (int j = 0; j < uv_h; j++) {
            n0ff = uv_plane_w * (uv_off_y + j) + uv_off_x;
            memcpy((uint8_t *)localVideoFrame.uBuffer + n0ff, d_rotate_ubuf + uv_w * j, uv_w);
            memcpy((uint8_t *)localVideoFrame.vBuffer + n0ff, d_rotate_vbuf + uv_w * j, uv_w);
        }

        free(d_scale_vbuf);
        free(d_scale_ybuf);
        free(d_scale_ubuf);
        free(d_rotate_ybuf);
        free(d_rotate_ubuf);
        free(d_rotate_vbuf);
        d_scale_ybuf = NULL;
        d_scale_vbuf = NULL;
        d_scale_vbuf = NULL;
        d_rotate_ybuf = NULL;
        d_rotate_ubuf = NULL;
        d_rotate_vbuf = NULL;
    }

    virtual bool onCaptureVideoFrame(VideoFrame &videoFrame) {
        mixVideoFrame(videoFrame);
        return true;
    }

    virtual bool onRenderVideoFrame(unsigned int uid, VideoFrame &videoFrame) {
//        if (uid == _uid) {
        videoFrame.renderTimeMs = getCurrentTime();
        _listRemoteVideoFrameBuffer.push_back(videoFrame);
//        }
        return true;
    }

    void setDelay(int64_t delay) {
        _delay = delay;
    }

    void setMixVideoFrameUid(unsigned int uid) {
        _uid = uid;
    }

private:
    list<VideoFrame> _listRemoteVideoFrameBuffer;
    int64_t _delay = 0;
    unsigned int _uid = 0;
    bool isStartMix;
};

static agora::rtc::IRtcEngine *rtcEngine = nullptr;
static VideoFrameObserver s_videoFrameObserver;

extern "C" __attribute__((visibility("default")))
int loadAgoraRtcEnginePlugin(agora::rtc::IRtcEngine *engine) {
    __android_log_print(ANDROID_LOG_DEBUG, "agora-raw-data-plugin", "loadAgoraRtcEnginePlugin");
    rtcEngine = engine;
    return 0;
}

extern "C" __attribute__((visibility("default")))
void unloadAgoraRtcEnginePlugin(agora::rtc::IRtcEngine *engine) {
    __android_log_print(ANDROID_LOG_DEBUG, "agora-raw-data-plugin", "unloadAgoraRtcEnginePlugin");

    rtcEngine = nullptr;
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_io_agora_rtc_plugin_rawdata_VideoRawDataApi_registerVideoRawData(JNIEnv *env,
                                                                      jobject instance) {

    bool result = false;

    __android_log_print(ANDROID_LOG_DEBUG, "agora-raw-data-plugin", "register");
    if (rtcEngine) {
        agora::util::AutoPtr<agora::media::IMediaEngine> mediaEngine;
        mediaEngine.queryInterface(rtcEngine, agora::INTERFACE_ID_TYPE::AGORA_IID_MEDIA_ENGINE);
        mediaEngine->registerVideoFrameObserver(&s_videoFrameObserver);
        __android_log_print(ANDROID_LOG_DEBUG, "agora-raw-data-plugin", "register");
    }

    return static_cast<jboolean>(result);
}

extern "C"
JNIEXPORT void JNICALL
Java_io_agora_rtc_plugin_rawdata_VideoRawDataApi_setVideoMixingLayoutsUid(JNIEnv *env,
                                                                          jobject instance,
                                                                          jint uid) {
    s_videoFrameObserver.setMixVideoFrameUid(static_cast<unsigned int>(uid));
}

extern "C"
JNIEXPORT void JNICALL
Java_io_agora_rtc_plugin_rawdata_VideoRawDataApi_setBufferDelay(JNIEnv *env, jobject instance,
                                                                jint delay) {
    s_videoFrameObserver.setDelay(delay);
}


extern "C"
JNIEXPORT void JNICALL
Java_io_agora_rtc_plugin_rawdata_VideoRawDataApi_unRegisterVideoRawData(JNIEnv *env,
                                                                        jobject instance) {
    if (rtcEngine) {
        agora::util::AutoPtr<agora::media::IMediaEngine> mediaEngine;
        mediaEngine.queryInterface(rtcEngine, agora::INTERFACE_ID_TYPE::AGORA_IID_MEDIA_ENGINE);
        if (mediaEngine) {
            mediaEngine->registerVideoFrameObserver(NULL);
        }
    }
    s_videoFrameObserver.setDelay(0);
    s_videoFrameObserver.setMixVideoFrameUid(0);
}

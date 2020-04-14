#include <jni.h>
#include <android/log.h>
#include <cstring>
#include "agora/IAgoraMediaEngine.h"

#include "agora/IAgoraRtcEngine.h"
#include <string.h>
#include "media_preprocessing_plugin_jni.h"
#include "VMUtil.h"

#include <map>
#include <list>
#include <libyuv.h>

using namespace std;

jobject gCallBack = nullptr;
jclass gCallbackClass = nullptr;
jmethodID recordAudioMethodId = nullptr;
jmethodID playbackAudioMethodId = nullptr;
jmethodID playBeforeMixAudioMethodId = nullptr;
jmethodID mixAudioMethodId = nullptr;
jmethodID captureVideoMethodId = nullptr;
jmethodID renderVideoMethodId = nullptr;
void *_javaDirectPlayBufferCapture = nullptr;
void *_javaDirectPlayBufferRecordAudio = nullptr;
void *_javaDirectPlayBufferPlayAudio = nullptr;
void *_javaDirectPlayBufferBeforeMixAudio = nullptr;
void *_javaDirectPlayBufferMixAudio = nullptr;
map<int, void *> decodeByfferMap;

static JavaVM *gJVM = nullptr;

class AgoraVideoFrameObserver : public agora::media::IVideoFrameObserver {

public:
    AgoraVideoFrameObserver() {

    }

    ~AgoraVideoFrameObserver() {
        _listVideoFrameBuffer.clear();
    }

    void getVideoFrame(VideoFrame &videoFrame, _jmethodID *jmethodID, void *_byteBufferObject, unsigned int uid) {

        if (_byteBufferObject) {
            int width = videoFrame.width;
            int height = videoFrame.height;
            size_t widthAndHeight = (size_t) width * height;
            size_t length = widthAndHeight * 3 / 2;

            AttachThreadScoped ats(gJVM);
            JNIEnv *env = ats.env();

            memcpy(_byteBufferObject, videoFrame.yBuffer, widthAndHeight);
            memcpy((uint8_t *) _byteBufferObject + widthAndHeight, videoFrame.uBuffer,
                   widthAndHeight / 4);
            memcpy((uint8_t *) _byteBufferObject + widthAndHeight * 5 / 4, videoFrame.vBuffer,
                   widthAndHeight / 4);

            if (uid == 0) {
                env->CallVoidMethod(gCallBack, jmethodID, videoFrame.type, width, height, length,
                                    videoFrame.yStride, videoFrame.uStride,
                                    videoFrame.vStride, videoFrame.rotation,
                                    videoFrame.renderTimeMs);
            } else {
                env->CallVoidMethod(gCallBack, jmethodID, uid, videoFrame.type, width, height,
                                    length,
                                    videoFrame.yStride, videoFrame.uStride,
                                    videoFrame.vStride, videoFrame.rotation,
                                    videoFrame.renderTimeMs);
            }
        }

    }

    void writebackVideoFrame(VideoFrame &videoFrame, void *byteBuffer) {
        if (byteBuffer == nullptr) {
            return;
        }

        int width = videoFrame.width;
        int height = videoFrame.height;
        size_t widthAndHeight = (size_t) width * height;

        memcpy(videoFrame.yBuffer, byteBuffer, widthAndHeight);
        memcpy(videoFrame.uBuffer, (uint8_t *) byteBuffer + widthAndHeight, widthAndHeight / 4);
        memcpy(videoFrame.vBuffer, (uint8_t *) byteBuffer + widthAndHeight * 5 / 4,
               widthAndHeight / 4);
    }

    void mergeVideoFrame(VideoFrame &localVideoFrame) {
        if(_listVideoFrameBuffer.empty())
            return;

        VideoFrame &remoteVideoFrame = _listVideoFrameBuffer.front();
        if(_listVideoFrameBuffer.size() > 5) {
            _listVideoFrameBuffer.pop_front();
        }

        const int scale_src_w = remoteVideoFrame.width;
        const int scale_src_h = remoteVideoFrame.height;

        const int plane_w = localVideoFrame.width;
        const int plane_h = localVideoFrame.height;
        const int scale_dst_w = plane_w / 2;
        const int scale_dst_h = plane_h / 2;

        const int d_yStride = scale_dst_w;
        const int d_uStride = scale_dst_w / 2;
        const int d_vStride = scale_dst_w / 2;
        //创建头像buf将数据传入buf中

        uint8_t *d_ybuf = (uint8_t *)malloc(scale_dst_w * scale_dst_h);
        uint8_t *d_ubuf = (uint8_t *)malloc(scale_dst_w * scale_dst_h /4);
        uint8_t *d_vbuf = (uint8_t *)malloc(scale_dst_w * scale_dst_h /4);

        //视频画面的放缩
        libyuv::I420Scale(static_cast<const uint8 *>(remoteVideoFrame.yBuffer), remoteVideoFrame.yStride,
                          static_cast<const uint8 *>(remoteVideoFrame.uBuffer), remoteVideoFrame.uStride,
                          static_cast<const uint8 *>(remoteVideoFrame.vBuffer), remoteVideoFrame.vStride,
                          scale_src_w, scale_src_h,
                          d_ybuf, d_yStride,
                          d_ubuf, d_uStride,
                          d_vbuf, d_vStride,
                          scale_dst_w, scale_dst_h,
                          libyuv::kFilterNone);

        int scale_dst_size = scale_dst_w  * scale_dst_w;

        int offset_right = 10;
        int offset_top = 10;
        //合图的方法处理
        int off_x = plane_w - offset_right - scale_dst_w;
        int off_y = offset_top;
        int n0ff = 0;
        for (int i = 0; i < scale_dst_h; i++) {
            n0ff = plane_w * (off_y + i) + off_x;
            memcpy(localVideoFrame.yBuffer + n0ff, d_ybuf + scale_dst_w * i, scale_dst_w);
        }
        for (int j = 0; j < scale_dst_h/2; j++) {
            n0ff = (plane_w/2) * (off_y/2 + j) + off_x/2;
            memcpy(localVideoFrame.uBuffer + n0ff, d_ubuf + scale_dst_w/2 * j, scale_dst_w/2);
            memcpy(localVideoFrame.vBuffer + n0ff, d_vbuf + scale_dst_w/2 * j, scale_dst_w/2);
        }

        free(d_vbuf);
        free(d_ybuf);
        free(d_ubuf);
        d_ybuf = NULL;
        d_vbuf = NULL;
        d_vbuf = NULL;
    }

public:
    virtual bool onCaptureVideoFrame(VideoFrame &videoFrame) override {
//        getVideoFrame(videoFrame, captureVideoMethodId, _javaDirectPlayBufferCapture, 0);
//        writebackVideoFrame(videoFrame, _javaDirectPlayBufferCapture);

        mergeVideoFrame(videoFrame);
        return true;
    }

    virtual bool onRenderVideoFrame(unsigned int uid, VideoFrame &videoFrame) override {
        map<int, void *>::iterator it_find;
        it_find = decodeByfferMap.find(uid);

        if (it_find != decodeByfferMap.end()) {
            if (it_find->second != nullptr) {
//                getVideoFrame(videoFrame, renderVideoMethodId, it_find->second, uid);
//                writebackVideoFrame(videoFrame, it_find->second);

                VideoFrame buffervideoFrame;
                buffervideoFrame.width = videoFrame.width;
                buffervideoFrame.height = videoFrame.height;
                buffervideoFrame.yBuffer = videoFrame.yBuffer;
                buffervideoFrame.vBuffer = videoFrame.vBuffer;
                buffervideoFrame.uBuffer = videoFrame.uBuffer;
                buffervideoFrame.yStride = videoFrame.yStride;
                buffervideoFrame.vStride = videoFrame.vStride;
                buffervideoFrame.uStride = videoFrame.uStride;
                buffervideoFrame.rotation = videoFrame.rotation;
                buffervideoFrame.renderTimeMs = videoFrame.renderTimeMs;
                buffervideoFrame.type = videoFrame.type;

                _listVideoFrameBuffer.push_back(buffervideoFrame);
            }
        }

        return true;
    }

private:
    list<VideoFrame> _listVideoFrameBuffer;
    bool isStartMerge = false;
};


class AgoraAudioFrameObserver : public agora::media::IAudioFrameObserver {

public:
    AgoraAudioFrameObserver() {
        gCallBack = nullptr;
    }

    ~AgoraAudioFrameObserver() {
    }

    void getAudioFrame(AudioFrame &audioFrame, _jmethodID *jmethodID, void *_byteBufferObject, unsigned int uid) {
        if (_byteBufferObject) {
            AttachThreadScoped ats(gJVM);
            JNIEnv *env = ats.env();
            if (env == nullptr) {
                return;
            }
            int len = audioFrame.samples * audioFrame.bytesPerSample;
            memcpy(_byteBufferObject, audioFrame.buffer, (size_t) len); // * sizeof(int16_t)

            if (uid == 0) {
                env->CallVoidMethod(gCallBack, jmethodID, audioFrame.type, audioFrame.samples,
                                    audioFrame.bytesPerSample,
                                    audioFrame.channels, audioFrame.samplesPerSec,
                                    audioFrame.renderTimeMs, len);
            } else {
                env->CallVoidMethod(gCallBack, jmethodID, uid, audioFrame.type, audioFrame.samples,
                                    audioFrame.bytesPerSample,
                                    audioFrame.channels, audioFrame.samplesPerSec,
                                    audioFrame.renderTimeMs, len);
            }
        }

    }

    void writebackAudioFrame(AudioFrame &audioFrame, void *byteBuffer) {
        if (byteBuffer == nullptr) {
            return;
        }

        int len = audioFrame.samples * audioFrame.bytesPerSample;
        memcpy(audioFrame.buffer, byteBuffer, (size_t) len);
    }

public:
    virtual bool onRecordAudioFrame(AudioFrame &audioFrame) override {
        getAudioFrame(audioFrame, recordAudioMethodId, _javaDirectPlayBufferRecordAudio, 0);
        writebackAudioFrame(audioFrame, _javaDirectPlayBufferRecordAudio);
        return true;
    }

    virtual bool onPlaybackAudioFrame(AudioFrame &audioFrame) override {
        getAudioFrame(audioFrame, playbackAudioMethodId, _javaDirectPlayBufferPlayAudio, 0);
        writebackAudioFrame(audioFrame, _javaDirectPlayBufferPlayAudio);
        return true;
    }

    virtual bool
    onPlaybackAudioFrameBeforeMixing(unsigned int uid, AudioFrame &audioFrame) override {
        getAudioFrame(audioFrame, playBeforeMixAudioMethodId, _javaDirectPlayBufferBeforeMixAudio, uid);
        writebackAudioFrame(audioFrame, _javaDirectPlayBufferBeforeMixAudio);
        return true;
    }

    virtual bool onMixedAudioFrame(AudioFrame &audioFrame) override {
        getAudioFrame(audioFrame, mixAudioMethodId, _javaDirectPlayBufferMixAudio, 0);
        writebackAudioFrame(audioFrame, _javaDirectPlayBufferMixAudio);
        return true;
    }
};


static AgoraAudioFrameObserver s_audioFrameObserver;

static AgoraVideoFrameObserver s_videoFrameObserver;
static agora::rtc::IRtcEngine *rtcEngine = nullptr;

#ifdef __cplusplus
extern "C" {
#endif

int __attribute__((visibility("default")))
loadAgoraRtcEnginePlugin(agora::rtc::IRtcEngine *engine) {
    __android_log_print(ANDROID_LOG_DEBUG, "agora-raw-data-plugin", "loadAgoraRtcEnginePlugin");
    rtcEngine = engine;
    return 0;
}

void __attribute__((visibility("default")))
unloadAgoraRtcEnginePlugin(agora::rtc::IRtcEngine *engine) {
    __android_log_print(ANDROID_LOG_DEBUG, "agora-raw-data-plugin", "unloadAgoraRtcEnginePlugin");

    rtcEngine = nullptr;
}

JNIEXPORT void JNICALL
Java_io_agora_rtc_plugin_rawdata_MediaPreProcessing_setCallback(JNIEnv *env, jobject obj,
                                                                jobject callback) {
    if (!rtcEngine) return;

    env->GetJavaVM(&gJVM);

    agora::util::AutoPtr<agora::media::IMediaEngine> mediaEngine;
    mediaEngine.queryInterface(rtcEngine, agora::INTERFACE_ID_TYPE::AGORA_IID_MEDIA_ENGINE);
    if (mediaEngine) {
        mediaEngine->registerVideoFrameObserver(&s_videoFrameObserver);
//        mediaEngine->registerAudioFrameObserver(&s_audioFrameObserver);
    }

    if (gCallBack == nullptr) {
        gCallBack = env->NewGlobalRef(callback);
        gCallbackClass = env->GetObjectClass(gCallBack);

        recordAudioMethodId = env->GetMethodID(gCallbackClass, "onRecordAudioFrame", "(IIIIIJI)V");
        playbackAudioMethodId = env->GetMethodID(gCallbackClass, "onPlaybackAudioFrame",
                                                 "(IIIIIJI)V");
        playBeforeMixAudioMethodId = env->GetMethodID(gCallbackClass,
                                                      "onPlaybackAudioFrameBeforeMixing",
                                                      "(IIIIIIJI)V");
        mixAudioMethodId = env->GetMethodID(gCallbackClass, "onMixedAudioFrame", "(IIIIIJI)V");

        captureVideoMethodId = env->GetMethodID(gCallbackClass, "onCaptureVideoFrame",
                                                "(IIIIIIIIJ)V");
        renderVideoMethodId = env->GetMethodID(gCallbackClass, "onRenderVideoFrame",
                                               "(IIIIIIIIIJ)V");

        __android_log_print(ANDROID_LOG_DEBUG, "setCallback", "setCallback done successfully");
    }
}

JNIEXPORT void JNICALL Java_io_agora_rtc_plugin_rawdata_MediaPreProcessing_setVideoCaptureByteBuffer
        (JNIEnv *env, jobject obj, jobject bytebuffer) {
    _javaDirectPlayBufferCapture = env->GetDirectBufferAddress(bytebuffer);
}

JNIEXPORT void JNICALL Java_io_agora_rtc_plugin_rawdata_MediaPreProcessing_setAudioRecordByteBuffer
        (JNIEnv *env, jobject obj, jobject bytebuffer) {
    _javaDirectPlayBufferRecordAudio = env->GetDirectBufferAddress(bytebuffer);
}

JNIEXPORT void JNICALL Java_io_agora_rtc_plugin_rawdata_MediaPreProcessing_setAudioPlayByteBuffer
        (JNIEnv *env, jobject obj, jobject bytebuffer) {
    _javaDirectPlayBufferPlayAudio = env->GetDirectBufferAddress(bytebuffer);
}

JNIEXPORT void JNICALL
Java_io_agora_rtc_plugin_rawdata_MediaPreProcessing_setBeforeAudioMixByteBuffer
        (JNIEnv *env, jobject obj, jobject bytebuffer) {
    _javaDirectPlayBufferBeforeMixAudio = env->GetDirectBufferAddress(bytebuffer);
}

JNIEXPORT void JNICALL Java_io_agora_rtc_plugin_rawdata_MediaPreProcessing_setAudioMixByteBuffer
        (JNIEnv *env, jobject obj, jobject bytebuffer) {
    _javaDirectPlayBufferMixAudio = env->GetDirectBufferAddress(bytebuffer);
}

JNIEXPORT void JNICALL
Java_io_agora_rtc_plugin_rawdata_MediaPreProcessing_setVideoDecodeByteBuffer(JNIEnv *env,
                                                                             jobject type,
                                                                             jint uid,
                                                                             jobject byteBuffer) {
    if (byteBuffer == nullptr) {
        decodeByfferMap.erase(uid);
    } else {
        void *_javaDirectDecodeBuffer = env->GetDirectBufferAddress(byteBuffer);
        decodeByfferMap.insert(make_pair(uid, _javaDirectDecodeBuffer));
        __android_log_print(ANDROID_LOG_DEBUG, "agora-raw-data-plugin",
                            "setVideoDecodeByteBuffer uid: %d, _javaDirectDecodeBuffer: %p",
                            uid, _javaDirectDecodeBuffer);
    }
}

JNIEXPORT void JNICALL
Java_io_agora_rtc_plugin_rawdata_MediaPreProcessing_releasePoint(JNIEnv *env, jobject type) {
    agora::util::AutoPtr<agora::media::IMediaEngine> mediaEngine;
    mediaEngine.queryInterface(rtcEngine, agora::INTERFACE_ID_TYPE::AGORA_IID_MEDIA_ENGINE);

    if (mediaEngine) {
        mediaEngine->registerVideoFrameObserver(NULL);
        mediaEngine->registerAudioFrameObserver(NULL);
    }

    if (gCallBack != nullptr) {
        env->DeleteGlobalRef(gCallBack);
        gCallBack = nullptr;
    }
    gCallbackClass = nullptr;

    recordAudioMethodId = nullptr;
    playbackAudioMethodId = nullptr;
    playBeforeMixAudioMethodId = nullptr;
    mixAudioMethodId = nullptr;
    captureVideoMethodId = nullptr;
    renderVideoMethodId = nullptr;

    _javaDirectPlayBufferCapture = nullptr;
    _javaDirectPlayBufferRecordAudio = nullptr;
    _javaDirectPlayBufferPlayAudio = nullptr;
    _javaDirectPlayBufferBeforeMixAudio = nullptr;
    _javaDirectPlayBufferMixAudio = nullptr;

    decodeByfferMap.clear();
}


#ifdef __cplusplus
}
#endif
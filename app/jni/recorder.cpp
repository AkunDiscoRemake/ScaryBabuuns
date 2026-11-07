#include "recorder.h"
#include <media/NdkMediaCodec.h>
#include <media/NdkMediaMuxer.h>
#include <media/NdkMediaFormat.h>
#include <android/log.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include <string.h>
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "BabuunHollow", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "BabuunHollow", __VA_ARGS__)

Recorder gRecorder;
static const int AUDIO_SR = 44100, AAC_CHUNK = 1024;

static double nowSec() { timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }
float Recorder::elapsed() const { return running ? (float)(nowSec() - startTime) : 0.f; }

static void mkdirs(const std::string &p) {
    for (size_t i = 1; i <= p.size(); i++)
        if (i == p.size() || p[i] == '/') mkdir(p.substr(0, i).c_str(), 0777);
}

bool Recorder::start(const std::string &dir) {
    if (running) return true;
    mkdirs(dir);
    time_t tt = time(nullptr); tm lt; localtime_r(&tt, &lt);
    char name[64];
    strftime(name, sizeof name, "BabuunHollow_%Y%m%d_%H%M%S.mp4", &lt);
    path = dir + "/" + name;
    fd = open(path.c_str(), O_CREAT | O_TRUNC | O_RDWR, 0666);
    if (fd < 0) { error = "não foi possível criar " + path; LOGE("%s", error.c_str()); return false; }
    {
        std::lock_guard<std::mutex> l(mtx);
        vq.clear(); aq.clear();
    }
    audioSamples = 0;
    startTime = nowSec();
    stopping = false;
    running = true;
    th = std::thread(&Recorder::loop, this);
    LOGI("gravando em %s", path.c_str());
    return true;
}

void Recorder::stop() {
    if (!running) return;
    stopping = true;
    cv.notify_all();
    if (th.joinable()) th.join();
    running = false;
    lastSaved = path;
}

void Recorder::pushVideo(const unsigned char *rgba, double t) {
    if (!running || stopping) return;
    Frame f;
    f.pts = (long long)(t * 1e6);
    f.nv12.resize(REC_W * REC_H * 3 / 2);
    unsigned char *Y = f.nv12.data(), *UV = Y + REC_W * REC_H;
    // RGBA (invertido verticalmente) -> NV12 (BT.601)
    for (int y = 0; y < REC_H; y++) {
        const unsigned char *row = rgba + (size_t)(REC_H - 1 - y) * REC_W * 4;
        unsigned char *yr = Y + y * REC_W;
        for (int x = 0; x < REC_W; x++) {
            int r = row[x * 4], g = row[x * 4 + 1], b = row[x * 4 + 2];
            yr[x] = (unsigned char)(((66 * r + 129 * g + 25 * b + 128) >> 8) + 16);
        }
        if ((y & 1) == 0) {
            unsigned char *uvr = UV + (y / 2) * REC_W;
            for (int x = 0; x < REC_W; x += 2) {
                int r = row[x * 4], g = row[x * 4 + 1], b = row[x * 4 + 2];
                uvr[x] = (unsigned char)(((-38 * r - 74 * g + 112 * b + 128) >> 8) + 128);
                uvr[x + 1] = (unsigned char)(((112 * r - 94 * g - 18 * b + 128) >> 8) + 128);
            }
        }
    }
    std::lock_guard<std::mutex> l(mtx);
    if (vq.size() > 6) vq.pop_front();  // codificador atrasado: descarta quadro antigo
    vq.push_back(std::move(f));
    cv.notify_all();
}

void Recorder::pushAudio(const short *mono, int n) {
    if (!running || stopping) return;
    std::lock_guard<std::mutex> l(mtx);
    for (int i = 0; i < n; i++) aq.push_back(mono[i]);
    while (aq.size() > (size_t)AUDIO_SR * 2) aq.pop_front();
    cv.notify_all();
}

void Recorder::loop() {
    AMediaMuxer *mux = AMediaMuxer_new(fd, AMEDIAMUXER_OUTPUT_FORMAT_MPEG_4);
    AMediaCodec *venc = AMediaCodec_createEncoderByType("video/avc");
    AMediaCodec *aenc = AMediaCodec_createEncoderByType("audio/mp4a-latm");
    AMediaFormat *vf = AMediaFormat_new();
    AMediaFormat_setString(vf, AMEDIAFORMAT_KEY_MIME, "video/avc");
    AMediaFormat_setInt32(vf, AMEDIAFORMAT_KEY_WIDTH, REC_W);
    AMediaFormat_setInt32(vf, AMEDIAFORMAT_KEY_HEIGHT, REC_H);
    AMediaFormat_setInt32(vf, AMEDIAFORMAT_KEY_BIT_RATE, 2500000);
    AMediaFormat_setInt32(vf, AMEDIAFORMAT_KEY_FRAME_RATE, REC_FPS);
    AMediaFormat_setInt32(vf, AMEDIAFORMAT_KEY_I_FRAME_INTERVAL, 1);
    AMediaFormat_setInt32(vf, AMEDIAFORMAT_KEY_COLOR_FORMAT, 21);  // YUV420SemiPlanar (NV12)
    AMediaFormat *af = AMediaFormat_new();
    AMediaFormat_setString(af, AMEDIAFORMAT_KEY_MIME, "audio/mp4a-latm");
    AMediaFormat_setInt32(af, AMEDIAFORMAT_KEY_SAMPLE_RATE, AUDIO_SR);
    AMediaFormat_setInt32(af, AMEDIAFORMAT_KEY_CHANNEL_COUNT, 1);
    AMediaFormat_setInt32(af, AMEDIAFORMAT_KEY_BIT_RATE, 96000);
    AMediaFormat_setInt32(af, AMEDIAFORMAT_KEY_AAC_PROFILE, 2);
    AMediaFormat_setInt32(af, AMEDIAFORMAT_KEY_MAX_INPUT_SIZE, 16384);
    bool vok = venc && AMediaCodec_configure(venc, vf, nullptr, nullptr, AMEDIACODEC_CONFIGURE_FLAG_ENCODE) == AMEDIA_OK && AMediaCodec_start(venc) == AMEDIA_OK;
    bool aok = aenc && AMediaCodec_configure(aenc, af, nullptr, nullptr, AMEDIACODEC_CONFIGURE_FLAG_ENCODE) == AMEDIA_OK && AMediaCodec_start(aenc) == AMEDIA_OK;
    AMediaFormat_delete(vf); AMediaFormat_delete(af);
    if (!vok || !mux) {
        error = "codificador de vídeo indisponível";
        LOGE("%s", error.c_str());
        if (venc) AMediaCodec_delete(venc);
        if (aenc) AMediaCodec_delete(aenc);
        if (mux) AMediaMuxer_delete(mux);
        close(fd); fd = -1;
        running = false;
        return;
    }
    int vtrack = -1, atrack = -1;
    bool muxStarted = false, vEos = false, aEos = !aok, vEosSent = false, aEosSent = !aok;
    long long lastVpts = -1;

    auto drain = [&](AMediaCodec *c, int &track, bool &eos) {
        while (true) {
            AMediaCodecBufferInfo info;
            ssize_t idx = AMediaCodec_dequeueOutputBuffer(c, &info, 0);
            if (idx == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED) {
                AMediaFormat *f = AMediaCodec_getOutputFormat(c);
                track = (int)AMediaMuxer_addTrack(mux, f);
                AMediaFormat_delete(f);
                if (vtrack >= 0 && (atrack >= 0 || !aok) && !muxStarted) { AMediaMuxer_start(mux); muxStarted = true; }
                continue;
            }
            if (idx < 0) break;
            size_t sz;
            uint8_t *buf = AMediaCodec_getOutputBuffer(c, idx, &sz);
            bool config = info.flags & AMEDIACODEC_BUFFER_FLAG_CODEC_CONFIG;
            if (muxStarted && !config && info.size > 0 && buf) AMediaMuxer_writeSampleData(mux, track, buf, &info);
            AMediaCodec_releaseOutputBuffer(c, idx, false);
            if (info.flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM) { eos = true; break; }
        }
    };

    std::vector<short> chunk(AAC_CHUNK);
    while (!(vEos && aEos)) {
        Frame f; bool haveV = false, haveA = false;
        {
            std::unique_lock<std::mutex> l(mtx);
            cv.wait_for(l, std::chrono::milliseconds(10));
            if (!vq.empty()) { f = std::move(vq.front()); vq.pop_front(); haveV = true; }
            if (aok && aq.size() >= (size_t)AAC_CHUNK) {
                for (int i = 0; i < AAC_CHUNK; i++) { chunk[i] = aq.front(); aq.pop_front(); }
                haveA = true;
            }
        }
        if (haveV && !vEosSent && f.pts > lastVpts) {
            ssize_t ii = AMediaCodec_dequeueInputBuffer(venc, 20000);
            if (ii >= 0) {
                size_t cap; uint8_t *ib = AMediaCodec_getInputBuffer(venc, ii, &cap);
                size_t n = f.nv12.size() < cap ? f.nv12.size() : cap;
                memcpy(ib, f.nv12.data(), n);
                AMediaCodec_queueInputBuffer(venc, ii, 0, n, f.pts, 0);
                lastVpts = f.pts;
            }
        }
        if (haveA && !aEosSent) {
            ssize_t ii = AMediaCodec_dequeueInputBuffer(aenc, 20000);
            if (ii >= 0) {
                size_t cap; uint8_t *ib = AMediaCodec_getInputBuffer(aenc, ii, &cap);
                size_t n = AAC_CHUNK * 2 < cap ? AAC_CHUNK * 2 : cap;
                memcpy(ib, chunk.data(), n);
                long long pts = audioSamples * 1000000LL / AUDIO_SR;
                AMediaCodec_queueInputBuffer(aenc, ii, 0, n, pts, 0);
                audioSamples += n / 2;
            }
        }
        if (stopping) {
            bool vEmpty, aEmpty;
            { std::lock_guard<std::mutex> l(mtx); vEmpty = vq.empty(); aEmpty = aq.size() < (size_t)AAC_CHUNK; }
            if (vEmpty && !vEosSent) {
                ssize_t ii = AMediaCodec_dequeueInputBuffer(venc, 20000);
                if (ii >= 0) { AMediaCodec_queueInputBuffer(venc, ii, 0, 0, lastVpts + 33000, AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM); vEosSent = true; }
            }
            if (aEmpty && !aEosSent) {
                ssize_t ii = AMediaCodec_dequeueInputBuffer(aenc, 20000);
                if (ii >= 0) { AMediaCodec_queueInputBuffer(aenc, ii, 0, 0, audioSamples * 1000000LL / AUDIO_SR, AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM); aEosSent = true; }
            }
        }
        drain(venc, vtrack, vEos);
        if (aok) drain(aenc, atrack, aEos);
    }
    if (muxStarted) AMediaMuxer_stop(mux);
    AMediaMuxer_delete(mux);
    AMediaCodec_stop(venc); AMediaCodec_delete(venc);
    if (aenc) { if (aok) AMediaCodec_stop(aenc); AMediaCodec_delete(aenc); }
    close(fd); fd = -1;
    LOGI("vídeo salvo: %s", path.c_str());
}

#include "audio.h"
#include "gfx.h"
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include <android/log.h>
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "BabuunHollow", __VA_ARGS__)

Audio gAudio;

static const int SR = 44100, FRAMES = 1024, MIC_FRAMES = 1024;
static SLObjectItf engObj, mixObj, playerObj, recObj;
static SLEngineItf eng;
static SLPlayItf playItf;
static SLAndroidSimpleBufferQueueItf bq, recBq;
static SLRecordItf recItf;
static short outBuf[2][FRAMES * 2];
static int outIdx = 0;
static short micBuf[2][MIC_FRAMES];
static int micIdx = 0;

static void playCb(SLAndroidSimpleBufferQueueItf q, void *) {
    gAudio.mixCallback(outBuf[outIdx], FRAMES);
    (*q)->Enqueue(q, outBuf[outIdx], sizeof(outBuf[0]));
    outIdx ^= 1;
}
static void micCb(SLAndroidSimpleBufferQueueItf q, void *) {
    gAudio.micCallback(micBuf[micIdx], MIC_FRAMES);
    (*q)->Enqueue(q, micBuf[micIdx], sizeof(micBuf[0]));
    micIdx ^= 1;
}

bool Audio::init() {
    voices.resize(48);
    ring.assign(SR, 0);
    if (slCreateEngine(&engObj, 0, nullptr, 0, nullptr, nullptr) != SL_RESULT_SUCCESS) return false;
    (*engObj)->Realize(engObj, SL_BOOLEAN_FALSE);
    (*engObj)->GetInterface(engObj, SL_IID_ENGINE, &eng);
    (*eng)->CreateOutputMix(eng, &mixObj, 0, nullptr, nullptr);
    (*mixObj)->Realize(mixObj, SL_BOOLEAN_FALSE);
    SLDataLocator_AndroidSimpleBufferQueue loc = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 2};
    SLDataFormat_PCM fmt = {SL_DATAFORMAT_PCM, 2, SL_SAMPLINGRATE_44_1, SL_PCMSAMPLEFORMAT_FIXED_16, SL_PCMSAMPLEFORMAT_FIXED_16,
                            SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT, SL_BYTEORDER_LITTLEENDIAN};
    SLDataSource src = {&loc, &fmt};
    SLDataLocator_OutputMix outLoc = {SL_DATALOCATOR_OUTPUTMIX, mixObj};
    SLDataSink sink = {&outLoc, nullptr};
    const SLInterfaceID ids[] = {SL_IID_BUFFERQUEUE};
    const SLboolean req[] = {SL_BOOLEAN_TRUE};
    if ((*eng)->CreateAudioPlayer(eng, &playerObj, &src, &sink, 1, ids, req) != SL_RESULT_SUCCESS) return false;
    (*playerObj)->Realize(playerObj, SL_BOOLEAN_FALSE);
    (*playerObj)->GetInterface(playerObj, SL_IID_PLAY, &playItf);
    (*playerObj)->GetInterface(playerObj, SL_IID_BUFFERQUEUE, &bq);
    (*bq)->RegisterCallback(bq, playCb, nullptr);
    (*playItf)->SetPlayState(playItf, SL_PLAYSTATE_PLAYING);
    playCb(bq, nullptr);
    playCb(bq, nullptr);
    LOGI("áudio iniciado");
    return true;
}

void Audio::shutdown() {
    stopMic();
    if (playerObj) { (*playerObj)->Destroy(playerObj); playerObj = nullptr; }
    if (mixObj) { (*mixObj)->Destroy(mixObj); mixObj = nullptr; }
    if (engObj) { (*engObj)->Destroy(engObj); engObj = nullptr; }
}

bool Audio::startMic() {
    if (recObj) return micOk;
    SLDataLocator_IODevice dev = {SL_DATALOCATOR_IODEVICE, SL_IODEVICE_AUDIOINPUT, SL_DEFAULTDEVICEID_AUDIOINPUT, nullptr};
    SLDataSource src = {&dev, nullptr};
    SLDataLocator_AndroidSimpleBufferQueue loc = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 2};
    SLDataFormat_PCM fmt = {SL_DATAFORMAT_PCM, 1, SL_SAMPLINGRATE_44_1, SL_PCMSAMPLEFORMAT_FIXED_16, SL_PCMSAMPLEFORMAT_FIXED_16,
                            SL_SPEAKER_FRONT_CENTER, SL_BYTEORDER_LITTLEENDIAN};
    SLDataSink sink = {&loc, &fmt};
    const SLInterfaceID ids[] = {SL_IID_ANDROIDSIMPLEBUFFERQUEUE};
    const SLboolean req[] = {SL_BOOLEAN_TRUE};
    if ((*eng)->CreateAudioRecorder(eng, &recObj, &src, &sink, 1, ids, req) != SL_RESULT_SUCCESS) { recObj = nullptr; micOk = false; return false; }
    if ((*recObj)->Realize(recObj, SL_BOOLEAN_FALSE) != SL_RESULT_SUCCESS) { (*recObj)->Destroy(recObj); recObj = nullptr; micOk = false; return false; }
    (*recObj)->GetInterface(recObj, SL_IID_RECORD, &recItf);
    (*recObj)->GetInterface(recObj, SL_IID_ANDROIDSIMPLEBUFFERQUEUE, &recBq);
    (*recBq)->RegisterCallback(recBq, micCb, nullptr);
    (*recBq)->Enqueue(recBq, micBuf[0], sizeof(micBuf[0]));
    (*recBq)->Enqueue(recBq, micBuf[1], sizeof(micBuf[1]));
    micOk = (*recItf)->SetRecordState(recItf, SL_RECORDSTATE_RECORDING) == SL_RESULT_SUCCESS;
    LOGI("microfone: %s", micOk ? "ok" : "falhou");
    return micOk;
}

void Audio::stopMic() {
    if (!recObj) return;
    (*recItf)->SetRecordState(recItf, SL_RECORDSTATE_STOPPED);
    (*recObj)->Destroy(recObj);
    recObj = nullptr;
    micOk = false;
}

int Audio::load(const char *name) {
    std::vector<unsigned char> d;
    SoundBuf sb;
    if (readAsset((std::string("snd/") + name + ".wav").c_str(), d) && d.size() > 44) {
        // procura blocos 'fmt ' e 'data'
        size_t o = 12;
        while (o + 8 <= d.size()) {
            unsigned sz; memcpy(&sz, &d[o + 4], 4);
            if (!memcmp(&d[o], "fmt ", 4)) { unsigned r; memcpy(&r, &d[o + 12], 4); sb.rate = (int)r; }
            if (!memcmp(&d[o], "data", 4)) {
                size_t n = std::min<size_t>(sz, d.size() - o - 8) / 2;
                sb.pcm.resize(n);
                memcpy(sb.pcm.data(), &d[o + 8], n * 2);
                break;
            }
            o += 8 + sz + (sz & 1);
        }
    }
    std::lock_guard<std::mutex> l(mtx);
    sounds.push_back(sb);
    names.push_back(name);
    return (int)sounds.size() - 1;
}

int Audio::find(const char *name) {
    for (size_t i = 0; i < names.size(); i++) if (names[i] == name) return (int)i;
    return load(name);
}

void Audio::computeGains(Voice &v) {
    if (!v.is3D) { v.gL = v.gR = v.gain; return; }
    V3 d = v.p - lpos;
    float dist = len(d);
    float att = v.refDist / fmaxf(v.refDist, dist);
    att *= att > 0.02f ? 1.f : 0.f;
    V3 local = lrot.conj().rot(d);
    float pan = dist > 0.01f ? clampf(local.x / dist, -1, 1) : 0;
    float back = dist > 0.01f && local.z > 0 ? 0.8f : 1.f;  // atrás soa um pouco abafado
    v.gL = v.gain * att * back * sqrtf(0.5f * (1 - pan * 0.85f));
    v.gR = v.gain * att * back * sqrtf(0.5f * (1 + pan * 0.85f));
}

int Audio::play(int snd, float gain, bool loop, float rate) {
    if (snd < 0) return 0;
    std::lock_guard<std::mutex> l(mtx);
    // procura voz livre (ou a mais antiga não-loop)
    int slot = -1;
    for (size_t i = 0; i < voices.size(); i++) if (voices[i].snd < 0) { slot = (int)i; break; }
    if (slot < 0) for (size_t i = 0; i < voices.size(); i++) if (!voices[i].loop) { slot = (int)i; break; }
    if (slot < 0) return 0;
    Voice &v = voices[slot];
    v = Voice();
    v.snd = snd; v.gain = gain; v.loop = loop; v.rate = rate; v.id = nextId++;
    computeGains(v);
    return v.id;
}

int Audio::play3D(int snd, const V3 &p, float gain, bool loop, float refDist, float rate) {
    int id = play(snd, gain, loop, rate);
    std::lock_guard<std::mutex> l(mtx);
    for (auto &v : voices) if (v.id == id && v.snd >= 0) { v.is3D = true; v.p = p; v.refDist = refDist; computeGains(v); }
    return id;
}

void Audio::setPos(int id, const V3 &p) {
    std::lock_guard<std::mutex> l(mtx);
    for (auto &v : voices) if (v.id == id && v.snd >= 0) { v.p = p; computeGains(v); }
}
void Audio::setGain(int id, float g) {
    std::lock_guard<std::mutex> l(mtx);
    for (auto &v : voices) if (v.id == id && v.snd >= 0) { v.gain = g; computeGains(v); }
}
void Audio::stop(int id) {
    std::lock_guard<std::mutex> l(mtx);
    for (auto &v : voices) if (v.id == id) v.snd = -1;
}
bool Audio::playing(int id) {
    std::lock_guard<std::mutex> l(mtx);
    for (auto &v : voices) if (v.id == id && v.snd >= 0) return true;
    return false;
}

void Audio::mixCallback(short *out, int frames) {
    static float acc[FRAMES * 2];
    memset(acc, 0, sizeof(float) * frames * 2);
    {
        std::lock_guard<std::mutex> l(mtx);
        for (auto &v : voices) {
            if (v.snd < 0) continue;
            computeGains(v);
            const SoundBuf &s = sounds[v.snd];
            if (s.pcm.empty()) { v.snd = -1; continue; }
            double step = (double)s.rate / SR * v.rate;
            size_t n = s.pcm.size();
            float gl = v.gL * master, gr = v.gR * master;
            for (int i = 0; i < frames; i++) {
                size_t i0 = (size_t)v.pos;
                if (i0 >= n) {
                    if (v.loop) { v.pos -= n; i0 = (size_t)v.pos; }
                    else { v.snd = -1; break; }
                }
                float fr = (float)(v.pos - i0);
                float a = s.pcm[i0], b = s.pcm[(i0 + 1 < n) ? i0 + 1 : (v.loop ? 0 : i0)];
                float smp = (a + (b - a) * fr) / 32768.f;
                acc[i * 2] += smp * gl;
                acc[i * 2 + 1] += smp * gr;
                v.pos += step;
            }
        }
    }
    bool rec = (bool)recordSink;
    for (int i = 0; i < frames; i++) {
        float l = acc[i * 2], r = acc[i * 2 + 1];
        // limitador suave
        l = l / (1 + fabsf(l) * 0.35f); r = r / (1 + fabsf(r) * 0.35f);
        out[i * 2] = (short)clampf(l * 32767, -32767, 32767);
        out[i * 2 + 1] = (short)clampf(r * 32767, -32767, 32767);
        if (rec) { ring[rw % ring.size()] = (short)((out[i * 2] + out[i * 2 + 1]) / 2); rw++; }
    }
    // sem microfone: o próprio mixer alimenta a gravação
    if (rec && !micOk) {
        static short mono[FRAMES];
        for (int i = 0; i < frames; i++) { mono[i] = ring[rr % ring.size()]; rr++; }
        recordSink(mono, frames);
    }
}

void Audio::micCallback(const short *in, int frames) {
    if (!recordSink) { rr = rw; return; }
    static short mix[MIC_FRAMES];
    for (int i = 0; i < frames; i++) {
        int g = 0;
        if (rr < rw) { g = ring[rr % ring.size()]; rr++; }
        int v = (int)(in[i] * 1.6f) + (int)(g * 0.8f);
        mix[i] = (short)(v > 32767 ? 32767 : (v < -32767 ? -32767 : v));
    }
    if (rw - rr > ring.size() / 2) rr = rw - ring.size() / 4;  // evita deriva
    recordSink(mix, frames);
}

// Áudio: mixer OpenSL ES 44.1kHz estéreo com sons 3D + captura de microfone
#pragma once
#include <vector>
#include <string>
#include <mutex>
#include <functional>
#include "mathx.h"

struct SoundBuf { std::vector<short> pcm; int rate = 22050; };

struct Voice {
    int snd = -1; double pos = 0; float rate = 1; bool loop = false;
    float gain = 1, gL = 1, gR = 1;
    bool is3D = false; V3 p; float refDist = 2;
    int id = 0;
};

class Audio {
public:
    bool init();
    void shutdown();
    int load(const char *name);           // carrega snd/<name>.wav
    int find(const char *name);
    int play(int snd, float gain = 1, bool loop = false, float rate = 1);
    int play3D(int snd, const V3 &p, float gain = 1, bool loop = false, float refDist = 2.5f, float rate = 1);
    int play(const char *name, float gain = 1) { return play(find(name), gain); }
    int play3D(const char *name, const V3 &p, float gain = 1, float refDist = 2.5f) { return play3D(find(name), p, gain, false, refDist); }
    void setPos(int voice, const V3 &p);
    void setGain(int voice, float g);
    void stop(int voice);
    bool playing(int voice);
    void setListener(const V3 &p, const Q &q) { std::lock_guard<std::mutex> l(mtx); lpos = p; lrot = q; }
    float master = 1.f;

    // microfone + saída para gravação (mono 44.1k)
    bool startMic();
    void stopMic();
    std::function<void(const short *, int)> recordSink;  // chamado na thread de áudio quando gravando
    bool micOk = false;

    void mixCallback(short *out, int frames);
    void micCallback(const short *in, int frames);
private:
    std::vector<SoundBuf> sounds;
    std::vector<std::string> names;
    std::vector<Voice> voices;
    std::mutex mtx;
    V3 lpos; Q lrot;
    int nextId = 1;
    // anel com o áudio do jogo (mono) para mixar com o microfone
    std::vector<short> ring; size_t rw = 0, rr = 0;
    void computeGains(Voice &v);
};

extern Audio gAudio;

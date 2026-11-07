// Gravador de vídeo da câmera do jogo: H.264 + AAC (microfone + som do jogo) -> MP4
#pragma once
#include <string>
#include <vector>
#include <deque>
#include <mutex>
#include <thread>
#include <atomic>
#include <condition_variable>

static const int REC_W = 640, REC_H = 352, REC_FPS = 30;

class Recorder {
public:
    bool start(const std::string &dir);   // cria arquivo em dir
    void stop();
    bool active() const { return running.load(); }
    // quadro RGBA (de baixo para cima, como glReadPixels); tempo em segundos desde o início
    void pushVideo(const unsigned char *rgba, double t);
    void pushAudio(const short *mono, int n);  // 44.1kHz
    std::string path, lastSaved, error;
    double startTime = 0;
    float elapsed() const;
private:
    void loop();
    struct Frame { std::vector<unsigned char> nv12; long long pts; };
    std::deque<Frame> vq;
    std::deque<short> aq;
    std::mutex mtx;
    std::condition_variable cv;
    std::thread th;
    std::atomic<bool> running{false}, stopping{false};
    long long audioSamples = 0;
    int fd = -1;
};

extern Recorder gRecorder;

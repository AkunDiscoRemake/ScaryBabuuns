// Simulação no PC da gorilla locomotion com a mão duplicada (stubs de gráficos/áudio/rede)
#include "../../app/jni/game.h"
#include "../../app/jni/audio.h"
#include "../../app/jni/recorder.h"
#include <cstdio>
#include <fstream>
#include <sstream>
// --- stubs
Audio gAudio; Recorder gRecorder;
bool Audio::init() { return true; } void Audio::shutdown() {} int Audio::load(const char *) { return 0; } int Audio::find(const char *) { return 0; }
int Audio::play(int, float, bool, float) { return 1; } int Audio::play3D(int, const V3 &, float, bool, float, float) { return 1; }
void Audio::setPos(int, const V3 &) {} void Audio::setGain(int, float) {} void Audio::stop(int) {} bool Audio::playing(int) { return false; }
bool Audio::startMic() { return false; } void Audio::stopMic() {} void Audio::mixCallback(short *, int) {} void Audio::micCallback(const short *, int) {}
bool Recorder::start(const std::string &) { return false; } void Recorder::stop() {} void Recorder::pushVideo(const unsigned char *, double) {}
void Recorder::pushAudio(const short *, int) {} float Recorder::elapsed() const { return 0; }
void Net::connect(const std::string &, int, const std::string &, const std::string &, const std::string &, int) {} void Net::disconnect() {}
void Net::send(const std::string &) {} void Net::poll(std::vector<std::string> &) {}
bool Gfx::init() { return true; } Texture *Gfx::tex(const std::string &) { static Texture t; return &t; }
Model *Gfx::model(const std::string &) { static Model m; return &m; } void Gfx::begin(const DrawCtx &, const Scene &) {}
void Gfx::draw(const Model *, const M4 &, const V3 &, const Pose *, GLuint, float, const V3 *) {}
void Gfx::drawNode(const Model *, int, const M4 &, const V3 &) {} void Gfx::quad(const M4 &, const float *, GLuint, bool) {}
void Gfx::text(const M4 &, const char *, float, const float *, int, float) {} float Gfx::textWidth(const char *, float) { return 0; }
extern "C" { void glDisable(GLenum) {} void glEnable(GLenum) {} void glClear(GLbitfield) {} }
bool readAsset(const char *path, std::vector<unsigned char> &out) {
    std::ifstream f(std::string("app/assets/") + path, std::ios::binary); if (!f) return false;
    out.assign(std::istreambuf_iterator<char>(f), {}); return true;
}
// acesso ao estado interno para o teste
struct GameTest {
    static int run(Gfx &gfx) {
        int fails = 0;
        for (int mi = 0; mi < 3; mi++) {
            Game G; G.init(&gfx, "build/testsave", "/tmp", "/tmp");
            G.enterGame();
            for (int k = 0; k < 3; k++) G.mons[k].active = (k == mi);
            // coloca o jogador num canto distante do monstro
            G.teleport(mi == 0 ? G.world.spawnForest : V3(G.gOx + 1.5f * G.gCell, 0.3f, G.gOz + 20.5f * G.gCell), 0);
            Input in; in.dt = 1 / 72.f; in.hasCtrl = true; in.ctrl = qAxis(V3(1, 0, 0), 0.6f); in.head = qYaw(PI * 0.5f);
            double t = 0; float caughtAt = -1;
            for (int f = 0; f < 72 * 120 && caughtAt < 0; f++) {
                in.time = t; t += in.dt;
                // o jogador grita (uivo de macaco) a cada 4 s — barulho atrai o Babuun e a Mamãe
                in.padClickDown = (f % (72 * 4) == 0); in.padClick = in.padClickDown; in.padTouch = in.padClickDown;
                G.update(in);
                if (G.pstate == PS_CAUGHT) caughtAt = (float)t;
            }
            V3 mp = G.mons[mi].pos;
            printf("monstro %d: %s (pos final %.1f,%.1f)\n", mi, caughtAt >= 0 ? "PEGOU o jogador" : "não pegou", mp.x, mp.z);
            if (caughtAt >= 0) printf("   em %.1f s\n", caughtAt);
            else fails++;
        }
        return fails;
    }
};
int main() {
    system("mkdir -p build/testsave && printf 'Teste\\n7 4 2 0 0 0 0 -1 0 0 0 0 1 1 25 8 0 0\\n' > build/testsave/perfil.txt");
    Gfx gfx; Game g; g.init(&gfx, "build/testsave", "/tmp", "/tmp");
    int fails = 0;
    for (int mode = 0; mode < 2; mode++) {
        // reconfigura modo da mão via arquivo de perfil
        char cmd[200]; snprintf(cmd, sizeof cmd, "printf 'Teste\\n7 4 2 0 0 0 0 -1 0 0 0 %d 1 1 25 8 0 0\\n' > build/testsave/perfil.txt", mode);
        system(cmd);
        Game G; G.init(&gfx, "build/testsave", "/tmp", "/tmp");
        Input in; in.dt = 1 / 72.f; in.hasCtrl = true;
        double t = 0;
        M4 r0 = G.rigMatrix(); V3 start = r0.pos();
        // olhando para frente (-Z no espaço de rastreamento); o rig começa virado para PI
        float maxSp = 0;
        V3 lastp = start;
        for (int f = 0; f < 72 * 1.5; f++) {
            float ph = fmodf((float)t * 1.6f, 1.f);  // 1.6 passadas/s
            // empurrão: desce à frente e puxa para trás (contato); depois levanta e volta
            float pitch;
            if (ph < 0.55f) pitch = lerpf(-0.75f, -1.45f, ph / 0.55f);        // mão no chão indo para trás
            else pitch = lerpf(-0.2f, -0.75f, (ph - 0.55f) / 0.45f);          // mão no ar voltando
            if (ph >= 0.55f && ph < 0.62f) pitch = -0.2f;
            in.ctrl = qAxis(V3(1, 0, 0), pitch);
            in.head = qYaw(PI); in.headPos = V3();  // olha para longe do portal
            in.ctrl = qYaw(PI) * in.ctrl;
            in.time = t; t += in.dt;
            G.update(in);
            V3 p = G.rigMatrix().pos();
            if (getenv("TRACE") && mode == 0 && f % 9 == 0) printf("f=%d ph=%.2f pitch=%.2f body=(%.2f %.2f %.2f)\n", f, ph, pitch, p.x, p.y - 0.3f, p.z);
            if (f > 5) maxSp = fmaxf(maxSp, len(p - lastp) / in.dt); lastp = p;
        }
        V3 end = G.rigMatrix().pos();
        V3 d = end - start;
        // rig yaw PI + cabeça PI = olhando para -Z no mundo
        printf("modo %s: deslocamento frente=%.2f m  lateral=%.2f m  altura=%.2f  vel.max=%.1f m/s\n", mode ? "ALTERNADA" : "ESPELHO", -d.z, d.x, end.y, maxSp);
        if (-d.z < 1.5f) { printf("FALHOU: não andou para frente\n"); fails++; }
        if (fabsf(d.x) > 0.15f * fabsf(d.z)) { printf("FALHOU: andou torto\n"); fails++; }
    }
    printf(fails ? "%d FALHAS\n" : "LOCOMOÇÃO OK\n", fails);
    int mf = GameTest::run(gfx);
    printf(mf ? "IA: %d FALHAS\n" : "IA DOS MONSTROS OK\n", mf);
    fails += mf;
    return fails;
}

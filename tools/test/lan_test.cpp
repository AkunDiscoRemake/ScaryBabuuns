// Teste de PC do servidor local + descoberta + troca de host, com 3 jogos ao mesmo tempo.
#include "../../app/jni/game.h"
#include "../../app/jni/audio.h"
#include "../../app/jni/recorder.h"
#include <cstdio>
#include <fstream>
#include <unistd.h>
Audio gAudio; Recorder gRecorder;
bool Audio::init() { return true; } void Audio::shutdown() {} int Audio::load(const char *) { return 0; } int Audio::find(const char *) { return 0; }
int Audio::play(int, float, bool, float) { return 1; } int Audio::play3D(int, const V3 &, float, bool, float, float) { return 1; }
void Audio::setPos(int, const V3 &) {} void Audio::setGain(int, float) {} void Audio::stop(int) {} bool Audio::playing(int) { return false; }
bool Audio::startMic() { return false; } void Audio::stopMic() {} void Audio::mixCallback(short *, int) {} void Audio::micCallback(const short *, int) {}
bool Recorder::start(const std::string &) { return false; } void Recorder::stop() {} void Recorder::pushVideo(const unsigned char *, double) {}
void Recorder::pushAudio(const short *, int) {} float Recorder::elapsed() const { return 0; }
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
static double T = 0;
struct GameTest {
    static void pump(std::vector<Game *> &gs, float secs) {
        Input in; in.dt = 1 / 72.f; in.hasCtrl = true;
        for (int f = 0; f < secs * 72; f++) {
            in.time = T; T += in.dt;
            for (auto g : gs) if (g) g->update(in);
            usleep(2000);
        }
    }
    static int run() {
        int fails = 0;
        auto CHECK = [&](bool c, const char *m) { printf("%s %s\n", c ? "  ok " : "FALHOU", m); if (!c) fails++; };
        Gfx gfx;
        system("mkdir -p build/t1 build/t2 build/t3 build/t4");
        const char *dirs[] = {"build/t1", "build/t2", "build/t3", "build/t4"};
        const char *names[] = {"Ana", "Beto", "Caio", "Duda"};
        std::vector<Game *> g(4);
        for (int i = 0; i < 4; i++) {
            char cmd[200]; snprintf(cmd, sizeof cmd, "printf '%s\\n7 4 2 0 0 0 0 -1 0 0 0 0 1 1 25 8 0 0\\n' > %s/perfil.txt", names[i], dirs[i]); system(cmd);
            g[i] = new Game(); g[i]->init(&gfx, dirs[i], "/tmp", "/tmp");
        }
        std::string ip = localIp();
        printf("IP local: %s\n", ip.c_str());
        // Ana cria servidor público com código, 3 jogadores, pesadelo, 12 bananas
        g[0]->createCfg.isPublic = true; g[0]->createCfg.maxPlayers = 3; g[0]->createCfg.diff = 2; g[0]->createCfg.bananas = 12;
        g[0]->codeEntry = "4321"; g[0]->createCfg.code = "4321";
        g[0]->hostLan();
        std::vector<Game *> all = {g[0], g[1], g[2], g[3]};
        pump(all, 2.5f);
        CHECK(g[0]->lan.running(), "Ana hospeda o servidor no próprio Go");
        CHECK(g[0]->net.state() == NET_ONLINE, "Ana conectou no próprio servidor (127.0.0.1)");
        bool seen = false;
        for (auto &e : g[1]->disc.list) if (e.name == g[0]->createCfg.name && e.locked && e.maxPlayers == 3) seen = true;
        CHECK(seen, "Beto vê o servidor da Ana na lista (anúncio UDP, com cadeado)");
        // Duda tenta com código errado
        g[3]->joinLan(ip, "0000");
        pump(all, 1.5f);
        CHECK(g[3]->net.state() != NET_ONLINE && g[3]->netMode == Game::NM_OFF, "código errado é recusado");
        // Beto entra pela lista com o código; Caio entra buscando o código (modo privado)
        g[1]->codeEntry = "4321"; g[1]->joinLan(ip, "4321");
        g[2]->codeEntry = "4321"; g[2]->disc.queryCode("4321"); g[2]->codeSearch = true; g[2]->codeSearchT = T + 3;
        pump(all, 3.0f);
        CHECK(g[1]->net.state() == NET_ONLINE, "Beto conectou");
        CHECK(g[2]->net.state() == NET_ONLINE, "Caio achou o servidor pelo código e conectou");
        CHECK(g[0]->remotes.size() == 2, "Ana vê 2 jogadores");
        CHECK(g[2]->curCfg.diff == 2 && g[2]->curCfg.bananas == 12, "Caio recebeu as regras do servidor (pesadelo, 12 bananas)");
        CHECK(g[1]->succ.size() == 2 && g[1]->succ[0].first == g[1]->net.myId, "fila de sucessão: Beto é o próximo host");
        // servidor cheio (máx 3)
        g[3]->joinLan(ip, "4321");
        pump(all, 1.5f);
        CHECK(g[3]->net.state() != NET_ONLINE, "4º jogador recusado: servidor cheio");
        int caioId = g[2]->net.myId, betoId = g[1]->net.myId;
        // Ana entra no depósito e começa a rodada; Caio pega uma banana
        g[0]->enterGame(); g[1]->enterGame(); g[2]->enterGame();
        pump(all, 1.0f);
        CHECK(g[2]->roundPhase == 1 && g[2]->roundBananas.size() == 12, "rodada sincronizada com 12 bananas");
        // ANA SAI DO JOGO -> troca de host
        printf("-- Ana fecha o jogo (host sai) --\n");
        g[0]->onDestroy();
        std::vector<Game *> rest = {g[1], g[2]};
        pump(rest, 5.0f);
        CHECK(g[1]->lan.running(), "Beto virou o novo host (servidor no Go dele)");
        CHECK(g[2]->net.state() == NET_ONLINE, "Caio reconectou no servidor do Beto");
        CHECK(g[2]->net.myId == caioId && g[1]->net.myId == betoId, "IDs mantidos após a troca");
        CHECK(g[2]->remotes.size() == 1 && g[2]->remotes[0].name == "Beto", "Caio vê o Beto");
        CHECK(g[1]->isHost(), "Beto agora simula os monstros");
        CHECK(g[2]->curCfg.code == "4321" && g[1]->lan.cfg.maxPlayers == 3, "novo servidor manteve código e regras");
        // QUEDA abrupta do Beto (sem aviso): Caio deve virar host sozinho
        printf("-- Beto cai sem avisar --\n");
        g[1]->lan.stop(-2, "x");    // fecha sem indicar sucessor válido (simula queda)
        g[1]->net.disconnect();
        delete g[1]; g[1] = nullptr;
        std::vector<Game *> last = {g[2]};
        pump(last, 4.0f);
        CHECK(g[2]->lan.running() && g[2]->net.state() == NET_ONLINE, "Caio virou host após a queda");
        printf(fails ? "%d FALHAS\n" : "SERVIDORES LOCAIS OK\n", fails);
        for (auto x : g) if (x) x->onDestroy();
        return fails;
    }
};
int main() { return GameTest::run(); }

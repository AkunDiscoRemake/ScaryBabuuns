// Servidor embutido no Oculus Go (rede local, sem nuvem) + anúncio/descoberta por UDP
#pragma once
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>

#define LAN_PORT 25570        // TCP do jogo
#define LAN_BEACON_PORT 25571 // anúncios (servidor -> todos)
#define LAN_QUERY_PORT 25572  // perguntas por código (clientes -> servidores)

struct RoomCfg {
    std::string name = "Toca do Macaco";
    std::string code;        // vazio = sem código
    bool isPublic = true;    // público aparece na lista; privado só entra com código
    int maxPlayers = 6;      // 2..10
    int diff = 1;            // 0 fácil, 1 normal, 2 pesadelo
    int bananas = 8;         // 4, 8 ou 12
};

std::string sanitizeName(const std::string &s);
std::string localIp();       // IP na rede local (Wi-Fi) ou "?"

class LanServer {
public:
    bool start(const RoomCfg &cfg, int firstId = 1);
    void stop(int successorId = -1, const std::string &successorIp = "");  // avisa os clientes antes de fechar
    bool running() const { return run.load(); }
    int players() const { return nPlayers.load(); }
    RoomCfg cfg;
    std::string error;
    ~LanServer() { stop(); }
private:
    void loop();
    std::thread th;
    std::atomic<bool> run{false};
    std::atomic<int> nPlayers{0};
    int listenSock = -1, beaconSock = -1, querySock = -1;
    int nextId = 1;
    std::mutex mtx;
    std::string pendingMigrate;
};

struct LanEntry {
    std::string ip, name;
    bool locked = false, isPublic = true, byCode = false;
    int players = 0, maxPlayers = 0, port = LAN_PORT, diff = 1, bananas = 8;
    double seen = 0;
};

class LanDiscovery {
public:
    bool start();
    void stop();
    void poll(double now);                 // lê anúncios (não bloqueia)
    void queryCode(const std::string &code);
    std::vector<LanEntry> list;            // servidores vistos nos últimos 4 s
    std::vector<LanEntry> codeReplies;     // respostas a uma busca por código
private:
    int sock = -1, qsock = -1;
};

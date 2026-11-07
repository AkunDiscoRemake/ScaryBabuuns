#include "lanserver.h"
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <algorithm>
#ifdef __ANDROID__
#include <android/log.h>
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "BabuunHollow", __VA_ARGS__)
#else
#include <stdio.h>
#define LOGI(...) (printf(__VA_ARGS__), printf("\n"))
#endif

static double nowS() { timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }

std::string sanitizeName(const std::string &s) {
    std::string o;
    for (unsigned char c : s) {
        if (c == ' ') o += '_';
        else if (c > 32 && c != '|' && c != 127) o += (char)c;
        if (o.size() >= 28) break;
    }
    return o.empty() ? "Macaco" : o;
}

std::string localIp() {
    ifaddrs *ifs = nullptr;
    std::string best = "?";
    if (getifaddrs(&ifs) != 0) return best;
    for (ifaddrs *i = ifs; i; i = i->ifa_next) {
        if (!i->ifa_addr || i->ifa_addr->sa_family != AF_INET || (i->ifa_flags & IFF_LOOPBACK) || !(i->ifa_flags & IFF_UP)) continue;
        char b[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &((sockaddr_in *)i->ifa_addr)->sin_addr, b, sizeof b);
        best = b;
        if (strncmp(i->ifa_name, "wlan", 4) == 0) break;  // prefere o Wi-Fi
    }
    freeifaddrs(ifs);
    return best;
}

static int udpSocket(int bindPort) {
    int s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0) return -1;
    int one = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
#ifdef SO_REUSEPORT
    setsockopt(s, SOL_SOCKET, SO_REUSEPORT, &one, sizeof one);
#endif
    setsockopt(s, SOL_SOCKET, SO_BROADCAST, &one, sizeof one);
    fcntl(s, F_SETFL, O_NONBLOCK);
    if (bindPort > 0) {
        sockaddr_in a = {};
        a.sin_family = AF_INET; a.sin_port = htons(bindPort); a.sin_addr.s_addr = htonl(INADDR_ANY);
        if (bind(s, (sockaddr *)&a, sizeof a) < 0) { close(s); return -1; }
    }
    return s;
}

static void udpSend(int s, const std::string &msg, in_addr_t ip, int port) {
    sockaddr_in a = {};
    a.sin_family = AF_INET; a.sin_port = htons(port); a.sin_addr.s_addr = ip;
    sendto(s, msg.data(), msg.size(), 0, (sockaddr *)&a, sizeof a);
}

// ------------------------------------------------------------------ servidor
struct LClient {
    int fd, id; std::string name, ip, rbuf, wbuf;
    bool hello = false, local = false;
};

bool LanServer::start(const RoomCfg &c, int firstId) {
    stop();
    cfg = c;
    cfg.name = sanitizeName(cfg.name);
    error.clear();
    listenSock = socket(AF_INET, SOCK_STREAM, 0);
    int one = 1;
    setsockopt(listenSock, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    sockaddr_in a = {};
    a.sin_family = AF_INET; a.sin_port = htons(LAN_PORT); a.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(listenSock, (sockaddr *)&a, sizeof a) < 0 || listen(listenSock, 16) < 0) {
        error = "porta " + std::to_string(LAN_PORT) + " ocupada";
        close(listenSock); listenSock = -1;
        return false;
    }
    fcntl(listenSock, F_SETFL, O_NONBLOCK);
    beaconSock = udpSocket(0);
    querySock = udpSocket(LAN_QUERY_PORT);
    nextId = std::max(1, firstId);
    pendingMigrate.clear();
    run = true;
    th = std::thread(&LanServer::loop, this);
    LOGI("servidor local '%s' iniciado (%s)", cfg.name.c_str(), cfg.isPublic ? "público" : "privado");
    return true;
}

void LanServer::stop(int succId, const std::string &succIp) {
    if (!run) { if (th.joinable()) th.join(); return; }
    {
        std::lock_guard<std::mutex> l(mtx);
        pendingMigrate = succId >= 0 ? ("MIGRATE " + std::to_string(succId) + " " + succIp) : "AUTO";
    }
    // a thread envia o MIGRATE, esvazia os buffers e encerra
    for (int i = 0; i < 50 && run; i++) usleep(10000);
    run = false;
    if (th.joinable()) th.join();
}

void LanServer::loop() {
    std::vector<LClient> cl;
    int simHost = -1;            // jogador que simula monstros/rodada
    double lastBeacon = 0;
    char tmp[4096];
    auto info = [&]() {
        return "INFO " + std::to_string(cfg.isPublic ? 1 : 0) + " " + std::to_string(cfg.maxPlayers) + " " + std::to_string(cfg.diff) + " " +
               std::to_string(cfg.bananas) + " " + (cfg.code.empty() ? std::string("-") : cfg.code) + " " + cfg.name;
    };
    auto beacon = [&](const char *tag) {
        int n = 0; for (auto &c : cl) if (c.hello) n++;
        return std::string(tag) + "|" + cfg.name + "|" + (cfg.code.empty() ? "0" : "1") + "|" + std::to_string(n) + "|" + std::to_string(cfg.maxPlayers) +
               "|" + std::to_string(LAN_PORT) + "|" + std::to_string(cfg.diff) + "|" + std::to_string(cfg.bananas) + "|" + (cfg.isPublic ? "1" : "0");
    };
    auto sendTo = [&](LClient &c, const std::string &l) { if (c.wbuf.size() < 512 * 1024 || l.compare(0, 3, "ST ")) c.wbuf += l + "\n"; };
    auto bcast = [&](const std::string &l, int except) { for (auto &c : cl) if (c.hello && c.id != except) sendTo(c, l); };
    auto succList = [&]() {
        // ordem de sucessão: ordem de entrada, ignorando o próprio dono do servidor (loopback)
        std::string s = "SUCC";
        for (auto &c : cl) if (c.hello && !c.local) s += " " + std::to_string(c.id) + " " + c.ip;
        return s;
    };
    auto drop = [&](size_t i) {
        LClient c = cl[i];
        close(c.fd);
        cl.erase(cl.begin() + i);
        if (!c.hello) return;
        LOGI("servidor: %s#%d saiu", c.name.c_str(), c.id);
        bcast("LEAVE " + std::to_string(c.id), -1);
        if (simHost == c.id) {
            simHost = -1;
            for (auto &o : cl) if (o.hello) { simHost = o.id; break; }
            if (simHost >= 0) bcast("HOST " + std::to_string(simHost), -1);
        }
        bcast(succList(), -1);
    };
    auto handle = [&](size_t i, const std::string &line) {
        LClient &c = cl[i];
        size_t sp = line.find(' ');
        std::string cmd = line.substr(0, sp), rest = sp == std::string::npos ? "" : line.substr(sp + 1);
        if (cmd == "HELLO") {
            // HELLO <proto> <sala> <nome> [código] [idDesejado]
            char room[64] = {0}, name[64] = {0}, code[32] = {0};
            int proto = 0, want = -1;
            sscanf(rest.c_str(), "%d %63s %63s %31s %d", &proto, room, name, code, &want);
            int n = 0; for (auto &o : cl) if (o.hello) n++;
            std::string cd = strcmp(code, "-") == 0 ? "" : code;
            if (!cfg.code.empty() && cd != cfg.code && !c.local) { sendTo(c, "ERR código incorreto"); c.rbuf.clear(); c.id = -2; return; }
            if (n >= cfg.maxPlayers) { sendTo(c, "ERR servidor cheio"); return; }
            bool taken = false;
            for (auto &o : cl) if (o.hello && o.id == want) taken = true;
            c.id = (want > 0 && !taken) ? want : nextId;
            nextId = std::max(nextId, c.id + 1);
            c.name = sanitizeName(name);
            c.hello = true;
            if (simHost < 0) simHost = c.id;
            sendTo(c, "WELCOME " + std::to_string(c.id) + " " + std::to_string(simHost));
            sendTo(c, info());
            for (auto &o : cl) if (o.hello && o.id != c.id) sendTo(c, "JOIN " + std::to_string(o.id) + " " + o.name);
            bcast("JOIN " + std::to_string(c.id) + " " + c.name, c.id);
            bcast(succList(), -1);
            LOGI("servidor: %s#%d entrou (%s)", c.name.c_str(), c.id, c.ip.c_str());
            return;
        }
        if (!c.hello) return;
        if (cmd == "PING") { sendTo(c, "PONG"); return; }
        if (cmd == "AWAY" && simHost == c.id) {
            // quem simulava tirou o óculos: passa a simulação para outro
            for (auto &o : cl) if (o.hello && o.id != c.id) { simHost = o.id; bcast("HOST " + std::to_string(simHost), -1); break; }
            return;
        }
        if (cmd == "ST" || cmd == "EV") bcast(cmd + " " + std::to_string(c.id) + " " + rest, c.id);
    };

    while (run) {
        // migração pedida pelo dono (saindo do jogo)
        std::string mig;
        { std::lock_guard<std::mutex> l(mtx); mig = pendingMigrate; }
        if (!mig.empty()) {
            if (mig == "AUTO") {
                mig.clear();
                for (auto &c : cl) if (c.hello && !c.local) { mig = "MIGRATE " + std::to_string(c.id) + " " + c.ip; break; }
            }
            if (!mig.empty()) bcast(mig, -1);
            for (auto &c : cl) if (!c.wbuf.empty()) send(c.fd, c.wbuf.data(), c.wbuf.size(), MSG_NOSIGNAL);
            break;
        }
        fd_set rf, wf; FD_ZERO(&rf); FD_ZERO(&wf);
        int mx = listenSock;
        FD_SET(listenSock, &rf);
        if (querySock >= 0) { FD_SET(querySock, &rf); mx = std::max(mx, querySock); }
        for (auto &c : cl) { FD_SET(c.fd, &rf); if (!c.wbuf.empty()) FD_SET(c.fd, &wf); mx = std::max(mx, c.fd); }
        timeval tv = {0, 20000};
        int r = select(mx + 1, &rf, &wf, nullptr, &tv);
        double t = nowS();
        // anúncio na rede local (só servidores públicos aparecem na lista)
        if (cfg.isPublic && beaconSock >= 0 && t - lastBeacon > 1.0) {
            lastBeacon = t;
            udpSend(beaconSock, beacon("BH1"), htonl(INADDR_BROADCAST), LAN_BEACON_PORT);
        }
        nPlayers = (int)std::count_if(cl.begin(), cl.end(), [](const LClient &c) { return c.hello; });
        if (r <= 0) continue;
        if (FD_ISSET(listenSock, &rf)) {
            sockaddr_in a; socklen_t al = sizeof a;
            int fd = accept(listenSock, (sockaddr *)&a, &al);
            if (fd >= 0) {
                fcntl(fd, F_SETFL, O_NONBLOCK);
                int one = 1; setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one);
                LClient c; c.fd = fd; c.id = -1;
                char b[INET_ADDRSTRLEN]; inet_ntop(AF_INET, &a.sin_addr, b, sizeof b);
                c.ip = b; c.local = c.ip == "127.0.0.1";
                cl.push_back(c);
            }
        }
        if (querySock >= 0 && FD_ISSET(querySock, &rf)) {
            sockaddr_in a; socklen_t al = sizeof a;
            ssize_t n = recvfrom(querySock, tmp, sizeof tmp - 1, 0, (sockaddr *)&a, &al);
            if (n > 0) {
                tmp[n] = 0;
                if (!strncmp(tmp, "BHQ|", 4) && !cfg.code.empty() && cfg.code == std::string(tmp + 4))
                    udpSend(querySock, beacon("BHR"), a.sin_addr.s_addr, ntohs(a.sin_port));  // responde na porta de quem perguntou
            }
        }
        for (size_t i = 0; i < cl.size();) {
            LClient &c = cl[i];
            bool dead = false;
            if (FD_ISSET(c.fd, &rf)) {
                ssize_t n = recv(c.fd, tmp, sizeof tmp, 0);
                if (n <= 0 && !(n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))) dead = true;
                else if (n > 0) {
                    c.rbuf.append(tmp, n);
                    if (c.rbuf.size() > 32768) dead = true;
                    size_t p;
                    while (!dead && (p = cl[i].rbuf.find('\n')) != std::string::npos) {
                        std::string line = cl[i].rbuf.substr(0, p);
                        cl[i].rbuf.erase(0, p + 1);
                        if (!line.empty() && line.back() == '\r') line.pop_back();
                        if (!line.empty() && line.size() < 2048) handle(i, line);
                    }
                }
            }
            if (!dead && !cl[i].wbuf.empty() && FD_ISSET(cl[i].fd, &wf)) {
                ssize_t n = send(cl[i].fd, cl[i].wbuf.data(), cl[i].wbuf.size(), MSG_NOSIGNAL);
                if (n > 0) cl[i].wbuf.erase(0, n);
                else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) dead = true;
            }
            // código errado: fecha depois de mandar o erro
            if (!dead && !cl[i].hello && cl[i].wbuf.empty() && cl[i].id == -2) dead = true;
            if (dead) drop(i); else i++;
        }
    }
    for (auto &c : cl) close(c.fd);
    if (listenSock >= 0) close(listenSock);
    if (beaconSock >= 0) close(beaconSock);
    if (querySock >= 0) close(querySock);
    listenSock = beaconSock = querySock = -1;
    nPlayers = 0;
    run = false;
    LOGI("servidor local encerrado");
}

// ------------------------------------------------------------------ descoberta
bool LanDiscovery::start() {
    if (sock >= 0) return true;
    sock = udpSocket(LAN_BEACON_PORT);
    qsock = udpSocket(0);
    return sock >= 0;
}

void LanDiscovery::stop() {
    if (sock >= 0) close(sock);
    if (qsock >= 0) close(qsock);
    sock = qsock = -1;
    list.clear();
}

void LanDiscovery::queryCode(const std::string &code) {
    codeReplies.clear();
    if (qsock < 0) qsock = udpSocket(0);
    if (qsock >= 0) udpSend(qsock, "BHQ|" + code, htonl(INADDR_BROADCAST), LAN_QUERY_PORT);
}

void LanDiscovery::poll(double now) {
    if (sock < 0) return;
    char b[1024];
    for (int pass = 0; pass < 2; pass++) while (true) {
        int fd = pass == 0 ? sock : qsock;   // anúncios (25571) e respostas de código (porta própria)
        if (fd < 0) break;
        sockaddr_in a; socklen_t al = sizeof a;
        ssize_t n = recvfrom(fd, b, sizeof b - 1, 0, (sockaddr *)&a, &al);
        if (n <= 0) break;
        b[n] = 0;
        bool reply = !strncmp(b, "BHR|", 4);
        if (strncmp(b, "BH1|", 4) && !reply) continue;
        LanEntry e;
        char ip[INET_ADDRSTRLEN]; inet_ntop(AF_INET, &a.sin_addr, ip, sizeof ip);
        e.ip = ip; e.seen = now; e.byCode = reply;
        // campos: tag|nome|trancado|jogadores|max|porta|dif|bananas|público
        std::vector<std::string> f;
        std::string cur;
        for (char *p = b; ; p++) { if (*p == '|' || !*p) { f.push_back(cur); cur.clear(); if (!*p) break; } else cur += *p; }
        if (f.size() < 9) continue;
        e.name = f[1]; e.locked = f[2] == "1"; e.players = atoi(f[3].c_str()); e.maxPlayers = atoi(f[4].c_str());
        e.port = atoi(f[5].c_str()); e.diff = atoi(f[6].c_str()); e.bananas = atoi(f[7].c_str()); e.isPublic = f[8] == "1";
        auto &dst = reply ? codeReplies : list;
        bool found = false;
        for (auto &o : dst) if (o.ip == e.ip && o.port == e.port) { o = e; found = true; }
        if (!found) dst.push_back(e);
    }
    list.erase(std::remove_if(list.begin(), list.end(), [&](const LanEntry &e) { return now - e.seen > 4; }), list.end());
}

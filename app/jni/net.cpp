#include "net.h"
#include <sys/socket.h>
#include <sys/select.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <android/log.h>
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "BabuunHollow", __VA_ARGS__)

void Net::connect(const std::string &host, int port, const std::string &room, const std::string &name) {
    disconnect();
    quit = false;
    st = NET_CONNECTING;
    myId = hostId = -1;
    error.clear();
    std::string n = name;
    for (auto &c : n) if (c == ' ' || c == '\n') c = '_';
    std::string hello = "HELLO " + std::to_string(NET_PROTO) + " " + room + " " + n + "\n";
    th = std::thread(&Net::run, this, host, port, hello);
}

void Net::disconnect() {
    quit = true;
    if (th.joinable()) th.join();
    st = NET_OFF;
    std::lock_guard<std::mutex> l(mtx);
    outq.clear(); inq.clear();
}

void Net::send(const std::string &line) {
    if (st != NET_ONLINE) return;
    std::lock_guard<std::mutex> l(mtx);
    if (outq.size() > 200) outq.pop_front();  // evita acúmulo se a rede travar
    outq.push_back(line + "\n");
}

void Net::poll(std::vector<std::string> &out) {
    std::lock_guard<std::mutex> l(mtx);
    while (!inq.empty()) { out.push_back(inq.front()); inq.pop_front(); }
}

void Net::run(std::string host, int port, std::string hello) {
    addrinfo hints = {}, *res = nullptr;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    char ps[16]; snprintf(ps, sizeof ps, "%d", port);
    if (getaddrinfo(host.c_str(), ps, &hints, &res) != 0 || !res) { error = "DNS falhou"; st = NET_FAILED; return; }
    int s = -1;
    for (addrinfo *a = res; a && s < 0; a = a->ai_next) {
        s = socket(a->ai_family, a->ai_socktype, a->ai_protocol);
        if (s < 0) continue;
        // conexão com timeout de 6s
        fcntl(s, F_SETFL, O_NONBLOCK);
        int r = ::connect(s, a->ai_addr, a->ai_addrlen);
        if (r < 0 && errno == EINPROGRESS) {
            fd_set w; FD_ZERO(&w); FD_SET(s, &w);
            timeval tv = {6, 0};
            int err = 0; socklen_t el = sizeof err;
            if (select(s + 1, nullptr, &w, nullptr, &tv) <= 0 || getsockopt(s, SOL_SOCKET, SO_ERROR, &err, &el) < 0 || err) { close(s); s = -1; }
        } else if (r < 0) { close(s); s = -1; }
    }
    freeaddrinfo(res);
    if (s < 0) { error = "Sem conexão com o servidor"; st = NET_FAILED; return; }
    int one = 1; setsockopt(s, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one);
    ::send(s, hello.data(), hello.size(), MSG_NOSIGNAL);
    st = NET_ONLINE;
    LOGI("conectado em %s:%d", host.c_str(), port);
    std::string rbuf, wbuf;
    char tmp[4096];
    while (!quit) {
        {
            std::lock_guard<std::mutex> l(mtx);
            while (!outq.empty()) { wbuf += outq.front(); outq.pop_front(); }
        }
        fd_set rf, wf; FD_ZERO(&rf); FD_ZERO(&wf); FD_SET(s, &rf);
        if (!wbuf.empty()) FD_SET(s, &wf);
        timeval tv = {0, 15000};
        int r = select(s + 1, &rf, &wf, nullptr, &tv);
        if (r < 0 && errno != EINTR) break;
        if (r > 0 && FD_ISSET(s, &rf)) {
            ssize_t n = recv(s, tmp, sizeof tmp, 0);
            if (n <= 0 && !(n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))) { error = "Servidor desconectou"; break; }
            if (n > 0) {
                rbuf.append(tmp, n);
                size_t p;
                std::lock_guard<std::mutex> l(mtx);
                while ((p = rbuf.find('\n')) != std::string::npos) {
                    inq.push_back(rbuf.substr(0, p));
                    rbuf.erase(0, p + 1);
                }
                if (rbuf.size() > 65536) rbuf.clear();
            }
        }
        if (!wbuf.empty() && r > 0 && FD_ISSET(s, &wf)) {
            ssize_t n = ::send(s, wbuf.data(), wbuf.size(), MSG_NOSIGNAL);
            if (n > 0) wbuf.erase(0, n);
            else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) { error = "Erro de envio"; break; }
        }
    }
    close(s);
    if (!quit) st = NET_FAILED; else st = NET_OFF;
}

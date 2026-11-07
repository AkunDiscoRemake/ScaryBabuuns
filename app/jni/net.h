// Cliente TCP simples (linhas de texto) para o servidor multiplayer
#pragma once
#include <string>
#include <vector>
#include <deque>
#include <mutex>
#include <thread>
#include <atomic>

#define SERVER_HOST "us-lax01.altare.global"
#define SERVER_PORT 25570
#define NET_PROTO 1

enum NetState { NET_OFF = 0, NET_CONNECTING, NET_ONLINE, NET_FAILED };

class Net {
public:
    void connect(const std::string &host, int port, const std::string &room, const std::string &name, const std::string &code = "-", int wantId = -1);
    void disconnect();
    void send(const std::string &line);
    void poll(std::vector<std::string> &out);
    NetState state() const { return st.load(); }
    std::string host; int port = 0;
    std::string error;
    int myId = -1, hostId = -1;
    ~Net() { disconnect(); }
private:
    void run(std::string host, int port, std::string hello);
    std::thread th;
    std::atomic<NetState> st{NET_OFF};
    std::atomic<bool> quit{false};
    std::mutex mtx;
    std::deque<std::string> outq, inq;
    int sock = -1;
};

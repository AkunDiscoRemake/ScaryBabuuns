// Babuun Hollow — servidores locais no próprio Oculus Go (sem nuvem) e troca automática de host
#include "game.h"
#include "audio.h"
#include <stdlib.h>

// O dono do servidor também joga: o jogo dele conecta no próprio servidor via 127.0.0.1.
void Game::hostLan() {
    leaveServer(true);
    RoomCfg c = createCfg;
    if (!c.isPublic && c.code.empty()) {  // privado precisa de código: gera um
        char b[8]; snprintf(b, sizeof b, "%04d", rand() % 10000);
        c.code = b; codeEntry = b;
    }
    if (!lan.start(c)) { gAudio.play("ui_error"); hud("Não deu para criar o servidor: " + lan.error, V3(1, 0.4f, 0.3f), 4); return; }
    curCfg = lan.cfg;
    netMode = NM_LAN;
    curIp = "127.0.0.1";
    net.connect("127.0.0.1", LAN_PORT, "LAN", prof.name, c.code);
    myIp = localIp();
    hud("Servidor '" + c.name + "' criado no seu Go! IP " + myIp + (c.code.empty() ? "" : "  Código " + c.code), V3(0.5f, 1, 0.6f), 6);
}

void Game::joinLan(const std::string &ip, const std::string &code) {
    leaveServer(true);
    netMode = NM_LAN;
    curIp = ip;
    net.connect(ip, LAN_PORT, "LAN", prof.name, code.empty() ? "-" : code);
    curCfg.code = code;
    hud("Entrando em " + ip + "...", V3(0.7f, 0.9f, 1), 2.5f);
}

void Game::leaveServer(bool quiet) {
    migState = 0;
    if (lan.running()) lan.stop();  // avisa os outros (MIGRATE) antes de fechar
    net.disconnect();
    netMode = NM_OFF;
    succ.clear();
    remotes.clear();
    wasOnline = false;
    if (!quiet) hud("Você saiu do servidor (modo offline)", V3(1, 0.7f, 0.4f), 2.5f);
}

// Troca de host: o próximo da fila de sucessão vira o novo servidor e os outros reconectam nele.
void Game::beginMigration(int candId, const std::string &candIp) {
    migState = 1; migId = candId; migIp = candIp; migTries = 0;
    migT = now + (candId == net.myId ? 0.2 : 1.5);  // os outros esperam o novo servidor subir
    remotes.clear();
    hud("O host saiu! Trocando de servidor...", V3(1, 0.85f, 0.3f), 3);
}

void Game::updateLan(float dt) {
    // descoberta de servidores na rede Wi-Fi
    disc.poll(now);
    if (codeSearch) {
        if (!disc.codeReplies.empty()) {
            codeSearch = false;
            joinLan(disc.codeReplies[0].ip, codeEntry);
        } else if (now > codeSearchT) {
            codeSearch = false;
            gAudio.play("ui_error");
            hud("Nenhum servidor com o código " + codeEntry + " na rede", V3(1, 0.5f, 0.4f), 3);
        }
    }
    if (netMode != NM_LAN) { wasOnline = false; return; }
    NetState st = net.state();
    // queda inesperada do servidor (o host desligou o Go, saiu do Wi-Fi...)
    if (migState == 0 && wasOnline && st == NET_FAILED) {
        migCand = 0;
        if (!succ.empty()) beginMigration(succ[0].first, succ[0].second);
        else { hud("O servidor caiu", V3(1, 0.5f, 0.4f), 3); leaveServer(true); }
    }
    if (st == NET_ONLINE) wasOnline = true;
    if (migState == 1 && now >= migT) {
        int me = net.myId;
        if (migId == me) {
            // EU sou o novo host: sobe o servidor aqui com a mesma configuração e os mesmos IDs
            int maxId = me;
            for (auto &s : succ) maxId = std::max(maxId, s.first);
            RoomCfg c = curCfg;
            if (lan.start(c, maxId + 1)) {
                curIp = "127.0.0.1";
                net.connect("127.0.0.1", LAN_PORT, "LAN", prof.name, c.code, me);
                myIp = localIp();
                hud("Agora VOCÊ é o host! (" + myIp + ")", V3(0.5f, 1, 0.6f), 4);
            } else hud("Falha ao virar host: " + lan.error, V3(1, 0.4f, 0.3f), 3);
            migState = 2; migT = now + 6;
            wasOnline = false;
        } else {
            curIp = migIp;
            net.connect(migIp, LAN_PORT, "LAN", prof.name, curCfg.code.empty() ? "-" : curCfg.code, me);
            migState = 2; migT = now + 3.5;
            wasOnline = false;
        }
    } else if (migState == 2) {
        if (st == NET_ONLINE) { migState = 0; hud("Reconectado ao novo host!", V3(0.5f, 1, 0.6f), 2.5f); }
        else if (now >= migT || st == NET_FAILED) {
            // não conectou: tenta de novo, depois passa para o próximo candidato da fila
            if (++migTries < 3) { migState = 1; migT = now + 1.0; }
            else if (++migCand < (int)succ.size()) { beginMigration(succ[migCand].first, succ[migCand].second); }
            else { hud("Não foi possível trocar de host", V3(1, 0.4f, 0.3f), 3); leaveServer(true); }
        }
    }
}

void Game::onResume() {}

void Game::onDestroy() {
    saveProfile();
    if (lan.running()) lan.stop();
    net.disconnect();
}

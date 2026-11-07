// Babuun Hollow — lobby (botões físicos), renderização, jumpscare e HUD
#include "game.h"
#include "audio.h"
#include "recorder.h"
#include <GLES3/gl3.h>
#include <stdio.h>

Q qLook(const V3 &fwd, const V3 &upHint);
static const char *HAT_NAMES[] = {"Nenhum", "Boné", "Gorro", "Cartola", "Coroa", "Coroa de Folhas", "Chifrinhos", "Fones"};
static const char *FACE_NAMES[] = {"Nenhum", "Óculos", "Óculos Escuros"};
static const char *NECK_NAMES[] = {"Nenhuma", "Gravata Borboleta"};
static const char *ACC_NAMES[] = {"Vermelho", "Azul", "Verde", "Amarelo", "Roxo", "Branco", "Preto", "Laranja"};
static const V3 ACC_COLORS[] = {V3(0.85f, 0.15f, 0.12f), V3(0.15f, 0.35f, 0.9f), V3(0.2f, 0.7f, 0.25f), V3(0.95f, 0.8f, 0.1f),
                                V3(0.6f, 0.2f, 0.8f), V3(0.95f, 0.95f, 0.95f), V3(0.1f, 0.1f, 0.1f), V3(1.0f, 0.5f, 0.1f)};
static const char *ROOMS[] = {"PUBLICA", "SALA1", "SALA2", "SALA3", "AMIGOS", "TREINO"};
static const int NROOMS = 6;
static const float WHITE[4] = {1, 1, 1, 1}, YELLOW[4] = {1, 0.85f, 0.25f, 1}, GREY[4] = {0.55f, 0.55f, 0.55f, 1};

static M4 stationM(const StationPt &s) { return mTRS(s.c, qYaw(s.yaw)); }

// ------------------------------------------------------------------ construção do lobby
void Game::buildLobby() {
    buttons.clear();
    auto add = [&](const StationPt &st, float x, float y, const std::string &label, std::function<void()> fn, V3 col = V3(0.85f, 0.85f, 0.85f), float r = 0.07f) {
        M4 m = stationM(st);
        Button b;
        b.pos = m.pt(V3(x, y, 0.08f));
        b.nrm = m.dir(V3(0, 0, 1));
        b.label = label; b.fn = fn; b.color = col; b.r = r;
        buttons.push_back(b);
    };
    auto changed = [this]() {
        static bool col = false, acc = false;
        saveProfile();
        (void)col; (void)acc;
    };
    // ---- ESTAÇÃO DE CORES (cores customizadas dentro do VR)
    const StationPt &cs = world.st[0];
    const char *ch[3] = {"R", "G", "B"};
    for (int k = 0; k < 3; k++) {
        float y = 0.3f - k * 0.3f;
        V3 c = k == 0 ? V3(1, 0.3f, 0.3f) : (k == 1 ? V3(0.3f, 1, 0.3f) : V3(0.35f, 0.5f, 1));
        add(cs, -0.95f, y, std::string("-") + ch[k], [this, k, changed]() { int *v = k == 0 ? &prof.r : (k == 1 ? &prof.g : &prof.b); *v = std::max(0, *v - 1); award(9); changed(); }, c);
        add(cs, -0.25f, y, std::string("+") + ch[k], [this, k, changed]() { int *v = k == 0 ? &prof.r : (k == 1 ? &prof.g : &prof.b); *v = std::min(9, *v + 1); award(9); changed(); }, c);
    }
    static const int PRESETS[8][3] = {{7, 4, 2}, {2, 2, 2}, {9, 9, 9}, {9, 2, 2}, {2, 4, 9}, {3, 8, 3}, {9, 8, 1}, {7, 2, 8}};
    for (int i = 0; i < 8; i++) {
        V3 c(PRESETS[i][0] / 9.f, PRESETS[i][1] / 9.f, PRESETS[i][2] / 9.f);
        add(cs, 0.25f + (i % 4) * 0.22f, 0.25f - (i / 4) * 0.25f, "", [this, i, changed]() { prof.r = PRESETS[i][0]; prof.g = PRESETS[i][1]; prof.b = PRESETS[i][2]; changed(); }, c, 0.06f);
    }
    add(cs, 0.58f, -0.35f, "ALEATÓRIA", [this, changed]() { prof.r = rand() % 10; prof.g = rand() % 10; prof.b = rand() % 10; award(9); changed(); }, V3(1, 1, 1));

    // ---- GUARDA-ROUPA
    const StationPt &ws = world.st[1];
    auto cyc = [](int &v, int n, int d) { v = (v + d + n) % n; };
    add(ws, -0.9f, 0.35f, "<", [this, cyc, changed]() { cyc(prof.hat, 8, -1); award(9); changed(); });
    add(ws, 0.9f, 0.35f, ">", [this, cyc, changed]() { cyc(prof.hat, 8, 1); award(9); changed(); });
    add(ws, -0.9f, 0.1f, "<", [this, cyc, changed]() { cyc(prof.face, 3, -1); changed(); });
    add(ws, 0.9f, 0.1f, ">", [this, cyc, changed]() { cyc(prof.face, 3, 1); changed(); });
    add(ws, -0.9f, -0.15f, "<", [this, cyc, changed]() { cyc(prof.neck, 2, -1); changed(); });
    add(ws, 0.9f, -0.15f, ">", [this, cyc, changed]() { cyc(prof.neck, 2, 1); changed(); });
    add(ws, -0.9f, -0.4f, "<", [this, cyc, changed]() { cyc(prof.acc, 8, -1); changed(); });
    add(ws, 0.9f, -0.4f, ">", [this, cyc, changed]() { cyc(prof.acc, 8, 1); changed(); });

    // ---- BADGES (toque para usar como título)
    const StationPt &bs = world.st[2];
    for (int i = 0; i < NUM_BADGES; i++) {
        float x = (i % 2) ? 0.1f : -1.05f, y = 0.6f - (i / 2) * 0.2f;
        add(bs, x, y, "", [this, i]() {
            if (!(prof.badges & (1u << i))) { gAudio.play("ui_error", 0.5f); return; }
            prof.title = prof.title == i ? -1 : i;
            hud(prof.title >= 0 ? std::string("Título: ") + BADGES[i].name : "Título removido", V3(1, 0.85f, 0.3f), 2);
            saveProfile();
        }, V3(1, 0.8f, 0.2f), 0.045f);
    }

    // ---- REDE
    const StationPt &ns = world.st[3];
    add(ns, -0.7f, 0.25f, "<", [this]() { prof.room = (prof.room + NROOMS - 1) % NROOMS; saveProfile(); });
    add(ns, 0.7f, 0.25f, ">", [this]() { prof.room = (prof.room + 1) % NROOMS; saveProfile(); });
    add(ns, -0.4f, -0.05f, "CONECTAR", [this]() {
        prof.online = true; saveProfile();
        net.connect(SERVER_HOST, SERVER_PORT, ROOMS[prof.room % NROOMS], prof.name);
        hud("Conectando a " SERVER_HOST "...", V3(0.7f, 0.9f, 1), 3);
    }, V3(0.3f, 1, 0.4f));
    add(ns, 0.4f, -0.05f, "SAIR", [this]() { prof.online = false; saveProfile(); net.disconnect(); hud("Modo offline", V3(1, 0.7f, 0.4f), 2); }, V3(1, 0.4f, 0.3f));
    add(ns, 0.0f, -0.35f, "NOVO NOME", [this]() {
        Rng r((unsigned)(now * 1000));
        static const char *a[] = {"Banana", "Macaco", "Bugio", "Mico", "Sagui", "Gorila", "Babu", "Chimpa", "Uakari", "Kong"};
        static const char *b[] = {"Veloz", "Medroso", "Valente", "Pelado", "Doido", "Ninja", "Real", "SemPerna", "Turbo", "Fantasma"};
        char buf[48]; snprintf(buf, sizeof buf, "%s%s%d", a[r.next() % 10], b[r.next() % 10], (int)(r.next() % 90 + 10));
        prof.name = buf; saveProfile();
        hud("Nome: " + prof.name + " (reconecte para atualizar)", V3(1, 1, 0.6f), 3);
    }, V3(0.9f, 0.9f, 0.3f));

    // ---- AJUSTES
    const StationPt &ms = world.st[5];
    add(ms, -0.55f, 0.35f, "ESPELHO", [this]() { prof.handMode = 0; saveProfile(); }, V3(0.5f, 0.8f, 1));
    add(ms, 0.45f, 0.35f, "ALTERNADA", [this]() { prof.handMode = 1; saveProfile(); }, V3(0.5f, 0.8f, 1));
    add(ms, -0.6f, 0.08f, "NORMAL", [this]() { prof.boost = 0; saveProfile(); }, V3(0.6f, 1, 0.6f));
    add(ms, 0.0f, 0.08f, "FORTE", [this]() { prof.boost = 1; saveProfile(); }, V3(1, 0.8f, 0.3f));
    add(ms, 0.6f, 0.08f, "TURBO", [this]() { prof.boost = 2; saveProfile(); }, V3(1, 0.35f, 0.3f));
    add(ms, -0.6f, -0.19f, "-", [this]() { prof.armAngle = std::max(0, prof.armAngle - 5); saveProfile(); });
    add(ms, 0.6f, -0.19f, "+", [this]() { prof.armAngle = std::min(60, prof.armAngle + 5); saveProfile(); });
    add(ms, -0.6f, -0.46f, "-", [this]() { prof.volume = std::max(0, prof.volume - 1); saveProfile(); });
    add(ms, 0.6f, -0.46f, "+", [this]() { prof.volume = std::min(10, prof.volume + 1); saveProfile(); });
    add(ms, 0.0f, -0.65f, "GIRO 30/45", [this]() { prof.snap ^= 1; saveProfile(); });

    // ---- SUPORTE DA CÂMERA
    const StationPt &cr = world.st[6];
    add(cr, 0.0f, 0.0f, "PEGAR CÂMERA", [this]() { camState = camState ? 0 : 1; hud(camState ? "Câmera na mão! Gatilho grava, deslize no touchpad para esticar." : "Câmera guardada", V3(0.8f, 0.9f, 1), 4); }, V3(1, 0.3f, 0.3f), 0.1f);
}

void Game::updateButtons(const Input &in) {
    hoverBtn = -1;
    laserOn = false;
    float dt = in.dt;
    // ponteiro laser do controle
    V3 a = handVis[ctrlHand], d = ctrlDirW;
    float best = 6;
    for (size_t i = 0; i < buttons.size(); i++) {
        Button &b = buttons[i];
        b.cool -= dt; b.flash -= dt;
        V3 oc = b.pos - a;
        float t = dot(oc, d);
        if (t < 0 || t > best) continue;
        float dist = len(oc - d * t);
        if (dist < b.r * 1.3f) { best = t; hoverBtn = (int)i; }
    }
    if (hoverBtn >= 0) { laserOn = true; laserFrom = a; laserTo = a + d * best; }
    auto press = [&](Button &b) {
        if (b.cool > 0) return;
        b.cool = 0.35f; b.flash = 0.2f;
        gAudio.play3D("ui_click", b.pos, 0.8f, 2);
        if (b.fn) b.fn();
    };
    if (in.triggerDown && hoverBtn >= 0) press(buttons[hoverBtn]);
    // toque físico com as mãos (as duas)
    static float prevD[256][2];
    for (size_t i = 0; i < buttons.size() && i < 256; i++) {
        for (int h = 0; h < 2; h++) {
            float dd = len(handVis[h] - buttons[i].pos);
            if (dd < buttons[i].r + 0.05f && prevD[i][h] >= buttons[i].r + 0.05f) press(buttons[i]);
            prevD[i][h] = dd;
        }
    }
}

// ------------------------------------------------------------------ avatar
void Game::drawAvatar(const V3 &bpos, float yaw, const Q &headRot, const V3 &lh, const V3 &rh, const Profile &p, bool drawHead, bool drawHands, float alpha) {
    M4 root = mTRS(bpos, qYaw(yaw));
    int hi = mMonkey->find("head");
    Pose pose; pose.init(mMonkey);
    if (hi >= 0) {
        Q rel = qYaw(-yaw) * headRot;
        pose.rot[hi] = rel;
        if (!drawHead) pose.off[hi] = V3(0, -500, 0);
    }
    V3 col = p.color();
    gfx->draw(mMonkey, root, col, &pose, 0, alpha);
    std::vector<M4> ws;
    computeWorlds(mMonkey, root, &pose, ws);
    V3 acc = ACC_COLORS[p.acc & 7];
    if (drawHead) {
        int ah = mMonkey->find("anchor_hat"), af = mMonkey->find("anchor_face");
        if (p.hat > 0 && p.hat < (int)mHats.size() && ah >= 0) gfx->draw(mHats[p.hat], ws[ah], acc);
        if (p.face > 0 && p.face < (int)mFaces.size() && af >= 0) gfx->draw(mFaces[p.face], ws[af], acc);
    }
    int an = mMonkey->find("anchor_neck");
    if (p.neck > 0 && p.neck < (int)mNecks.size() && an >= 0) gfx->draw(mNecks[p.neck], ws[an], acc);
    if (!drawHands) return;
    int sr = mMonkey->find("shoulder_r"), sl = mMonkey->find("shoulder_l");
    V3 sh[2] = {sl >= 0 ? ws[sl].pos() : bpos, sr >= 0 ? ws[sr].pos() : bpos};
    V3 hp[2] = {lh, rh};
    for (int i = 0; i < 2; i++) {
        V3 d = hp[i] - sh[i];
        float L = len(d);
        if (L < 0.01f) continue;
        V3 dn = d / L;
        V3 wrist = hp[i] - dn * 0.09f;
        float al = len(wrist - sh[i]);
        gfx->draw(mArm, mTRS(sh[i], qFromTo(V3(0, 1, 0), dn), V3(1, al, 1)), col);
        Q hq = qFromTo(V3(0, 0, -1), dn);
        gfx->draw(mHand, mTRS(wrist, hq), col);
    }
}

// ------------------------------------------------------------------ quadros do lobby
void Game::drawBoards() {
    auto board = [&](const StationPt &st, float w, float h, const char *title) {
        M4 m = stationM(st);
        gfx->draw(mBoard, m * mRot(qYaw(PI)) * mScale(V3(w, h, 1)));
        gfx->text(m * mTrans(V3(0, h / 2 - 0.13f, 0.035f)), title, 0.09f, YELLOW, 1);
        return m;
    };
    char b[256];
    // cores
    M4 m = board(world.st[0], 2.4f, 1.3f, "COR DO MACACO");
    const char *ch[3] = {"R", "G", "B"};
    int vals[3] = {prof.r, prof.g, prof.b};
    for (int k = 0; k < 3; k++) {
        snprintf(b, sizeof b, "%s %d", ch[k], vals[k]);
        gfx->text(m * mTrans(V3(-0.6f, 0.27f - k * 0.3f, 0.04f)), b, 0.07f, WHITE, 1);
    }
    gfx->text(m * mTrans(V3(0.58f, -0.52f, 0.04f)), "Toque os botões com as mãos", 0.04f, GREY, 1);
    // prévia (mini manequim em frente à estação)
    {
        V3 c = world.st[0].c;
        M4 mm = stationM(world.st[0]);
        V3 pos = mm.pt(V3(0, -0.75f, 0.9f));
        Profile p = prof;
        drawAvatar(pos, world.st[0].yaw + PI + sinf((float)now) * 0.6f, qYaw(world.st[0].yaw + PI), pos, pos, p, true, false);
        (void)c;
    }
    // guarda-roupa
    m = board(world.st[1], 2.3f, 1.3f, "GUARDA-ROUPA");
    snprintf(b, sizeof b, "Chapéu: %s", HAT_NAMES[prof.hat % 8]); gfx->text(m * mTrans(V3(0, 0.32f, 0.04f)), b, 0.065f, WHITE, 1);
    snprintf(b, sizeof b, "Rosto: %s", FACE_NAMES[prof.face % 3]); gfx->text(m * mTrans(V3(0, 0.07f, 0.04f)), b, 0.065f, WHITE, 1);
    snprintf(b, sizeof b, "Pescoço: %s", NECK_NAMES[prof.neck % 2]); gfx->text(m * mTrans(V3(0, -0.18f, 0.04f)), b, 0.065f, WHITE, 1);
    snprintf(b, sizeof b, "Cor do acessório: %s", ACC_NAMES[prof.acc & 7]); gfx->text(m * mTrans(V3(0, -0.43f, 0.04f)), b, 0.065f, WHITE, 1);
    {
        M4 mm = stationM(world.st[1]);
        V3 pos = mm.pt(V3(0, -0.75f, 0.9f));
        float yaw = world.st[1].yaw + PI + (float)now * 0.8f;
        drawAvatar(pos, yaw, qYaw(yaw), pos, pos, prof, true, false);
    }
    // badges
    m = board(world.st[2], 2.6f, 1.75f, "BADGES");
    for (int i = 0; i < NUM_BADGES; i++) {
        bool got = prof.badges & (1u << i);
        float x = (i % 2) ? 0.18f : -0.97f, y = 0.58f - (i / 2) * 0.2f;
        snprintf(b, sizeof b, "%s%s", got ? "" : "? ", BADGES[i].name);
        float c[4] = {got ? 1.f : 0.45f, got ? (prof.title == i ? 0.6f : 0.9f) : 0.45f, got ? (prof.title == i ? 0.2f : 0.5f) : 0.45f, 1};
        gfx->text(m * mTrans(V3(x, y - 0.03f, 0.04f)), b, 0.055f, c);
        gfx->text(m * mTrans(V3(x, y - 0.09f, 0.04f)), BADGES[i].desc, 0.03f, GREY);
    }
    // rede
    m = board(world.st[3], 2.0f, 1.35f, "MULTIPLAYER");
    const char *st = net.state() == NET_ONLINE ? "ONLINE" : (net.state() == NET_CONNECTING ? "CONECTANDO..." : (net.state() == NET_FAILED ? "FALHOU" : "OFFLINE"));
    snprintf(b, sizeof b, "Sala: %s", ROOMS[prof.room % NROOMS]); gfx->text(m * mTrans(V3(0, 0.22f, 0.04f)), b, 0.07f, WHITE, 1);
    snprintf(b, sizeof b, "%s  %s:%d", st, SERVER_HOST, SERVER_PORT); gfx->text(m * mTrans(V3(0, 0.4f, 0.04f)), b, 0.04f, net.state() == NET_ONLINE ? YELLOW : GREY, 1);
    if (net.state() == NET_FAILED) gfx->text(m * mTrans(V3(0, 0.08f, 0.04f)), net.error.c_str(), 0.035f, GREY, 1);
    snprintf(b, sizeof b, "Você: %s", prof.name.c_str()); gfx->text(m * mTrans(V3(0, -0.22f, 0.04f)), b, 0.05f, WHITE, 1);
    std::string list = "Na sala: ";
    for (auto &r : remotes) list += r.name + "  ";
    if (remotes.empty()) list += "(ninguém)";
    gfx->text(m * mTrans(V3(-0.9f, -0.5f, 0.04f)), list.c_str(), 0.035f, GREY, 0, 1.8f);
    // como jogar
    m = board(world.st[4], 2.6f, 1.75f, "COMO JOGAR / CORRER");
    gfx->text(m * mTrans(V3(-1.2f, 0.55f, 0.04f)),
              "Você é um macaco SEM PERNAS: anda com as mãos!\n"
              "- Aponte o controle para baixo até a mão tocar o chão e EMPURRE para trás. A outra mão copia o movimento.\n"
              "- Para CORRER: empurre rápido várias vezes e solte no meio do movimento. O impulso acelera e fica mais forte.\n"
              "- Suba em prateleiras e caixas empurrando as paredes.\n"
              "- Touchpad esq/dir: girar. Centro: grito de macaco.\n"
              "- VOLTAR: pegar câmera. Gatilho: gravar. Deslize: esticar o bastão. Cima: selfie/mundo. Baixo: soltar no ar.\n"
              "- Pegue 8 bananas douradas, a porta de aço abre: FUJA!\n"
              "- O Pálido só anda quando ninguém olha para ele.",
              0.045f, WHITE, 0, 2.4f);
    // ajustes
    m = board(world.st[5], 2.0f, 1.75f, "AJUSTES");
    snprintf(b, sizeof b, "Mão duplicada: %s", prof.handMode ? "ALTERNADA" : "ESPELHO"); gfx->text(m * mTrans(V3(0, 0.5f, 0.04f)), b, 0.05f, WHITE, 1);
    snprintf(b, sizeof b, "Impulso: %s", prof.boost == 0 ? "NORMAL" : (prof.boost == 1 ? "FORTE" : "TURBO")); gfx->text(m * mTrans(V3(0, 0.2f, 0.04f)), b, 0.05f, WHITE, 1);
    snprintf(b, sizeof b, "Ângulo do braço: %d°", prof.armAngle); gfx->text(m * mTrans(V3(0, -0.21f, 0.04f)), b, 0.05f, WHITE, 1);
    snprintf(b, sizeof b, "Volume: %d", prof.volume); gfx->text(m * mTrans(V3(0, -0.48f, 0.04f)), b, 0.05f, WHITE, 1);
    snprintf(b, sizeof b, "Giro: %d°", prof.snap ? 45 : 30); gfx->text(m * mTrans(V3(0, -0.78f, 0.04f)), b, 0.04f, GREY, 1);
    // suporte da câmera
    {
        M4 cm = stationM(world.st[6]);
        gfx->draw(mStick, cm * mTrans(V3(0, -1.2f, 0.3f)) * mScale(V3(2, 1.0f, 2)));
        if (camState == 0) gfx->draw(mCamera, cm * mTrans(V3(0, -0.12f, 0.3f)) * mRot(qYaw(PI)));
        gfx->text(cm * mTrans(V3(0, 0.2f, 0.1f)), "CÂMERA", 0.08f, YELLOW, 1);
    }
    // botões
    for (size_t i = 0; i < buttons.size(); i++) {
        Button &bt = buttons[i];
        if (!bt.visible) continue;
        Q q = qFromTo(V3(0, 0, -1), -bt.nrm);  // o modelo do botão olha para -Z
        float push = bt.flash > 0 ? -0.015f : 0;
        V3 c = (int)i == hoverBtn ? bt.color * 1.25f : bt.color;
        float s = bt.r / 0.05f;
        gfx->draw(mButton, mTRS(bt.pos + bt.nrm * push, q, V3(s, s, 1)), c);
        if (!bt.label.empty()) {
            M4 tm = mTRS(bt.pos + bt.nrm * 0.03f + V3(0, -bt.r - 0.06f, 0), qLook(-bt.nrm, V3(0, 1, 0)));
            gfx->text(tm, bt.label.c_str(), 0.045f, WHITE, 1);
        }
    }
    // texto do portal
    gfx->text(mTRS(world.portalC + V3(0, 2.4f, -1.8f), qYaw(PI)), "ENTRAR NO DEPÓSITO", 0.14f, YELLOW, 1);
}

// ------------------------------------------------------------------ renderização
void Game::render(const DrawCtx &ctx, bool forCamera) {
    gfx->begin(ctx, scene());
    gfx->draw(mMap, M4());
    // porta de aço da saída
    gfx->draw(mGate, mTrans(world.gateC + V3(0, gateAnim * 3.9f, 0)));
    // bananas
    if (roundPhase == 1)
        for (size_t i = 0; i < roundBananas.size(); i++) {
            if (collected & (1u << i)) continue;
            V3 p = world.bananas[roundBananas[i]] + V3(0, sinf((float)now * 2 + i) * 0.06f, 0);
            gfx->draw(mBanana, mTRS(p, qYaw((float)now * 1.5f + i) * qAxis(V3(0, 0, 1), 0.5f), V3(1.3f, 1.3f, 1.3f)));
        }
    // monstros
    for (int i = 0; i < 3; i++) {
        Monster &m = mons[i];
        if (!m.active || roundPhase != 1) continue;
        Model *mdl = i == 0 ? mBabuun : (i == 1 ? mPalido : mMamae);
        Pose &p = pMon[i];
        p.init(mdl);
        float ph = m.anim, amp = m.state == 5 ? 0 : clampf(m.speed / 4, 0.15f, 0.8f);
        auto R = [&](const char *n, const Q &q) { int k = mdl->find(n); if (k >= 0) p.rot[k] = q; };
        if (i != 1) {
            R("leg_fr", qAxis(V3(1, 0, 0), sinf(ph) * amp));
            R("leg_bl", qAxis(V3(1, 0, 0), sinf(ph) * amp));
            R("leg_fl", qAxis(V3(1, 0, 0), -sinf(ph) * amp));
            R("leg_br", qAxis(V3(1, 0, 0), -sinf(ph) * amp));
            R("tail", qAxis(V3(0, 1, 0), sinf(ph * 0.7f) * 0.5f));
            R("head", qAxis(V3(1, 0, 0), sinf(ph * 2) * 0.08f + (m.state == 2 ? -0.2f : 0.1f)));
            R("jaw", qAxis(V3(1, 0, 0), m.jaw * 0.6f + sinf((float)now * 9) * 0.05f * m.jaw));
        } else {
            R("leg_r", qAxis(V3(1, 0, 0), sinf(ph) * amp * 0.7f));
            R("leg_l", qAxis(V3(1, 0, 0), -sinf(ph) * amp * 0.7f));
            R("arm_r", qAxis(V3(1, 0, 0), -sinf(ph) * amp * 0.5f) * qAxis(V3(0, 0, 1), 0.1f));
            R("arm_l", qAxis(V3(1, 0, 0), sinf(ph) * amp * 0.5f) * qAxis(V3(0, 0, 1), -0.1f));
            R("head", qAxis(V3(0, 0, 1), sinf((float)now * 0.7f) * 0.35f) * qAxis(V3(1, 0, 0), 0.25f));
            R("jaw", qAxis(V3(1, 0, 0), m.jaw * 0.5f));
        }
        float bob = (i != 1) ? fabsf(sinf(ph)) * 0.05f * amp : 0;
        gfx->draw(mdl, mTRS(m.pos + V3(0, bob, 0), qYaw(m.yaw)), V3(1, 1, 1), &p);
    }
    // jogadores remotos
    for (auto &r : remotes) {
        if (r.state == 'D') continue;
        Profile p; p.r = r.r; p.g = r.g; p.b = r.b; p.hat = r.hat; p.face = r.face; p.neck = r.neck; p.acc = r.acc;
        drawAvatar(r.body, r.yaw, r.headRot, r.lh, r.rh, p, true, true);
        std::string tag = r.name;
        if (r.title >= 0 && r.title < NUM_BADGES) tag = std::string("[") + BADGES[r.title].name + "] " + tag;
        V3 tp = r.body + V3(0, 0.75f, 0);
        gfx->text(mTRS(tp, qLook(tp - ctx.camPos, V3(0, 1, 0))), tag.c_str(), 0.07f, WHITE, 1);
    }
    // jogador local
    if (pstate != PS_CAUGHT) drawAvatar(body, bodyYaw, headWorld, handVis[0], handVis[1], prof, forCamera, true);
    // lobby
    if (body.x < -150 || forCamera) drawBoards();
    // câmera segurável
    if (camState != 0) {
        if (camState == 1) {
            V3 base = handVis[ctrlHand];
            V3 d = camPos - base; float L = len(d);
            if (L > 0.01f) gfx->draw(mStick, mTRS(base, qFromTo(V3(0, 1, 0), d / L), V3(1, L, 1)));
            gfx->draw(mGrip, mTRS(base, qFromTo(V3(0, 0, 1), d / fmaxf(L, 0.01f))));
        }
        if (!forCamera) {
            gfx->draw(mCamera, mTRS(camPos, camRot), V3(1, 1, 1), nullptr, camTex);
            if (gRecorder.active() && fmodf((float)now, 1.f) < 0.5f) {
                float red[4] = {1, 0.1f, 0.1f, 1};
                gfx->quad(mTRS(camPos + camRot.rot(V3(-0.06f, 0.06f, 0.03f)), camRot, V3(0.02f, 0.02f, 1)), red);
            }
        }
    }
    if (laserOn && !forCamera) {
        V3 d = laserTo - laserFrom; float L = len(d);
        if (L > 0.01f) gfx->draw(mStick, mTRS(laserFrom, qFromTo(V3(0, 1, 0), d / L), V3(0.3f, L, 0.3f)), V3(1, 0.3f, 0.3f));
    }
    if (!forCamera) drawHud();
}

// ------------------------------------------------------------------ HUD, jumpscare, fade
void Game::drawHud() {
    // HUD acompanha a cabeça com atraso suave
    float hy = qGetYaw(headWorld);
    hudYaw += wrapAngle(hy - hudYaw) * 0.08f;
    glDisable(GL_DEPTH_TEST);
    M4 hm = mTRS(headPosW, qYaw(hudYaw) * qAxis(V3(1, 0, 0), -0.25f));
    float y = -0.05f;
    for (auto &m : msgs) {
        float a = clampf(m.time, 0, 1);
        float c[4] = {m.col.x, m.col.y, m.col.z, a};
        gfx->text(hm * mTrans(V3(0, y, -1.2f)), m.t.c_str(), 0.04f, c, 1, 1.3f);
        y -= 0.07f;
    }
    if (pstate == PS_GAME && roundPhase == 1) {
        char b[64];
        snprintf(b, sizeof b, "Bananas %d/%d%s", __builtin_popcount(collected), (int)roundBananas.size(), gateOpen ? "  - SAÍDA ABERTA!" : "");
        gfx->text(mTRS(headPosW, qYaw(hudYaw) * qAxis(V3(1, 0, 0), 0.3f)) * mTrans(V3(0, 0, -1.3f)), b, 0.045f, YELLOW, 1);
    }
    if (gRecorder.active()) {
        char b[64]; int s = (int)gRecorder.elapsed();
        snprintf(b, sizeof b, "%s REC %02d:%02d", fmodf((float)now, 1.f) < 0.5f ? "\xE2\x97\x8F" : " ", s / 60, s % 60);
        float red[4] = {1, 0.25f, 0.25f, 1};
        gfx->text(mTRS(headPosW, qYaw(hudYaw) * qAxis(V3(0, 1, 0), 0.35f) * qAxis(V3(1, 0, 0), 0.25f)) * mTrans(V3(0, 0, -1.3f)), b, 0.045f, red, 1);
    }
    // JUMPSCARE: o monstro avança no seu rosto
    if (pstate == PS_CAUGHT && jsT < 2.7f) {
        glEnable(GL_DEPTH_TEST);
        glClear(GL_DEPTH_BUFFER_BIT);
        Model *mdl = jsType == 0 ? mBabuun : (jsType == 1 ? mPalido : mMamae);
        float headH = jsType == 0 ? 1.0f : (jsType == 1 ? 2.15f : 2.0f);
        float sc = jsType == 2 ? 0.55f : 1.0f;
        float t = jsT;
        float dist = lerpf(1.6f, 0.38f, smoothstep(0, 0.22f, t)) * sc + (jsType == 2 ? 0.35f : 0);
        Rng r((unsigned)(t * 60) + 7);
        V3 shake(r.r(-1, 1) * 0.03f, r.r(-1, 1) * 0.03f, 0);
        Pose p; p.init(mdl);
        int jw = mdl->find("jaw"), hd = mdl->find("head");
        if (jw >= 0) p.rot[jw] = qAxis(V3(1, 0, 0), 0.9f + sinf(t * 40) * 0.1f);
        if (hd >= 0) p.rot[hd] = qAxis(V3(0, 0, 1), sinf(t * 31) * 0.15f) * qAxis(V3(1, 0, 0), -0.25f);
        M4 w = mTRS(headPosW, headWorld) * mTrans(V3(0, -headH * sc + 0.05f, -dist) + shake) * mRot(qYaw(PI)) * mScale(V3(sc, sc, sc));
        Scene s = scene();
        s.nLights = 1; s.lights[0] = {headPosW, 4, V3(1.4f, 0.4f, 0.3f)};
        gfx->begin(gfx->ctx, s);
        gfx->draw(mdl, w, V3(1, 1, 1), &p);
        glDisable(GL_DEPTH_TEST);
        float pulse = 0.25f + 0.25f * sinf(t * 30);
        float red[4] = {0.8f, 0, 0, pulse * clampf(1 - t / 2.7f, 0, 1)};
        gfx->quad(mTRS(headPosW, headWorld) * mTrans(V3(0, 0, -0.3f)) * mScale(V3(2, 2, 1)), red);
    }
    if (fade > 0.001f) {
        float blk[4] = {0, 0, 0, fade};
        gfx->quad(mTRS(headPosW, headWorld) * mTrans(V3(0, 0, -0.25f)) * mScale(V3(3, 3, 1)), blk);
    }
    glEnable(GL_DEPTH_TEST);
}

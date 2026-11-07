// Babuun Hollow — núcleo: jogador, gorilla locomotion, câmera, rodada e rede
#include "game.h"
#include "audio.h"
#include "recorder.h"
#include <android/log.h>
#include <stdio.h>
#include <sstream>
#include <fstream>
#include <algorithm>
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "BabuunHollow", __VA_ARGS__)

const Badge BADGES[] = {
    {"Primeiro Passo", "Entre no Depósito pela primeira vez"},
    {"Banana!", "Pegue uma banana dourada"},
    {"Colecionador", "Pegue todas as bananas de uma rodada"},
    {"Fuga!", "Escape pela porta de aço"},
    {"Sobrevivente", "Fique 5 minutos vivo numa rodada"},
    {"Mordido", "Seja pego pelo Babuun"},
    {"Não Pisque", "Seja pego pelo Pálido"},
    {"Mamãe Brava", "Seja pego pela Mamãe"},
    {"Macaco Social", "Jogue com outro macaco online"},
    {"Estilista", "Mude sua cor e um acessório"},
    {"Cinegrafista", "Grave um vídeo com a câmera"},
    {"Alpinista", "Suba no topo de uma prateleira"},
    {"Velocista", "Alcance 10 m/s"},
    {"Veterano", "Escape 5 vezes"},
};
const int NUM_BADGES = sizeof(BADGES) / sizeof(BADGES[0]);

static const char *HAT_MODELS[] = {"", "cos_cap", "cos_beanie", "cos_tophat", "cos_crown", "cos_leafcrown", "cos_horns", "cos_headphones"};
static const char *FACE_MODELS[] = {"", "cos_glasses", "cos_shades"};
static const char *NECK_MODELS[] = {"", "cos_bowtie"};
static const V3 ACC_COLORS[] = {V3(0.85f, 0.15f, 0.12f), V3(0.15f, 0.35f, 0.9f), V3(0.2f, 0.7f, 0.25f), V3(0.95f, 0.8f, 0.1f),
                                V3(0.6f, 0.2f, 0.8f), V3(0.95f, 0.95f, 0.95f), V3(0.1f, 0.1f, 0.1f), V3(1.0f, 0.5f, 0.1f)};
static const char *ROOMS[] = {"PUBLICA", "SALA1", "SALA2", "SALA3", "AMIGOS", "TREINO"};
static const int NROOMS = 6;
static const float GRAV = 9.8f, BODY_R = 0.28f, ARM_LEN = 0.85f;
static const float BOOST_MUL[] = {1.3f, 1.6f, 1.9f}, BOOST_MAX[] = {8.5f, 10.5f, 12.5f};

bool Game::isHost() const { return net.state() != NET_ONLINE || net.myId == net.hostId; }
int Game::myId() const { return net.state() == NET_ONLINE ? net.myId : 0; }

// ------------------------------------------------------------------ perfil
void Game::saveProfile() {
    FILE *f = fopen((saveDir + "/perfil.txt").c_str(), "w");
    if (!f) return;
    fprintf(f, "%s\n%d %d %d %d %d %d %d %d %u %d %d %d %d %d %d %d %d %d\n", prof.name.c_str(), prof.r, prof.g, prof.b, prof.hat, prof.face,
            prof.neck, prof.acc, prof.title, prof.badges, prof.escapes, prof.catches, prof.handMode, prof.boost, prof.snap, prof.armAngle,
            prof.volume, prof.room, prof.online ? 1 : 0);
    fclose(f);
}

static std::string randomName(Rng &r) {
    static const char *a[] = {"Banana", "Macaco", "Bugio", "Mico", "Sagui", "Gorila", "Babu", "Chimpa", "Uakari", "Kong"};
    static const char *b[] = {"Veloz", "Medroso", "Valente", "Pelado", "Doido", "Ninja", "Real", "Sem-Perna", "Turbo", "Fantasma"};
    char buf[48];
    snprintf(buf, sizeof buf, "%s%s%d", a[r.next() % 10], b[r.next() % 10], (int)(r.next() % 90 + 10));
    return buf;
}

bool Game::init(Gfx *g, const std::string &sd, const std::string &vd, const std::string &vd2) {
    gfx = g; saveDir = sd; videoDir = vd; videoDir2 = vd2;
    std::vector<unsigned char> mt;
    readAsset("map.txt", mt);
    mt.push_back(0);
    world.load((const char *)mt.data());
    grid = world.grid; gCols = world.gCols; gRows = world.gRows; gCell = world.gCell; gOx = world.gOx; gOz = world.gOz;
    // perfil
    Rng rng((unsigned)time(nullptr));
    FILE *f = fopen((saveDir + "/perfil.txt").c_str(), "r");
    if (f) {
        char name[64] = {0};
        if (fgets(name, sizeof name, f)) { name[strcspn(name, "\r\n")] = 0; prof.name = name; }
        int on = 1;
        fscanf(f, "%d %d %d %d %d %d %d %d %u %d %d %d %d %d %d %d %d %d", &prof.r, &prof.g, &prof.b, &prof.hat, &prof.face, &prof.neck, &prof.acc,
               &prof.title, &prof.badges, &prof.escapes, &prof.catches, &prof.handMode, &prof.boost, &prof.snap, &prof.armAngle, &prof.volume,
               &prof.room, &on);
        prof.online = on != 0;
        fclose(f);
    } else prof.name = randomName(rng);
    gAudio.master = prof.volume / 10.f;

    mMap = gfx->model("map"); mMonkey = gfx->model("monkey"); mHand = gfx->model("hand"); mArm = gfx->model("arm");
    mBanana = gfx->model("banana"); mBabuun = gfx->model("babuun"); mPalido = gfx->model("palido"); mMamae = gfx->model("mamae");
    mGate = gfx->model("gate"); mCamera = gfx->model("camera"); mStick = gfx->model("stick"); mGrip = gfx->model("grip");
    mButton = gfx->model("button"); mBoard = gfx->model("board");
    for (auto n : HAT_MODELS) mHats.push_back(*n ? gfx->model(n) : nullptr);
    for (auto n : FACE_MODELS) mFaces.push_back(*n ? gfx->model(n) : nullptr);
    for (auto n : NECK_MODELS) mNecks.push_back(*n ? gfx->model(n) : nullptr);
    pMonkey.init(mMonkey);
    pMon[0].init(mBabuun); pMon[1].init(mPalido); pMon[2].init(mMamae);
    for (int i = 0; i < 3; i++) { mons[i].type = i; mons[i].pos = V3(world.mspawn[i].x, 0, world.mspawn[i].z); mons[i].netPos = mons[i].pos; }

    // pré-carrega sons
    const char *snds[] = {"tap_wood", "tap_ground", "tap_stone", "tap_metal", "tap_leaf", "whoosh", "banana", "badge", "ui_click", "ui_error",
                          "rec_start", "rec_stop", "shutter", "gate_open", "hoot", "babuun_growl", "babuun_bark", "babuun_screech",
                          "palido_breath", "palido_shriek", "mamae_roar", "step_light", "step_heavy", "js_babuun", "js_palido", "js_mamae",
                          "amb_night", "amb_lobby", "chase", "heartbeat", "escape"};
    for (auto s : snds) gAudio.find(s);

    buildLobby();
    teleport(world.spawnLobby, PI);
    toLobby();
    if (prof.online) net.connect(SERVER_HOST, SERVER_PORT, ROOMS[prof.room % NROOMS], prof.name);
    hud("Bem-vindo ao Babuun Hollow, " + prof.name + "!", V3(1, 0.9f, 0.4f), 5);
    hud("Mire o controle para baixo e empurre o chão para andar.", V3(1, 1, 1), 6);
    if (askPermissions) askPermissions();
    return true;
}

void Game::onPause() { saveProfile(); if (gRecorder.active()) { gRecorder.stop(); gAudio.stopMic(); gAudio.recordSink = nullptr; } }

// ------------------------------------------------------------------ utilidades
void Game::hud(const std::string &t, V3 col, float time) {
    msgs.push_back({t, time, col});
    if (msgs.size() > 4) msgs.erase(msgs.begin());
}

void Game::award(int b) {
    if (b < 0 || b >= NUM_BADGES || (prof.badges & (1u << b))) return;
    prof.badges |= 1u << b;
    hud(std::string("BADGE: ") + BADGES[b].name + "!", V3(1, 0.85f, 0.2f), 5);
    gAudio.play("badge", 0.8f);
    saveProfile();
}

void Game::teleport(const V3 &p, float yaw) {
    body = p; vel = V3();
    rigYaw = yaw;
    touching[0] = touching[1] = false;
    histN = 0;
}

M4 Game::rigMatrix() const {
    // o rastreamento do Go é 3DoF: a cabeça fica 0.3m acima do centro do corpo
    return mTRS(body + V3(0, 0.3f, 0), qYaw(rigYaw));
}

void Game::computeWorlds(const Model *m, const M4 &root, const Pose *pose, std::vector<M4> &out) const {
    out.resize(m->nodes.size());
    for (size_t i = 0; i < m->nodes.size(); i++) {
        M4 local = m->nodes[i].local;
        if (pose && i < pose->rot.size()) local = local * mTrans(pose->off[i]) * mRot(pose->rot[i]);
        out[i] = m->nodes[i].parent >= 0 ? out[m->nodes[i].parent] * local : root * local;
    }
}

void Game::enterGame() {
    pstate = PS_GAME;
    aliveT = 0;
    teleport(world.spawnForest, 0);
    award(0);
    sendEv("ENTER");
    if (isHost() && roundPhase == 0) startRound((unsigned)(now * 1000) ^ 0x5bd1e995u);
    hud("Pegue as bananas douradas e fuja pela porta de aço!", V3(1, 0.9f, 0.3f), 6);
}

void Game::toLobby() {
    pstate = PS_LOBBY;
    teleport(world.spawnLobby, PI);
    jsT = -1;
}

// ------------------------------------------------------------------ atualização principal
void Game::update(const Input &in) {
    float dt = clampf(in.dt, 0.001f, 0.05f);
    now = in.time;
    gfx->sc.time = (float)now;
    for (auto &m : msgs) m.time -= dt;
    msgs.erase(std::remove_if(msgs.begin(), msgs.end(), [](const Msg &m) { return m.time <= 0; }), msgs.end());

    // orientação da cabeça no mundo
    headWorld = qYaw(rigYaw) * in.head;
    headPosW = rigMatrix().pt(in.headPos);
    float hy = qGetYaw(in.head);
    bodyYaw = rigYaw + hy;
    gAudio.setListener(headPosW, headWorld);

    if (pstate == PS_CAUGHT) {
        jsT += dt;
        if (jsT > 2.6f) { fade = clampf((jsT - 2.6f) / 0.8f, 0, 1); }
        if (jsT > 3.6f) { toLobby(); fade = 1; }
    } else {
        fade = fmaxf(0, fade - dt * 1.5f);
        handleInput(in);
        locomotion(in, dt);
    }
    updateCamera(in, dt);
    if (pstate == PS_LOBBY) updateButtons(in);

    // gatilhos de zona
    if (pstate == PS_LOBBY && len(body - world.portalC) < world.portalR + 0.3f) enterGame();
    if (pstate == PS_GAME) {
        aliveT += dt;
        if (aliveT > 300) award(4);
        if (body.y > 3.3f) award(11);
        // bananas
        for (size_t i = 0; i < roundBananas.size(); i++) {
            if (collected & (1u << i)) continue;
            V3 bp = world.bananas[roundBananas[i]];
            if (len(handVis[0] - bp) < 0.35f || len(handVis[1] - bp) < 0.35f || len(body - bp) < 0.5f) {
                if (isHost()) pickBanana((int)i, myId());
                else { sendEv("PICK " + std::to_string(i)); collected |= 1u << i; gAudio.play("banana", 0.9f); }
            }
        }
        if (gateOpen && len(V3(body.x, 0, body.z) - V3(world.gateExit.x, 0, world.gateExit.z)) < 2.2f) {
            pstate = PS_ESCAPED;
            prof.escapes++;
            award(3);
            if (prof.escapes >= 5) award(13);
            gAudio.play("escape", 1);
            hud("VOCÊ ESCAPOU DO DEPÓSITO!", V3(0.4f, 1, 0.5f), 5);
            sendEv("ESCAPED");
            saveProfile();
            toLobby();
        }
        if (!world.inForest(body) && body.x > -150) { /* fora dos limites */ }
    }
    if (isHost()) hostUpdate(dt);
    netUpdate(dt);
    updateAudio(dt);
    // anima portão
    gateAnim = clampf(gateAnim + (gateOpen ? dt * 0.35f : -dt), 0, 1);
}

// ------------------------------------------------------------------ entrada: giro, uivo, câmera
void Game::handleInput(const Input &in) {
    bool horiz = fabsf(in.padX) > 0.45f && fabsf(in.padX) > fabsf(in.padY);
    if (in.padClickDown && horiz) {
        float a = (prof.snap ? 45.f : 30.f) * PI / 180.f;
        rigYaw += in.padX > 0 ? -a : a;
        // gira em torno da cabeça
        gAudio.play("ui_click", 0.3f);
    } else if (in.padClickDown && camState == 0 && fabsf(in.padX) < 0.45f && fabsf(in.padY) < 0.45f) {
        gAudio.play3D("hoot", headPosW, 1.0f, 4);
        sendEv("HOOT");
        makeNoise(body, 1.5f);
    }
}

// ------------------------------------------------------------------ GORILLA LOCOMOTION (com mão duplicada)
void Game::locomotion(const Input &in, float dt) {
    ctrlHand = in.leftHanded ? 0 : 1;
    // direção do braço: controle inclinado para baixo pelo ângulo configurado
    Q armQ = in.ctrl * qAxis(V3(1, 0, 0), -prof.armAngle * PI / 180.f);
    V3 dT = armQ.rot(V3(0, 0, -1));                 // espaço de rastreamento
    ctrlDirW = qYaw(rigYaw).rot(in.ctrl.rot(V3(0, 0, -1)));
    V3 dW = qYaw(rigYaw).rot(dT);
    // para o referencial do corpo (yaw da cabeça)
    Q toBody = qYaw(-bodyYaw);
    V3 dM = toBody.rot(dW);
    // decompõe no plano sagital do corpo: theta = ângulo frente/baixo/trás, x = componente lateral.
    // Assim apontar para trás (passando da vertical) não "vira" a mão para a frente.
    float side = ctrlHand == 1 ? 1.f : -1.f;
    float x = clampf(dM.x * side, -0.35f, 0.8f) * side;  // limita cruzar os braços
    float sp = sqrtf(fmaxf(0.f, 1 - x * x));
    float theta = atan2f(dM.y, -dM.z);                    // 0 = frente, -PI/2 = baixo, -PI = trás
    dM = V3(x, sinf(theta) * sp, -cosf(theta) * sp);
    // mão duplicada
    V3 dO;
    if (prof.handMode == 0) {
        dO = V3(-dM.x, dM.y, dM.z);  // espelho: as duas mãos juntas (anda reto)
    } else {
        // alternada: enquanto a mão do controle empurra o chão, a duplicada vai para o lado
        // oposto (para a frente e para cima); quando a mão levanta, as duas se juntam de novo.
        const float rest = -1.45f;
        float t2 = fmaxf(clampf(2 * rest - theta, -2.7f, 0.6f), -0.35f);
        altBlend = lerpf(altBlend, touching[ctrlHand] ? 1.f : 0.f, 1 - expf(-dt * 14));
        V3 opp(-dM.x, sinf(t2) * sp, -cosf(t2) * sp), mir(-dM.x, dM.y, dM.z);
        dO = norm(lerp(mir, opp, altBlend));
    }
    // quando a mão sobe, as duas se aproximam do centro (juntando)
    float tl = smoothstep(-0.25f, 0.65f, dM.y);
    lift = lerpf(lift, tl, 1 - expf(-dt * 12));
    float shX = 0.2f * (1 - 0.7f * lift);
    V3 dirs[2];
    dirs[ctrlHand] = dM; dirs[1 - ctrlHand] = dO;
    Q fromBody = qYaw(bodyYaw);
    V3 bodyDelta; int cnt = 0;
    for (int i = 0; i < 2; i++) {
        float sx = i == 1 ? 1.f : -1.f;
        V3 shL(sx * shX, 0.14f, 0);
        V3 d = dirs[i];
        d.x -= sx * lift * 0.18f;  // convergência das mãos à frente
        d = norm(d);
        shoulderW[i] = body + fromBody.rot(shL);
        V3 tgt = shoulderW[i] + fromBody.rot(d) * ARM_LEN;
        V3 seg = tgt - shoulderW[i];
        float L = len(seg);
        V3 sd = seg / L;
        if (touching[i]) {
            Hit h = world.raycast(shoulderW[i], tgt + sd * 0.04f);
            if (!h.hit) touching[i] = false;  // mão saiu da superfície
            else { bodyDelta += anchor[i] - tgt; cnt++; }
        } else {
            Hit h = world.raycast(shoulderW[i], tgt);
            if (h.hit) {
                touching[i] = true;
                anchor[i] = h.p + h.n * 0.01f;
                float sp = len(tgt - prevTarget[i]) / dt;
                if (sp > 0.35f) {
                    static const char *tap[] = {"tap_ground", "tap_wood", "tap_stone", "tap_metal", "tap_leaf"};
                    gAudio.play3D(tap[h.mat % 5], anchor[i], clampf(sp * 0.25f, 0.2f, 1.0f), 3);
                    if (sp > 2.0f) makeNoise(anchor[i], 1.0f);
                }
                bodyDelta += anchor[i] - tgt; cnt++;
            }
        }
        handTarget[i] = tgt;
        prevTarget[i] = tgt;
    }
    bool anyTouch = cnt > 0;
    if (anyTouch) {
        V3 delta = bodyDelta / (float)cnt;
        if (len(delta) > 0.4f) delta = norm(delta) * 0.4f;
        V3 before = body;
        body += delta;
        V3 n = world.resolveSphere(body, BODY_R);
        V3 moved = body - before;
        hist[histI] = moved / dt; histI = (histI + 1) % 6; if (histN < 6) histN++;
        vel = V3();
        (void)n;
    } else {
        if (wasTouching && histN > 0) {
            // soltou: impulso (acelerado e mais forte)
            V3 avg; for (int k = 0; k < histN; k++) avg += hist[k];
            avg = avg / (float)histN;
            vel = avg * BOOST_MUL[prof.boost];
            float sp = len(vel);
            if (sp > BOOST_MAX[prof.boost]) vel = vel * (BOOST_MAX[prof.boost] / sp);
            if (sp > 3.5f) gAudio.play3D("whoosh", body, clampf(sp / 12, 0.1f, 0.6f), 2);
            histN = 0;
        }
        vel.y -= GRAV * dt;
        body += vel * dt;
        V3 n = world.resolveSphere(body, BODY_R);
        if (len(n) > 0) {
            V3 nn = norm(n);
            float vn = dot(vel, nn);
            if (vn < 0) vel -= nn * vn;
            if (nn.y > 0.6f) { vel.x *= 1 - fminf(1, dt * 1.5f); vel.z *= 1 - fminf(1, dt * 1.5f); }
        }
    }
    wasTouching = anyTouch;
    for (int i = 0; i < 2; i++) handVis[i] = touching[i] ? anchor[i] : handTarget[i];
    speedNow = len(vel);
    if (speedNow > 10.f) award(12);
    if (body.y < -5) teleport(pstate == PS_GAME ? world.spawnForest : world.spawnLobby, rigYaw);
}

// ------------------------------------------------------------------ CÂMERA SEGURÁVEL E ESTICÁVEL
Q qLook(const V3 &fwd, const V3 &upHint) {
    V3 z = norm(-fwd);
    V3 x = norm(cross(upHint, z));
    if (len(x) < 1e-4f) x = V3(1, 0, 0);
    V3 y = cross(z, x);
    float m00 = x.x, m11 = y.y, m22 = z.z, tr = m00 + m11 + m22;
    Q q;
    if (tr > 0) { float s = sqrtf(tr + 1) * 2; q = Q((y.z - z.y) / s, (z.x - x.z) / s, (x.y - y.x) / s, 0.25f * s); }
    else if (m00 > m11 && m00 > m22) { float s = sqrtf(1 + m00 - m11 - m22) * 2; q = Q(0.25f * s, (y.x + x.y) / s, (z.x + x.z) / s, (y.z - z.y) / s); }
    else if (m11 > m22) { float s = sqrtf(1 + m11 - m00 - m22) * 2; q = Q((y.x + x.y) / s, 0.25f * s, (z.y + y.z) / s, (z.x - x.z) / s); }
    else { float s = sqrtf(1 + m22 - m00 - m11) * 2; q = Q((z.x + x.z) / s, (z.y + y.z) / s, 0.25f * s, (x.y - y.x) / s); }
    return qNorm(q);
}

bool Game::recording() const { return gRecorder.active(); }

void Game::updateCamera(const Input &in, float dt) {
    // botão VOLTAR: pega / guarda a câmera
    if (in.backDown) {
        if (camState == 1) {
            camState = 0;
            if (gRecorder.active()) { gRecorder.stop(); gAudio.stopMic(); gAudio.recordSink = nullptr; gAudio.play("rec_stop", 0.7f); }
        } else camState = 1;
        gAudio.play("ui_click", 0.5f);
    }
    if (camState == 1) {
        // deslizar no touchpad: estica / recolhe o bastão
        if (in.padTouch && !in.padClick) {
            if (padWasTouch) stickLen = clampf(stickLen + (in.padY - lastPadY) * 1.2f, 0.12f, 2.6f);
            lastPadY = in.padY;
        }
        padWasTouch = in.padTouch;
        if (in.padClickDown && fabsf(in.padY) > 0.45f && fabsf(in.padY) > fabsf(in.padX)) {
            if (in.padY > 0) { selfie = !selfie; hud(selfie ? "Câmera: SELFIE" : "Câmera: MUNDO", V3(0.8f, 0.9f, 1), 1.5f); }
            else { camState = 2; hud("Câmera parada no ar (VOLTAR pega de novo)", V3(0.8f, 0.9f, 1), 2.5f); }
            gAudio.play("ui_click", 0.5f);
        }
        V3 base = handVis[ctrlHand];
        camPos = base + ctrlDirW * stickLen;
        V3 look = selfie ? norm(headPosW - camPos) : ctrlDirW;
        camRot = qLook(look, V3(0, 1, 0));
    } else if (camState == 2) {
        if (in.backDown) camState = 1;
    }
    // gatilho: grava/para (se não estiver mirando em um botão)
    if (in.triggerDown && camState != 0 && hoverBtn < 0) {
        if (!gRecorder.active()) {
            bool ok = gRecorder.start(videoDir) || gRecorder.start(videoDir2);
            if (ok) {
                gAudio.startMic();
                gAudio.recordSink = [](const short *s, int n) { gRecorder.pushAudio(s, n); };
                gAudio.play("rec_start", 0.7f);
                hud(gAudio.micOk ? "GRAVANDO (com microfone)" : "GRAVANDO (sem microfone)", V3(1, 0.3f, 0.3f), 2);
                award(10);
            } else { gAudio.play("ui_error"); hud("Erro ao gravar: " + gRecorder.error, V3(1, 0.4f, 0.4f), 4); }
        } else {
            gAudio.recordSink = nullptr;
            gRecorder.stop();
            gAudio.stopMic();
            gAudio.play("rec_stop", 0.7f);
            hud("Vídeo salvo: " + gRecorder.lastSaved, V3(0.5f, 1, 0.6f), 5);
            if (scanFile) scanFile(gRecorder.lastSaved);
        }
    }
}

void Game::onCameraFrame(const unsigned char *rgba, double t) {
    if (gRecorder.active()) gRecorder.pushVideo(rgba, t - gRecorder.startTime);
}

// ------------------------------------------------------------------ RODADA (host)
void Game::startRound(unsigned seed) {
    applyRound(seed, 0, 0, 1);
    for (int i = 0; i < 3; i++) {
        mons[i].pos = V3(world.mspawn[i].x, 0, world.mspawn[i].z);
        mons[i].active = i == 0;
        mons[i].state = 1; mons[i].path.clear(); mons[i].target = -1;
    }
    sendEv("R " + std::to_string(roundSeed) + " 0 0 1");
}

void Game::applyRound(unsigned seed, unsigned mask, int gate, int phase) {
    if (seed != roundSeed || roundBananas.empty()) {
        roundSeed = seed;
        Rng r(seed | 1);
        std::vector<int> idx;
        for (size_t i = 0; i < world.bananas.size(); i++) idx.push_back((int)i);
        for (size_t i = idx.size(); i > 1; i--) std::swap(idx[i - 1], idx[r.next() % i]);
        roundBananas.assign(idx.begin(), idx.begin() + std::min<size_t>(8, idx.size()));
    }
    unsigned newBits = mask & ~collected;
    if (newBits && pstate == PS_GAME) {
        for (int i = 0; i < 8; i++) if (newBits & (1u << i)) gAudio.play3D("banana", world.bananas[roundBananas[i]], 0.8f, 4);
    }
    collected = mask;
    if (gate && !gateOpen) {
        gAudio.play3D("gate_open", world.gateC, 1.0f, 12);
        hud("A PORTA DE AÇO ABRIU! CORRA PARA A SAÍDA!", V3(0.4f, 1, 0.4f), 5);
        if (pstate == PS_GAME) award(2);
    }
    gateOpen = gate != 0;
    world.setEnabled("gate", !gateOpen);
    roundPhase = phase;
    if (phase == 0) { roundBananas.clear(); collected = 0; gateOpen = false; world.setEnabled("gate", true); }
}

void Game::pickBanana(int slot, int byId) {
    if (slot < 0 || slot >= (int)roundBananas.size() || (collected & (1u << slot))) return;
    unsigned mask = collected | (1u << slot);
    int n = __builtin_popcount(mask);
    if (byId == myId()) award(1);
    bool gate = n >= (int)roundBananas.size();
    applyRound(roundSeed, mask, gate, roundPhase);
    gAudio.play("banana", 0.9f);
    hud("Bananas: " + std::to_string(n) + "/" + std::to_string(roundBananas.size()), V3(1, 0.9f, 0.2f), 2.5f);
    // escalada: monstros despertam com o progresso
    if (n >= 2 && !mons[M_PALIDO].active) { mons[M_PALIDO].active = true; mons[M_PALIDO].state = 1; hud("Algo pálido acordou...", V3(0.8f, 0.8f, 1), 3); }
    if (n >= 5 && !mons[M_MAMAE].active) { mons[M_MAMAE].active = true; mons[M_MAMAE].state = 1; hud("A MAMÃE ESTÁ VINDO.", V3(1, 0.3f, 0.2f), 3); gAudio.play3D("mamae_roar", mons[2].pos, 1, 20); }
    sendEv("R " + std::to_string(roundSeed) + " " + std::to_string(collected) + " " + std::to_string(gateOpen ? 1 : 0) + " " + std::to_string(roundPhase));
}

void Game::hostUpdate(float dt) {
    // há alguém no depósito?
    bool anyIn = pstate == PS_GAME;
    for (auto &r : remotes) if (r.state == 'F') anyIn = true;
    if (roundPhase == 1 && !anyIn) {
        applyRound(roundSeed, 0, 0, 0);
        for (auto &m : mons) m.active = false;
        sendEv("R 0 0 0 0");
    }
    if (roundPhase == 1) for (auto &m : mons) if (m.active) monsterAI(m, dt);
    roundSendT -= dt;
    if (roundSendT <= 0 && net.state() == NET_ONLINE) {
        roundSendT = 1.0f;
        sendEv("R " + std::to_string(roundSeed) + " " + std::to_string(collected) + " " + std::to_string(gateOpen ? 1 : 0) + " " + std::to_string(roundPhase));
    }
}

void Game::catchPlayer(int mi, int victimId) {
    Monster &m = mons[mi];
    m.state = 6; m.timer = 3.0f; m.target = -1;
    if (victimId == myId()) {
        if (pstate != PS_GAME) return;
        pstate = PS_CAUGHT; jsT = 0; jsType = m.type;
        static const char *js[] = {"js_babuun", "js_palido", "js_mamae"};
        gAudio.play(js[m.type], 1.0f);
        prof.catches++;
        award(5 + m.type);
        static const char *nm[] = {"PELO BABUUN", "PELO PÁLIDO", "PELA MAMÃE"};
        hud(std::string("VOCÊ FOI PEGO ") + nm[m.type] + "!", V3(1, 0.2f, 0.2f), 4);
        sendEv("DEAD");
    }
    sendEv("CAUGHT " + std::to_string(victimId) + " " + std::to_string(mi));
}

// ------------------------------------------------------------------ REDE
Remote *Game::remote(int id, bool create) {
    for (auto &r : remotes) if (r.id == id) return &r;
    if (!create) return nullptr;
    remotes.push_back(Remote());
    remotes.back().id = id;
    return &remotes.back();
}

void Game::hearNoise(float x, float z, float loud) {
    for (auto &m : mons) {
        if (!m.active) continue;
        float hear = m.type == M_MAMAE ? 32.f : (m.type == M_BABUUN ? 15.f : 0.f);
        if (len(V3(x, 0, z) - V3(m.pos.x, 0, m.pos.z)) < hear * loud && (m.state == 1 || m.state == 3)) {
            m.state = 3; m.lastSeen = V3(x, 0, z); m.path.clear();
        }
    }
}

void Game::makeNoise(const V3 &p, float loud) {
    if (pstate != PS_GAME) return;
    if (isHost()) hearNoise(p.x, p.z, loud);
    else sendEv("NOISE " + std::to_string(p.x) + " " + std::to_string(p.z) + " " + std::to_string(loud));
}

void Game::sendEv(const std::string &s) { if (net.state() == NET_ONLINE) net.send("EV " + s); }

void Game::netUpdate(float dt) {
    std::vector<std::string> lines;
    net.poll(lines);
    for (auto &l : lines) handleLine(l);
    if (net.state() != NET_ONLINE) { remotes.clear(); return; }
    sendT -= dt;
    if (sendT <= 0) {
        sendT = 1 / 15.f;
        char b[512];
        char st = pstate == PS_GAME ? 'F' : (pstate == PS_CAUGHT ? 'D' : 'L');
        snprintf(b, sizeof b, "ST %.2f %.2f %.2f %.3f %.3f %.3f %.3f %.2f %.2f %.2f %.2f %.2f %.2f %.3f %d %d %d %d %d %d %d %d %c",
                 body.x, body.y, body.z, headWorld.x, headWorld.y, headWorld.z, headWorld.w, handVis[0].x, handVis[0].y, handVis[0].z,
                 handVis[1].x, handVis[1].y, handVis[1].z, bodyYaw, prof.r, prof.g, prof.b, prof.hat, prof.face, prof.neck, prof.acc, prof.title, st);
        net.send(b);
    }
    if (isHost()) {
        monSendT -= dt;
        if (monSendT <= 0) {
            monSendT = 0.1f;
            for (int i = 0; i < 3; i++) {
                Monster &m = mons[i];
                char b[200];
                snprintf(b, sizeof b, "M %d %d %.2f %.2f %.2f %.3f %d %.2f", i, m.active ? 1 : 0, m.pos.x, m.pos.y, m.pos.z, m.yaw, m.state, m.jaw);
                sendEv(b);
            }
        }
    } else {
        // interpola monstros recebidos
        for (auto &m : mons) {
            float k = 1 - expf(-dt * 10);
            V3 old = m.pos;
            m.pos = lerp(m.pos, m.netPos, k);
            m.yaw += wrapAngle(m.netYaw - m.yaw) * k;
            m.speed = len(m.pos - old) / dt;
            m.anim += dt * (1.5f + m.speed * 1.4f);
        }
    }
    for (auto &r : remotes) {
        float k = 1 - expf(-dt * 12);
        r.body = lerp(r.body, r.tBody, k); r.lh = lerp(r.lh, r.tLh, k); r.rh = lerp(r.rh, r.tRh, k);
        r.headRot = qSlerp(r.headRot, r.tHeadRot, k);
        r.yaw += wrapAngle(r.tYaw - r.yaw) * k;
    }
    if (!remotes.empty()) award(8);
}

void Game::handleLine(const std::string &line) {
    std::istringstream in(line);
    std::string cmd; in >> cmd;
    if (cmd == "WELCOME") { in >> net.myId >> net.hostId; hud("Conectado! Sala " + std::string(ROOMS[prof.room % NROOMS]), V3(0.5f, 1, 0.6f)); }
    else if (cmd == "HOST") { in >> net.hostId; }
    else if (cmd == "JOIN") { int id; std::string n; in >> id >> n; Remote *r = remote(id, true); r->name = n; hud(n + " entrou", V3(0.7f, 0.9f, 1), 2.5f); }
    else if (cmd == "LEAVE") {
        int id; in >> id;
        for (size_t i = 0; i < remotes.size(); i++) if (remotes[i].id == id) { hud(remotes[i].name + " saiu", V3(0.7f, 0.7f, 0.7f), 2); remotes.erase(remotes.begin() + i); break; }
    } else if (cmd == "ST") {
        int id; in >> id;
        Remote *r = remote(id, true);
        float yaw;
        in >> r->tBody.x >> r->tBody.y >> r->tBody.z >> r->tHeadRot.x >> r->tHeadRot.y >> r->tHeadRot.z >> r->tHeadRot.w >> r->tLh.x >> r->tLh.y >>
            r->tLh.z >> r->tRh.x >> r->tRh.y >> r->tRh.z >> yaw >> r->r >> r->g >> r->b >> r->hat >> r->face >> r->neck >> r->acc >> r->title >> r->state;
        r->tYaw = yaw;
        if (r->last == 0) { r->body = r->tBody; r->lh = r->tLh; r->rh = r->tRh; r->headRot = r->tHeadRot; r->yaw = yaw; }
        r->last = now;
    } else if (cmd == "EV") {
        int from; std::string ev; in >> from >> ev;
        if (ev == "M" && !isHost()) {
            int i, act, st; float x, y, z, yaw, jaw;
            in >> i >> act >> x >> y >> z >> yaw >> st >> jaw;
            if (i >= 0 && i < 3) {
                Monster &m = mons[i];
                if (!m.active && act) m.pos = V3(x, y, z);
                m.active = act; m.netPos = V3(x, y, z); m.netYaw = yaw; m.state = st; m.jaw = jaw;
            }
        } else if (ev == "R" && !isHost()) {
            unsigned seed, mask; int gate, phase; in >> seed >> mask >> gate >> phase;
            applyRound(seed, mask, gate, phase);
        } else if (ev == "PICK" && isHost()) {
            int s; in >> s; pickBanana(s, from);
        } else if (ev == "NOISE" && isHost()) {
            float x, z, loud; in >> x >> z >> loud;
            hearNoise(x, z, loud);
        } else if (ev == "CAUGHT") {
            int victim, mi; in >> victim >> mi;
            if (victim == myId() && pstate == PS_GAME && mi >= 0 && mi < 3) {
                pstate = PS_CAUGHT; jsT = 0; jsType = mi;
                static const char *js[] = {"js_babuun", "js_palido", "js_mamae"};
                gAudio.play(js[mi], 1.0f);
                prof.catches++;
                award(5 + mi);
                hud("VOCÊ FOI PEGO!", V3(1, 0.2f, 0.2f), 4);
            } else if (mi >= 0 && mi < 3) {
                static const char *sc[] = {"babuun_screech", "palido_shriek", "mamae_roar"};
                gAudio.play3D(sc[mi], mons[mi].pos, 0.9f, 8);
                Remote *r = remote(victim);
                if (r) hud(r->name + " foi pego!", V3(1, 0.5f, 0.4f), 2.5f);
            }
        } else if (ev == "HOOT") {
            Remote *r = remote(from);
            if (r) gAudio.play3D("hoot", r->body + V3(0, 0.3f, 0), 1.0f, 4);
        } else if (ev == "ESCAPED") {
            Remote *r = remote(from);
            if (r) hud(r->name + " escapou!", V3(0.5f, 1, 0.5f), 2.5f);
        }
    } else if (cmd == "ERR") {
        std::string rest; std::getline(in, rest);
        hud("Servidor: " + rest, V3(1, 0.5f, 0.4f), 4);
    }
}

// ------------------------------------------------------------------ ÁUDIO AMBIENTE
void Game::updateAudio(float dt) {
    gAudio.master = prof.volume / 10.f;
    bool inGame = pstate == PS_GAME || pstate == PS_CAUGHT;
    static int ambMode = -1;
    int mode = inGame ? 1 : 0;
    if (mode != ambMode) {
        if (ambVoice) gAudio.stop(ambVoice);
        ambVoice = gAudio.play(gAudio.find(inGame ? "amb_night" : "amb_lobby"), inGame ? 0.45f : 0.35f, true);
        ambMode = mode;
    }
    // perigo: perseguição + batimentos
    float danger = 0;
    if (pstate == PS_GAME)
        for (auto &m : mons) {
            if (!m.active) continue;
            float d = len(m.pos - body);
            bool chasing = m.state == 2 || m.state == 4 || (m.type == M_PALIDO && m.state != 5 && d < 15);
            danger = fmaxf(danger, chasing ? clampf(1.3f - d / 25, 0, 1) : clampf(0.6f - d / 30, 0, 0.6f));
        }
    chaseLevel = lerpf(chaseLevel, danger, 1 - expf(-dt * 2));
    if (chaseLevel > 0.05f && !chaseVoice) chaseVoice = gAudio.play(gAudio.find("chase"), 0, true);
    if (chaseVoice) {
        gAudio.setGain(chaseVoice, clampf(chaseLevel - 0.3f, 0, 1) * 0.8f);
        if (chaseLevel < 0.03f) { gAudio.stop(chaseVoice); chaseVoice = 0; }
    }
    if (chaseLevel > 0.25f && !heartVoice) heartVoice = gAudio.play(gAudio.find("heartbeat"), 0, true, 1.0f);
    if (heartVoice) {
        gAudio.setGain(heartVoice, clampf(chaseLevel, 0, 1) * 0.7f);
        if (chaseLevel < 0.2f) { gAudio.stop(heartVoice); heartVoice = 0; }
    }
    // vozes dos monstros
    for (int i = 0; i < 3; i++) {
        Monster &m = mons[i];
        m.voiceT -= dt;
        if (!m.active || !inGame) continue;
        if (m.voiceT <= 0) {
            static const char *idle[] = {"babuun_growl", "palido_breath", "mamae_roar"};
            static const char *hunt[] = {"babuun_bark", "palido_breath", "mamae_roar"};
            bool h = m.state == 2;
            gAudio.play3D(h ? hunt[i] : idle[i], m.pos + V3(0, 1, 0), i == 2 ? 1.0f : 0.8f, i == 2 ? 6.f : 3.f);
            m.voiceT = h ? 2.0f + i : 5.0f + (float)(rand() % 40) / 10.f;
        }
        // passos
        float ph = m.anim;
        static float lastPh[3] = {0, 0, 0};
        if (floorf(ph / PI) != floorf(lastPh[i] / PI) && m.speed > 0.5f)
            gAudio.play3D(i == 2 ? "step_heavy" : "step_light", m.pos, i == 2 ? 0.9f : 0.5f, i == 2 ? 5.f : 2.5f);
        lastPh[i] = ph;
    }
}

Scene Game::scene() const {
    Scene s;
    bool inLobby = body.x < -150;
    if (inLobby) {
        s.skyAmb = V3(0.55f, 0.56f, 0.6f); s.groundAmb = V3(0.3f, 0.3f, 0.3f);
        s.sunCol = V3(0.15f, 0.15f, 0.15f); s.fogCol = V3(0.2f, 0.2f, 0.22f); s.fogDens = 0.02f;
    } else {
        s.skyAmb = V3(0.26f, 0.27f, 0.3f); s.groundAmb = V3(0.13f, 0.13f, 0.14f);
        s.sunCol = V3(0.06f, 0.06f, 0.07f); s.fogCol = V3(0.05f, 0.05f, 0.06f); s.fogDens = 0.055f;
    }
    s.clear = s.fogCol;
    // 4 luzes fluorescentes mais próximas (algumas piscam)
    std::vector<std::pair<float, int>> d;
    for (size_t i = 0; i < world.lights.size(); i++) d.push_back({len2(world.lights[i] - body), (int)i});
    std::partial_sort(d.begin(), d.begin() + std::min<size_t>(4, d.size()), d.end());
    s.nLights = 0;
    for (size_t k = 0; k < d.size() && k < 4; k++) {
        int i = d[k].second;
        float fl = 1;
        if (!inLobby && i % 7 == 3) { float t = gfx->sc.time * 13 + i; fl = (sinf(t) * sinf(t * 2.3f) > 0.3f) ? 0.15f : 1.f; }
        s.lights[s.nLights++] = {world.lights[i] + V3(0, -0.3f, 0), 9.f, V3(0.75f, 0.78f, 0.82f) * fl};
    }
    s.time = gfx->sc.time;
    return s;
}

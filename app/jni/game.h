// Babuun Hollow — lógica do jogo
#pragma once
#include <string>
#include <vector>
#include <functional>
#include "mathx.h"
#include "gfx.h"
#include "world.h"
#include "net.h"
#include "lanserver.h"

// entrada vinda do VrApi (espaço de rastreamento)
struct Input {
    bool hasCtrl = false, leftHanded = false;
    Q ctrl;                    // orientação do controle
    Q head; V3 headPos;        // cabeça
    bool trigger = false, triggerDown = false;
    bool padClick = false, padClickDown = false, padTouch = false;
    float padX = 0, padY = 0;  // -1..1 (y+ = cima)
    bool backDown = false;
    float dt = 1 / 72.f;
    double time = 0;
};

struct Profile {
    std::string name = "Macaco";
    int r = 7, g = 4, b = 2;          // 0..9
    int hat = 0, face = 0, neck = 0, acc = 0;
    int title = -1;                   // badge exibida como título
    unsigned badges = 0;
    int escapes = 0, catches = 0;
    int handMode = 0;                 // 0 = espelho (juntas), 1 = alternada (oposta)
    int boost = 1;                    // 0 normal, 1 forte, 2 turbo
    int snap = 1;                     // 0 = 30°, 1 = 45°
    int armAngle = 25;                // inclinação do braço (graus)
    int volume = 8;                   // 0..10
    int room = 0;
    bool online = true;
    V3 color() const { return V3(0.08f + r / 9.f * 0.92f, 0.08f + g / 9.f * 0.92f, 0.08f + b / 9.f * 0.92f); }
};

enum PState { PS_LOBBY = 0, PS_GAME, PS_CAUGHT, PS_ESCAPED };
enum MType { M_BABUUN = 0, M_PALIDO, M_MAMAE };

struct Monster {
    int type = 0; bool active = false;
    V3 pos; float yaw = 0;
    int state = 0;            // 0 parado, 1 patrulha, 2 caça, 3 busca, 4 salto, 5 congelado, 6 descanso
    int target = -1;          // id do jogador alvo
    float timer = 0, anim = 0, speed = 0, jaw = 0, voiceT = 0, stuckT = 0, pathT = 0;
    V3 goal, lastSeen, leapFrom, leapTo, lastPos;
    float leapT = -1;
    std::vector<V3> path;
    // rede (interpolação)
    V3 netPos; float netYaw = 0;
};

struct Remote {
    int id = -1; std::string name;
    V3 body, head, lh, rh; Q headRot; float yaw = 0;
    V3 tBody, tHead, tLh, tRh; Q tHeadRot; float tYaw = 0;
    int r = 7, g = 4, b = 2, hat = 0, face = 0, neck = 0, acc = 0, title = -1;
    char state = 'L';
    double last = 0;
};

struct Button {
    V3 pos, nrm; float r = 0.07f;
    std::string label;
    std::function<void()> fn;
    V3 color = V3(0.9f, 0.9f, 0.9f);
    float cool = 0, flash = 0;
    bool visible = true;
};

struct Badge { const char *name; const char *desc; };

class Game {
    friend struct GameTest;
public:
    bool init(Gfx *g, const std::string &saveDir, const std::string &videoDir, const std::string &videoDir2);
    void update(const Input &in);
    // matriz do "rig" (origem do espaço de rastreamento no mundo)
    M4 rigMatrix() const;
    void render(const DrawCtx &ctx, bool forCamera);
    // câmera segurável
    bool cameraActive() const { return camState != 0; }
    M4 cameraWorld() const { return mTRS(camPos, camRot); }
    GLuint camTex = 0;   // textura da prévia (definida em main)
    bool camWantsFrame() const { return camState != 0; }
    void onCameraFrame(const unsigned char *rgba, double t);  // chamado quando há pixels lidos
    bool recording() const;
    void saveProfile();
    void onPause();
    void onResume();
    void onDestroy();
    Scene scene() const;
    std::function<void(const std::string &)> scanFile;
    std::function<void()> askPermissions;

private:
    Gfx *gfx = nullptr;
    World world;
    Net net;
    // ---- servidores locais (no próprio Go, sem nuvem)
    enum NetMode { NM_OFF = 0, NM_CLOUD, NM_LAN };
    int netMode = NM_OFF;
    LanServer lan;            // servidor rodando NESTE óculos
    LanDiscovery disc;        // lista de servidores na rede Wi-Fi
    RoomCfg createCfg;        // ajustes para criar servidor
    RoomCfg curCfg;           // ajustes do servidor atual (recebidos em INFO)
    int nameIdx = 0, lanBtn0 = 0;
    std::string codeEntry;    // teclado numérico
    std::string curIp;        // IP do servidor atual
    std::vector<std::pair<int, std::string>> succ;  // ordem de sucessão (id, ip)
    bool wasOnline = false;
    int migState = 0, migCand = 0, migTries = 0; double migT = 0; std::string migIp; int migId = -1;
    bool codeSearch = false; double codeSearchT = 0;
    std::string myIp;
    void hostLan();
    void joinLan(const std::string &ip, const std::string &code);
    void leaveServer(bool quiet = false);
    void beginMigration(int candId, const std::string &candIp);
    void updateLan(float dt);
    const RoomCfg &cfgNow() const { return netMode == NM_LAN ? curCfg : createCfg; }
    float diffMul() const { static const float m[] = {0.8f, 1.0f, 1.25f}; return m[cfgNow().diff % 3]; }
    Profile prof;
    std::string saveDir, videoDir, videoDir2;
    double now = 0;

    // ---- jogador local
    PState pstate = PS_LOBBY;
    V3 body, vel;
    float rigYaw = 0, bodyYaw = 0;
    Q headWorld; V3 headPosW;
    V3 shoulderW[2], handTarget[2], handVis[2], anchor[2], prevTarget[2];
    bool touching[2] = {false, false};
    V3 hist[8]; int histN = 0, histI = 0;
    V3 ctrlDirW; int ctrlHand = 1;
    float lift = 0, altBlend = 0;
    bool wasTouching = false;
    float speedNow = 0, aliveT = 0;
    bool snapLatch = false;

    // ---- câmera
    int camState = 0;         // 0 guardada, 1 na mão, 2 parada no ar
    float stickLen = 0.6f; bool selfie = true;
    V3 camPos; Q camRot;
    float lastPadY = 0; bool padWasTouch = false;
    double recStart = 0;

    // ---- rodada (host)
    bool isHost() const;
    int myId() const;
    unsigned roundSeed = 0;
    std::vector<int> roundBananas;   // índices em world.bananas
    unsigned collected = 0;          // máscara
    bool gateOpen = false; float gateAnim = 0;
    int roundPhase = 0;              // 0 ocioso, 1 rodando
    void startRound(unsigned seed);
    void hostUpdate(float dt);
    void pickBanana(int slot, int byId);
    void applyRound(unsigned seed, unsigned mask, int gate, int phase);

    // ---- monstros
    Monster mons[3];
    void monsterAI(Monster &m, float dt);
    bool findPath(const V3 &from, const V3 &to, std::vector<V3> &out);
    void moveMonster(Monster &m, const V3 &goal, float speed, float dt);
    std::vector<std::string> grid; int gCols = 0, gRows = 0; float gCell = 3, gOx = 0, gOz = 0;
    bool walkable(int c, int r) const;
    void catchPlayer(int monster, int victimId);

    // ---- jumpscare
    float jsT = -1; int jsType = 0;

    // ---- remotos
    std::vector<Remote> remotes;
    Remote *remote(int id, bool create = false);
    void netUpdate(float dt);
    float sendT = 0, monSendT = 0, roundSendT = 0;
    void handleLine(const std::string &line);
    void sendEv(const std::string &s);
    void hearNoise(float x, float z, float loud);
    void makeNoise(const V3 &p, float loud);

    // ---- lobby / UI
    std::vector<Button> buttons;
    void buildLobby();
    void updateButtons(const Input &in);
    int hoverBtn = -1;
    V3 laserFrom, laserTo; bool laserOn = false;
    void drawBoards();
    void drawAvatar(const V3 &body, float yaw, const Q &headRot, const V3 &lh, const V3 &rh, const Profile &p, bool drawHead, bool drawHands, float alpha = 1);
    void teleport(const V3 &p, float yaw);
    void enterGame();
    void toLobby();

    // ---- badges / HUD
    void award(int b);
    struct Msg { std::string t; float time; V3 col; };
    std::vector<Msg> msgs;
    void hud(const std::string &t, V3 col = V3(1, 1, 1), float time = 3.5f);
    void drawHud();
    float hudYaw = 0;

    // ---- áudio
    int ambVoice = 0, chaseVoice = 0, monVoice[3] = {0, 0, 0}, heartVoice = 0;
    float chaseLevel = 0;
    void updateAudio(float dt);
    float fade = 0;  // 0..1 preto

    // ---- modelos
    Model *mMap, *mMonkey, *mHand, *mArm, *mBanana, *mBabuun, *mPalido, *mMamae, *mGate, *mCamera, *mStick, *mGrip, *mButton, *mBoard;
    std::vector<Model *> mHats, mFaces, mNecks;
    Pose pMonkey, pMon[3];
    std::vector<int> mapLights;
    void computeWorlds(const Model *m, const M4 &root, const Pose *pose, std::vector<M4> &out) const;
    void handleInput(const Input &in);
    void locomotion(const Input &in, float dt);
    void updateCamera(const Input &in, float dt);
};

extern const Badge BADGES[];
extern const int NUM_BADGES;

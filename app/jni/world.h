// Mundo de colisão: caixas com yaw, cilindros verticais e chão (y=0)
#pragma once
#include <vector>
#include <string>
#include "mathx.h"

enum SurfMat { SM_GROUND = 0, SM_WOOD, SM_STONE, SM_METAL, SM_LEAF };

struct Collider {
    int type;          // 0 = caixa, 1 = cilindro
    V3 c;              // centro (caixa) / (x, ?, z) cilindro
    V3 h;              // meia-extensão (caixa)
    float yaw = 0, cs = 1, sn = 0;
    float r = 0, y0 = 0, y1 = 0;  // cilindro
    int mat = SM_WOOD;
    std::string kind, id;
    bool enabled = true;
    V3 bmin, bmax;     // AABB para broadphase
};

struct Hit { bool hit = false; float t = 1e9f; V3 p, n; int mat = SM_GROUND; int idx = -1; };

struct StationPt { V3 c; float yaw; };

class World {
public:
    std::vector<Collider> cols;
    // pontos especiais
    V3 forestMin, forestMax, spawnForest, spawnLobby, portalC; float portalR = 1.5f;
    V3 gateC, gateExit; float gateYaw = 0;
    std::vector<V3> lights;
    std::vector<std::string> grid; int gCols = 0, gRows = 0; float gCell = 3, gOx = 0, gOz = 0;
    std::vector<V3> bananas, patrol;
    V3 mspawn[3];
    StationPt st[8];  // 0 cor,1 roupa,2 badges,3 rede,4 como jogar,5 ajustes,6 câmera
    bool load(const char *text);
    // raio de a até b (segmento); retorna primeiro impacto
    Hit raycast(const V3 &a, const V3 &b, float radius = 0) const;
    // empurra esfera para fora da geometria; retorna normal média do contato (len 0 se nenhum)
    V3 resolveSphere(V3 &p, float r, int *matOut = nullptr) const;
    bool lineOfSight(const V3 &a, const V3 &b) const { return !raycast(a, b).hit; }
    void setEnabled(const char *id, bool en);
    bool inForest(const V3 &p) const { return p.x > forestMin.x - 5 && p.x < forestMax.x + 5 && p.z > forestMin.z - 5 && p.z < forestMax.z + 5; }
private:
    bool closestPoint(const Collider &c, const V3 &p, V3 &cp, V3 &n, float &depthInside) const;
};

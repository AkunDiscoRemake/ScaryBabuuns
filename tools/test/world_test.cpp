// Teste de PC: carrega map.txt e valida colisão, raycast e conectividade da grade.
#include "../../app/jni/world.h"
#include <cstdio>
#include <fstream>
#include <sstream>
#include <queue>
int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { printf("FALHOU: " __VA_ARGS__); printf("\n"); fails++; } } while (0)
int main() {
    std::ifstream f("app/assets/map.txt"); std::stringstream ss; ss << f.rdbuf();
    World w; CHECK(w.load(ss.str().c_str()), "load");
    printf("colisores=%zu luzes=%zu bananas=%zu grade=%dx%d\n", w.cols.size(), w.lights.size(), w.bananas.size(), w.gCols, w.gRows);
    // spawns livres
    for (V3 p : {w.spawnForest, w.spawnLobby}) { V3 q = p; w.resolveSphere(q, 0.28f); CHECK(len(q - p) < 0.05f, "spawn preso em (%.1f,%.1f,%.1f)", p.x, p.y, p.z); }
    // raycast para o chão
    Hit h = w.raycast(w.spawnForest, w.spawnForest + V3(0, -2, 0)); CHECK(h.hit && fabsf(h.p.y) < 0.01f, "chão");
    // parede: raio horizontal a partir do spawn deve bater em algo dentro de 40m
    h = w.raycast(w.spawnForest + V3(0, 1, 0), w.spawnForest + V3(0, 1, -40)); CHECK(h.hit, "parede");
    // esfera empurrada para fora de uma prateleira
    for (auto &c : w.cols) if (c.kind == "shelf" && c.c.x > -100) { V3 p = c.c; w.resolveSphere(p, 0.3f); CHECK(len(p - c.c) > 0.3f, "prateleira"); break; }
    // portão bloqueia e depois libera
    V3 g = w.gateC; h = w.raycast(g + V3(-2, 0, 0), g + V3(3, 0, 0)); CHECK(h.hit, "portão fechado bloqueia");
    w.setEnabled("gate", false); h = w.raycast(g + V3(-1, 0, 0), g + V3(1.5f, 0, 0)); CHECK(!h.hit || h.t > 1.5f, "portão aberto passa");
    // todas as células de chão conectadas (monstros alcançam todo o mapa)
    auto walk = [&](int c, int r) { if (r < 0 || r >= w.gRows || c < 0 || c >= w.gCols) return false; char ch = w.grid[r][c]; return ch == '.' || ch == 'P'; };
    int total = 0, sc = -1, sr = -1;
    for (int r = 0; r < w.gRows; r++) for (int c = 0; c < w.gCols; c++) if (walk(c, r)) { total++; if (sc < 0) { sc = c; sr = r; } }
    std::vector<char> seen(w.gCols * w.gRows, 0); std::queue<int> q; q.push(sr * w.gCols + sc); seen[sr * w.gCols + sc] = 1; int n = 0;
    while (!q.empty()) { int cur = q.front(); q.pop(); n++; int c = cur % w.gCols, r = cur / w.gCols; int dc[] = {1, -1, 0, 0}, dr[] = {0, 0, 1, -1};
        for (int k = 0; k < 4; k++) { int nc = c + dc[k], nr = r + dr[k]; if (walk(nc, nr) && !seen[nr * w.gCols + nc]) { seen[nr * w.gCols + nc] = 1; q.push(nr * w.gCols + nc); } } }
    CHECK(n == total, "grade desconectada: %d de %d células", n, total);
    // células livres da grade não podem ter colisor no meio (altura 1m)
    int blocked = 0;
    for (int r = 0; r < w.gRows; r++) for (int c = 0; c < w.gCols; c++) if (walk(c, r)) {
        V3 p(w.gOx + (c + .5f) * w.gCell, 1.0f, w.gOz + (r + .5f) * w.gCell), q2 = p; w.resolveSphere(q2, 0.4f); if (len(q2 - p) > 0.3f) blocked++; }
    CHECK(blocked == 0, "%d células livres bloqueadas", blocked);
    // bananas alcançáveis (não enterradas)
    int bad = 0; for (auto b : w.bananas) { V3 p = b; w.resolveSphere(p, 0.1f); if (len(p - b) > 0.25f) bad++; }
    CHECK(bad == 0, "%d bananas dentro de geometria", bad);
    printf(fails ? "%d FALHAS\n" : "TUDO OK\n", fails);
    return fails != 0;
}

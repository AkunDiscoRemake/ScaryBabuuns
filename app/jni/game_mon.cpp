// Babuun Hollow — IA dos monstros (executada pelo host)
#include "game.h"
#include "audio.h"
#include <queue>
#include <algorithm>
#include <stdlib.h>

bool Game::walkable(int c, int r) const {
    if (r < 0 || r >= gRows || c < 0 || c >= gCols) return false;
    char ch = grid[r][c];
    return ch == '.' || ch == 'P';
}

bool Game::findPath(const V3 &from, const V3 &to, std::vector<V3> &out) {
    out.clear();
    if (grid.empty()) return false;
    auto cellOf = [&](const V3 &p, int &c, int &r) { c = (int)floorf((p.x - gOx) / gCell); r = (int)floorf((p.z - gOz) / gCell); };
    int sc, sr, tc, tr;
    cellOf(from, sc, sr); cellOf(to, tc, tr);
    // se o alvo estiver numa célula bloqueada (ex.: em cima de prateleira), usa vizinha livre mais próxima
    if (!walkable(tc, tr)) {
        float best = 1e9f; int bc = tc, br = tr;
        for (int dr = -2; dr <= 2; dr++) for (int dc = -2; dc <= 2; dc++)
            if (walkable(tc + dc, tr + dr)) {
                V3 cc(gOx + (tc + dc + 0.5f) * gCell, 0, gOz + (tr + dr + 0.5f) * gCell);
                float d = len(cc - V3(to.x, 0, to.z));
                if (d < best) { best = d; bc = tc + dc; br = tr + dr; }
            }
        tc = bc; tr = br;
    }
    if (!walkable(sc, sr)) {
        for (int dr = -1; dr <= 1; dr++) for (int dc = -1; dc <= 1; dc++) if (walkable(sc + dc, sr + dr)) { sc += dc; sr += dr; goto ok; }
        return false;
    }
ok:
    std::vector<int> prev(gCols * gRows, -2);
    std::queue<int> q;
    q.push(sr * gCols + sc); prev[sr * gCols + sc] = -1;
    int goal = tr * gCols + tc;
    while (!q.empty()) {
        int cur = q.front(); q.pop();
        if (cur == goal) break;
        int c = cur % gCols, r = cur / gCols;
        static const int dc[] = {1, -1, 0, 0}, dr[] = {0, 0, 1, -1};
        for (int k = 0; k < 4; k++) {
            int nc = c + dc[k], nr = r + dr[k];
            if (!walkable(nc, nr)) continue;
            int ni = nr * gCols + nc;
            if (prev[ni] != -2) continue;
            prev[ni] = cur; q.push(ni);
        }
    }
    if (prev[goal] == -2) return false;
    for (int c = goal; c >= 0; c = prev[c]) out.push_back(V3(gOx + (c % gCols + 0.5f) * gCell, 0, gOz + (c / gCols + 0.5f) * gCell));
    std::reverse(out.begin(), out.end());
    if (!out.empty()) out.back() = V3(to.x, 0, to.z);
    return true;
}

void Game::moveMonster(Monster &m, const V3 &goal, float speed, float dt) {
    // segue o caminho; recalcula de tempos em tempos
    m.pathT -= dt;
    if (m.path.empty() || m.pathT <= 0) { findPath(m.pos, goal, m.path); m.pathT = 0.6f; }
    V3 tgt = goal;
    while (!m.path.empty()) {
        V3 p = m.path.front();
        if (hlen(p - m.pos) < 0.8f && m.path.size() > 1) m.path.erase(m.path.begin());
        else { tgt = p; break; }
    }
    // atalho: se enxerga o alvo final, vai direto
    if (world.lineOfSight(m.pos + V3(0, 0.7f, 0), V3(goal.x, 0.7f, goal.z))) tgt = goal;
    V3 d = V3(tgt.x - m.pos.x, 0, tgt.z - m.pos.z);
    float dl = hlen(d);
    if (dl > 0.05f) {
        d = d / dl;
        float step = fminf(speed * dt, dl);
        V3 np = m.pos + d * step;
        float rad = m.type == M_MAMAE ? 0.8f : 0.4f;
        V3 probe(np.x, rad + 0.05f, np.z);
        world.resolveSphere(probe, rad);
        m.pos = V3(probe.x, m.pos.y, probe.z);
        float ty = atan2f(-d.x, -d.z);
        m.yaw += wrapAngle(ty - m.yaw) * fminf(1, dt * 8);
    }
    m.speed = speed;
    m.anim += dt * (1.5f + speed * 1.4f);
    // preso?
    m.stuckT += dt;
    if (m.stuckT > 2.5f) {
        if (hlen(m.pos - m.lastPos) < 0.4f && m.state == 1) { m.goal = V3(); m.path.clear(); }
        m.lastPos = m.pos; m.stuckT = 0;
    }
}

struct PlayerRef { int id; V3 body, head; Q headRot; };

void Game::monsterAI(Monster &m, float dt) {
    // lista de jogadores dentro do depósito
    PlayerRef ps[16]; int np = 0;
    if (pstate == PS_GAME) ps[np++] = {myId(), body, headPosW, headWorld};
    for (auto &r : remotes) if (r.state == 'F' && np < 16) ps[np++] = {r.id, r.body, r.body + V3(0, 0.3f, 0), r.headRot};
    float sight = m.type == M_MAMAE ? 13.f : 22.f;
    V3 eye = m.pos + V3(0, m.type == M_PALIDO ? 2.1f : (m.type == M_MAMAE ? 1.9f : 1.0f), 0);
    V3 fwd(-sinf(m.yaw), 0, -cosf(m.yaw));
    auto sees = [&](const PlayerRef &p) {
        V3 d = p.head - eye; float dl = len(d);
        if (dl > sight) return false;
        if (dl > 3 && dot(norm(V3(d.x, 0, d.z)), fwd) < 0.2f) return false;
        return world.lineOfSight(eye, p.head);
    };
    int nearest = -1; float nd = 1e9f;
    for (int i = 0; i < np; i++) { float d = len(ps[i].body - m.pos); if (d < nd) { nd = d; nearest = i; } }
    m.jaw = lerpf(m.jaw, m.state == 2 || m.state == 4 ? 0.8f : 0.15f, dt * 4);

    // pegou alguém?
    float reach = m.type == M_MAMAE ? 1.9f : (m.type == M_PALIDO ? 1.2f : 1.1f);
    float reachY = m.type == M_MAMAE ? 2.6f : (m.type == M_PALIDO ? 2.8f : 1.7f);
    if (m.state != 6) for (int i = 0; i < np; i++) {
        V3 d = ps[i].body - m.pos;
        if (hlen(d) < reach && d.y < reachY + m.pos.y && m.state != 5) { catchPlayer(m.type, ps[i].id); return; }
    }

    if (m.type == M_PALIDO) {
        // O Pálido só se move quando NINGUÉM está olhando para ele
        bool watched = false;
        for (int i = 0; i < np; i++) {
            V3 d = (m.pos + V3(0, 1.4f, 0)) - ps[i].head;
            float dl = len(d);
            if (dl > 35) continue;
            V3 look = ps[i].headRot.rot(V3(0, 0, -1));
            if (dot(look, d / dl) > 0.78f && world.lineOfSight(ps[i].head, m.pos + V3(0, 1.4f, 0))) { watched = true; break; }
        }
        if (m.state == 6) { m.timer -= dt; if (m.timer <= 0) m.state = 1; return; }
        if (watched) { m.state = 5; m.speed = 0; return; }
        if (nearest >= 0) {
            m.state = 2;
            moveMonster(m, ps[nearest].body, nd < 8 ? 7.5f : 5.0f, dt);
            // vira o rosto direto para a vítima (arrepiante)
            V3 d = ps[nearest].body - m.pos;
            m.yaw = atan2f(-d.x, -d.z);
        } else {
            m.state = 1;
            if ((m.goal.x == 0 && m.goal.z == 0) || hlen(m.goal - m.pos) < 1.5f) m.goal = world.patrol[rand() % world.patrol.size()];
            moveMonster(m, m.goal, 2.0f, dt);
        }
        return;
    }

    // salto (Babuun): parábola até o alvo
    if (m.state == 4) {
        m.leapT += dt / 0.65f;
        float t = clampf(m.leapT, 0, 1);
        V3 p = lerp(m.leapFrom, m.leapTo, t);
        p.y = m.leapFrom.y + (m.leapTo.y - m.leapFrom.y) * t + sinf(t * PI) * 1.6f;
        m.pos = p;
        m.anim += dt * 2;
        if (t >= 1) { m.state = 2; m.pos.y = 0; }
        return;
    }
    if (m.state == 6) {  // descansando depois de pegar alguém / cansada
        m.timer -= dt; m.speed = 0;
        if (m.timer <= 0) { m.state = 1; m.goal = V3(); }
        return;
    }
    // procura alvo visível
    int seen = -1;
    for (int i = 0; i < np; i++) if (sees(ps[i])) { seen = i; break; }
    if (seen >= 0) {
        if (m.state != 2) {
            gAudio.play3D(m.type == M_BABUUN ? "babuun_screech" : "mamae_roar", eye, 1.0f, 8);
            m.timer = m.type == M_MAMAE ? 4.5f : 0;
        }
        m.state = 2; m.target = ps[seen].id; m.lastSeen = ps[seen].body; m.stuckT = 0;
    }
    if (m.state == 2) {
        PlayerRef *tp = nullptr;
        for (int i = 0; i < np; i++) if (ps[i].id == m.target) tp = &ps[i];
        if (!tp) { m.state = 1; return; }
        if (seen < 0) {
            m.timer += dt;
            if (m.timer > 4.0f && m.type == M_BABUUN) { m.state = 3; m.timer = 0; }
        } else if (m.type == M_BABUUN) m.timer = 0;
        V3 d = tp->body - m.pos;
        if (m.type == M_BABUUN) {
            // salta em quem está em cima de prateleiras/caixas
            if (hlen(d) < 4.0f && d.y > 1.2f && d.y < 4.2f && world.lineOfSight(eye, tp->head)) {
                m.state = 4; m.leapT = 0; m.leapFrom = m.pos; m.leapTo = tp->body - V3(0, 0.3f, 0);
                gAudio.play3D("babuun_bark", eye, 1, 6);
                return;
            }
            moveMonster(m, m.lastSeen, 6.3f, dt);
        } else {  // Mamãe: investida e cansaço
            m.timer -= dt;
            moveMonster(m, m.lastSeen, 7.2f, dt);
            if (m.timer <= 0) { m.state = 6; m.timer = 2.5f; }
        }
        return;
    }
    if (m.state == 3) {  // busca no último ponto visto / barulho
        moveMonster(m, m.lastSeen, m.type == M_MAMAE ? 5.0f : 3.6f, dt);
        if (hlen(m.lastSeen - m.pos) < 1.2f) { m.state = 1; m.goal = V3(); }
        return;
    }
    // patrulha
    m.state = 1;
    if ((m.goal.x == 0 && m.goal.z == 0) || hlen(m.goal - m.pos) < 1.5f) m.goal = world.patrol[rand() % world.patrol.size()];
    moveMonster(m, m.goal, m.type == M_MAMAE ? 1.8f : 2.4f, dt);
}

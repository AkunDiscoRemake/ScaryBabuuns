#include "world.h"
#include <stdio.h>
#include <stdlib.h>
#include <sstream>

static int matFrom(const std::string &m, const std::string &kind) {
    if (m == "stone" || m == "rock" || m == "concrete" || m == "tile") return SM_STONE;
    if (m == "metal" || m == "steel") return SM_METAL;
    if (m == "toy") return SM_LEAF;
    if (kind == "tree" || kind == "bigtree" || kind == "branch" || kind == "log") return SM_WOOD;
    return SM_WOOD;
}

bool World::load(const char *text) {
    std::istringstream in(text);
    std::string line;
    int ns = 0;
    while (std::getline(in, line)) {
        std::istringstream ls(line);
        std::string tag; ls >> tag;
        if (tag == "B" || tag == "C") {
            Collider c;
            std::string mat;
            ls >> c.kind >> mat;
            if (tag == "B") {
                c.type = 0;
                float sx, sy, sz;
                ls >> c.c.x >> c.c.y >> c.c.z >> sx >> sy >> sz >> c.yaw >> c.id;
                c.h = V3(sx / 2, sy / 2, sz / 2);
                c.cs = cosf(c.yaw); c.sn = sinf(c.yaw);
                float ex = fabsf(c.cs) * c.h.x + fabsf(c.sn) * c.h.z, ez = fabsf(c.sn) * c.h.x + fabsf(c.cs) * c.h.z;
                c.bmin = V3(c.c.x - ex, c.c.y - c.h.y, c.c.z - ez); c.bmax = V3(c.c.x + ex, c.c.y + c.h.y, c.c.z + ez);
            } else {
                c.type = 1;
                ls >> c.c.x >> c.c.z >> c.r >> c.y0 >> c.y1 >> c.id;
                c.bmin = V3(c.c.x - c.r, c.y0, c.c.z - c.r); c.bmax = V3(c.c.x + c.r, c.y1, c.c.z + c.r);
            }
            if (c.id == "-") c.id.clear();
            c.mat = matFrom(mat, c.kind);
            if (c.kind == "tree" || c.kind == "bigtree" || c.kind == "branch") c.mat = SM_WOOD;
            cols.push_back(c);
        } else if (tag == "FOREST") { ls >> forestMin.x >> forestMin.z >> forestMax.x >> forestMax.z; }
        else if (tag == "SPAWN_FOREST") ls >> spawnForest.x >> spawnForest.y >> spawnForest.z;
        else if (tag == "SPAWN_LOBBY") ls >> spawnLobby.x >> spawnLobby.y >> spawnLobby.z;
        else if (tag == "PORTAL") ls >> portalC.x >> portalC.y >> portalC.z >> portalR;
        else if (tag == "GATE") ls >> gateC.x >> gateC.y >> gateC.z >> gateExit.x >> gateExit.y >> gateExit.z >> gateYaw;
        else if (tag == "BANANA") { V3 v; ls >> v.x >> v.y >> v.z; bananas.push_back(v); }
        else if (tag == "GRID") ls >> gCols >> gRows >> gCell >> gOx >> gOz;
        else if (tag == "ROW") { std::string r; ls >> r; grid.push_back(r); }
        else if (tag == "LIGHT") { V3 v; ls >> v.x >> v.y >> v.z; lights.push_back(v); }
        else if (tag == "PATROL") { V3 v; ls >> v.x >> v.z; patrol.push_back(v); }
        else if (tag == "MSPAWN") {
            std::string k; V3 v; ls >> k >> v.x >> v.z;
            int i = k == "babuun" ? 0 : (k == "palido" ? 1 : 2);
            mspawn[i] = v;
        } else if (tag.compare(0, 3, "ST_") == 0) {
            static const char *names[] = {"ST_colorStation", "ST_wardrobe", "ST_badgeBoard", "ST_netBoard", "ST_howTo", "ST_modeBoard", "ST_cameraRack"};
            for (int i = 0; i < 7; i++)
                if (tag == names[i]) { ls >> st[i].c.x >> st[i].c.y >> st[i].c.z >> st[i].yaw; ns++; }
        }
    }
    return !cols.empty();
}

void World::setEnabled(const char *id, bool en) {
    for (auto &c : cols) if (c.id == id) c.enabled = en;
}

bool World::closestPoint(const Collider &c, const V3 &p, V3 &cp, V3 &n, float &inside) const {
    inside = 0;
    if (c.type == 0) {
        V3 d = p - c.c;
        // para espaço local (rotação inversa em Y)
        float lx = c.cs * d.x - c.sn * d.z, lz = c.sn * d.x + c.cs * d.z, ly = d.y;
        float qx = clampf(lx, -c.h.x, c.h.x), qy = clampf(ly, -c.h.y, c.h.y), qz = clampf(lz, -c.h.z, c.h.z);
        V3 ln;
        if (qx == lx && qy == ly && qz == lz) {  // dentro: menor eixo de saída
            float dx = c.h.x - fabsf(lx), dy = c.h.y - fabsf(ly), dz = c.h.z - fabsf(lz);
            if (dx < dy && dx < dz) { qx = lx > 0 ? c.h.x : -c.h.x; ln = V3(lx > 0 ? 1 : -1, 0, 0); inside = dx; }
            else if (dy < dz) { qy = ly > 0 ? c.h.y : -c.h.y; ln = V3(0, ly > 0 ? 1 : -1, 0); inside = dy; }
            else { qz = lz > 0 ? c.h.z : -c.h.z; ln = V3(0, 0, lz > 0 ? 1 : -1); inside = dz; }
        }
        // de volta para mundo
        float wx = c.cs * qx + c.sn * qz, wz = -c.sn * qx + c.cs * qz;
        cp = c.c + V3(wx, qy, wz);
        if (inside > 0) n = V3(c.cs * ln.x + c.sn * ln.z, ln.y, -c.sn * ln.x + c.cs * ln.z);
        else n = norm(p - cp);
        return true;
    } else {
        float dx = p.x - c.c.x, dz = p.z - c.c.z;
        float d = sqrtf(dx * dx + dz * dz);
        bool inR = d < c.r, inY = p.y > c.y0 && p.y < c.y1;
        if (inR && inY) {
            float side = c.r - d, top = c.y1 - p.y, bot = p.y - c.y0;
            if (side < top && side < bot) {
                V3 dir = d > 1e-5f ? V3(dx / d, 0, dz / d) : V3(1, 0, 0);
                cp = V3(c.c.x, p.y, c.c.z) + dir * c.r; n = dir; inside = side;
            } else if (top < bot) { cp = V3(p.x, c.y1, p.z); n = V3(0, 1, 0); inside = top; }
            else { cp = V3(p.x, c.y0, p.z); n = V3(0, -1, 0); inside = bot; }
            return true;
        }
        float k = d > c.r ? c.r / d : 1.f;
        cp = V3(c.c.x + dx * k, clampf(p.y, c.y0, c.y1), c.c.z + dz * k);
        n = norm(p - cp);
        return true;
    }
}

V3 World::resolveSphere(V3 &p, float r, int *matOut) const {
    V3 acc;
    for (int it = 0; it < 3; it++) {
        bool any = false;
        if (p.y < r) { p.y = r; acc += V3(0, 1, 0); any = true; if (matOut) *matOut = SM_GROUND; }
        for (auto &c : cols) {
            if (!c.enabled) continue;
            if (p.x + r < c.bmin.x || p.x - r > c.bmax.x || p.y + r < c.bmin.y || p.y - r > c.bmax.y || p.z + r < c.bmin.z || p.z - r > c.bmax.z) continue;
            V3 cp, n; float inside;
            closestPoint(c, p, cp, n, inside);
            if (inside > 0) { p = cp + n * r; acc += n; any = true; if (matOut) *matOut = c.mat; continue; }
            V3 d = p - cp; float dl = len(d);
            if (dl < r && dl > 1e-6f) { p = cp + d / dl * r; acc += d / dl; any = true; if (matOut) *matOut = c.mat; }
        }
        if (!any) break;
    }
    return acc;
}

static bool rayBox(const Collider &c, const V3 &a, const V3 &dir, float maxT, float &tOut, V3 &nOut) {
    V3 d = a - c.c;
    V3 lo(c.cs * d.x - c.sn * d.z, d.y, c.sn * d.x + c.cs * d.z);
    V3 ld(c.cs * dir.x - c.sn * dir.z, dir.y, c.sn * dir.x + c.cs * dir.z);
    float tmin = -1e9f, tmax = 1e9f; int axis = -1; float sgn = 1;
    const float o[3] = {lo.x, lo.y, lo.z}, dd[3] = {ld.x, ld.y, ld.z}, h[3] = {c.h.x, c.h.y, c.h.z};
    for (int i = 0; i < 3; i++) {
        if (fabsf(dd[i]) < 1e-8f) { if (o[i] < -h[i] || o[i] > h[i]) return false; continue; }
        float t1 = (-h[i] - o[i]) / dd[i], t2 = (h[i] - o[i]) / dd[i];
        float s = -1;
        if (t1 > t2) { float tt = t1; t1 = t2; t2 = tt; s = 1; }
        if (t1 > tmin) { tmin = t1; axis = i; sgn = s; }
        if (t2 < tmax) tmax = t2;
        if (tmin > tmax) return false;
    }
    if (tmin < 0) { if (tmax < 0) return false; tOut = 0; nOut = norm(-dir); return true; }  // começa dentro
    if (tmin > maxT) return false;
    tOut = tmin;
    V3 ln(axis == 0 ? sgn : 0, axis == 1 ? sgn : 0, axis == 2 ? sgn : 0);
    nOut = V3(c.cs * ln.x + c.sn * ln.z, ln.y, -c.sn * ln.x + c.cs * ln.z);
    return true;
}

static bool rayCyl(const Collider &c, const V3 &a, const V3 &dir, float maxT, float &tOut, V3 &nOut) {
    float ox = a.x - c.c.x, oz = a.z - c.c.z;
    // começa dentro?
    if (ox * ox + oz * oz < c.r * c.r && a.y > c.y0 && a.y < c.y1) { tOut = 0; nOut = norm(-dir); return true; }
    float best = 1e9f; V3 bn;
    float A = dir.x * dir.x + dir.z * dir.z, B = 2 * (ox * dir.x + oz * dir.z), C = ox * ox + oz * oz - c.r * c.r;
    if (A > 1e-9f) {
        float disc = B * B - 4 * A * C;
        if (disc >= 0) {
            float t = (-B - sqrtf(disc)) / (2 * A);
            if (t >= 0 && t <= maxT) {
                float y = a.y + dir.y * t;
                if (y >= c.y0 && y <= c.y1) { best = t; bn = norm(V3(ox + dir.x * t, 0, oz + dir.z * t)); }
            }
        }
    }
    if (fabsf(dir.y) > 1e-8f) {
        for (int k = 0; k < 2; k++) {
            float yy = k ? c.y1 : c.y0;
            float t = (yy - a.y) / dir.y;
            if (t >= 0 && t <= maxT && t < best) {
                float x = ox + dir.x * t, z = oz + dir.z * t;
                if (x * x + z * z <= c.r * c.r) { best = t; bn = V3(0, k ? 1.f : -1.f, 0); }
            }
        }
    }
    if (best > maxT) return false;
    tOut = best; nOut = bn; return true;
}

Hit World::raycast(const V3 &a, const V3 &b, float radius) const {
    Hit h;
    V3 d = b - a; float L = len(d);
    if (L < 1e-6f) return h;
    V3 dir = d / L;
    // chão
    if (dir.y < 0 && a.y >= 0) {
        float t = -a.y / dir.y;
        if (t <= L) { h.hit = true; h.t = t; h.p = a + dir * t; h.n = V3(0, 1, 0); h.mat = SM_GROUND; }
    } else if (a.y < 0) { h.hit = true; h.t = 0; h.p = a; h.n = V3(0, 1, 0); h.mat = SM_GROUND; }
    V3 lo(fminf(a.x, b.x) - radius, fminf(a.y, b.y) - radius, fminf(a.z, b.z) - radius);
    V3 hi(fmaxf(a.x, b.x) + radius, fmaxf(a.y, b.y) + radius, fmaxf(a.z, b.z) + radius);
    for (size_t i = 0; i < cols.size(); i++) {
        const Collider &c = cols[i];
        if (!c.enabled) continue;
        if (hi.x < c.bmin.x || lo.x > c.bmax.x || hi.y < c.bmin.y || lo.y > c.bmax.y || hi.z < c.bmin.z || lo.z > c.bmax.z) continue;
        float t; V3 n;
        bool ok = c.type == 0 ? rayBox(c, a, dir, fminf(L, h.t), t, n) : rayCyl(c, a, dir, fminf(L, h.t), t, n);
        if (ok && t < h.t) { h.hit = true; h.t = t; h.p = a + dir * t; h.n = n; h.mat = c.mat; h.idx = (int)i; }
    }
    return h;
}

// Mundo de colisão: caixas com yaw, cilindros verticais e chão (y = 0). Lê app/assets/map.txt.
import { Vector3 } from 'three';

export const SM = { GROUND: 0, WOOD: 1, STONE: 2, METAL: 3, SOFT: 4 };
const matFrom = m => (m === 'stone' || m === 'rock' || m === 'concrete' || m === 'tile') ? SM.STONE
  : (m === 'metal' || m === 'steel') ? SM.METAL : m === 'toy' ? SM.SOFT : SM.WOOD;

const clamp = (v, a, b) => v < a ? a : v > b ? b : v;

export class World {
  constructor(text) {
    this.cols = []; this.bananas = []; this.patrol = []; this.lights = []; this.grid = [];
    this.st = {}; this.mspawn = {}; this.safe = [];
    for (const line of text.split('\n')) {
      const p = line.trim().split(/\s+/);
      const t = p[0];
      const n = i => parseFloat(p[i]);
      if (t === 'B' || t === 'C') {
        const c = { kind: p[1], mat: matFrom(p[2]), enabled: true };
        if (t === 'B') {
          c.type = 0; c.c = new Vector3(n(3), n(4), n(5)); c.h = new Vector3(n(6) / 2, n(7) / 2, n(8) / 2);
          c.yaw = n(9); c.cs = Math.cos(c.yaw); c.sn = Math.sin(c.yaw); c.id = p[10] === '-' ? '' : p[10];
          const ex = Math.abs(c.cs) * c.h.x + Math.abs(c.sn) * c.h.z, ez = Math.abs(c.sn) * c.h.x + Math.abs(c.cs) * c.h.z;
          c.min = [c.c.x - ex, c.c.y - c.h.y, c.c.z - ez]; c.max = [c.c.x + ex, c.c.y + c.h.y, c.c.z + ez];
        } else {
          c.type = 1; c.c = new Vector3(n(3), 0, n(4)); c.r = n(5); c.y0 = n(6); c.y1 = n(7); c.id = p[8] === '-' ? '' : p[8];
          c.min = [c.c.x - c.r, c.y0, c.c.z - c.r]; c.max = [c.c.x + c.r, c.y1, c.c.z + c.r];
        }
        this.cols.push(c);
      } else if (t === 'FOREST') { this.fmin = [n(1), n(2)]; this.fmax = [n(3), n(4)]; }
      else if (t === 'SPAWN_FOREST') this.spawnGame = new Vector3(n(1), n(2), n(3));
      else if (t === 'SPAWN_LOBBY') this.spawnLobby = new Vector3(n(1), n(2), n(3));
      else if (t === 'PORTAL') { this.portal = new Vector3(n(1), n(2), n(3)); this.portalR = n(4); }
      else if (t === 'GATE') { this.gate = new Vector3(n(1), n(2), n(3)); this.gateExit = new Vector3(n(4), n(5), n(6)); }
      else if (t === 'BANANA') this.bananas.push(new Vector3(n(1), n(2), n(3)));
      else if (t === 'PATROL') this.patrol.push(new Vector3(n(1), 0, n(2)));
      else if (t === 'LIGHT') this.lights.push(new Vector3(n(1), n(2), n(3)));
      else if (t === 'MSPAWN') this.mspawn[p[1]] = new Vector3(n(2), 0, n(3));
      else if (t === 'GRID') { this.gCols = +p[1]; this.gRows = +p[2]; this.gCell = n(3); this.gOx = n(4); this.gOz = n(5); }
      else if (t === 'ROW') this.grid.push(p[1]);
      else if (t === 'SAFE') this.safe.push({ rect: [+p[1], +p[2], +p[3], +p[4]], name: (p[5] || '').replace(/_/g, ' '), doors: [] });
      else if (t === 'SDOOR') this.safe[this.safe.length - 1].doors.push([+p[1], +p[2], p[3]]);
      else if (t && t.startsWith('ST_')) this.st[t.slice(3)] = { c: new Vector3(n(1), n(2), n(3)), yaw: n(4) };
    }
  }

  setEnabled(id, en) { for (const c of this.cols) if (c.id === id) c.enabled = en; }

  // ponto mais próximo na superfície; retorna {cp, n, inside}
  closest(c, p) {
    if (c.type === 0) {
      const dx = p.x - c.c.x, dy = p.y - c.c.y, dz = p.z - c.c.z;
      const lx = c.cs * dx - c.sn * dz, lz = c.sn * dx + c.cs * dz, ly = dy;
      let qx = clamp(lx, -c.h.x, c.h.x), qy = clamp(ly, -c.h.y, c.h.y), qz = clamp(lz, -c.h.z, c.h.z);
      let inside = 0, nx = 0, ny = 0, nz = 0;
      if (qx === lx && qy === ly && qz === lz) {
        const ddx = c.h.x - Math.abs(lx), ddy = c.h.y - Math.abs(ly), ddz = c.h.z - Math.abs(lz);
        if (ddx < ddy && ddx < ddz) { qx = lx > 0 ? c.h.x : -c.h.x; nx = lx > 0 ? 1 : -1; inside = ddx; }
        else if (ddy < ddz) { qy = ly > 0 ? c.h.y : -c.h.y; ny = ly > 0 ? 1 : -1; inside = ddy; }
        else { qz = lz > 0 ? c.h.z : -c.h.z; nz = lz > 0 ? 1 : -1; inside = ddz; }
      }
      const cp = new Vector3(c.c.x + c.cs * qx + c.sn * qz, c.c.y + qy, c.c.z - c.sn * qx + c.cs * qz);
      const n = inside > 0 ? new Vector3(c.cs * nx + c.sn * nz, ny, -c.sn * nx + c.cs * nz) : p.clone().sub(cp).normalize();
      return { cp, n, inside };
    }
    const dx = p.x - c.c.x, dz = p.z - c.c.z, d = Math.hypot(dx, dz);
    if (d < c.r && p.y > c.y0 && p.y < c.y1) {
      const side = c.r - d, top = c.y1 - p.y, bot = p.y - c.y0;
      if (side < top && side < bot) {
        const dir = d > 1e-5 ? new Vector3(dx / d, 0, dz / d) : new Vector3(1, 0, 0);
        return { cp: new Vector3(c.c.x + dir.x * c.r, p.y, c.c.z + dir.z * c.r), n: dir, inside: side };
      }
      if (top < bot) return { cp: new Vector3(p.x, c.y1, p.z), n: new Vector3(0, 1, 0), inside: top };
      return { cp: new Vector3(p.x, c.y0, p.z), n: new Vector3(0, -1, 0), inside: bot };
    }
    const k = d > c.r ? c.r / d : 1;
    const cp = new Vector3(c.c.x + dx * k, clamp(p.y, c.y0, c.y1), c.c.z + dz * k);
    return { cp, n: p.clone().sub(cp).normalize(), inside: 0 };
  }

  // empurra esfera para fora; retorna soma das normais de contato
  resolveSphere(p, r, out) {
    const acc = new Vector3();
    for (let it = 0; it < 3; it++) {
      let any = false;
      if (p.y < r) { p.y = r; acc.y += 1; any = true; if (out) out.mat = SM.GROUND; }
      for (const c of this.cols) {
        if (!c.enabled) continue;
        if (p.x + r < c.min[0] || p.x - r > c.max[0] || p.y + r < c.min[1] || p.y - r > c.max[1] || p.z + r < c.min[2] || p.z - r > c.max[2]) continue;
        const { cp, n, inside } = this.closest(c, p);
        if (inside > 0) { p.copy(cp).addScaledVector(n, r); acc.add(n); any = true; if (out) out.mat = c.mat; continue; }
        const dl = p.distanceTo(cp);
        if (dl < r && dl > 1e-6) { const d = p.clone().sub(cp).divideScalar(dl); p.copy(cp).addScaledVector(d, r); acc.add(d); any = true; if (out) out.mat = c.mat; }
      }
      if (!any) break;
    }
    return acc;
  }

  // raio de a até b: {hit, t, p, n, mat}
  raycast(a, b) {
    const d = b.clone().sub(a), L = d.length();
    const h = { hit: false, t: 1e9, p: null, n: null, mat: SM.GROUND };
    if (L < 1e-6) return h;
    const dir = d.divideScalar(L);
    if (a.y < 0) { h.hit = true; h.t = 0; h.p = a.clone(); h.n = new Vector3(0, 1, 0); }
    else if (dir.y < 0) { const t = -a.y / dir.y; if (t <= L) { h.hit = true; h.t = t; h.p = a.clone().addScaledVector(dir, t); h.n = new Vector3(0, 1, 0); } }
    const lo = [Math.min(a.x, b.x), Math.min(a.y, b.y), Math.min(a.z, b.z)], hi = [Math.max(a.x, b.x), Math.max(a.y, b.y), Math.max(a.z, b.z)];
    for (const c of this.cols) {
      if (!c.enabled) continue;
      if (hi[0] < c.min[0] || lo[0] > c.max[0] || hi[1] < c.min[1] || lo[1] > c.max[1] || hi[2] < c.min[2] || lo[2] > c.max[2]) continue;
      const r = c.type === 0 ? this.rayBox(c, a, dir, Math.min(L, h.t)) : this.rayCyl(c, a, dir, Math.min(L, h.t));
      if (r && r.t < h.t) { h.hit = true; h.t = r.t; h.p = a.clone().addScaledVector(dir, r.t); h.n = r.n; h.mat = c.mat; }
    }
    return h;
  }

  rayBox(c, a, dir, maxT) {
    const dx = a.x - c.c.x, dy = a.y - c.c.y, dz = a.z - c.c.z;
    const o = [c.cs * dx - c.sn * dz, dy, c.sn * dx + c.cs * dz], dd = [c.cs * dir.x - c.sn * dir.z, dir.y, c.sn * dir.x + c.cs * dir.z];
    const hh = [c.h.x, c.h.y, c.h.z];
    let tmin = -1e9, tmax = 1e9, axis = -1, sgn = 1;
    for (let i = 0; i < 3; i++) {
      if (Math.abs(dd[i]) < 1e-8) { if (o[i] < -hh[i] || o[i] > hh[i]) return null; continue; }
      let t1 = (-hh[i] - o[i]) / dd[i], t2 = (hh[i] - o[i]) / dd[i], s = -1;
      if (t1 > t2) { const tt = t1; t1 = t2; t2 = tt; s = 1; }
      if (t1 > tmin) { tmin = t1; axis = i; sgn = s; }
      if (t2 < tmax) tmax = t2;
      if (tmin > tmax) return null;
    }
    if (tmin < 0) { if (tmax < 0) return null; return { t: 0, n: dir.clone().negate() }; }
    if (tmin > maxT) return null;
    const ln = [axis === 0 ? sgn : 0, axis === 1 ? sgn : 0, axis === 2 ? sgn : 0];
    return { t: tmin, n: new Vector3(c.cs * ln[0] + c.sn * ln[2], ln[1], -c.sn * ln[0] + c.cs * ln[2]) };
  }

  rayCyl(c, a, dir, maxT) {
    const ox = a.x - c.c.x, oz = a.z - c.c.z;
    if (ox * ox + oz * oz < c.r * c.r && a.y > c.y0 && a.y < c.y1) return { t: 0, n: dir.clone().negate() };
    let best = 1e9, bn = null;
    const A = dir.x * dir.x + dir.z * dir.z, B = 2 * (ox * dir.x + oz * dir.z), C = ox * ox + oz * oz - c.r * c.r;
    if (A > 1e-9) {
      const disc = B * B - 4 * A * C;
      if (disc >= 0) {
        const t = (-B - Math.sqrt(disc)) / (2 * A);
        if (t >= 0 && t <= maxT) { const y = a.y + dir.y * t; if (y >= c.y0 && y <= c.y1) { best = t; bn = new Vector3(ox + dir.x * t, 0, oz + dir.z * t).normalize(); } }
      }
    }
    if (Math.abs(dir.y) > 1e-8) for (const k of [0, 1]) {
      const yy = k ? c.y1 : c.y0, t = (yy - a.y) / dir.y;
      if (t >= 0 && t <= maxT && t < best) { const x = ox + dir.x * t, z = oz + dir.z * t; if (x * x + z * z <= c.r * c.r) { best = t; bn = new Vector3(0, k ? 1 : -1, 0); } }
    }
    return best > maxT ? null : { t: best, n: bn };
  }

  los(a, b) { return !this.raycast(a, b).hit; }

  // célula de zona segura (sala ou porta com barreira)
  safeCell(c, r) {
    for (const z of this.safe) {
      const [r0, c0, r1, c1] = z.rect;
      if (r >= r0 && r <= r1 && c >= c0 && c <= c1) return z;
      for (const d of z.doors) if (d[0] === r && d[1] === c) return z;
    }
    return null;
  }
  // ponto dentro de zona segura? (margin > 0 aumenta a zona — usado para manter monstros longe da barreira)
  inSafe(p, margin = 0) {
    if (p.x < -150) return null;
    for (const [dx, dz] of margin ? [[0, 0], [margin, 0], [-margin, 0], [0, margin], [0, -margin]] : [[0, 0]]) {
      const z = this.safeCell(Math.floor((p.x + dx - this.gOx) / this.gCell), Math.floor((p.z + dz - this.gOz) / this.gCell));
      if (z) return z;
    }
    return null;
  }
  walkable(c, r) {
    if (r < 0 || r >= this.gRows || c < 0 || c >= this.gCols) return false;
    const ch = this.grid[r][c];
    return (ch === '.' || ch === 'P') && !this.safeCell(c, r);   // monstros nunca entram em zona segura
  }

  // caminho BFS na grade da planta
  findPath(from, to) {
    const cellOf = p => [Math.floor((p.x - this.gOx) / this.gCell), Math.floor((p.z - this.gOz) / this.gCell)];
    let [sc, sr] = cellOf(from), [tc, tr] = cellOf(to);
    if (!this.walkable(tc, tr)) {
      let best = 1e9, bc = tc, br = tr;
      for (let dr = -2; dr <= 2; dr++) for (let dc = -2; dc <= 2; dc++) if (this.walkable(tc + dc, tr + dr)) {
        const x = this.gOx + (tc + dc + 0.5) * this.gCell, z = this.gOz + (tr + dr + 0.5) * this.gCell;
        const d = Math.hypot(x - to.x, z - to.z); if (d < best) { best = d; bc = tc + dc; br = tr + dr; }
      }
      tc = bc; tr = br;
    }
    if (!this.walkable(sc, sr)) {
      let ok = false;
      for (let dr = -1; dr <= 1 && !ok; dr++) for (let dc = -1; dc <= 1 && !ok; dc++) if (this.walkable(sc + dc, sr + dr)) { sc += dc; sr += dr; ok = true; }
      if (!ok) return [];
    }
    const W = this.gCols, prev = new Int32Array(W * this.gRows).fill(-2), q = [sr * W + sc];
    prev[sr * W + sc] = -1;
    const goal = tr * W + tc;
    for (let qi = 0; qi < q.length; qi++) {
      const cur = q[qi]; if (cur === goal) break;
      const c = cur % W, r = (cur / W) | 0;
      for (const [dc, dr] of [[1, 0], [-1, 0], [0, 1], [0, -1]]) {
        const nc = c + dc, nr = r + dr; if (!this.walkable(nc, nr)) continue;
        const ni = nr * W + nc; if (prev[ni] !== -2) continue; prev[ni] = cur; q.push(ni);
      }
    }
    if (prev[goal] === -2) return [];
    const out = [];
    for (let c = goal; c >= 0; c = prev[c]) out.push(new Vector3(this.gOx + (c % W + 0.5) * this.gCell, 0, this.gOz + ((c / W | 0) + 0.5) * this.gCell));
    out.reverse();
    out[out.length - 1] = new Vector3(to.x, 0, to.z);
    return out;
  }
}

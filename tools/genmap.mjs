// Gera shared/map.json (Blender) e app/assets/map.txt (motor C++).
// Mapa: "O Depósito" — instalação subterrânea abandonada: concreto, piso de ladrilho,
// luzes fluorescentes, corredores e corredores de prateleiras de metal cheias de brinquedos.
// Coordenadas three.js/jogo: Y para cima. Chão implícito em y = 0.
import { writeFileSync } from 'fs';

let seed = 1337;
const rnd = () => ((seed = (seed * 16807) % 2147483647) - 1) / 2147483646;
const R = (a, b) => a + rnd() * (b - a);

const colliders = [];
const box = (kind, x, y, z, sx, sy, sz, yaw = 0, mat = 'concrete', extra = {}) =>
  colliders.push({ type: 'box', kind, c: [x, y, z], s: [sx, sy, sz], yaw, mat, ...extra });
const cyl = (kind, x, z, r, y0, y1, mat = 'metal', extra = {}) =>
  colliders.push({ type: 'cyl', kind, c: [x, z], r, y0, y1, mat, ...extra });

// ---------------- O DEPÓSITO (planta em grade) ----------------
// # parede  . chão  S prateleira (corre no eixo Z)  s prateleira (eixo X)
// C pilha de caixotes  T mesa  G gerador  B pilha de bolas de brinquedo
// P nascimento  E saída (portão)  O pilar  V duto de ventilação (passagem baixa)
const PLAN = [
  '##########################',
  '#.......#........#.......#',
  '#.S..S..#..T..T..#..G..G.#',
  '#.S..S..#........#.......#',
  '#.S..S.....O..O.....G....#',
  '#.S..S..#........#.......#',
  '#.......#..T..T..###.#####',
  '###.#####........#.......#',
  '#.......######.###..C....#',
  '#.sss...#.............C..#',
  '#.......#..S.S.S.S.......#',
  '#.sss......S.S.S.S.......E',
  '#.......#..S.S.S.S...BB..#',
  '#.sss...#..S.S.S.S...BB..#',
  '#.......#..S.S.S.S.......#',
  '####.####.............C..#',
  '#.......#####.############',
  '#..CC...#.......#........#',
  '#.......#...P...#..T..T..#',
  '#..O........O.......O....#',
  '#.......#.......#..T..T..#',
  '#..B....#.......#........#',
  '##########################',
];
const CELL = 3, WALLH = 5;
const ROWS = PLAN.length, COLS = PLAN[0].length;
const OX = -(COLS * CELL) / 2, OZ = -(ROWS * CELL) / 2;
const cx = c => OX + (c + 0.5) * CELL, cz = r => OZ + (r + 0.5) * CELL;
const at = (r, c) => (r < 0 || r >= ROWS || c < 0 || c >= COLS ? '#' : PLAN[r][c]);

// paredes: funde trechos horizontais de '#'
const used = PLAN.map(row => [...row].map(() => false));
for (let r = 0; r < ROWS; r++) {
  for (let c = 0; c < COLS; c++) {
    if (at(r, c) !== '#' || used[r][c]) continue;
    let e = c;
    while (e + 1 < COLS && at(r, e + 1) === '#' && !used[r][e + 1]) e++;
    // tenta crescer verticalmente se a faixa for de 1 célula
    let b = r;
    if (e === c) while (b + 1 < ROWS && at(b + 1, c) === '#' && !used[b + 1][c] && (c + 1 >= COLS || at(b + 1, c + 1) !== '#' || true)) {
      // só cresce se a linha de baixo também for coluna isolada
      if (at(b + 1, c + 1) === '#' && c + 1 < COLS && !used[b + 1][c + 1]) break;
      b++;
    }
    for (let rr = r; rr <= b; rr++) for (let cc = c; cc <= e; cc++) used[rr][cc] = true;
    const w = (e - c + 1) * CELL, d = (b - r + 1) * CELL;
    box('cwall', OX + c * CELL + w / 2, WALLH / 2, OZ + r * CELL + d / 2, w, WALLH, d, 0, 'concrete');
  }
}
// teto (não dá para sair por cima)
const W = COLS * CELL, D = ROWS * CELL;
box('ceiling', 0, WALLH + 0.25, 0, W, 0.5, D, 0, 'concrete');

// prateleiras: funde S verticais e s horizontais
const SH = 3.4, SD = 1.1;
const shelfUsed = PLAN.map(row => [...row].map(() => false));
for (let r = 0; r < ROWS; r++) for (let c = 0; c < COLS; c++) {
  const ch = at(r, c);
  if (shelfUsed[r][c]) continue;
  if (ch === 'S') {
    let b = r; while (at(b + 1, c) === 'S') b++;
    for (let rr = r; rr <= b; rr++) shelfUsed[rr][c] = true;
    const L = (b - r + 1) * CELL - 0.4;
    box('shelf', cx(c), SH / 2, (cz(r) + cz(b)) / 2, SD, SH, L, 0, 'steel');
  } else if (ch === 's') {
    let e = c; while (at(r, e + 1) === 's') e++;
    for (let cc = c; cc <= e; cc++) shelfUsed[r][cc] = true;
    const L = (e - c + 1) * CELL - 0.4;
    box('shelf', (cx(c) + cx(e)) / 2, SH / 2, cz(r), L, SH, SD, 0, 'steel');
  }
}

let spawn = [0, 0.6, 0], gate = null;
const bananaSpots = [], patrol = [];
for (let r = 0; r < ROWS; r++) for (let c = 0; c < COLS; c++) {
  const ch = at(r, c), x = cx(c), z = cz(r);
  if (ch === 'C') {
    const s = R(1.0, 1.4);
    box('crate', x + R(-0.3, 0.3), s / 2, z + R(-0.3, 0.3), s, s, s, R(0, 1.5), 'crate');
    box('crate', x + R(-0.2, 0.2), s + 0.45, z + R(-0.2, 0.2), 0.9, 0.9, 0.9, R(0, 1.5), 'crate');
    bananaSpots.push([x, s + 1.1, z]);
  } else if (ch === 'T') {
    box('table', x, 0.4, z, 2.0, 0.8, 1.0, 0, 'steel');
    bananaSpots.push([x, 1.05, z]);
  } else if (ch === 'G') {
    box('machine', x, 1.1, z, 2.2, 2.2, 1.6, 0, 'steel');
    bananaSpots.push([x, 2.5, z]);
  } else if (ch === 'B') {
    box('toypile', x, 0.4, z, 2.4, 0.8, 2.4, 0, 'toy');
    bananaSpots.push([x, 1.1, z]);
  } else if (ch === 'O') {
    box('pillar', x, WALLH / 2, z, 0.9, WALLH, 0.9, 0, 'concrete');
  } else if (ch === 'P') {
    spawn = [x, 0.6, z];
  } else if (ch === 'E') {
    gate = { c: [x - 0.5, 2, z], exit: [x + 2.5, 1, z], yaw: Math.PI / 2 };
  }
  if (ch === '.' && (r + c) % 4 === 0 && rnd() < 0.5) patrol.push([x, z]);
}
// bananas também em cima das prateleiras
for (const c of colliders.filter(c => c.kind === 'shelf')) {
  bananaSpots.push([c.c[0], SH + 0.35, c.c[2] + (c.s[2] > c.s[0] ? R(-2, 2) : 0)]);
  // na frente da prateleira (lado do corredor), na altura do 2º andar
  if (c.s[2] > c.s[0]) bananaSpots.push([c.c[0] + (rnd() < 0.5 ? -1 : 1) * (c.s[0] / 2 + 0.3), 1.95, c.c[2] + R(-2, 2)]);
  else bananaSpots.push([c.c[0] + R(-1, 1), 1.95, c.c[2] + (rnd() < 0.5 ? -1 : 1) * (c.s[2] / 2 + 0.3)]);
}
// portão da saída: buraco na parede leste + porta de aço
const gx = OX + COLS * CELL - CELL / 2;
// remove a parede que cobre a saída (era '#'? 'E' não gera parede) e cria batentes
box('gate', gate.c[0] + 0.5, 2, gate.c[2], 0.4, 4, 2.8, 0, 'steel', { id: 'gate' });
box('cwall', gate.c[0] + 0.5, 4.5, gate.c[2], CELL, 1, CELL, 0, 'concrete');
// corredor de saída fora do depósito
box('cwall', gate.exit[0] + 1, WALLH / 2, gate.c[2] - 2.2, 6, WALLH, 0.6, 0, 'concrete');
box('cwall', gate.exit[0] + 1, WALLH / 2, gate.c[2] + 2.2, 6, WALLH, 0.6, 0, 'concrete');
box('cwall', gate.exit[0] + 4, WALLH / 2, gate.c[2], 0.6, WALLH, 4.4, 0, 'concrete');
// luzes fluorescentes: uma por célula livre alternada
const lights = [];
for (let r = 1; r < ROWS - 1; r++) for (let c = 1; c < COLS - 1; c++) {
  const ch = at(r, c);
  if (ch === '#' || (r + c) % 2) continue;
  if (r % 2 || c % 2) continue;
  lights.push([cx(c), WALLH - 0.05, cz(r)]);
}
// IDs para pontos de patrulha mínimos
if (patrol.length < 10) for (let i = 0; i < 10; i++) patrol.push([R(-W / 3, W / 3), R(-D / 3, D / 3)]);

// ---------------- LOBBY (sala de controle limpa) ----------------
const LX = -200, LZ = 0, LW = 16, LH = 6;
box('lobbyfloor', LX, -0.25, LZ, LW + 1, 0.5, LW + 1, 0, 'tile');
box('lobbywall', LX, LH / 2, LZ - LW / 2, LW, LH, 0.5, 0, 'concrete');
box('lobbywall', LX - LW / 2, LH / 2, LZ, 0.5, LH, LW, 0, 'concrete');
box('lobbywall', LX + LW / 2, LH / 2, LZ, 0.5, LH, LW, 0, 'concrete');
box('lobbywall', LX - 5, LH / 2, LZ + LW / 2, 6, LH, 0.5, 0, 'concrete');
box('lobbywall', LX + 5, LH / 2, LZ + LW / 2, 6, LH, 0.5, 0, 'concrete');
box('lobbywall', LX, LH - 1.5, LZ + LW / 2, 4, 3, 0.5, 0, 'concrete');
box('lobbyceil', LX, LH + 0.25, LZ, LW + 1, 0.5, LW + 1, 0, 'concrete');
box('pillar', LX, LH / 2, LZ, 1.2, LH, 1.2, 0, 'concrete');
box('shelf', LX - 6.6, SH / 2, LZ - 3, SD, SH, 5, 0, 'steel');
box('shelf', LX + 6.6, SH / 2, LZ + 3.5, SD, SH, 4, 0, 'steel');
// túnel de saída (portal)
box('lobbywall', LX - 2.25, 2, LZ + LW / 2 + 2, 0.5, 4, 4, 0, 'concrete');
box('lobbywall', LX + 2.25, 2, LZ + LW / 2 + 2, 0.5, 4, 4, 0, 'concrete');
box('lobbywall', LX, 4.25, LZ + LW / 2 + 2, 5, 0.5, 4, 0, 'concrete');
box('lobbywall', LX, 2, LZ + LW / 2 + 4.25, 5, 4, 0.5, 0, 'concrete');
for (const [dx, dz] of [[-5, -5], [5, -5], [-5, 5], [5, 5], [0, -2], [0, 3]]) lights.push([LX + dx, LH - 0.05, LZ + dz]);

const map = {
  version: 2,
  plan: PLAN, cell: CELL, wallH: WALLH, origin: [OX, OZ],
  forest: { min: [OX, OZ], max: [OX + W, OZ + D], spawn },
  lobby: {
    center: [LX, 0, LZ], size: LW, height: LH, spawn: [LX, 0.6, LZ + 4],
    portal: { c: [LX, 1.5, LZ + LW / 2 + 2.5], r: 1.6 },
    colorStation: { c: [LX - 4, 1.4, LZ - LW / 2 + 0.3], yaw: 0 },
    wardrobe: { c: [LX + 3.5, 1.4, LZ - LW / 2 + 0.3], yaw: 0 },
    badgeBoard: { c: [LX - LW / 2 + 0.3, 2.6, LZ + 2.5], yaw: Math.PI / 2 },
    netBoard: { c: [LX + LW / 2 - 0.3, 1.6, LZ], yaw: -Math.PI / 2 },
    howTo: { c: [LX - LW / 2 + 0.3, 2.6, LZ - 2.5], yaw: Math.PI / 2 },
    modeBoard: { c: [LX + LW / 2 - 0.3, 1.6, LZ - 5], yaw: -Math.PI / 2 },
    cameraRack: { c: [LX + 4.5, 1.2, LZ + 5.5], yaw: Math.PI }
  },
  gate,
  bananaSpots,
  patrol,
  lights,
  monsterSpawns: { babuun: [cx(21), cz(3)], palido: [cx(4), cz(11)], mamae: [cx(21), cz(19)] },
  colliders
};
writeFileSync(new URL('../shared/map.json', import.meta.url), JSON.stringify(map));

// Formato texto simples para o motor C++ (app/assets/map.txt)
const f = n => (+n).toFixed(3);
const L = [];
for (const c of colliders) {
  if (c.type === 'box') L.push(['B', c.kind, c.mat, ...c.c.map(f), ...c.s.map(f), f(c.yaw), c.id || '-'].join(' '));
  else L.push(['C', c.kind, c.mat, f(c.c[0]), f(c.c[1]), f(c.r), f(c.y0), f(c.y1), c.id || '-'].join(' '));
}
const P = (tag, v) => L.push([tag, ...v.map(f)].join(' '));
P('FOREST', [...map.forest.min, ...map.forest.max]);
P('SPAWN_FOREST', map.forest.spawn);
P('SPAWN_LOBBY', map.lobby.spawn);
P('PORTAL', [...map.lobby.portal.c, map.lobby.portal.r]);
for (const k of ['colorStation', 'wardrobe', 'badgeBoard', 'netBoard', 'howTo', 'modeBoard', 'cameraRack'])
  P('ST_' + k, [...map.lobby[k].c, map.lobby[k].yaw]);
P('GATE', [...gate.c, ...gate.exit, gate.yaw]);
for (const b of bananaSpots) P('BANANA', b);
for (const p of patrol) P('PATROL', p);
for (const l of lights) P('LIGHT', l);
for (const [k, v] of Object.entries(map.monsterSpawns)) L.push(['MSPAWN', k, ...v.map(f)].join(' '));
L.push(['GRID', COLS, ROWS, CELL, f(OX), f(OZ)].join(' '));
for (const row of PLAN) L.push('ROW ' + row);
writeFileSync(new URL('../app/assets/map.txt', import.meta.url), L.join('\n') + '\n');
console.log('map:', colliders.length, 'colisores,', bananaSpots.length, 'bananas,', lights.length, 'luzes,', patrol.length, 'patrulha');

// Babuun Hollow — jogador, gorilla locomotion com mão duplicada, câmera, rodada, monstros e rede
import * as THREE from 'three';
import { model, setTint } from './assets.js';
import { audio } from './audio.js';
import { SM } from './world.js';
import { Net } from './net.js';
import { Lobby } from './lobby.js';
import { CameraRig } from './camera.js';
import { buildSafeZones } from './safezone.js';

const V = (x = 0, y = 0, z = 0) => new THREE.Vector3(x, y, z);
const clamp = (v, a, b) => v < a ? a : v > b ? b : v;
const lerp = (a, b, t) => a + (b - a) * t;
const smooth = (a, b, x) => { const t = clamp((x - a) / (b - a), 0, 1); return t * t * (3 - 2 * t); };
const wrap = a => { while (a > Math.PI) a -= 2 * Math.PI; while (a < -Math.PI) a += 2 * Math.PI; return a; };
const qYaw = a => new THREE.Quaternion().setFromAxisAngle(V(0, 1, 0), a);
const qAxis = (ax, a) => new THREE.Quaternion().setFromAxisAngle(ax, a);
const yawOf = q => { const f = V(0, 0, -1).applyQuaternion(q); return Math.atan2(-f.x, -f.z); };
const hlen = v => Math.hypot(v.x, v.z);

export const BADGES = [
  ['Primeiro Passo', 'Entre no Depósito pela primeira vez'], ['Banana!', 'Pegue uma banana dourada'],
  ['Colecionador', 'Pegue todas as bananas de uma rodada'], ['Fuga!', 'Escape pela porta de aço'],
  ['Sobrevivente', 'Fique 5 minutos vivo numa rodada'], ['Mordido', 'Seja pego pelo Babuun'],
  ['Não Pisque', 'Seja pego pelo Pálido'], ['Mamãe Brava', 'Seja pego pela Mamãe'],
  ['Macaco Social', 'Jogue com outro macaco online'], ['Estilista', 'Mude sua cor e um acessório'],
  ['Cinegrafista', 'Grave um vídeo com a câmera'], ['Alpinista', 'Suba no topo de uma prateleira'],
  ['Velocista', 'Alcance 10 m/s'], ['Veterano', 'Escape 5 vezes'],
  ['Corajoso', 'Encha a barra de coragem'],
];
export const HATS = ['', 'cos_cap', 'cos_beanie', 'cos_tophat', 'cos_crown', 'cos_leafcrown', 'cos_horns', 'cos_headphones'];
export const FACES = ['', 'cos_glasses', 'cos_shades'];
export const NECKS = ['', 'cos_bowtie'];
export const ACC = [[0.85, 0.15, 0.12], [0.15, 0.35, 0.9], [0.2, 0.7, 0.25], [0.95, 0.8, 0.1], [0.6, 0.2, 0.8], [0.95, 0.95, 0.95], [0.1, 0.1, 0.1], [1, 0.5, 0.1]];
const GRAV = 9.8, BODY_R = 0.28, ARM_LEN = 0.85;
const BOOST_MUL = [1.3, 1.6, 1.9], BOOST_MAX = [8.5, 10.5, 12.5];
const NAMES_A = ['Banana', 'Macaco', 'Bugio', 'Mico', 'Sagui', 'Gorila', 'Babu', 'Chimpa', 'Uakari', 'Kong'];
const NAMES_B = ['Veloz', 'Medroso', 'Valente', 'Pelado', 'Doido', 'Ninja', 'Real', 'SemPerna', 'Turbo', 'Fantasma'];
export const randomName = () => NAMES_A[Math.random() * 10 | 0] + NAMES_B[Math.random() * 10 | 0] + (10 + (Math.random() * 90 | 0));

// ------------------------------------------------------------------ perfil (salvo no navegador)
function loadProfile() {
  const d = { name: randomName(), r: 7, g: 4, b: 2, hat: 0, face: 0, neck: 0, acc: 0, title: -1, badges: 0, escapes: 0, catches: 0,
              handMode: 0, boost: 1, snap: 1, armAngle: 25, volume: 8 };
  try { return { ...d, ...JSON.parse(localStorage.getItem('babuun_perfil') || '{}') }; } catch (e) { return d; }
}
export const colorOf = p => new THREE.Color(0.08 + p.r / 9 * 0.92, 0.08 + p.g / 9 * 0.92, 0.08 + p.b / 9 * 0.92);

// ------------------------------------------------------------------ avatar (macaco sem pernas)
export class Avatar {
  constructor(scene, prof, showHead = true) {
    this.root = new THREE.Group();
    this.body = model('monkey', colorOf(prof));
    this.root.add(this.body);
    this.n = this.body.userData.nodes;
    this.hands = [model('hand', colorOf(prof)), model('hand', colorOf(prof))];
    this.arms = [model('arm', colorOf(prof)), model('arm', colorOf(prof))];
    for (const o of [...this.hands, ...this.arms]) scene.add(o);
    this.cos = {};
    scene.add(this.root);
    this.showHead = showHead;
    this.key = '';
    this.apply(prof);
    if (!showHead && this.n.head) this.n.head.visible = false;
  }
  apply(p) {
    const key = [p.r, p.g, p.b, p.hat, p.face, p.neck, p.acc].join(',');
    if (key === this.key) return;
    this.key = key;
    const c = colorOf(p);
    for (const o of [this.body, ...this.hands, ...this.arms]) setTint(o, c);
    const acc = new THREE.Color(...ACC[p.acc & 7]);
    const slot = (k, list, idx, anchor) => {
      if (this.cos[k]) { this.cos[k].removeFromParent(); this.cos[k] = null; }
      const name = list[idx] || '';
      if (name && this.n[anchor]) { this.cos[k] = model(name, acc); this.n[anchor].add(this.cos[k]); }
    };
    slot('hat', HATS, p.hat, 'anchor_hat'); slot('face', FACES, p.face, 'anchor_face'); slot('neck', NECKS, p.neck, 'anchor_neck');
  }
  set(bodyPos, yaw, headQ, lh, rh) {
    this.root.position.copy(bodyPos);
    this.root.quaternion.copy(qYaw(yaw));
    if (this.n.head) this.n.head.quaternion.copy(this.n.head.userData.baseQ).multiply(qYaw(-yaw).multiply(headQ));
    this.root.updateMatrixWorld(true);
    const sh = [V(), V()];
    if (this.n.shoulder_l) this.n.shoulder_l.getWorldPosition(sh[0]); else sh[0].copy(bodyPos);
    if (this.n.shoulder_r) this.n.shoulder_r.getWorldPosition(sh[1]); else sh[1].copy(bodyPos);
    const hp = [lh, rh];
    for (let i = 0; i < 2; i++) {
      const d = hp[i].clone().sub(sh[i]); const L = d.length();
      if (L < 0.01) continue;
      d.divideScalar(L);
      const wrist = hp[i].clone().addScaledVector(d, -0.09);
      const al = wrist.distanceTo(sh[i]);
      this.arms[i].position.copy(sh[i]);
      this.arms[i].quaternion.setFromUnitVectors(V(0, 1, 0), d);
      this.arms[i].scale.set(1, al, 1);
      this.hands[i].position.copy(wrist);
      this.hands[i].quaternion.setFromUnitVectors(V(0, 0, -1), d);
    }
  }
  setVisible(v) { this.root.visible = v; for (const o of [...this.hands, ...this.arms]) o.visible = v; }
  dispose() { this.root.removeFromParent(); for (const o of [...this.hands, ...this.arms]) o.removeFromParent(); }
}

// ------------------------------------------------------------------ JOGO
export class Game {
  constructor(scene, rig, camera, world) {
    this.scene = scene; this.rig = rig; this.camera = camera; this.world = world;
    this.prof = loadProfile();
    this.now = 0;
    this.pstate = 'lobby';        // lobby | game | caught
    this.body = world.spawnLobby.clone(); this.vel = V(); this.rigYaw = Math.PI; this.bodyYaw = Math.PI;
    this.headQ = new THREE.Quaternion(); this.headPos = V();
    this.shoulder = [V(), V()]; this.target = [V(), V()]; this.handVis = [V(), V()]; this.anchor = [V(), V()]; this.prevT = [V(), V()];
    this.touching = [false, false]; this.hist = []; this.wasTouching = false;
    this.prevLoc = [V(), V()]; this.anchorN = [V(0, 1, 0), V(0, 1, 0)]; this.lift = 0; this.altBlend = 0; this.ctrlHand = 1;
    this.ctrlDir = V(0, 0, -1); this.aliveT = 0; this.speed = 0;
    this.msgs = [];
    // rodada
    this.roundSeed = 0; this.roundBananas = []; this.collected = 0; this.gateOpen = false; this.gateAnim = 0; this.phase = 0;
    this.mons = [0, 1, 2].map(t => ({ type: t, active: false, pos: world.mspawn[['babuun', 'palido', 'mamae'][t]].clone(), yaw: 0, state: 1,
      target: -1, timer: 0, anim: 0, speed: 0, jaw: 0, voiceT: 0, stuckT: 0, pathT: 0, goal: null, lastSeen: V(), lastPos: V(), path: [],
      leapT: 0, leapFrom: V(), leapTo: V(), netPos: null, netYaw: 0, lastPh: 0 }));
    this.remotes = new Map();
    this.jsT = -1; this.jsType = 0; this.fade = 0;
    this.net = new Net(m => this.onNet(m));
    this.createCfg = { name: 'Toca do ' + this.prof.name, isPublic: true, code: '', maxPlayers: 6, diff: 1, bananas: 8 };
    this.codeEntry = '';
    this.sendT = 0; this.monT = 0; this.roundT = 0;
    this.chase = 0; this.chaseV = 0; this.heartV = 0; this.ambV = 0; this.ambMode = -1;
    this.buildScene();
    this.lobby = new Lobby(this);
    this.cam = new CameraRig(this);
    this.teleport(world.spawnLobby, Math.PI);
    this.hud('Bem-vindo ao Babuun Hollow, ' + this.prof.name + '!', '#ffe066', 5);
    this.hud('Aponte o controle para baixo e empurre o chão para andar.', '#fff', 6);
    setInterval(() => this.lobby.scanTick(), 1000);
  }

  get cfg() { return this.net.online && this.net.cfg ? this.net.cfg : this.createCfg; }
  get myId() { return this.net.online ? this.net.myId : 0; }
  isHost() { return this.net.isSim(); }
  save() { try { localStorage.setItem('babuun_perfil', JSON.stringify(this.prof)); } catch (e) {} }

  buildScene() {
    const s = this.scene;
    this.mapModel = model('map'); s.add(this.mapModel);
    this.gateModel = model('gate'); s.add(this.gateModel);
    this.bananaModels = [];
    for (let i = 0; i < 24; i++) { const b = model('banana'); b.scale.setScalar(1.3); b.visible = false; s.add(b); this.bananaModels.push(b); }
    this.monModels = ['babuun', 'palido', 'mamae'].map(n => { const m = model(n); m.visible = false; s.add(m); return m; });
    this.me = new Avatar(s, this.prof, false);
    this.safeFx = buildSafeZones(s, this.world);
    // lanterna presa na mão do controle
    this.torch = new THREE.SpotLight(0xfff1cf, 0, 12, 0.5, 0.45, 1.2);
    s.add(this.torch, this.torch.target);
    this.torchModel = new THREE.Group();
    const tb = new THREE.Mesh(new THREE.CylinderGeometry(0.022, 0.028, 0.16, 12), new THREE.MeshLambertMaterial({ color: 0x2a2a2a }));
    tb.rotation.x = Math.PI / 2; this.torchModel.add(tb);
    const lens = new THREE.Mesh(new THREE.CircleGeometry(0.024, 16), new THREE.MeshBasicMaterial({ color: 0xfff6d8 }));
    lens.position.z = -0.081; lens.rotation.y = Math.PI; this.torchModel.add(lens);
    s.add(this.torchModel);
    this.courage = 0; this.courageMaxed = false;
    // HUD preso à cabeça
    this.hudGroup = new THREE.Group();
    this.camera.add(this.hudGroup);
    // luz
    this.hemi = new THREE.HemisphereLight(0xcfd3dd, 0x2a2a2c, 1.0); s.add(this.hemi);
    this.plights = [0, 1, 2].map(() => { const l = new THREE.PointLight(0xdfe6ff, 6, 9, 1.6); s.add(l); return l; });
  }

  hud(t, color = '#fff', time = 3.5) { this.msgs.push({ t, color, time }); if (this.msgs.length > 4) this.msgs.shift(); }
  award(b) {
    if (b < 0 || b >= BADGES.length || (this.prof.badges & (1 << b))) return;
    this.prof.badges |= 1 << b;
    this.hud('BADGE: ' + BADGES[b][0] + '!', '#ffd84a', 5);
    audio.play('badge', 0.8);
    this.save();
  }
  teleport(p, yaw) { this.body.copy(p); this.vel.set(0, 0, 0); this.rigYaw = yaw; this.touching = [false, false]; this.hist = []; }
  enterGame() {
    this.pstate = 'game'; this.aliveT = 0;
    this.teleport(this.world.spawnGame, 0);
    this.award(0);
    this.net.ev('ENTER');
    if (this.isHost() && this.phase === 0) this.startRound((Math.random() * 1e9) | 0);
    this.hud('Pegue as bananas douradas e fuja pela porta de aço!', '#ffe066', 6);
  }
  toLobby() { this.pstate = 'lobby'; this.teleport(this.world.spawnLobby, Math.PI); this.jsT = -1; }

  // ------------------------------------------------------------------ atualização
  update(dt, inp) {
    dt = clamp(dt, 0.001, 0.05);
    this.now += dt;
    for (const m of this.msgs) m.time -= dt;
    this.msgs = this.msgs.filter(m => m.time > 0);
    this.headQ.copy(qYaw(this.rigYaw)).multiply(inp.head);
    this.bodyYaw = this.rigYaw + yawOf(inp.head);
    this.rig.position.copy(this.body).add(V(0, 0.3, 0));
    this.rig.quaternion.copy(qYaw(this.rigYaw));
    this.rig.updateMatrixWorld(true);
    this.headPos.copy(inp.headPos).applyMatrix4(this.rig.matrixWorld);
    audio.setListener(this.headPos, this.headQ);

    if (this.pstate === 'caught') {
      this.jsT += dt;
      if (this.jsT > 2.6) this.fade = clamp((this.jsT - 2.6) / 0.8, 0, 1);
      if (this.jsT > 3.6) { this.toLobby(); this.fade = 1; }
    } else {
      this.fade = Math.max(0, this.fade - dt * 1.5);
      this.handleInput(inp);
      this.locomotion(inp, dt);
    }
    this.cam.update(inp, dt);
    if (this.pstate === 'lobby') this.lobby.update(inp, dt);

    if (this.pstate === 'lobby' && this.body.distanceTo(this.world.portal) < this.world.portalR + 0.3) this.enterGame();
    if (this.pstate === 'game') {
      const sz = this.world.inSafe(this.body);
      if (sz !== this.inSafeZone) {
        if (sz) { this.hud(`ZONA SEGURA (${sz.name}): os monstros não entram aqui. Sua coragem diminui aqui dentro!`, '#66ccff', 3.5); audio.play('badge', 0.25); }
        else if (this.inSafeZone) this.hud('Você saiu da zona segura!', '#ff9966', 2);
        this.inSafeZone = sz;
      }
      this.aliveT += dt;
      // CORAGEM: enche fora da zona segura (mais rápido com monstro por perto), esvazia dentro
      if (this.inSafeZone) this.courage = Math.max(0, this.courage - dt / 25);
      else this.courage = Math.min(1, this.courage + dt / 45 * (1 + 2.5 * this.chase));
      if (this.courage >= 1 && !this.courageMaxed) { this.courageMaxed = true; this.hud('CORAGEM MÁXIMA! Impulso turbinado e lanterna forte!', '#ffb02e', 4); audio.play('badge', 0.7); this.award(14); }
      if (this.courage < 0.8) this.courageMaxed = false;
      if (this.aliveT > 300) this.award(4);
      if (this.body.y > 3.3) this.award(11);
      this.roundBananas.forEach((bi, i) => {
        if (this.collected & (1 << i)) return;
        const bp = this.world.bananas[bi];
        if (this.handVis[0].distanceTo(bp) < 0.35 || this.handVis[1].distanceTo(bp) < 0.35 || this.body.distanceTo(bp) < 0.5) {
          if (this.isHost()) this.pickBanana(i, this.myId);
          else { this.net.ev('PICK', i); this.collected |= 1 << i; audio.play('banana', 0.9); }
        }
      });
      const ge = this.world.gateExit;
      if (this.gateOpen && Math.hypot(this.body.x - ge.x, this.body.z - ge.z) < 2.2) {
        this.prof.escapes++; this.award(3); if (this.prof.escapes >= 5) this.award(13);
        audio.play('escape'); this.hud('VOCÊ ESCAPOU DO DEPÓSITO!', '#66ff88', 5);
        this.net.ev('ESCAPED'); this.save(); this.toLobby();
      }
    }
    if (this.isHost()) this.hostUpdate(dt);
    this.netUpdate(dt);
    this.updateAudio(dt);
    this.gateAnim = clamp(this.gateAnim + (this.gateOpen ? dt * 0.35 : -dt), 0, 1);
    this.updateVisuals(dt);
  }

  handleInput(inp) {
    const horiz = Math.abs(inp.padX) > 0.45 && Math.abs(inp.padX) > Math.abs(inp.padY);
    if (inp.padClickDown && horiz) {
      const a = (this.prof.snap ? 45 : 30) * Math.PI / 180;
      this.rigYaw += inp.padX > 0 ? -a : a;
      audio.play('ui_click', 0.3);
    } else if (inp.padClickDown && this.cam.state === 0 && Math.abs(inp.padX) < 0.45 && Math.abs(inp.padY) < 0.45) {
      audio.play3D('hoot', this.headPos, 1, 4);
      this.net.ev('HOOT');
      this.makeNoise(this.body, 1.5);
    }
  }

  // ------------------------------------------------------------------ GORILLA LOCOMOTION (mão duplicada)
  locomotion(inp, dt) {
    const P = this.prof;
    this.ctrlHand = inp.leftHanded ? 0 : 1;
    const armQ = inp.ctrl.clone().multiply(qAxis(V(1, 0, 0), -P.armAngle * Math.PI / 180));
    const dW = V(0, 0, -1).applyQuaternion(armQ).applyQuaternion(qYaw(this.rigYaw));
    this.ctrlDir = V(0, 0, -1).applyQuaternion(inp.ctrl).applyQuaternion(qYaw(this.rigYaw));
    let dM = dW.clone().applyQuaternion(qYaw(-this.bodyYaw));
    // plano sagital: theta (frente/baixo/trás) + componente lateral limitada
    const side = this.ctrlHand === 1 ? 1 : -1;
    const x = clamp(dM.x * side, -0.35, 0.8) * side;
    const sp = Math.sqrt(Math.max(0, 1 - x * x));
    const theta = Math.atan2(dM.y, -dM.z);
    dM = V(x, Math.sin(theta) * sp, -Math.cos(theta) * sp);
    let dO;
    if (P.handMode === 0) dO = V(-dM.x, dM.y, dM.z);                     // ESPELHO: juntas, anda reto
    else {                                                                // ALTERNADA: oposta enquanto empurra
      const t2 = Math.max(clamp(2 * -1.45 - theta, -2.7, 0.6), -0.35);
      this.altBlend = lerp(this.altBlend, this.touching[this.ctrlHand] ? 1 : 0, 1 - Math.exp(-dt * 14));
      dO = V(-dM.x, dM.y, dM.z).lerp(V(-dM.x, Math.sin(t2) * sp, -Math.cos(t2) * sp), this.altBlend).normalize();
    }
    // mão subindo: as duas vão se juntando no centro
    this.lift = lerp(this.lift, smooth(-0.25, 0.65, dM.y), 1 - Math.exp(-dt * 12));
    const shX = 0.2 * (1 - 0.7 * this.lift);
    const dirs = []; dirs[this.ctrlHand] = dM; dirs[1 - this.ctrlHand] = dO;
    const fromBody = qYaw(this.bodyYaw);
    const delta = V(); let cnt = 0, firstContact = false;
    for (let i = 0; i < 2; i++) {
      const sx = i === 1 ? 1 : -1;
      const d = dirs[i].clone(); d.x -= sx * this.lift * 0.18; d.normalize();
      this.shoulder[i].copy(V(sx * shX, 0.14, 0).applyQuaternion(fromBody).add(this.body));
      const tgt = d.applyQuaternion(fromBody).multiplyScalar(ARM_LEN).add(this.shoulder[i]);
      const sd = tgt.clone().sub(this.shoulder[i]).normalize();
      // movimento da mão no referencial do corpo (frente = +f, cima = +y) desde o quadro anterior
      const loc = tgt.clone().sub(this.body).applyQuaternion(qYaw(-this.bodyYaw));
      const mv = loc.clone().sub(this.prevLoc[i]);
      this.prevLoc[i].copy(loc);
      if (this.touching[i]) {
        const h = this.world.raycast(this.shoulder[i], tgt.clone().addScaledVector(sd, 0.04));
        // PEGADA DE MÃO ÚNICA: só gruda enquanto empurra (para trás/baixo).
        // Levar a mão para a frente ou para cima solta (senão o corpo é puxado para trás/cima).
        const onFloor = this.anchorN[i].y > 0.5;
        const forward = -mv.z, up = mv.y;
        if (!h.hit || up > 0.004 || (onFloor && forward > 0.003)) this.touching[i] = false;
        else { delta.add(this.anchor[i].clone().sub(tgt)); cnt++; }
      } else {
        const h = this.world.raycast(this.shoulder[i], tgt);
        // só agarra se a mão estiver indo para trás/baixo (não ao trazer o braço de volta)
        if (h.hit && !(h.n.y > 0.5 && -mv.z > 0.006) && mv.y < 0.006) {
          this.touching[i] = true;
          this.anchor[i].copy(h.p).addScaledVector(h.n, 0.01);
          this.anchorN[i].copy(h.n);
          firstContact = true;
          const spd = tgt.distanceTo(this.prevT[i]) / dt;
          if (spd > 0.35) {
            audio.play3D(['tap_ground', 'tap_wood', 'tap_stone', 'tap_metal', 'tap_leaf'][h.mat % 5], this.anchor[i], clamp(spd * 0.25, 0.2, 1), 3);
            if (spd > 2) this.makeNoise(this.anchor[i], 1);
          }
          // correção da penetração inicial: pequena e fora do histórico de velocidade
          const corr = this.anchor[i].clone().sub(tgt);
          if (corr.length() > 0.05) corr.setLength(0.05);
          delta.add(corr); cnt++;
        }
      }
      this.target[i].copy(tgt); this.prevT[i].copy(tgt);
    }
    if (cnt > 0) {
      delta.divideScalar(cnt);
      if (delta.length() > 0.12) delta.setLength(0.12);   // no máx. ~8,6 m/s por quadro
      const before = this.body.clone();
      this.body.add(delta);
      this.world.resolveSphere(this.body, BODY_R);
      if (!firstContact) {
        this.hist.push(this.body.clone().sub(before).divideScalar(dt));
        if (this.hist.length > 6) this.hist.shift();
      }
      this.vel.set(0, 0, 0);
    } else {
      if (this.wasTouching && this.hist.length) {
        const avg = V(); for (const h of this.hist) avg.add(h); avg.divideScalar(this.hist.length);
        this.vel.copy(avg).multiplyScalar(BOOST_MUL[P.boost] * (1 + 0.35 * this.courage));
        // pulo para cima mais contido (não sai voando para o teto)
        this.vel.y = Math.min(this.vel.y * 0.7, 4.5);
        const s = this.vel.length();
        const vmax = BOOST_MAX[P.boost] + 2.5 * this.courage;
        if (s > vmax) this.vel.setLength(vmax);
        if (s > 3.5) audio.play3D('whoosh', this.body, clamp(s / 12, 0.1, 0.6), 2);
        this.hist = [];
      }
      this.vel.y -= GRAV * dt;
      this.body.addScaledVector(this.vel, dt);
      const n = this.world.resolveSphere(this.body, BODY_R);
      if (n.lengthSq() > 0) {
        n.normalize();
        const vn = this.vel.dot(n);
        if (vn < 0) this.vel.addScaledVector(n, -vn);
        if (n.y > 0.6) { const k = 1 - Math.min(1, dt * 1.5); this.vel.x *= k; this.vel.z *= k; }
      }
    }
    this.wasTouching = cnt > 0;
    for (let i = 0; i < 2; i++) this.handVis[i].copy(this.touching[i] ? this.anchor[i] : this.target[i]);
    this.speed = this.vel.length();
    if (this.speed > 10) this.award(12);
    if (this.body.y < -5) this.teleport(this.pstate === 'game' ? this.world.spawnGame : this.world.spawnLobby, this.rigYaw);
  }

  // ------------------------------------------------------------------ rodada (host)
  startRound(seed) {
    this.applyRound(seed, 0, 0, 1);
    for (const m of this.mons) {
      m.pos.copy(this.world.mspawn[['babuun', 'palido', 'mamae'][m.type]]);
      m.active = m.type === 0; m.state = 1; m.path = []; m.target = -1; m.goal = null;
    }
    this.net.ev('R', this.roundSeed, 0, 0, 1);
  }
  applyRound(seed, mask, gate, phase) {
    if (seed !== this.roundSeed || !this.roundBananas.length) {
      this.roundSeed = seed;
      let s = (seed | 1) >>> 0;
      const rnd = () => { s ^= s << 13; s >>>= 0; s ^= s >>> 17; s ^= s << 5; s >>>= 0; return s; };
      const idx = this.world.bananas.map((_, i) => i);
      for (let i = idx.length; i > 1; i--) { const j = rnd() % i; [idx[i - 1], idx[j]] = [idx[j], idx[i - 1]]; }
      this.roundBananas = idx.slice(0, Math.min(clamp(this.cfg.bananas || 8, 1, 24), idx.length));
    }
    const newBits = mask & ~this.collected;
    if (newBits && this.pstate === 'game') this.roundBananas.forEach((bi, i) => { if (newBits & (1 << i)) audio.play3D('banana', this.world.bananas[bi], 0.8, 4); });
    this.collected = mask;
    if (gate && !this.gateOpen) {
      audio.play3D('gate_open', this.world.gate, 1, 12);
      this.hud('A PORTA DE AÇO ABRIU! CORRA PARA A SAÍDA!', '#66ff88', 5);
      if (this.pstate === 'game') this.award(2);
    }
    this.gateOpen = !!gate;
    this.world.setEnabled('gate', !this.gateOpen);
    this.phase = phase;
    if (phase === 0) { this.roundBananas = []; this.collected = 0; this.gateOpen = false; this.world.setEnabled('gate', true); }
  }
  pickBanana(slot, by) {
    if (slot < 0 || slot >= this.roundBananas.length || (this.collected & (1 << slot))) return;
    const mask = this.collected | (1 << slot);
    let n = 0; for (let i = 0; i < 32; i++) if (mask & (1 << i)) n++;
    if (by === this.myId) this.award(1);
    this.applyRound(this.roundSeed, mask, n >= this.roundBananas.length ? 1 : 0, this.phase);
    audio.play('banana', 0.9);
    this.hud(`Bananas: ${n}/${this.roundBananas.length}`, '#ffe066', 2.5);
    const tot = this.roundBananas.length;
    if (n >= Math.ceil(tot * 0.25) && !this.mons[1].active) { this.mons[1].active = true; this.mons[1].state = 1; this.hud('Algo pálido acordou...', '#ccd', 3); }
    if (n >= Math.ceil(tot * 0.6) && !this.mons[2].active) { this.mons[2].active = true; this.mons[2].state = 1; this.hud('A MAMÃE ESTÁ VINDO.', '#ff5533', 3); audio.play3D('mamae_roar', this.mons[2].pos, 1, 20); }
    this.net.ev('R', this.roundSeed, this.collected, this.gateOpen ? 1 : 0, this.phase);
  }
  hostUpdate(dt) {
    let anyIn = this.pstate === 'game';
    for (const r of this.remotes.values()) if (r.state === 'F') anyIn = true;
    if (this.phase === 1 && !anyIn) { this.applyRound(this.roundSeed, 0, 0, 0); for (const m of this.mons) m.active = false; this.net.ev('R', 0, 0, 0, 0); }
    if (this.phase === 1) for (const m of this.mons) if (m.active) this.monsterAI(m, dt);
    this.roundT -= dt;
    if (this.roundT <= 0 && this.net.online) { this.roundT = 1; this.net.ev('R', this.roundSeed, this.collected, this.gateOpen ? 1 : 0, this.phase); }
  }
  makeNoise(p, loud) {
    if (this.pstate !== 'game') return;
    if (this.isHost()) this.hearNoise(p.x, p.z, loud); else this.net.ev('NOISE', p.x, p.z, loud);
  }
  hearNoise(x, z, loud) {
    for (const m of this.mons) {
      if (!m.active) continue;
      const hear = m.type === 2 ? 32 : m.type === 0 ? 15 : 0;
      if (Math.hypot(x - m.pos.x, z - m.pos.z) < hear * loud && (m.state === 1 || m.state === 3)) { m.state = 3; m.lastSeen.set(x, 0, z); m.path = []; }
    }
  }
  caught(mi, victim) {
    const m = this.mons[mi];
    m.state = 6; m.timer = 3; m.target = -1;
    if (victim === this.myId) this.startJumpscare(mi);
    this.net.ev('CAUGHT', victim, mi);
  }
  startJumpscare(mi) {
    if (this.pstate !== 'game') return;
    this.pstate = 'caught'; this.jsT = 0; this.jsType = mi;
    audio.play(['js_babuun', 'js_palido', 'js_mamae'][mi], 1);
    this.prof.catches++; this.award(5 + mi); this.courage = 0;
    this.hud('VOCÊ FOI PEGO ' + ['PELO BABUUN', 'PELO PÁLIDO', 'PELA MAMÃE'][mi] + '!', '#ff4040', 4);
    this.net.ev('DEAD');
  }

  // ------------------------------------------------------------------ IA dos monstros
  moveMonster(m, goal, speed, dt) {
    speed *= [0.8, 1, 1.25][this.cfg.diff ?? 1];
    m.pathT -= dt;
    if (!m.path.length || m.pathT <= 0) { m.path = this.world.findPath(m.pos, goal); m.pathT = 0.6; }
    let tgt = goal;
    while (m.path.length) {
      const p = m.path[0];
      if (hlen(p.clone().sub(m.pos)) < 0.8 && m.path.length > 1) m.path.shift(); else { tgt = p; break; }
    }
    if (this.world.los(m.pos.clone().setY(0.7), V(goal.x, 0.7, goal.z))) tgt = goal;
    const d = V(tgt.x - m.pos.x, 0, tgt.z - m.pos.z), dl = hlen(d);
    if (dl > 0.05) {
      d.divideScalar(dl);
      const np = m.pos.clone().addScaledVector(d, Math.min(speed * dt, dl));
      const rad = m.type === 2 ? 0.8 : 0.4;
      const probe = V(np.x, rad + 0.05, np.z);
      this.world.resolveSphere(probe, rad);
      if (!this.world.inSafe(probe, rad + 0.3)) m.pos.set(probe.x, m.pos.y, probe.z);
      else m.pathT = 0;   // bateu na barreira azul: recalcula o caminho
      m.yaw += wrap(Math.atan2(-d.x, -d.z) - m.yaw) * Math.min(1, dt * 8);
    }
    m.speed = speed;
    m.anim += dt * (1.5 + speed * 1.4);
    m.stuckT += dt;
    if (m.stuckT > 2.5) { if (hlen(m.pos.clone().sub(m.lastPos)) < 0.4 && m.state === 1) { m.goal = null; m.path = []; } m.lastPos.copy(m.pos); m.stuckT = 0; }
  }
  monsterAI(m, dt) {
    const ps = [];
    if (this.pstate === 'game' && !this.world.inSafe(this.body)) ps.push({ id: this.myId, body: this.body, head: this.headPos, q: this.headQ, hand: this.handVis[this.ctrlHand], dir: this.ctrlDir, range: 12 + 6 * this.courage });
    for (const r of this.remotes.values()) if (r.state === 'F' && !this.world.inSafe(r.body)) ps.push({ id: r.id, body: r.body, head: r.body.clone().add(V(0, 0.3, 0)), q: r.headQ, hand: r.handC || r.body, dir: r.dir, range: 12 + 6 * (r.courage || 0) });
    const sight = m.type === 2 ? 13 : 22;
    const eye = m.pos.clone().add(V(0, m.type === 1 ? 2.1 : m.type === 2 ? 1.9 : 1.0, 0));
    const fwd = V(-Math.sin(m.yaw), 0, -Math.cos(m.yaw));
    const sees = p => {
      const d = p.head.clone().sub(eye), dl = d.length();
      if (dl > sight) return false;
      if (dl > 3 && V(d.x, 0, d.z).normalize().dot(fwd) < 0.2) return false;
      return this.world.los(eye, p.head);
    };
    let nearest = null, nd = 1e9;
    for (const p of ps) { const d = p.body.distanceTo(m.pos); if (d < nd) { nd = d; nearest = p; } }
    m.jaw = lerp(m.jaw, m.state === 2 || m.state === 4 ? 0.8 : 0.15, dt * 4);
    const reach = m.type === 2 ? 1.9 : m.type === 1 ? 1.2 : 1.1, reachY = m.type === 2 ? 2.6 : m.type === 1 ? 2.8 : 1.7;
    if (m.state !== 6 && m.state !== 5) for (const p of ps) {
      const d = p.body.clone().sub(m.pos);
      if (hlen(d) < reach && d.y < reachY + m.pos.y) { this.caught(m.type, p.id); return; }
    }
    if (m.type === 1) {   // PÁLIDO: só anda quando ninguém olha
      let watched = false;
      for (const p of ps) {
        const d = m.pos.clone().add(V(0, 1.4, 0)).sub(p.head), dl = d.length();
        if (dl > 35) continue;
        if (V(0, 0, -1).applyQuaternion(p.q).dot(d.divideScalar(dl)) > 0.78 && this.world.los(p.head, m.pos.clone().add(V(0, 1.4, 0)))) { watched = true; break; }
        // a lanterna também congela o Pálido
        if (p.dir) {
          // cone da lanterna (~28°) contra pernas, tronco e cabeça do Pálido
          for (const hy of [0.5, 1.3, 2.1]) {
            const tgt = m.pos.clone().add(V(0, hy, 0)), ld = tgt.clone().sub(p.hand), ll = ld.length();
            if (ll < p.range && p.dir.dot(ld.divideScalar(ll)) > 0.88 && this.world.los(p.hand, tgt)) { watched = true; break; }
          }
          if (watched) break;
        }
      }
      if (m.state === 6) { m.timer -= dt; if (m.timer <= 0) m.state = 1; return; }
      if (watched) { m.state = 5; m.speed = 0; return; }
      if (nearest) {
        m.state = 2; this.moveMonster(m, nearest.body, nd < 8 ? 7.5 : 5, dt);
        const d = nearest.body.clone().sub(m.pos); m.yaw = Math.atan2(-d.x, -d.z);
      } else {
        m.state = 1;
        if (!m.goal || hlen(m.goal.clone().sub(m.pos)) < 1.5) m.goal = this.world.patrol[Math.random() * this.world.patrol.length | 0];
        this.moveMonster(m, m.goal, 2, dt);
      }
      return;
    }
    if (m.state === 4) {  // salto do Babuun
      m.leapT += dt / 0.65;
      const t = clamp(m.leapT, 0, 1);
      m.pos.copy(m.leapFrom).lerp(m.leapTo, t);
      m.pos.y = m.leapFrom.y + (m.leapTo.y - m.leapFrom.y) * t + Math.sin(t * Math.PI) * 1.6;
      m.anim += dt * 2;
      if (t >= 1) { m.state = 2; m.pos.y = 0; }
      return;
    }
    if (m.state === 6) { m.timer -= dt; m.speed = 0; if (m.timer <= 0) { m.state = 1; m.goal = null; } return; }
    const seen = ps.find(sees);
    if (seen) {
      if (m.state !== 2) { audio.play3D(m.type === 0 ? 'babuun_screech' : 'mamae_roar', eye, 1, 8); m.timer = m.type === 2 ? 4.5 : 0; }
      m.state = 2; m.target = seen.id; m.lastSeen.copy(seen.body); m.stuckT = 0;
    }
    if (m.state === 2) {
      const tp = ps.find(p => p.id === m.target);
      if (!tp) { m.state = 1; return; }
      if (!seen) { m.timer += dt; if (m.timer > 4 && m.type === 0) { m.state = 3; m.timer = 0; } } else if (m.type === 0) m.timer = 0;
      const d = tp.body.clone().sub(m.pos);
      if (m.type === 0) {
        if (hlen(d) < 4 && d.y > 1.2 && d.y < 4.2 && this.world.los(eye, tp.head)) {
          m.state = 4; m.leapT = 0; m.leapFrom.copy(m.pos); m.leapTo.copy(tp.body).add(V(0, -0.3, 0));
          audio.play3D('babuun_bark', eye, 1, 6); return;
        }
        this.moveMonster(m, m.lastSeen, 6.3, dt);
      } else {
        m.timer -= dt; this.moveMonster(m, m.lastSeen, 7.2, dt);
        if (m.timer <= 0) { m.state = 6; m.timer = 2.5; }
      }
      return;
    }
    if (m.state === 3) {
      this.moveMonster(m, m.lastSeen, m.type === 2 ? 5 : 3.6, dt);
      if (hlen(m.lastSeen.clone().sub(m.pos)) < 1.2) { m.state = 1; m.goal = null; }
      return;
    }
    m.state = 1;
    if (!m.goal || hlen(m.goal.clone().sub(m.pos)) < 1.5) m.goal = this.world.patrol[Math.random() * this.world.patrol.length | 0];
    this.moveMonster(m, m.goal, m.type === 2 ? 1.8 : 2.4, dt);
  }

  // ------------------------------------------------------------------ rede
  netUpdate(dt) {
    if (!this.net.online) {
      if (this.remotes.size) { for (const r of this.remotes.values()) r.av.dispose(); this.remotes.clear(); }
      return;
    }
    this.sendT -= dt;
    if (this.sendT <= 0) {
      this.sendT = 1 / 15;
      const P = this.prof, b = this.body, q = this.headQ, h = this.handVis;
      const r2 = v => Math.round(v * 100) / 100;
      this.net.send({ t: 'st', s: [r2(b.x), r2(b.y), r2(b.z), q.x, q.y, q.z, q.w, r2(h[0].x), r2(h[0].y), r2(h[0].z), r2(h[1].x), r2(h[1].y), r2(h[1].z), r2(this.bodyYaw)],
                      c: [P.r, P.g, P.b, P.hat, P.face, P.neck, P.acc, P.title], n: P.name,
                      d: [+this.ctrlDir.x.toFixed(2), +this.ctrlDir.y.toFixed(2), +this.ctrlDir.z.toFixed(2), this.ctrlHand, +this.courage.toFixed(2)], st: this.pstate === 'game' ? 'F' : this.pstate === 'caught' ? 'D' : 'L' });
    }
    if (this.isHost()) {
      this.monT -= dt;
      if (this.monT <= 0) {
        this.monT = 0.1;
        this.net.ev('M', this.mons.map(m => [m.active ? 1 : 0, +m.pos.x.toFixed(2), +m.pos.y.toFixed(2), +m.pos.z.toFixed(2), +m.yaw.toFixed(3), m.state, +m.jaw.toFixed(2)]));
      }
    } else {
      for (const m of this.mons) {
        if (!m.netPos) continue;
        const k = 1 - Math.exp(-dt * 10), old = m.pos.clone();
        m.pos.lerp(m.netPos, k); m.yaw += wrap(m.netYaw - m.yaw) * k;
        m.speed = m.pos.distanceTo(old) / dt; m.anim += dt * (1.5 + m.speed * 1.4);
      }
    }
    for (const r of this.remotes.values()) {
      const k = 1 - Math.exp(-dt * 12);
      r.body.lerp(r.tBody, k); r.lh.lerp(r.tLh, k); r.rh.lerp(r.tRh, k); r.headQ.slerp(r.tHeadQ, k); r.yaw += wrap(r.tYaw - r.yaw) * k;
    }
    if (this.remotes.size) this.award(8);
  }
  remote(id, create) {
    let r = this.remotes.get(id);
    if (!r && create) {
      r = { id, name: '?', body: V(), lh: V(), rh: V(), headQ: new THREE.Quaternion(), yaw: 0, tBody: V(), tLh: V(), tRh: V(), tHeadQ: new THREE.Quaternion(), tYaw: 0,
            prof: { r: 7, g: 4, b: 2, hat: 0, face: 0, neck: 0, acc: 0, title: -1 }, state: 'L', fresh: true };
      r.av = new Avatar(this.scene, r.prof, true);
      r.tag = this.lobby.makeTag();
      this.scene.add(r.tag.mesh);
      this.remotes.set(id, r);
    }
    return r;
  }
  dropRemote(id) { const r = this.remotes.get(id); if (r) { r.av.dispose(); if (r.beam) r.beam.removeFromParent(); r.tag.mesh.removeFromParent(); this.remotes.delete(id); } }
  onNet(m) {
    switch (m.t) {
      case 'welcome': this.hud(`Conectado em '${m.cfg.name}'`, '#88ff99', 3); break;
      case 'join': { const r = this.remote(m.id, true); r.name = m.name; this.hud(m.name + ' entrou', '#bde', 2.5); break; }
      case 'leave': { const r = this.remotes.get(m.id); if (r) this.hud(r.name + ' saiu', '#aaa', 2); this.dropRemote(m.id); break; }
      case 'migrating': this.hud('O host saiu! Trocando de servidor...', '#ffd84a', 3);
        for (const id of [...this.remotes.keys()]) this.dropRemote(id); break;
      case 'became-host': this.hud('Agora VOCÊ é o host desta sala!', '#88ff99', 4); break;
      case 'err': this.hud('Servidor: ' + m.msg, '#ff7755', 4); audio.play('ui_error'); break;
      case 'st': {
        const r = this.remote(m.id, true), s = m.s;
        r.tBody.set(s[0], s[1], s[2]); r.tHeadQ.set(s[3], s[4], s[5], s[6]); r.tLh.set(s[7], s[8], s[9]); r.tRh.set(s[10], s[11], s[12]); r.tYaw = s[13];
        [r.prof.r, r.prof.g, r.prof.b, r.prof.hat, r.prof.face, r.prof.neck, r.prof.acc, r.prof.title] = m.c;
        r.name = m.n; r.state = m.st;
        if (m.d) { r.dir = (r.dir || V()).set(m.d[0], m.d[1], m.d[2]); r.hand = m.d[3]; r.courage = m.d[4]; }
        if (r.fresh) { r.body.copy(r.tBody); r.lh.copy(r.tLh); r.rh.copy(r.tRh); r.headQ.copy(r.tHeadQ); r.yaw = r.tYaw; r.fresh = false; }
        break;
      }
      case 'ev': this.onEvent(m.id, m.ev); break;
    }
  }
  onEvent(from, ev) {
    const [e, ...a] = ev;
    if (e === 'M' && !this.isHost()) {
      a[0].forEach((d, i) => {
        const m = this.mons[i];
        const np = V(d[1], d[2], d[3]);
        if (!m.active && d[0]) m.pos.copy(np);
        m.active = !!d[0]; m.netPos = np; m.netYaw = d[4]; m.state = d[5]; m.jaw = d[6];
      });
    } else if (e === 'R' && !this.isHost()) this.applyRound(a[0], a[1], a[2], a[3]);
    else if (e === 'PICK' && this.isHost()) this.pickBanana(a[0], from);
    else if (e === 'NOISE' && this.isHost()) this.hearNoise(a[0], a[1], a[2]);
    else if (e === 'CAUGHT') {
      if (a[0] === this.myId) this.startJumpscare(a[1]);
      else { audio.play3D(['babuun_screech', 'palido_shriek', 'mamae_roar'][a[1]], this.mons[a[1]].pos, 0.9, 8); const r = this.remotes.get(a[0]); if (r) this.hud(r.name + ' foi pego!', '#ff8866', 2.5); }
    } else if (e === 'HOOT') { const r = this.remotes.get(from); if (r) audio.play3D('hoot', r.body.clone().add(V(0, 0.3, 0)), 1, 4); }
    else if (e === 'ESCAPED') { const r = this.remotes.get(from); if (r) this.hud(r.name + ' escapou!', '#88ff99', 2.5); }
  }
  onAway() { if (this.net.online && this.isHost()) this.net.send({ t: 'away' }); }

  // ------------------------------------------------------------------ áudio ambiente
  updateAudio(dt) {
    audio.setVolume(this.prof.volume / 10);
    const inGame = this.pstate !== 'lobby';
    const mode = inGame ? 1 : 0;
    if (mode !== this.ambMode) {
      if (this.ambV) audio.stop(this.ambV);
      this.ambV = audio.play(inGame ? 'amb_night' : 'amb_lobby', inGame ? 0.45 : 0.35, true);
      this.ambMode = mode;
    }
    let danger = 0;
    if (this.pstate === 'game' && !this.inSafeZone) for (const m of this.mons) {
      if (!m.active) continue;
      const d = m.pos.distanceTo(this.body);
      const chasing = m.state === 2 || m.state === 4 || (m.type === 1 && m.state !== 5 && d < 15);
      danger = Math.max(danger, chasing ? clamp(1.3 - d / 25, 0, 1) : clamp(0.6 - d / 30, 0, 0.6));
    }
    this.chase = lerp(this.chase, danger, 1 - Math.exp(-dt * 2));
    if (this.chase > 0.05 && !this.chaseV) this.chaseV = audio.play('chase', 0, true);
    if (this.chaseV) { audio.setGain(this.chaseV, clamp(this.chase - 0.3, 0, 1) * 0.8); if (this.chase < 0.03) { audio.stop(this.chaseV); this.chaseV = 0; } }
    if (this.chase > 0.25 && !this.heartV) this.heartV = audio.play('heartbeat', 0, true);
    if (this.heartV) { audio.setGain(this.heartV, clamp(this.chase, 0, 1) * 0.7); if (this.chase < 0.2) { audio.stop(this.heartV); this.heartV = 0; } }
    this.mons.forEach((m, i) => {
      m.voiceT -= dt;
      if (!m.active || !inGame) return;
      if (m.voiceT <= 0) {
        const h = m.state === 2;
        audio.play3D(h ? ['babuun_bark', 'palido_breath', 'mamae_roar'][i] : ['babuun_growl', 'palido_breath', 'mamae_roar'][i], m.pos.clone().add(V(0, 1, 0)), i === 2 ? 1 : 0.8, i === 2 ? 6 : 3);
        m.voiceT = h ? 2 + i : 5 + Math.random() * 4;
      }
      if (Math.floor(m.anim / Math.PI) !== Math.floor(m.lastPh / Math.PI) && m.speed > 0.5) audio.play3D(i === 2 ? 'step_heavy' : 'step_light', m.pos, i === 2 ? 0.9 : 0.5, i === 2 ? 5 : 2.5);
      m.lastPh = m.anim;
    });
  }

  // ------------------------------------------------------------------ visual
  updateVisuals(dt) {
    const inLobby = this.body.x < -150;
    // luz e névoa
    this.scene.fog.density = inLobby ? 0.02 : 0.055;
    this.scene.fog.color.setRGB(inLobby ? 0.2 : 0.05, inLobby ? 0.2 : 0.05, inLobby ? 0.22 : 0.06);
    this.scene.background = this.scene.fog.color;
    this.hemi.intensity = inLobby ? 1.8 : 0.7;
    const ls = this.world.lights.map((l, i) => [l.distanceToSquared(this.body), i]).sort((a, b) => a[0] - b[0]);
    this.plights.forEach((pl, k) => {
      const i = ls[k][1];
      pl.position.copy(this.world.lights[i]).add(V(0, -0.3, 0));
      const flick = !inLobby && i % 7 === 3 && Math.sin(this.now * 13 + i) * Math.sin(this.now * 30 + i) > 0.3 ? 0.15 : 1;
      pl.intensity = (inLobby ? 5 : 6) * flick;
    });
    this.safeFx.update(this.now);
    // lanterna (só no depósito)
    const torchOn = this.pstate === 'game';
    this.torchModel.visible = torchOn;
    this.torch.intensity = torchOn ? 22 * (1 + 0.7 * this.courage) : 0;
    this.torch.distance = 12 + 6 * this.courage;
    if (torchOn) {
      const tp = this.handVis[this.ctrlHand].clone().addScaledVector(this.ctrlDir, 0.1).add(V(0, 0.04, 0));
      this.torch.position.copy(tp);
      this.torch.target.position.copy(tp).addScaledVector(this.ctrlDir, 5);
      this.torchModel.position.copy(tp);
      this.torchModel.quaternion.setFromUnitVectors(V(0, 0, -1), this.ctrlDir);
    }
    // portão
    this.gateModel.position.copy(this.world.gate).add(V(0, this.gateAnim * 3.9, 0));
    // bananas
    this.bananaModels.forEach((b, i) => {
      const on = this.phase === 1 && i < this.roundBananas.length && !(this.collected & (1 << i));
      b.visible = on;
      if (on) {
        b.position.copy(this.world.bananas[this.roundBananas[i]]).add(V(0, Math.sin(this.now * 2 + i) * 0.06, 0));
        b.quaternion.copy(qYaw(this.now * 1.5 + i)).multiply(qAxis(V(0, 0, 1), 0.5));
      }
    });
    // monstros + animação procedural
    this.mons.forEach((m, i) => {
      const mdl = this.monModels[i];
      mdl.visible = m.active && this.phase === 1 && !(this.pstate === 'caught' && this.jsT < 2.7 && this.jsType === i);
      if (!mdl.visible) return;
      const n = mdl.userData.nodes, ph = m.anim, amp = m.state === 5 ? 0 : clamp(m.speed / 4, 0.15, 0.8);
      const R = (k, q) => { if (n[k]) n[k].quaternion.copy(n[k].userData.baseQ).multiply(q); };
      const X = a => qAxis(V(1, 0, 0), a), Y = a => qAxis(V(0, 1, 0), a), Z = a => qAxis(V(0, 0, 1), a);
      if (i !== 1) {
        R('leg_fr', X(Math.sin(ph) * amp)); R('leg_bl', X(Math.sin(ph) * amp)); R('leg_fl', X(-Math.sin(ph) * amp)); R('leg_br', X(-Math.sin(ph) * amp));
        R('tail', Y(Math.sin(ph * 0.7) * 0.5)); R('head', X(Math.sin(ph * 2) * 0.08 + (m.state === 2 ? -0.2 : 0.1)));
        R('jaw', X(m.jaw * 0.6 + Math.sin(this.now * 9) * 0.05 * m.jaw));
      } else {
        R('leg_r', X(Math.sin(ph) * amp * 0.7)); R('leg_l', X(-Math.sin(ph) * amp * 0.7));
        R('arm_r', X(-Math.sin(ph) * amp * 0.5).multiply(Z(0.1))); R('arm_l', X(Math.sin(ph) * amp * 0.5).multiply(Z(-0.1)));
        R('head', Z(Math.sin(this.now * 0.7) * 0.35).multiply(X(0.25))); R('jaw', X(m.jaw * 0.5));
      }
      mdl.position.copy(m.pos).add(V(0, i !== 1 ? Math.abs(Math.sin(ph)) * 0.05 * amp : 0, 0));
      mdl.quaternion.copy(qYaw(m.yaw));
    });
    // jogador local e remotos
    this.me.setVisible(this.pstate !== 'caught');
    this.me.apply(this.prof);
    this.me.set(this.body, this.bodyYaw, this.headQ, this.handVis[0], this.handVis[1]);
    for (const r of this.remotes.values()) {
      r.av.setVisible(r.state !== 'D');
      r.av.apply(r.prof);
      r.av.set(r.body, r.yaw, r.headQ, r.lh, r.rh);
      // feixe de luz da lanterna dos outros jogadores (cone translúcido, sem custo de luz real)
      if (!r.beam) {
        r.beam = new THREE.Mesh(new THREE.ConeGeometry(1.4, 6, 20, 1, true).translate(0, -3, 0).rotateX(-Math.PI / 2),
          new THREE.MeshBasicMaterial({ color: 0xfff0c0, transparent: true, opacity: 0.08, blending: THREE.AdditiveBlending, depthWrite: false, side: THREE.DoubleSide }));
        this.scene.add(r.beam);
      }
      r.beam.visible = r.state === 'F' && !!r.dir;
      if (r.beam.visible) {
        r.handC = (r.hand === 0 ? r.lh : r.rh).clone();
        r.beam.position.copy(r.handC);
        r.beam.quaternion.setFromUnitVectors(V(0, 0, -1), r.dir.clone().normalize());
        r.beam.material.opacity = 0.06 + 0.06 * (r.courage || 0);
      }
      const tag = (r.prof.title >= 0 && BADGES[r.prof.title] ? `[${BADGES[r.prof.title][0]}] ` : '') + r.name;
      r.tag.draw(tag, (c, W, H, p) => p.text(0, 0, tag, 0.07, '#fff'));
      r.tag.mesh.position.copy(r.body).add(V(0, 0.75, 0));
      r.tag.mesh.lookAt(this.headPos);
      r.tag.mesh.visible = r.state !== 'D';
    }
    this.lobby.draw(dt);
    this.drawHud(dt);
  }

  drawHud(dt) {
    if (!this.hudPanel) {
      this.hudPanel = new (this.lobby.Panel)(1.4, 0.5, 768, null);
      this.hudPanel.mesh.position.set(0, -0.28, -1.2);
      this.hudPanel.mesh.material.depthTest = false; this.hudPanel.mesh.renderOrder = 999;
      this.hudGroup.add(this.hudPanel.mesh);
      // jumpscare: monstro preso à cabeça + flash vermelho
      this.jsModels = ['babuun', 'palido', 'mamae'].map(n => { const m = model(n); m.visible = false; this.hudGroup.add(m); return m; });
      this.flash = new THREE.Mesh(new THREE.PlaneGeometry(3, 3), new THREE.MeshBasicMaterial({ color: 0x000000, transparent: true, opacity: 0, depthTest: false, fog: false }));
      this.flash.position.z = -0.25; this.flash.renderOrder = 1000;
      this.hudGroup.add(this.flash);
    }
    let n = 0; for (let i = 0; i < 32; i++) if (this.collected & (1 << i)) n++;
    const rec = this.cam.recording ? Math.floor(this.cam.recElapsed()) : -1;
    const top = this.pstate === 'game' && this.phase === 1 ? `Bananas ${n}/${this.roundBananas.length}${this.gateOpen ? '  - SAÍDA ABERTA!' : ''}` : '';
    const key = (this.pstate === 'game' ? Math.round(this.courage * 40) + ':' + !!this.inSafeZone + ':' : '') + this.msgs.map(m => m.t + (m.time < 1 ? (m.time * 4 | 0) : '')).join('|') + top + rec + (rec >= 0 ? (this.now * 2 | 0) % 2 : '');
    this.hudPanel.draw(key, (c, W, H, p) => {
      if (top) p.text(0, 0.21, top, 0.045, '#ffe066');
      if (this.pstate === 'game') {
        const [bx, by] = p.px(-0.3, -0.21), [ex] = p.px(0.3, 0), bw = ex - bx, bh = 14;
        c.fillStyle = 'rgba(0,0,0,0.55)'; c.fillRect(bx - 2, by - bh / 2 - 2, bw + 4, bh + 4);
        const cc = this.courage;
        c.fillStyle = cc >= 1 ? '#ffb02e' : `rgb(${80 + 175 * cc | 0},${120 + 60 * cc | 0},60)`;
        c.fillRect(bx, by - bh / 2, bw * cc, bh);
        p.text(0, -0.165, this.inSafeZone ? 'CORAGEM (diminuindo na zona segura)' : 'CORAGEM', 0.03, cc >= 1 ? '#ffb02e' : '#ddd');
      }
      if (rec >= 0) p.text(0.5, 0.21, `${(this.now * 2 | 0) % 2 ? '●' : ' '} REC ${String(rec / 60 | 0).padStart(2, '0')}:${String(rec % 60).padStart(2, '0')}`, 0.04, '#ff5050');
      this.msgs.forEach((m, i) => { c.globalAlpha = clamp(m.time, 0, 1); p.text(0, 0.12 - i * 0.08, m.t, 0.04, m.color, 'center', 1.35); c.globalAlpha = 1; });
    });
    // jumpscare
    this.jsModels.forEach((m, i) => { m.visible = this.pstate === 'caught' && this.jsT < 2.7 && this.jsType === i; });
    if (this.pstate === 'caught' && this.jsT < 2.7) {
      const t = this.jsT, i = this.jsType, m = this.jsModels[i];
      const headH = [1.0, 2.15, 2.0][i], sc = i === 2 ? 0.55 : 1;
      const dist = lerp(1.6, 0.38, smooth(0, 0.22, t)) * sc + (i === 2 ? 0.35 : 0);
      m.position.set((Math.random() - 0.5) * 0.06, -headH * sc + 0.05 + (Math.random() - 0.5) * 0.06, -dist);
      m.rotation.set(0, Math.PI, 0); m.scale.setScalar(sc);
      const nn = m.userData.nodes;
      if (nn.jaw) nn.jaw.quaternion.copy(nn.jaw.userData.baseQ).multiply(qAxis(V(1, 0, 0), 0.9 + Math.sin(t * 40) * 0.1));
      if (nn.head) nn.head.quaternion.copy(nn.head.userData.baseQ).multiply(qAxis(V(0, 0, 1), Math.sin(t * 31) * 0.15));
      this.flash.material.color.setRGB(0.8, 0, 0);
      this.flash.material.opacity = (0.25 + 0.25 * Math.sin(t * 30)) * clamp(1 - t / 2.7, 0, 1);
    } else {
      this.flash.material.color.setRGB(0, 0, 0);
      this.flash.material.opacity = this.fade;
    }
  }
}

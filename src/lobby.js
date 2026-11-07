// Lobby: estação de cores RGB, guarda-roupa, badges, servidores, ajustes, como jogar, suporte da câmera
import * as THREE from 'three';
import { model } from './assets.js';
import { audio } from './audio.js';
import { Panel, Button } from './ui.js';
import { Avatar, BADGES, randomName } from './game.js';

const V = (x = 0, y = 0, z = 0) => new THREE.Vector3(x, y, z);
const C = (r, g, b) => new THREE.Color(r, g, b);
const HAT_N = ['Nenhum', 'Boné', 'Gorro', 'Cartola', 'Coroa', 'Coroa de Folhas', 'Chifrinhos', 'Fones'];
const FACE_N = ['Nenhum', 'Óculos', 'Óculos Escuros'];
const NECK_N = ['Nenhuma', 'Gravata Borboleta'];
const ACC_N = ['Vermelho', 'Azul', 'Verde', 'Amarelo', 'Roxo', 'Branco', 'Preto', 'Laranja'];
const DIFF_N = ['Fácil', 'Normal', 'Pesadelo'];
const SNAMES = ['Depósito Assombrado', 'Galera do Go', 'Só Amigos', 'Caça aos Babuuns', 'Bananal', 'Noite do Terror'];

export class Lobby {
  constructor(game) {
    this.g = game; this.Panel = Panel;
    this.buttons = []; this.boards = {}; this.nameIdx = 0; this.lastScan = -99;
    const st = game.world.st;
    const board = (key, w, h, title) => {
      const grp = new THREE.Group();
      grp.position.copy(st[key].c); grp.rotation.y = st[key].yaw;
      const frame = model('board'); frame.rotation.y = Math.PI; frame.scale.set(w, h, 1); grp.add(frame);
      const p = new Panel(w, h, Math.round(380 * w));
      p.mesh.position.z = 0.025; grp.add(p.mesh);
      p.title = title;
      game.scene.add(grp);
      this.boards[key] = { grp, p, w, h };
      return grp;
    };
    const P = game.prof, save = () => game.save();
    const btn = (grp, x, y, l, fn, col, r) => { const b = new Button(grp, x, y, l, fn, col, r); this.buttons.push(b); return b; };

    // ---- COR DO MACACO (cores customizadas dentro do VR)
    let g = board('colorStation', 2.4, 1.3, 'COR DO MACACO');
    ['r', 'g', 'b'].forEach((k, i) => {
      const col = [C(1, 0.3, 0.3), C(0.3, 1, 0.3), C(0.35, 0.5, 1)][i];
      btn(g, -0.95, 0.3 - i * 0.3, '-', () => { P[k] = Math.max(0, P[k] - 1); game.award(9); save(); }, col);
      btn(g, -0.25, 0.3 - i * 0.3, '+', () => { P[k] = Math.min(9, P[k] + 1); game.award(9); save(); }, col);
    });
    const PRE = [[7, 4, 2], [2, 2, 2], [9, 9, 9], [9, 2, 2], [2, 4, 9], [3, 8, 3], [9, 8, 1], [7, 2, 8]];
    PRE.forEach((c, i) => btn(g, 0.25 + (i % 4) * 0.22, 0.25 - (i / 4 | 0) * 0.25, '', () => { [P.r, P.g, P.b] = c; save(); }, C(c[0] / 9, c[1] / 9, c[2] / 9), 0.06));
    btn(g, 0.58, -0.35, 'ALEATÓRIA', () => { P.r = Math.random() * 10 | 0; P.g = Math.random() * 10 | 0; P.b = Math.random() * 10 | 0; game.award(9); save(); }, C(1, 1, 1));

    // ---- GUARDA-ROUPA
    g = board('wardrobe', 2.3, 1.3, 'GUARDA-ROUPA');
    const cyc = (k, n) => [() => { P[k] = (P[k] + n - 1) % n; game.award(9); save(); }, () => { P[k] = (P[k] + 1) % n; game.award(9); save(); }];
    [['hat', 8], ['face', 3], ['neck', 2], ['acc', 8]].forEach(([k, n], i) => {
      const [a, b] = cyc(k, n);
      btn(g, -0.9, 0.35 - i * 0.25, '<', a); btn(g, 0.9, 0.35 - i * 0.25, '>', b);
    });

    // ---- BADGES (toque para usar como título)
    g = board('badgeBoard', 2.6, 1.75, 'BADGES');
    BADGES.forEach((b, i) => btn(g, i % 2 ? 0.1 : -1.05, 0.6 - (i / 2 | 0) * 0.2, '', () => {
      if (!(P.badges & (1 << i))) { audio.play('ui_error', 0.5); return; }
      P.title = P.title === i ? -1 : i;
      game.hud(P.title >= 0 ? 'Título: ' + b[0] : 'Título removido', '#ffd84a', 2); save();
    }, C(1, 0.8, 0.2), 0.045));

    // ---- SERVIDORES (o seu óculos vira o servidor)
    g = board('netBoard', 3.5, 2.05, 'SALAS (O SEU GO É O SERVIDOR)');
    const cfg = game.createCfg;
    const setName = () => { cfg.name = this.nameIdx === 0 ? 'Toca do ' + P.name : SNAMES[(this.nameIdx - 1) % 6]; };
    btn(g, -1.6, 0.62, '<', () => { this.nameIdx = (this.nameIdx + 6) % 7; setName(); });
    btn(g, -0.55, 0.62, '>', () => { this.nameIdx = (this.nameIdx + 1) % 7; setName(); });
    btn(g, -1.4, 0.4, 'PÚBLICA', () => { cfg.isPublic = true; }, C(0.4, 1, 0.5), 0.06);
    btn(g, -0.8, 0.4, 'PRIVADA', () => { cfg.isPublic = false; }, C(1, 0.5, 0.3), 0.06);
    btn(g, -1.6, 0.18, '<', () => { cfg.maxPlayers = Math.max(2, cfg.maxPlayers - 1); });
    btn(g, -0.55, 0.18, '>', () => { cfg.maxPlayers = Math.min(10, cfg.maxPlayers + 1); });
    btn(g, -1.6, -0.04, '<', () => { cfg.diff = (cfg.diff + 2) % 3; });
    btn(g, -0.55, -0.04, '>', () => { cfg.diff = (cfg.diff + 1) % 3; });
    btn(g, -1.6, -0.26, '<', () => { cfg.bananas = cfg.bananas <= 4 ? 12 : cfg.bananas - 4; });
    btn(g, -0.55, -0.26, '>', () => { cfg.bananas = cfg.bananas >= 12 ? 4 : cfg.bananas + 4; });
    btn(g, -1.1, -0.52, 'CRIAR / FECHAR', () => this.toggleHost(), C(0.3, 1, 0.4), 0.07);
    for (let k = 1; k <= 9; k++) btn(g, -0.25 + ((k - 1) % 3) * 0.25, 0.38 - ((k - 1) / 3 | 0) * 0.22, String(k), () => { if (game.codeEntry.length < 6) game.codeEntry += k; }, C(0.8, 0.8, 0.9), 0.055);
    btn(g, 0, -0.28, '0', () => { if (game.codeEntry.length < 6) game.codeEntry += '0'; }, C(0.8, 0.8, 0.9), 0.055);
    btn(g, -0.25, -0.28, '<-', () => { game.codeEntry = game.codeEntry.slice(0, -1); }, C(1, 0.6, 0.4), 0.055);
    btn(g, 0.25, -0.28, 'X', () => { game.codeEntry = ''; }, C(1, 0.4, 0.4), 0.055);
    btn(g, 0, -0.52, 'ENTRAR COM CÓDIGO', () => this.joinCode(), C(1, 0.85, 0.3), 0.07);
    this.listBtns = [];
    for (let i = 0; i < 5; i++) this.listBtns.push(btn(g, 1.55, 0.4 - i * 0.2, '', () => this.joinList(i), C(0.4, 1, 0.5), 0.055));
    btn(g, -0.6, -0.88, 'SAIR DA SALA', () => { game.net.leave(); game.hud('Você saiu da sala (offline)', '#ffb366', 2.5); }, C(1, 0.4, 0.3), 0.06);
    btn(g, 0.6, -0.88, 'ATUALIZAR LISTA', () => { this.lastScan = -99; this.scanTick(true); }, C(0.6, 0.7, 1), 0.06);

    // ---- AJUSTES
    g = board('modeBoard', 2.0, 1.75, 'AJUSTES');
    btn(g, -0.55, 0.35, 'ESPELHO', () => { P.handMode = 0; save(); }, C(0.5, 0.8, 1));
    btn(g, 0.45, 0.35, 'ALTERNADA', () => { P.handMode = 1; save(); }, C(0.5, 0.8, 1));
    btn(g, -0.6, 0.08, 'NORMAL', () => { P.boost = 0; save(); }, C(0.6, 1, 0.6));
    btn(g, 0, 0.08, 'FORTE', () => { P.boost = 1; save(); }, C(1, 0.8, 0.3));
    btn(g, 0.6, 0.08, 'TURBO', () => { P.boost = 2; save(); }, C(1, 0.35, 0.3));
    btn(g, -0.6, -0.19, '-', () => { P.armAngle = Math.max(0, P.armAngle - 5); save(); });
    btn(g, 0.6, -0.19, '+', () => { P.armAngle = Math.min(60, P.armAngle + 5); save(); });
    btn(g, -0.6, -0.46, '-', () => { P.volume = Math.max(0, P.volume - 1); save(); });
    btn(g, 0.6, -0.46, '+', () => { P.volume = Math.min(10, P.volume + 1); save(); });
    btn(g, -0.45, -0.68, 'GIRO 30/45', () => { P.snap ^= 1; save(); });
    btn(g, 0.45, -0.68, 'NOVO NOME', () => { P.name = randomName(); setName(); save(); game.hud('Nome: ' + P.name, '#ffff99', 3); });

    // ---- COMO JOGAR
    board('howTo', 2.6, 1.75, 'COMO JOGAR / CORRER');

    // ---- SUPORTE DA CÂMERA
    const cr = new THREE.Group(); cr.position.copy(st.cameraRack.c); cr.rotation.y = st.cameraRack.yaw; game.scene.add(cr);
    const stick = model('stick'); stick.position.set(0, -1.2, 0.3); stick.scale.set(2, 1, 2); cr.add(stick);
    this.rackCam = model('camera'); this.rackCam.position.set(0, -0.12, 0.3); this.rackCam.rotation.y = Math.PI; cr.add(this.rackCam);
    btn(cr, 0, 0, 'PEGAR CÂMERA', () => { game.cam.toggle(); }, C(1, 0.3, 0.3), 0.1);
    const t = new Panel(0.8, 0.14, 256, null); t.mesh.position.set(0, 0.22, 0.1); cr.add(t.mesh);
    t.draw('c', () => t.text(0, 0, 'CÂMERA', 0.09, '#ffd84a'));

    // ---- texto do portal
    const pt = new Panel(2.6, 0.3, 512, null);
    pt.mesh.position.copy(game.world.portal).add(V(0, 2.4, -1.8)); pt.mesh.rotation.y = Math.PI;
    pt.draw('p', () => pt.text(0, 0, 'ENTRAR NO DEPÓSITO', 0.15, '#ffd84a'));
    game.scene.add(pt.mesh);

    // ---- manequins (prévia do seu macaco)
    this.manis = ['colorStation', 'wardrobe'].map(k => new Avatar(game.scene, game.prof, true));
    this.manis.forEach(m => { for (const o of [...m.hands, ...m.arms]) o.visible = false; });

    // ---- laser do controle
    this.laser = model('stick', C(1, 0.3, 0.3)); this.laser.visible = false; game.scene.add(this.laser);
    this.hover = -1;
  }

  makeTag() { return new Panel(1.2, 0.14, 384, null); }

  async toggleHost() {
    const g = this.g;
    if (g.net.mode === 'host') { g.net.leave(); g.hud('Sala fechada', '#ffb366', 2.5); return; }
    const cfg = { ...g.createCfg, code: g.codeEntry };
    if (!cfg.isPublic && !cfg.code) { cfg.code = String(Math.random() * 10000 | 0).padStart(4, '0'); g.codeEntry = cfg.code; }
    g.hud('Criando sala no seu Go...', '#bde', 2);
    try {
      await g.net.host(cfg, g.prof.name);
      g.hud(`Sala '${cfg.name}' criada!${cfg.code ? ' Código ' + cfg.code : ''}`, '#88ff99', 6);
    } catch (e) { audio.play('ui_error'); g.hud('Não deu para criar: ' + g.net.error, '#ff7755', 4); }
  }
  async joinList(i) {
    const g = this.g, e = g.net.list[i];
    if (!e || e.mine) return;
    if (e.locked && !g.codeEntry) { audio.play('ui_error'); g.hud('Essa sala tem código: digite no teclado', '#ffaa66', 3); return; }
    if (e.players >= e.max) { audio.play('ui_error'); g.hud('Sala cheia', '#ffaa66', 2); return; }
    g.hud(`Entrando em '${e.name}'...`, '#bde', 2.5);
    const ok = await g.net.join(e.id, g.codeEntry, g.prof.name).catch(() => false);
    if (!ok) g.hud('Não conectou: ' + (g.net.error || 'sem resposta'), '#ff7755', 3);
  }
  async joinCode() {
    const g = this.g;
    if (!g.codeEntry) { audio.play('ui_error'); g.hud('Digite o código no teclado', '#ffaa66', 2.5); return; }
    g.hud('Procurando a sala ' + g.codeEntry + '...', '#bde', 2.5);
    const ok = await g.net.join(g.net.privateRoomId(g.codeEntry), g.codeEntry, g.prof.name).catch(() => false);
    if (!ok) g.hud('Nenhuma sala com o código ' + g.codeEntry, '#ff7755', 3);
  }
  scanTick(force) {
    const g = this.g;
    if (g.pstate !== 'lobby' || !navigator.onLine) return;
    if (force || g.now - this.lastScan > 8) { this.lastScan = g.now; g.net.scan(); }
  }

  update(inp, dt) {
    const g = this.g;
    this.listBtns.forEach((b, i) => b.setVisible(i < g.net.list.length && !g.net.list[i].mine));
    // laser
    const a = g.handVis[g.ctrlHand], d = g.ctrlDir;
    let best = 6, hover = -1;
    this.buttons.forEach((b, i) => {
      b.update(dt, i === this.hover);
      if (!b.visible) return;
      const oc = b.world.clone().sub(a), t = oc.dot(d);
      if (t < 0 || t > best) return;
      if (oc.sub(d.clone().multiplyScalar(t)).length() < b.r * 1.3) { best = t; hover = i; }
    });
    this.hover = hover;
    g.cam.uiHover = hover >= 0;
    if (inp.triggerDown && hover >= 0) this.buttons[hover].press(audio);
    // toque com as duas mãos
    for (const b of this.buttons) for (let h = 0; h < 2; h++) {
      const dd = b.visible ? g.handVis[h].distanceTo(b.world) : 99;
      if (dd < b.r + 0.05 && b.prevD[h] >= b.r + 0.05) b.press(audio);
      b.prevD[h] = dd;
    }
    this.laser.visible = hover >= 0;
    if (hover >= 0) {
      this.laser.position.copy(a);
      this.laser.quaternion.setFromUnitVectors(V(0, 1, 0), d);
      this.laser.scale.set(0.3, best, 0.3);
    }
  }

  draw() {
    const g = this.g, P = g.prof;
    const B = this.boards;
    const head = (p, extra) => p.text(0, p.h / 2 - 0.12, p.title, 0.09, '#ffd84a');
    // cores
    let p = B.colorStation.p;
    p.draw([P.r, P.g, P.b].join(), () => {
      head(p);
      ['R', 'G', 'B'].forEach((k, i) => p.text(-0.6, 0.3 - i * 0.3, `${k} ${[P.r, P.g, P.b][i]}`, 0.08));
      p.text(0.58, -0.52, 'Toque os botões com as mãos', 0.04, '#999');
    });
    // guarda-roupa
    p = B.wardrobe.p;
    p.draw([P.hat, P.face, P.neck, P.acc].join(), () => {
      head(p);
      p.text(0, 0.35, 'Chapéu: ' + HAT_N[P.hat % 8], 0.065); p.text(0, 0.1, 'Rosto: ' + FACE_N[P.face % 3], 0.065);
      p.text(0, -0.15, 'Pescoço: ' + NECK_N[P.neck % 2], 0.065); p.text(0, -0.4, 'Cor do acessório: ' + ACC_N[P.acc & 7], 0.065);
    });
    // badges
    p = B.badgeBoard.p;
    p.draw(P.badges + '|' + P.title, () => {
      head(p);
      BADGES.forEach((b, i) => {
        const got = P.badges & (1 << i), x = i % 2 ? 0.18 : -0.97, y = 0.6 - (i / 2 | 0) * 0.2;
        p.text(x, y + 0.01, (got ? '' : '? ') + b[0], 0.055, got ? (P.title === i ? '#ff9933' : '#ffe680') : '#666', 'left');
        p.text(x, y - 0.06, b[1], 0.03, '#999', 'left');
      });
    });
    // salas
    p = B.netBoard.p;
    const n = g.net, c = g.createCfg;
    const st = n.migrating ? 'TROCANDO DE HOST...' : n.mode === 'host' ? `HOST da sala '${n.cfg.name}' - ${n.server ? n.server.clients.size : 1}/${n.cfg.maxPlayers} jogadores${n.cfg.code ? ' - código ' + n.cfg.code : ''}`
      : n.state === 'online' ? `CONECTADO em '${n.cfg.name}' - ${g.remotes.size + 1} jogadores` : n.state === 'connecting' ? 'CONECTANDO...' : n.state === 'failed' ? 'FALHOU: ' + n.error : 'OFFLINE - você: ' + P.name;
    const list = n.list.map(e => [e.name, e.players, e.max, e.locked, e.diff, e.bananas, e.mine].join(':')).join('|');
    p.draw([c.name, c.isPublic, c.maxPlayers, c.diff, c.bananas, g.codeEntry, list, st, n.scanning, [...g.remotes.values()].map(r => r.name).join()].join('#'), () => {
      head(p);
      p.text(-1.08, 0.74, 'CRIAR', 0.05, '#ffd84a'); p.text(-1.08, 0.6, c.name, 0.04);
      p.text(-1.08, 0.27, c.isPublic ? 'Pública (aparece na lista)' : 'Privada (só com código)', 0.035, '#aaa');
      p.text(-1.08, 0.16, 'Jogadores: ' + c.maxPlayers, 0.045); p.text(-1.08, -0.06, 'Monstros: ' + DIFF_N[c.diff], 0.045);
      p.text(-1.08, -0.28, 'Bananas: ' + c.bananas, 0.045);
      p.text(0, 0.74, 'CÓDIGO (opcional)', 0.05, '#ffd84a'); p.text(0, 0.55, g.codeEntry || '- - - -', 0.07);
      p.text(1.1, 0.74, 'SALAS PÚBLICAS', 0.05, '#ffd84a');
      if (!n.list.length) p.text(1.05, 0.38, n.scanning ? 'procurando...' : 'nenhuma sala aberta', 0.04, '#999');
      n.list.slice(0, 5).forEach((e, i) => {
        p.text(0.55, 0.42 - i * 0.2, (e.locked ? '[COD] ' : '') + e.name + (e.mine ? ' (sua)' : ''), 0.04, '#fff', 'left');
        p.text(0.55, 0.36 - i * 0.2, `${e.players}/${e.max}  ${DIFF_N[e.diff] || ''}  ${e.bananas} bananas`, 0.028, '#999', 'left');
      });
      p.text(0, -0.7, st, 0.04, n.state === 'online' ? '#ffd84a' : '#aaa', 'center', 3.3);
      if (n.state === 'online') p.text(0, -0.78, 'Jogadores: ' + [P.name, ...[...g.remotes.values()].map(r => r.name)].join(', '), 0.03, '#999', 'center', 3.3);
    });
    // ajustes
    p = B.modeBoard.p;
    p.draw([P.handMode, P.boost, P.armAngle, P.volume, P.snap, P.name].join(), () => {
      head(p);
      p.text(0, 0.5, 'Mão duplicada: ' + (P.handMode ? 'ALTERNADA' : 'ESPELHO'), 0.05);
      p.text(0, 0.2, 'Impulso: ' + ['NORMAL', 'FORTE', 'TURBO'][P.boost], 0.05);
      p.text(0, -0.19, `Ângulo do braço: ${P.armAngle}°`, 0.05); p.text(0, -0.46, 'Volume: ' + P.volume, 0.05);
      p.text(0, -0.82, `Giro: ${P.snap ? 45 : 30}°  -  Nome: ${P.name}`, 0.035, '#aaa');
    });
    // como jogar
    p = B.howTo.p;
    p.draw('how', () => {
      head(p);
      const lines = ['Você é um macaco SEM PERNAS: anda com as mãos!',
        '- Aponte o controle para baixo até a mão tocar o chão e EMPURRE para trás. A outra mão copia o movimento.',
        '- Para CORRER: empurre rápido várias vezes e solte no meio do movimento. O impulso acelera e fica mais forte.',
        '- Suba em prateleiras e caixas empurrando as paredes.',
        '- Touchpad esq/dir: girar. Centro: grito de macaco.',
        '- Câmera: touchpad CIMA pega/vira selfie, BAIXO solta no ar (segure para guardar). Gatilho grava. Deslize para esticar.',
        '- Pegue as bananas douradas: a porta de aço abre. FUJA!',
        '- O Pálido só anda quando ninguém olha para ele.'];
      let y = 0.62;
      for (const l of lines) { p.text(-1.2, y, l, 0.042, '#fff', 'left'); y -= l.length > 60 ? 0.17 : 0.11; }
    });
    // manequins
    ['colorStation', 'wardrobe'].forEach((k, i) => {
      const s = g.world.st[k], m = this.manis[i];
      const pos = V(0, -0.75, 0.9).applyAxisAngle(V(0, 1, 0), s.yaw).add(s.c);
      const yaw = s.yaw + Math.PI + (i ? g.now * 0.8 : Math.sin(g.now) * 0.6);
      m.apply(P);
      m.set(pos, yaw, new THREE.Quaternion().setFromAxisAngle(V(0, 1, 0), yaw), pos, pos);
      for (const o of [...m.hands, ...m.arms]) o.visible = false;
    });
    this.rackCam.visible = g.cam.state === 0;
  }
}

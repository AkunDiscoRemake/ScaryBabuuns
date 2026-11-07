// Multiplayer P2P (WebRTC via PeerJS). O óculos que cria a sala É o servidor:
// os outros conectam direto nele. O serviço público do PeerJS só apresenta os peers (sinalização).
// Salas públicas ocupam "vagas" com IDs conhecidos (para dar para listar); privadas usam o código no ID.
import { Peer } from 'peerjs';

const APP = 'babuunhollow-v2';
export const PUB_SLOTS = 12;
const pubId = i => `${APP}-pub-${i}`;
const prvId = code => `${APP}-prv-${code}`;
const clean = s => String(s || '').replace(/[^\wÀ-ÿ \-]/g, '').slice(0, 26) || 'Macaco';

// ------------------------------------------------------------------ servidor (roda no óculos do host)
class RoomServer {
  constructor(cfg, firstId, onLocal) {
    this.cfg = cfg; this.nextId = firstId; this.clients = new Map(); // id -> {conn|null, name}
    this.order = []; this.sim = -1; this.onLocal = onLocal; this.localId = -1;
    this.timer = setInterval(() => {
      this.bcast({ t: 'ping' });
      const now = Date.now();
      for (const [id, c] of this.clients) if (c.conn && now - (c.seen || now) > 6000) { try { c.conn.close(); } catch (e) {} this.leave(id); }
    }, 1000);
  }
  stop() { clearInterval(this.timer); }
  info() {
    return { t: 'info', name: this.cfg.name, pub: this.cfg.isPublic, locked: !!this.cfg.code, max: this.cfg.maxPlayers,
             diff: this.cfg.diff, bananas: this.cfg.bananas, players: this.clients.size, code: undefined };
  }
  sendTo(id, msg) {
    const c = this.clients.get(id);
    if (!c) return;
    if (c.conn) { if (c.conn.open) c.conn.send(msg); } else this.onLocal(msg);
  }
  bcast(msg, except) { for (const id of this.clients.keys()) if (id !== except) this.sendTo(id, msg); }
  succ() { return { t: 'succ', list: this.order.filter(id => id !== this.localId) }; }
  hello(conn, m) {
    if (this.cfg.code && m.code !== this.cfg.code && conn) { conn.send({ t: 'err', msg: 'código incorreto' }); setTimeout(() => conn.close(), 300); return -1; }
    if (this.clients.size >= this.cfg.maxPlayers) { conn && conn.send({ t: 'err', msg: 'servidor cheio' }); conn && setTimeout(() => conn.close(), 300); return -1; }
    let id = m.want > 0 && !this.clients.has(m.want) ? m.want : this.nextId;
    this.nextId = Math.max(this.nextId, id + 1);
    this.clients.set(id, { conn, name: clean(m.name), seen: Date.now() });
    this.order.push(id);
    if (!conn) this.localId = id;
    if (this.sim < 0) this.sim = id;
    this.sendTo(id, { t: 'welcome', id, host: this.sim, cfg: { ...this.cfg } });
    for (const [oid, o] of this.clients) if (oid !== id) this.sendTo(id, { t: 'join', id: oid, name: o.name });
    this.bcast({ t: 'join', id, name: clean(m.name) }, id);
    this.bcast(this.succ());
    return id;
  }
  leave(id) {
    if (!this.clients.has(id)) return;
    this.clients.delete(id);
    this.order = this.order.filter(x => x !== id);
    this.bcast({ t: 'leave', id });
    if (this.sim === id) { this.sim = this.order[0] ?? -1; if (this.sim >= 0) this.bcast({ t: 'host', id: this.sim }); }
    this.bcast(this.succ());
  }
  msg(id, m) {
    const c = this.clients.get(id); if (c) c.seen = Date.now();
    if (m.t === 'st' || m.t === 'ev') { m.id = id; this.bcast(m, id); }
    else if (m.t === 'away' && this.sim === id) {
      const other = this.order.find(x => x !== id);
      if (other !== undefined) { this.sim = other; this.bcast({ t: 'host', id: other }); }
    }
  }
}

// ------------------------------------------------------------------ cliente / gerenciador
export class Net {
  constructor(onMsg, log) {
    this.onMsg = onMsg; this.log = log || (() => {});
    this.mode = 'off';      // off | host | client
    this.state = 'off';     // off | connecting | online | failed
    this.myId = -1; this.hostId = -1; this.cfg = null; this.roomId = ''; this.succ = [];
    this.list = []; this.error = '';
    this.migrating = false;
    this.lastRx = Date.now();
    setInterval(() => {
      if (this.mode === 'client' && this.state === 'online' && Date.now() - this.lastRx > 4000) {
        const c = this.conn; this.conn = null; try { c && c.close(); } catch (e) {}
        this._lost();
      }
    }, 1000);
  }
  get online() { return this.state === 'online'; }
  isSim() { return !this.online || this.myId === this.hostId; }

  _peer(id) {
    return new Promise((res, rej) => {
      const p = id ? new Peer(id, { debug: 0 }) : new Peer({ debug: 0 });
      const to = setTimeout(() => { p.destroy(); rej(new Error('tempo esgotado no serviço de conexão')); }, 12000);
      // só durante a criação: depois de aberto, erros como "peer-unavailable" (vaga vazia) são normais
      const onErr = e => { clearTimeout(to); p.off('error', onErr); p.destroy(); rej(e); };
      p.on('error', onErr);
      p.on('open', () => { clearTimeout(to); p.off('error', onErr); p.on('error', () => {}); res(p); });
    });
  }

  // ---- criar sala no próprio óculos
  async host(cfg, name, wantId = -1, firstId = 1) {
    this.leave(true);
    this.leaving = false;
    this.name = name;
    this.mode = 'host'; this.state = 'connecting'; this.cfg = { ...cfg };
    let peer = null, roomId = '';
    try {
      if (cfg.isPublic) {
        const order = cfg.slot !== undefined ? [cfg.slot] : [...Array(PUB_SLOTS).keys()];
        for (const i of order) {
          try { peer = await this._peer(pubId(i)); roomId = pubId(i); this.cfg.slot = i; break; }
          catch (e) { if (e.type !== 'unavailable-id') throw e; }
        }
        if (!peer) throw new Error('todas as vagas públicas estão ocupadas');
      } else {
        roomId = prvId(cfg.code);
        peer = await this._peer(roomId);
      }
    } catch (e) {
      this.state = 'failed'; this.mode = 'off';
      this.error = e.type === 'unavailable-id' ? 'esse código já está em uso' : (e.message || String(e));
      throw e;
    }
    this.peer = peer; this.roomId = roomId;
    const srv = this.server = new RoomServer(this.cfg, firstId, m => this._recv(m));
    peer.on('connection', conn => {
      let id = -1;
      conn.on('open', () => conn.send(srv.info()));  // quem só está listando recebe as infos
      conn.on('data', m => {
        if (!m || typeof m !== 'object') return;
        if (m.t === 'hello') { if (id < 0) id = srv.hello(conn, m); }
        else if (id >= 0) srv.msg(id, m);
      });
      conn.on('close', () => { if (id >= 0) srv.leave(id); });
      conn.on('error', () => { if (id >= 0) srv.leave(id); });
    });
    peer.on('disconnected', () => { try { peer.reconnect(); } catch (e) {} });  // perdeu a sinalização: tenta voltar
    this.state = 'online';
    srv.hello(null, { name, want: wantId });
    return roomId;
  }

  // ---- entrar numa sala
  async join(roomId, code, name, wantId = -1) {
    this.name = name;
    if (this.mode !== 'client' || !this.peer || this.peer.destroyed) {
      this.leave(true);
      this.peer = await this._peer();
      this.peer.on('error', e => { if (e.type === 'peer-unavailable') { this._fail('sala não encontrada'); if (this._joinRes) this._joinRes(false); } });
    }
    this.mode = 'client'; this.state = 'connecting'; this.roomId = roomId; this.code = code; this.rejected = false;
    return new Promise(res0 => {
      const res = v => { this._joinRes = null; res0(v); };
      this._joinRes = res;
      const conn = this.conn = this.peer.connect(roomId, { reliable: true, serialization: 'json' });
      const to = setTimeout(() => { if (this.state === 'connecting') { this._fail('sem resposta do host'); res(false); } }, 7000);
      conn.on('open', () => conn.send({ t: 'hello', name, code, want: wantId }));
      conn.on('data', m => {
        if (this.conn !== conn) return;
        this.lastRx = Date.now();
        if (m.t === 'welcome') { clearTimeout(to); this.state = 'online'; this.error = ''; this.migrating = false; res(true); }
        if (m.t === 'err') { clearTimeout(to); this.error = m.msg; this.rejected = true; this.state = 'failed'; res(false); }
        if (m.t === 'migrate') { this.conn = null; try { conn.close(); } catch (e) {} this._recv(m); this._lost(); return; }
        this._recv(m);
      });
      conn.on('close', () => {
        clearTimeout(to);
        if (this.conn === conn) { this.conn = null; if (this.rejected) { this.state = 'failed'; this.mode = 'off'; } else this._lost(); }
        res(false);
      });
      conn.on('error', () => {});
    });
  }

  _fail(msg) { this.error = msg; this.state = 'failed'; }

  _recv(m) {
    if (m.t === 'welcome') { this.myId = m.id; this.hostId = m.host; this.cfg = m.cfg; }
    else if (m.t === 'host') this.hostId = m.id;
    else if (m.t === 'succ') this.succ = m.list;
    else if (m.t === 'migrate' && this.mode === 'client') this.expectMigrate = true;
    this.onMsg(m);
  }

  send(m) {
    if (this.mode === 'host' && this.server) this.server.msg(this.myId, m);
    else if (this.conn && this.conn.open) this.conn.send(m);
  }
  ev(...args) { this.send({ t: 'ev', ev: args }); }

  // ---- troca de host: o próximo da fila vira servidor com o MESMO id de sala
  async _lost() {
    if (this.mode !== 'client' || this.leaving || this.state === 'connecting' && !this.migrating && this.myId < 0) return;
    if (!this.succ.length || this.migrating) { if (!this.migrating) { this.state = 'failed'; this.error = 'o host saiu'; } return; }
    this.migrating = true; this.state = 'connecting';
    this.onMsg({ t: 'migrating' });
    const me = this.myId, cfg = this.cfg, roomId = this.roomId, name = this.name;
    for (let k = 0; k < this.succ.length; k++) {
      const cand = this.succ[k];
      if (cand === me) {
        // EU viro o host: tento pegar o id da sala (o serviço libera o id antigo em alguns segundos)
        const maxId = Math.max(me, ...this.succ);
        for (let tries = 0; tries < 15; tries++) {
          try {
            if (this.peer) { this.peer.destroy(); this.peer = null; }
            await this.host({ ...cfg, slot: cfg.slot }, name, me, maxId + 1);
            if (this.roomId !== roomId && !cfg.isPublic) throw new Error('id');
            this.migrating = false;
            this.onMsg({ t: 'became-host' });
            return;
          } catch (e) { await new Promise(r => setTimeout(r, 1200)); }
        }
      } else {
        for (let tries = 0; tries < 8; tries++) {
          await new Promise(r => setTimeout(r, 1500));
          if (this.leaving) return;
          const ok = await this.join(roomId, this.code, name, me).catch(() => false);
          if (ok) { this.migrating = false; return; }
          this.state = 'connecting';
        }
      }
    }
    this.migrating = false; this.state = 'failed'; this.error = 'não foi possível trocar de host';
  }

  leave(quiet) {
    this.leaving = true;
    if (this.mode === 'host' && this.server) { this.server.bcast({ t: 'migrate' }); this.server.stop(); }
    const p = this.peer;
    setTimeout(() => { try { p && p.destroy(); } catch (e) {} }, this.mode === 'host' ? 250 : 0);
    this.peer = null; this.conn = null; this.server = null;
    this.mode = 'off'; this.state = 'off'; this.myId = -1; this.hostId = -1; this.succ = [];
    setTimeout(() => { this.leaving = false; }, 300);
  }

  // ---- lista de salas públicas: testa cada vaga
  async scan() {
    if (this.scanning) return;
    this.scanning = true;
    let p = this.scanPeer;
    try { if (!p || p.destroyed || p.disconnected) p = this.scanPeer = await this._peer(); }
    catch (e) { this.scanning = false; this.error = 'sem internet para achar salas'; return; }
    const found = [];
    await Promise.all([...Array(PUB_SLOTS).keys()].map(i => new Promise(res => {
      const id = pubId(i);
      if (this.mode === 'host' && this.roomId === id) { found.push({ id, slot: i, ...this.server.info(), mine: true }); return res(); }
      const c = p.connect(id, { serialization: 'json' });
      const to = setTimeout(() => { c.close(); res(); }, 3500);
      c.on('data', m => { if (m.t === 'info') { found.push({ id, slot: i, ...m }); clearTimeout(to); c.close(); res(); } });
      c.on('error', () => { clearTimeout(to); res(); });
    })));
    this.list = found.sort((a, b) => a.slot - b.slot);
    this.scanning = false;
  }
  privateRoomId(code) { return prvId(code); }
}

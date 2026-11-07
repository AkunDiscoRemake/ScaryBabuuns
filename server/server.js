#!/usr/bin/env node
// Servidor multiplayer do Babuun Hollow — TCP puro, protocolo de linhas de texto.
// Rode no seu servidor:  node server/server.js   (porta padrão 25570, ou PORT=xxxx)
//
// Protocolo (cliente -> servidor):
//   HELLO <proto> <sala> <nome>      entra numa sala
//   ST <estado...>                   estado do jogador (15x/s) — repassado aos outros
//   EV <evento...>                   evento de jogo — repassado aos outros
//   PING                             mantém a conexão viva
// Servidor -> cliente:
//   WELCOME <seuId> <hostId> | JOIN <id> <nome> | LEAVE <id> | HOST <id>
//   ST <id> <estado...> | EV <id> <evento...> | ERR <mensagem> | PONG
// O "host" de cada sala (primeiro a entrar) simula os monstros e a rodada.
'use strict';
const net = require('net');

const PORT = parseInt(process.env.PORT || '25570', 10);
const HOST = process.env.HOST || '0.0.0.0';
const MAX_PER_ROOM = parseInt(process.env.MAX_PER_ROOM || '10', 10);
const PROTO = 1;
const MAX_LINE = 2048;
const MAX_LINES_PER_SEC = 120;

let nextId = 1;
const rooms = new Map(); // nome -> { clients: Set, host: id }

const log = (...a) => console.log(new Date().toISOString(), ...a);
const clean = s => String(s || '').replace(/[^\w\-À-ÿ]/g, '').slice(0, 24) || 'Macaco';

function send(c, line) {
  if (!c.sock.destroyed && c.sock.writable) {
    // se o cliente não consome, descarta estado (evita estourar memória)
    if (c.sock.writableLength > 256 * 1024 && line.startsWith('ST ')) return;
    c.sock.write(line + '\n');
  }
}
function broadcast(room, line, except) {
  for (const o of room.clients) if (o !== except) send(o, line);
}

function leave(c) {
  if (!c.room) return;
  const room = rooms.get(c.room);
  c.room = null;
  if (!room) return;
  room.clients.delete(c);
  broadcast(room, `LEAVE ${c.id}`);
  if (room.clients.size === 0) { rooms.delete(c.roomName); log(`sala ${c.roomName} vazia`); return; }
  if (room.host === c.id) {
    room.host = [...room.clients][0].id;
    broadcast(room, `HOST ${room.host}`);
    log(`sala ${c.roomName}: novo host ${room.host}`);
  }
}

function handle(c, line) {
  const sp = line.indexOf(' ');
  const cmd = sp < 0 ? line : line.slice(0, sp);
  const rest = sp < 0 ? '' : line.slice(sp + 1);
  if (cmd === 'HELLO') {
    const [proto, roomName, name] = rest.split(' ');
    if (parseInt(proto, 10) !== PROTO) { send(c, `ERR versao incompativel (servidor ${PROTO})`); return c.sock.end(); }
    leave(c);
    const rn = clean(roomName).toUpperCase();
    let room = rooms.get(rn);
    if (!room) { room = { clients: new Set(), host: c.id }; rooms.set(rn, room); }
    if (room.clients.size >= MAX_PER_ROOM) { send(c, 'ERR sala cheia'); return c.sock.end(); }
    c.name = clean(name);
    c.room = rn; c.roomName = rn;
    room.clients.add(c);
    send(c, `WELCOME ${c.id} ${room.host}`);
    for (const o of room.clients) if (o !== c) send(c, `JOIN ${o.id} ${o.name}`);
    broadcast(room, `JOIN ${c.id} ${c.name}`, c);
    log(`${c.name}#${c.id} entrou em ${rn} (${room.clients.size} jogadores, host ${room.host})`);
    return;
  }
  if (cmd === 'PING') return send(c, 'PONG');
  if (!c.room) return;
  const room = rooms.get(c.room);
  if (!room) return;
  if (cmd === 'ST' || cmd === 'EV') broadcast(room, `${cmd} ${c.id} ${rest}`, c);
}

const server = net.createServer(sock => {
  sock.setNoDelay(true);
  sock.setKeepAlive(true, 15000);
  sock.setTimeout(60000);
  const c = { id: nextId++, sock, buf: '', room: null, name: '?', tick: Date.now(), count: 0 };
  sock.setEncoding('utf8');
  sock.on('data', d => {
    // se alguém abrir no navegador, responde com o status
    if (c.buf === '' && d.startsWith('GET ')) {
      const total = [...rooms.values()].reduce((a, r) => a + r.clients.size, 0);
      sock.end(`HTTP/1.1 200 OK\r\nContent-Type: text/plain; charset=utf-8\r\nConnection: close\r\n\r\nServidor Babuun Hollow online. Salas: ${rooms.size}, jogadores: ${total}\n`);
      return;
    }
    c.buf += d;
    if (c.buf.length > MAX_LINE * 8) { sock.destroy(); return; }
    let i;
    while ((i = c.buf.indexOf('\n')) >= 0) {
      const line = c.buf.slice(0, i).replace(/\r$/, '');
      c.buf = c.buf.slice(i + 1);
      if (!line || line.length > MAX_LINE) continue;
      const now = Date.now();
      if (now - c.tick > 1000) { c.tick = now; c.count = 0; }
      if (++c.count > MAX_LINES_PER_SEC) continue; // limite de spam
      try { handle(c, line); } catch (e) { log('erro', e.message); }
    }
  });
  sock.on('timeout', () => sock.destroy());
  sock.on('close', () => { if (c.room) log(`${c.name}#${c.id} saiu`); leave(c); });
  sock.on('error', () => {});
});

server.listen(PORT, HOST, () => log(`Babuun Hollow servidor ouvindo em ${HOST}:${PORT}`));

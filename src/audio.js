// Áudio WebAudio: sons 3D (PannerNode), loops e saída para gravação.
export class Audio {
  constructor() {
    this.ctx = null; this.buffers = new Map(); this.master = null; this.voices = new Map(); this.nextId = 1;
  }
  async init(names) {
    const AC = window.AudioContext || window.webkitAudioContext;
    this.ctx = new AC();
    this.master = this.ctx.createGain();
    this.master.connect(this.ctx.destination);
    // compressor suave para os jumpscares não estourarem
    await Promise.all(names.map(async n => {
      try {
        const r = await fetch('assets/snd/' + n + '.wav');
        this.buffers.set(n, await this.ctx.decodeAudioData(await r.arrayBuffer()));
      } catch (e) { console.warn('som', n, e); }
    }));
  }
  resume() { if (this.ctx && this.ctx.state !== 'running') this.ctx.resume(); }
  setVolume(v) { if (this.master) this.master.gain.value = v; }
  setListener(pos, quat) {
    if (!this.ctx) return;
    const l = this.ctx.listener;
    const f = { x: 0, y: 0, z: -1 }, u = { x: 0, y: 1, z: 0 };
    const rot = v => {
      const { x, y, z, w } = quat;
      const ix = w * v.x + y * v.z - z * v.y, iy = w * v.y + z * v.x - x * v.z, iz = w * v.z + x * v.y - y * v.x, iw = -x * v.x - y * v.y - z * v.z;
      return { x: ix * w + iw * -x + iy * -z - iz * -y, y: iy * w + iw * -y + iz * -x - ix * -z, z: iz * w + iw * -z + ix * -y - iy * -x };
    };
    const ff = rot(f), uu = rot(u);
    if (l.positionX) {
      l.positionX.value = pos.x; l.positionY.value = pos.y; l.positionZ.value = pos.z;
      l.forwardX.value = ff.x; l.forwardY.value = ff.y; l.forwardZ.value = ff.z;
      l.upX.value = uu.x; l.upY.value = uu.y; l.upZ.value = uu.z;
    } else { l.setPosition(pos.x, pos.y, pos.z); l.setOrientation(ff.x, ff.y, ff.z, uu.x, uu.y, uu.z); }
  }
  play(name, gain = 1, loop = false, rate = 1, pos = null, ref = 2.5) {
    const b = this.buffers.get(name);
    if (!b || !this.ctx) return 0;
    const src = this.ctx.createBufferSource();
    src.buffer = b; src.loop = loop; src.playbackRate.value = rate;
    const g = this.ctx.createGain(); g.gain.value = gain;
    let p = null;
    if (pos) {
      p = this.ctx.createPanner();
      p.panningModel = 'equalpower'; p.distanceModel = 'inverse'; p.refDistance = ref; p.rolloffFactor = 1.2; p.maxDistance = 80;
      if (p.positionX) { p.positionX.value = pos.x; p.positionY.value = pos.y; p.positionZ.value = pos.z; } else p.setPosition(pos.x, pos.y, pos.z);
      src.connect(g).connect(p).connect(this.master);
    } else src.connect(g).connect(this.master);
    src.start();
    const id = this.nextId++;
    this.voices.set(id, { src, g, p });
    src.onended = () => this.voices.delete(id);
    return id;
  }
  play3D(name, pos, gain = 1, ref = 2.5) { return this.play(name, gain, false, 1, pos, ref); }
  setGain(id, v) { const vo = this.voices.get(id); if (vo) vo.g.gain.value = v; }
  stop(id) { const vo = this.voices.get(id); if (vo) { try { vo.src.stop(); } catch (e) {} this.voices.delete(id); } }
  // fluxo de áudio do jogo para a gravação
  recordStream() {
    if (!this.recDest) { this.recDest = this.ctx.createMediaStreamDestination(); this.master.connect(this.recDest); }
    return this.recDest;
  }
}
export const audio = new Audio();

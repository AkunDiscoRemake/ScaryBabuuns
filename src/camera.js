// Câmera segurável e esticável: prévia na telinha, grava vídeo (WebM) com microfone + som do jogo
import * as THREE from 'three';
import { model } from './assets.js';
import { audio } from './audio.js';

const V = (x = 0, y = 0, z = 0) => new THREE.Vector3(x, y, z);
export const REC_W = 480, REC_H = 272, REC_FPS = 24;
let micStream = null;

// pedido de microfone (feito na página antes de entrar no VR)
export async function askMic() {
  try { micStream = await navigator.mediaDevices.getUserMedia({ audio: { echoCancellation: true, noiseSuppression: true } }); return true; }
  catch (e) { return false; }
}

export class CameraRig {
  constructor(game) {
    this.g = game;
    this.state = 0;          // 0 guardada, 1 na mão, 2 parada no ar
    this.stickLen = 0.6; this.selfie = true; this.uiHover = false;
    this.pos = V(); this.quat = new THREE.Quaternion();
    this.camModel = model('camera'); this.camModel.visible = false; game.scene.add(this.camModel);
    this.stick = model('stick'); this.stick.visible = false; game.scene.add(this.stick);
    this.rt = new THREE.WebGLRenderTarget(REC_W, REC_H, { depthBuffer: true });
    this.rt.texture.colorSpace = THREE.SRGBColorSpace;
    this.cam3 = new THREE.PerspectiveCamera(70, REC_W / REC_H, 0.05, 80);
    this.camModel.traverse(o => { if (o.isMesh && o.material.userData.screen) { o.material.map = this.rt.texture; o.material.needsUpdate = true; } });
    this.recording = false; this.lastFrame = 0;
    this.canvas = document.createElement('canvas'); this.canvas.width = REC_W; this.canvas.height = REC_H;
    this.c2d = this.canvas.getContext('2d');
    this.pix = new Uint8Array(REC_W * REC_H * 4);
    this.img = this.c2d.createImageData(REC_W, REC_H);
    this.downT = 0; this.lastPadY = 0; this.padWas = false;
  }
  toggle() {
    if (this.state) { this.stopRec(); this.state = 0; this.g.hud('Câmera guardada', '#cde', 1.5); }
    else { this.state = 1; this.g.hud('Câmera na mão! Gatilho grava, deslize no touchpad para esticar.', '#cde', 4); }
  }
  update(inp, dt) {
    const g = this.g;
    const vert = Math.abs(inp.padY) > 0.45 && Math.abs(inp.padY) > Math.abs(inp.padX);
    // CIMA: pega a câmera / alterna selfie-mundo
    if (inp.padClickDown && vert && inp.padY > 0) {
      if (this.state === 0) this.toggle();
      else if (this.state === 2) this.state = 1;
      else { this.selfie = !this.selfie; g.hud(this.selfie ? 'Câmera: SELFIE' : 'Câmera: MUNDO', '#cde', 1.5); }
      audio.play('ui_click', 0.5);
    }
    // BAIXO: solta a câmera parada no ar; segurar 0,8 s guarda
    if (inp.padClick && vert && inp.padY < 0 && this.state) {
      this.downT += dt;
      if (this.downT > 0.8 && this.downT - dt <= 0.8) { this.toggle(); audio.play('ui_click', 0.5); }
    } else {
      if (this.downT > 0 && this.downT <= 0.8 && this.state === 1) { this.state = 2; g.hud('Câmera parada no ar (CIMA pega de novo)', '#cde', 2.5); }
      this.downT = 0;
    }
    if (this.state === 1) {
      // deslizar: estica/recolhe o bastão
      if (inp.padTouch && !inp.padClick) {
        if (this.padWas) this.stickLen = Math.min(2.6, Math.max(0.12, this.stickLen + (inp.padY - this.lastPadY) * 1.2));
        this.lastPadY = inp.padY;
      }
      this.padWas = inp.padTouch && !inp.padClick;
      if (inp.wheel) this.stickLen = Math.min(2.6, Math.max(0.12, this.stickLen + inp.wheel));
      const base = g.handVis[g.ctrlHand];
      this.pos.copy(base).addScaledVector(g.ctrlDir, this.stickLen);
      const look = this.selfie ? g.headPos.clone().sub(this.pos).normalize() : g.ctrlDir.clone();
      const m = new THREE.Matrix4().lookAt(V(), look.clone().negate(), V(0, 1, 0));
      this.quat.setFromRotationMatrix(m);
    }
    // gatilho: grava/para
    if (inp.triggerDown && this.state && !this.uiHover) this.recording ? this.stopRec() : this.startRec();
    // visual
    this.camModel.visible = this.state !== 0;
    this.camModel.position.copy(this.pos); this.camModel.quaternion.copy(this.quat);
    this.stick.visible = this.state === 1;
    if (this.state === 1) {
      const base = g.handVis[g.ctrlHand], d = this.pos.clone().sub(base), L = d.length();
      this.stick.position.copy(base); this.stick.quaternion.setFromUnitVectors(V(0, 1, 0), d.divideScalar(Math.max(L, 1e-3))); this.stick.scale.set(1, L, 1);
    }
  }
  // chamado pelo laço principal ANTES da renderização do VR
  render(renderer, scene, now) {
    if (!this.state || now - this.lastFrame < 1 / REC_FPS) return;
    this.lastFrame = now;
    this.cam3.position.copy(this.pos); this.cam3.quaternion.copy(this.quat);
    this.cam3.updateMatrixWorld();
    const xrOn = renderer.xr.enabled;
    renderer.xr.enabled = false;               // renderiza com a câmera do jogo, não a do óculos
    const old = renderer.getRenderTarget();
    this.camModel.visible = false;
    const head = this.g.me.n.head; if (head) head.visible = true;  // na selfie você aparece inteiro
    renderer.setRenderTarget(this.rt);
    renderer.render(scene, this.cam3);
    if (this.recording) {
      renderer.readRenderTargetPixels(this.rt, 0, 0, REC_W, REC_H, this.pix);
      const d = this.img.data, row = REC_W * 4;
      for (let y = 0; y < REC_H; y++) d.set(this.pix.subarray((REC_H - 1 - y) * row, (REC_H - y) * row), y * row);
      this.c2d.putImageData(this.img, 0, 0);
    }
    renderer.setRenderTarget(old);
    if (head) head.visible = false;
    this.camModel.visible = true;
    renderer.xr.enabled = xrOn;
  }
  recElapsed() { return this.recording ? (performance.now() - this.recStart) / 1000 : 0; }
  startRec() {
    const g = this.g;
    if (!window.MediaRecorder || !this.canvas.captureStream) { audio.play('ui_error'); g.hud('Este navegador não grava vídeo', '#ff7755', 3); return; }
    const vs = this.canvas.captureStream(REC_FPS);
    const dest = audio.recordStream();
    if (micStream && !this.micNode) { this.micNode = audio.ctx.createMediaStreamSource(micStream); this.micGain = audio.ctx.createGain(); this.micGain.gain.value = 1.6; this.micNode.connect(this.micGain).connect(dest); }
    const stream = new MediaStream([...vs.getVideoTracks(), ...dest.stream.getAudioTracks()]);
    const types = ['video/webm;codecs=vp8,opus', 'video/webm;codecs=vp9,opus', 'video/webm', 'video/mp4'];
    const mime = types.find(t => MediaRecorder.isTypeSupported && MediaRecorder.isTypeSupported(t)) || '';
    try { this.rec = new MediaRecorder(stream, mime ? { mimeType: mime, videoBitsPerSecond: 2500000 } : undefined); }
    catch (e) { audio.play('ui_error'); g.hud('Erro ao gravar: ' + e.message, '#ff7755', 3); return; }
    this.chunks = [];
    this.rec.ondataavailable = e => { if (e.data.size) this.chunks.push(e.data); };
    this.rec.onstop = () => this.saveVideo(mime);
    this.rec.start(1000);
    this.recording = true; this.recStart = performance.now();
    audio.play('rec_start', 0.7);
    g.hud(micStream ? 'GRAVANDO (com microfone)' : 'GRAVANDO (sem microfone)', '#ff5050', 2);
    g.award(10);
  }
  stopRec() {
    if (!this.recording) return;
    this.recording = false;
    try { this.rec.stop(); } catch (e) {}
    audio.play('rec_stop', 0.7);
  }
  saveVideo(mime) {
    const blob = new Blob(this.chunks, { type: mime || 'video/webm' });
    const d = new Date(), pad = n => String(n).padStart(2, '0');
    const name = `BabuunHollow_${d.getFullYear()}${pad(d.getMonth() + 1)}${pad(d.getDate())}_${pad(d.getHours())}${pad(d.getMinutes())}${pad(d.getSeconds())}.${(mime || '').includes('mp4') ? 'mp4' : 'webm'}`;
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a'); a.href = url; a.download = name; document.body.appendChild(a); a.click(); a.remove();
    (window.babuunVideos = window.babuunVideos || []).push({ name, url });
    window.dispatchEvent(new CustomEvent('babuun-video', { detail: { name, url } }));
    this.g.hud('Vídeo salvo: ' + name + ' (Downloads)', '#88ff99', 5);
  }
}

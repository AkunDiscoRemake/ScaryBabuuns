// Painéis de texto (canvas) e botões físicos para o VR
import * as THREE from 'three';
import { model, setTint } from './assets.js';

// Painel: plano com textura de canvas; draw(ctx, w, h) desenha o conteúdo
export class Panel {
  constructor(w, h, px = 512, bg = 'rgba(14,16,15,0.92)') {
    this.w = w; this.h = h; this.bg = bg;
    this.canvas = document.createElement('canvas');
    this.canvas.width = px; this.canvas.height = Math.round(px * h / w);
    this.ctx = this.canvas.getContext('2d');
    this.tex = new THREE.CanvasTexture(this.canvas);
    this.tex.colorSpace = THREE.SRGBColorSpace;
    this.mesh = new THREE.Mesh(new THREE.PlaneGeometry(w, h), new THREE.MeshBasicMaterial({ map: this.tex, transparent: true, fog: false }));
    this.key = '';
  }
  // redesenha só quando o conteúdo muda
  draw(key, fn) {
    if (key === this.key) return;
    this.key = key;
    const c = this.ctx, W = this.canvas.width, H = this.canvas.height;
    c.clearRect(0, 0, W, H);
    if (this.bg) {
      c.fillStyle = this.bg; roundRect(c, 0, 0, W, H, W * 0.02); c.fill();
      c.strokeStyle = '#6b5a3a'; c.lineWidth = W * 0.008; roundRect(c, 3, 3, W - 6, H - 6, W * 0.02); c.stroke();
    }
    fn(c, W, H, this);
    this.tex.needsUpdate = true;
  }
  // coordenadas em metros (x: -w/2..w/2, y: -h/2..h/2) -> pixels
  px(x, y) { return [(x / this.w + 0.5) * this.canvas.width, (0.5 - y / this.h) * this.canvas.height]; }
  text(x, y, str, size, color = '#fff', align = 'center', maxW) {
    const c = this.ctx, [X, Y] = this.px(x, y), S = size / this.h * this.canvas.height;
    c.font = `bold ${S}px sans-serif`; c.fillStyle = color; c.textAlign = align; c.textBaseline = 'middle';
    if (maxW) {
      const words = String(str).split(' '); let line = '', yy = Y, mw = maxW / this.w * this.canvas.width;
      for (const w of words) {
        if (w === '\n') { c.fillText(line, X, yy); line = ''; yy += S * 1.25; continue; }
        const t = line ? line + ' ' + w : w;
        if (c.measureText(t).width > mw && line) { c.fillText(line, X, yy); line = w; yy += S * 1.25; } else line = t;
      }
      if (line) c.fillText(line, X, yy);
    } else c.fillText(str, X, Y);
  }
}

function roundRect(c, x, y, w, h, r) {
  c.beginPath(); c.moveTo(x + r, y); c.arcTo(x + w, y, x + w, y + h, r); c.arcTo(x + w, y + h, x, y + h, r);
  c.arcTo(x, y + h, x, y, r); c.arcTo(x, y, x + w, y, r); c.closePath();
}

// Rótulo simples (texto solto)
export function label(text, size = 0.06, color = '#fff', widthM) {
  const w = widthM || Math.max(0.2, text.length * size * 0.62);
  const p = new Panel(w, size * 1.6, 256, null);
  p.draw(text, () => p.text(0, 0, text, size, color));
  return p;
}

// Botão: cilindro do modelo button.bm + rótulo; pressionado por toque das mãos ou laser + gatilho
export class Button {
  constructor(parent, x, y, labelText, fn, color = new THREE.Color(0.85, 0.85, 0.85), r = 0.07) {
    this.r = r; this.fn = fn; this.color = color.clone(); this.cool = 0; this.flash = 0; this.visible = true;
    this.root = new THREE.Group();
    this.root.position.set(x, y, 0.06);
    this.mesh = model('button');
    this.mesh.rotation.y = Math.PI;            // o modelo olha para -Z; o quadro olha para +Z
    this.mesh.scale.setScalar(r / 0.05);
    this.mesh.scale.z = 1;
    setTint(this.mesh, this.color);
    this.root.add(this.mesh);
    if (labelText) {
      const short = labelText.length <= 2;
      this.lbl = label(labelText, short ? 0.05 : 0.035, short ? '#111' : '#fff');
      this.lbl.mesh.position.set(0, short ? 0 : -r - 0.04, short ? 0.04 : 0.02);
      this.root.add(this.lbl.mesh);
    }
    parent.add(this.root);
    this.world = new THREE.Vector3();
    this.prevD = [9, 9];
  }
  setLabel(t) {
    if (!this.lbl || this.lbl.key === t) return;
    const short = t.length <= 2;
    this.lbl.draw(t, (c, W, H, p) => p.text(0, 0, t, short ? 0.05 : 0.035, short ? '#111' : '#fff'));
  }
  setVisible(v) { this.visible = v; this.root.visible = v; }
  update(dt, hover) {
    this.cool -= dt; this.flash -= dt;
    this.mesh.position.z = this.flash > 0 ? -0.015 : 0;
    setTint(this.mesh, hover ? this.color.clone().multiplyScalar(1.35) : this.color);
    this.root.getWorldPosition(this.world);
  }
  press(audio) {
    if (this.cool > 0 || !this.visible) return;
    this.cool = 0.35; this.flash = 0.2;
    audio && audio.play3D('ui_click', this.world, 0.8, 2);
    this.fn && this.fn();
  }
}

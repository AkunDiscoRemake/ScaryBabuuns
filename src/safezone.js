// Zonas seguras: barreira de energia azul decorada nas portas (jogadores passam, monstros não),
// faixas no chão, emblema central e placas "ZONA SEGURA".
import * as THREE from 'three';
import { Panel } from './ui.js';

function hexTexture() {
  const c = document.createElement('canvas'); c.width = c.height = 256;
  const g = c.getContext('2d');
  const grad = g.createLinearGradient(0, 0, 0, 256);
  grad.addColorStop(0, 'rgba(60,150,255,0.10)'); grad.addColorStop(0.5, 'rgba(80,180,255,0.28)'); grad.addColorStop(1, 'rgba(60,150,255,0.10)');
  g.fillStyle = grad; g.fillRect(0, 0, 256, 256);
  g.strokeStyle = 'rgba(150,220,255,0.9)'; g.lineWidth = 2.5;
  const r = 18, h = r * Math.sqrt(3);
  for (let row = -1; row < 10; row++) for (let col = -1; col < 10; col++) {
    const cx = col * r * 3 + (row % 2 ? r * 1.5 : 0), cy = row * h / 2;
    g.beginPath();
    for (let k = 0; k <= 6; k++) { const a = k * Math.PI / 3; g.lineTo(cx + Math.cos(a) * r, cy + Math.sin(a) * r); }
    g.stroke();
  }
  const t = new THREE.CanvasTexture(c);
  t.wrapS = t.wrapT = THREE.RepeatWrapping;
  t.colorSpace = THREE.SRGBColorSpace;
  return t;
}

export function buildSafeZones(scene, world) {
  const group = new THREE.Group(); scene.add(group);
  const tex = hexTexture();
  const fieldMat = new THREE.MeshBasicMaterial({ map: tex, transparent: true, opacity: 0.75, side: THREE.DoubleSide,
    blending: THREE.AdditiveBlending, depthWrite: false, fog: false });
  const glowMat = new THREE.MeshBasicMaterial({ color: 0x3fb4ff, fog: false });
  const darkMat = new THREE.MeshLambertMaterial({ color: 0x1b2430 });
  const lineMat = new THREE.MeshBasicMaterial({ color: 0x2f9cff, transparent: true, opacity: 0.85, fog: false });
  const C = world.gCell, H = 5;
  const cellC = (r, c) => new THREE.Vector3(world.gOx + (c + 0.5) * C, 0, world.gOz + (r + 0.5) * C);
  const signs = [];

  for (const z of world.safe) {
    // ---- portas com barreira
    for (const [r, c, axis] of z.doors) {
      const p = cellC(r, c);
      const door = new THREE.Group(); door.position.copy(p);
      if (axis === 'x') door.rotation.y = Math.PI / 2;  // passagem no eixo X: barreira de frente para X
      group.add(door);
      const field = new THREE.Mesh(new THREE.PlaneGeometry(C, H), fieldMat);
      field.position.y = H / 2; door.add(field);
      for (const s of [-1, 1]) {
        const post = new THREE.Mesh(new THREE.BoxGeometry(0.18, H, 0.3), darkMat);
        post.position.set(s * (C / 2 - 0.09), H / 2, 0); door.add(post);
        const strip = new THREE.Mesh(new THREE.BoxGeometry(0.06, H - 0.2, 0.32), glowMat);
        strip.position.set(s * (C / 2 - 0.09), H / 2, 0); door.add(strip);
        for (let k = 0; k < 4; k++) {  // anéis luminosos nos postes
          const ring = new THREE.Mesh(new THREE.BoxGeometry(0.26, 0.06, 0.38), glowMat);
          ring.position.set(s * (C / 2 - 0.09), 0.6 + k * 1.2, 0); door.add(ring);
        }
      }
      const beam = new THREE.Mesh(new THREE.BoxGeometry(C, 0.12, 0.34), glowMat);
      beam.position.y = 0.06; door.add(beam);
      // placas dos dois lados
      for (const side of [1, -1]) {
        const sign = new Panel(1.8, 0.42, 512, 'rgba(10,30,60,0.92)');
        sign.draw('s', (g, W, Hh, pp) => { pp.text(0, 0.06, 'ZONA SEGURA', 0.16, '#8fd8ff'); pp.text(0, -0.12, 'monstros não entram', 0.07, '#cfefff'); });
        sign.mesh.position.set(0, 3.3, side * 0.2);
        if (side < 0) sign.mesh.rotation.y = Math.PI;
        door.add(sign.mesh);
        signs.push(sign.mesh);
      }
    }
    // ---- faixa azul no chão marcando a borda da sala
    const [r0, c0, r1, c1] = z.rect;
    const x0 = world.gOx + c0 * C + 0.25, x1 = world.gOx + (c1 + 1) * C - 0.25;
    const z0 = world.gOz + r0 * C + 0.25, z1 = world.gOz + (r1 + 1) * C - 0.25;
    const strip = (cx, cz, w, d) => { const m = new THREE.Mesh(new THREE.BoxGeometry(w, 0.02, d), lineMat); m.position.set(cx, 0.012, cz); group.add(m); };
    strip((x0 + x1) / 2, z0, x1 - x0, 0.12); strip((x0 + x1) / 2, z1, x1 - x0, 0.12);
    strip(x0, (z0 + z1) / 2, 0.12, z1 - z0); strip(x1, (z0 + z1) / 2, 0.12, z1 - z0);
    // ---- emblema no centro do chão
    const center = new THREE.Vector3((x0 + x1) / 2, 0.015, (z0 + z1) / 2);
    const ring = new THREE.Mesh(new THREE.RingGeometry(1.0, 1.15, 48), lineMat);
    ring.rotation.x = -Math.PI / 2; ring.position.copy(center); group.add(ring);
    const ring2 = new THREE.Mesh(new THREE.RingGeometry(0.6, 0.68, 6), lineMat);
    ring2.rotation.x = -Math.PI / 2; ring2.position.copy(center).add(new THREE.Vector3(0, 0.002, 0)); group.add(ring2);
    const lbl = new Panel(2.6, 0.5, 512, null);
    lbl.draw('l', (g, W, Hh, pp) => pp.text(0, 0, 'ZONA SEGURA - ' + z.name.toUpperCase(), 0.2, '#6cc8ff'));
    lbl.mesh.rotation.x = -Math.PI / 2; lbl.mesh.position.copy(center).add(new THREE.Vector3(0, 0.02, 1.6)); group.add(lbl.mesh);
  }

  return {
    group,
    update(t) {
      tex.offset.y = -t * 0.12;
      tex.offset.x = Math.sin(t * 0.7) * 0.03;
      fieldMat.opacity = 0.55 + 0.25 * Math.sin(t * 2.3);
      const k = 0.75 + 0.25 * Math.sin(t * 3.1);
      glowMat.color.setRGB(0.25 * k, 0.7 * k, 1.0 * k);
    },
  };
}

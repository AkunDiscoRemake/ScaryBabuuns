// Carrega modelos .bm (exportados pelo Blender), texturas PNG e sons WAV.
import * as THREE from 'three';

export const F = { TINT: 1, UNLIT: 2, ALPHA: 4, SCREEN: 8, DOUBLE: 16, WIND: 32 };
const BASE = 'assets/';
const texCache = new Map();
const loader = new THREE.TextureLoader();

export function tex(name) {
  if (texCache.has(name)) return texCache.get(name);
  const t = loader.load(BASE + 'tex/' + name + '.png');
  t.flipY = false;                       // mesma convenção de UV do exportador
  t.wrapS = t.wrapT = THREE.RepeatWrapping;
  t.colorSpace = THREE.SRGBColorSpace;
  t.anisotropy = 2;
  texCache.set(name, t);
  return t;
}

const matCache = new Map();
function material(texName, color, flags) {
  const key = texName + '|' + color.join(',') + '|' + flags;
  if (matCache.has(key)) return matCache.get(key);
  const opts = { map: tex(texName), color: new THREE.Color(color[0], color[1], color[2]) };
  if (flags & F.ALPHA) opts.alphaTest = 0.5;
  if (flags & F.DOUBLE) opts.side = THREE.DoubleSide;
  let m;
  if (flags & F.UNLIT) { m = new THREE.MeshBasicMaterial(opts); m.fog = true; }
  else m = new THREE.MeshLambertMaterial({ ...opts, vertexColors: true });
  m.userData = { tint: !!(flags & F.TINT), screen: !!(flags & F.SCREEN), base: m.color.clone() };
  matCache.set(key, m);
  return m;
}

// Lê o formato .bm: nós com matriz local + seções (material, vértices, índices)
export function parseBM(buf) {
  const dv = new DataView(buf);
  const dec = new TextDecoder();
  let o = 4;
  const u32 = () => { const v = dv.getUint32(o, true); o += 4; return v; };
  const str = () => { const b = new Uint8Array(buf, o, 32); o += 32; const z = b.indexOf(0); return dec.decode(b.subarray(0, z < 0 ? 32 : z)); };
  u32();
  const n = u32();
  const nodes = [];
  const root = new THREE.Group();
  for (let i = 0; i < n; i++) {
    const name = str();
    const parent = dv.getInt32(o, true); o += 4;
    const m = new THREE.Matrix4().fromArray(new Float32Array(buf.slice(o, o + 64))); o += 64;
    const obj = new THREE.Group();
    obj.name = name;
    m.decompose(obj.position, obj.quaternion, obj.scale);
    obj.userData.baseQ = obj.quaternion.clone();
    obj.userData.basePos = obj.position.clone();
    const ns = u32();
    for (let s = 0; s < ns; s++) {
      const texName = str();
      const col = [dv.getFloat32(o, true), dv.getFloat32(o + 4, true), dv.getFloat32(o + 8, true), dv.getFloat32(o + 12, true)]; o += 16;
      const flags = u32(), vc = u32(), ic = u32();
      const vb = buf.slice(o, o + vc * 36); o += vc * 36;
      const ib = new Uint32Array(buf.slice(o, o + ic * 4)); o += ic * 4;
      const f32 = new Float32Array(vb), u8 = new Uint8Array(vb);
      const pos = new Float32Array(vc * 3), nrm = new Float32Array(vc * 3), uv = new Float32Array(vc * 2), colr = new Uint8Array(vc * 3);
      for (let v = 0; v < vc; v++) {
        const b = v * 9;
        pos[v * 3] = f32[b]; pos[v * 3 + 1] = f32[b + 1]; pos[v * 3 + 2] = f32[b + 2];
        nrm[v * 3] = f32[b + 3]; nrm[v * 3 + 1] = f32[b + 4]; nrm[v * 3 + 2] = f32[b + 5];
        uv[v * 2] = f32[b + 6]; uv[v * 2 + 1] = f32[b + 7];
        colr[v * 3] = u8[v * 36 + 32]; colr[v * 3 + 1] = u8[v * 36 + 33]; colr[v * 3 + 2] = u8[v * 36 + 34];
      }
      const g = new THREE.BufferGeometry();
      g.setAttribute('position', new THREE.BufferAttribute(pos, 3));
      g.setAttribute('normal', new THREE.BufferAttribute(nrm, 3));
      g.setAttribute('uv', new THREE.BufferAttribute(uv, 2));
      g.setAttribute('color', new THREE.BufferAttribute(colr, 3, true));
      g.setIndex(new THREE.BufferAttribute(vc > 65535 ? ib : new Uint16Array(ib), 1));
      g.computeBoundingSphere();
      const mesh = new THREE.Mesh(g, material(texName, col, flags));
      mesh.matrixAutoUpdate = false;
      obj.add(mesh);
    }
    nodes.push(obj);
    (parent >= 0 ? nodes[parent] : root).add(obj);
  }
  return root;
}

const modelCache = new Map();
export async function loadModels(names, onProgress) {
  let done = 0;
  await Promise.all(names.map(async n => {
    const r = await fetch(BASE + 'models/' + n + '.bm');
    modelCache.set(n, parseBM(await r.arrayBuffer()));
    onProgress && onProgress(++done / names.length);
  }));
}

// Cópia de um modelo. Materiais "tint" são clonados para poder mudar a cor por instância.
export function model(name, tint) {
  const src = modelCache.get(name);
  if (!src) return new THREE.Group();
  const g = src.clone(true);
  const tinted = [];
  g.traverse(o => {
    if (o.userData.baseQ) { o.userData = { ...o.userData }; }
    if (o.isMesh && (o.material.userData.tint || o.material.userData.screen)) {
      o.material = o.material.clone();
      o.material.userData = { ...o.material.userData };
      if (o.material.userData.tint) tinted.push(o.material);
    }
  });
  g.userData.tinted = tinted;
  g.userData.nodes = {};
  g.traverse(o => { if (o.name) g.userData.nodes[o.name] = o; });
  if (tint) setTint(g, tint);
  return g;
}

export function setTint(g, color) {
  for (const m of g.userData.tinted || []) m.color.copy(m.userData.base).multiply(color);
}

export async function loadText(path) { return (await fetch(BASE + path)).text(); }

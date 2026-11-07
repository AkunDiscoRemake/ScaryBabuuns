#!/usr/bin/env python3
"""Renderizador de pré-visualização por software dos arquivos .bm (valida o exportador).
uso: preview_bm.py saida.png modelo.bm [yaw_graus] [pitch_graus] [dist]"""
import struct, sys, os, math
import numpy as np
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def load_tex(name, cache={}):
    if name not in cache:
        with open(os.path.join(ROOT, 'app/assets/tex', name + '.tex'), 'rb') as f:
            assert f.read(4) == b'BTEX'
            w, h = struct.unpack('<HH', f.read(4))
            cache[name] = np.frombuffer(f.read(), np.uint8).reshape(h, w, 4).astype(np.float32) / 255
    return cache[name]


def load_bm(path):
    d = open(path, 'rb').read()
    assert d[:4] == b'BMDL'
    o = 4
    ver, n = struct.unpack_from('<II', d, o); o += 8
    nodes = []
    for i in range(n):
        name = d[o:o + 32].split(b'\0')[0].decode(); o += 32
        par, = struct.unpack_from('<i', d, o); o += 4
        m = np.array(struct.unpack_from('<16f', d, o), np.float32).reshape(4, 4).T; o += 64
        ns, = struct.unpack_from('<I', d, o); o += 4
        secs = []
        for s in range(ns):
            tex = d[o:o + 32].split(b'\0')[0].decode(); o += 32
            col = struct.unpack_from('<4f', d, o); o += 16
            flags, vc, ic = struct.unpack_from('<III', d, o); o += 12
            vb = np.frombuffer(d, np.dtype([('p', '<3f4'), ('n', '<3f4'), ('uv', '<2f4'), ('c', 'u1', 4)]), vc, o); o += vc * 36
            ib = np.frombuffer(d, '<u4', ic, o); o += ic * 4
            secs.append((tex, col, flags, vb, ib))
        nodes.append((name, par, m, secs))
    return nodes


def render(path, out, yaw=30, pitch=15, dist=None, W=640, H=640, tint=(0.85, 0.45, 0.2)):
    nodes = load_bm(path)
    world = []
    for i, (name, par, m, secs) in enumerate(nodes):
        world.append(world[par] @ m if par >= 0 else m)
    tris = []
    for i, (name, par, m, secs) in enumerate(nodes):
        M = world[i]
        for tex, col, flags, vb, ib in secs:
            p = (np.c_[vb['p'], np.ones(len(vb))] @ M.T)[:, :3]
            n = vb['n'] @ M[:3, :3].T
            tris.append((p, n, vb['uv'], vb['c'] / 255.0, ib.reshape(-1, 3), tex, np.array(col), flags))
    allp = np.concatenate([t[0] for t in tris])
    lo, hi = allp.min(0), allp.max(0)
    ctr = (lo + hi) / 2
    rad = np.linalg.norm(hi - lo) / 2
    dist = dist or rad * 2.6
    cy, sy = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    cp, sp = math.cos(math.radians(pitch)), math.sin(math.radians(pitch))
    # câmera olhando para o centro, de frente (-Z do modelo fica de frente p/ câmera)
    eye = ctr + np.array([sy * cp, sp, -cy * cp]) * dist
    if os.environ.get('EYE'):
        eye = np.array([float(v) for v in os.environ['EYE'].split(',')])
        ctr = np.array([float(v) for v in os.environ['TGT'].split(',')])
    fwd = (ctr - eye); fwd /= np.linalg.norm(fwd)
    right = np.cross(fwd, [0, 1, 0]); right /= np.linalg.norm(right)
    up = np.cross(right, fwd)
    f = 1 / math.tan(math.radians(25))
    zb = np.full((H, W), np.inf)
    img = np.zeros((H, W, 3)); img[:] = (0.12, 0.13, 0.16)
    L = np.array([0.4, 0.8, -0.45]); L /= np.linalg.norm(L)
    for p, n, uv, c, ib, tex, col, flags in tris:
        T = load_tex(tex)
        th, tw = T.shape[:2]
        rel = p - eye
        x, y, z = rel @ right, rel @ up, rel @ fwd
        sx = (x / z * f * 0.5 + 0.5) * W
        syy = (0.5 - y / z * f * 0.5) * H
        for a, b, cc in ib:
            if z[a] < 0.05 or z[b] < 0.05 or z[cc] < 0.05:
                continue
            xs = np.array([sx[a], sx[b], sx[cc]]); ys = np.array([syy[a], syy[b], syy[cc]])
            area = (xs[1] - xs[0]) * (ys[2] - ys[0]) - (xs[2] - xs[0]) * (ys[1] - ys[0])
            if abs(area) < 1e-9:
                continue
            if area > 0 and not (flags & 16):
                continue  # backface (CCW na tela = frente)
            x0, x1 = int(max(0, xs.min())), int(min(W - 1, xs.max() + 1))
            y0, y1 = int(max(0, ys.min())), int(min(H - 1, ys.max() + 1))
            if x0 > x1 or y0 > y1:
                continue
            gx, gy = np.meshgrid(np.arange(x0, x1 + 1) + 0.5, np.arange(y0, y1 + 1) + 0.5)
            w0 = ((xs[1] - gx) * (ys[2] - gy) - (xs[2] - gx) * (ys[1] - gy)) / area
            w1 = ((xs[2] - gx) * (ys[0] - gy) - (xs[0] - gx) * (ys[2] - gy)) / area
            w2 = 1 - w0 - w1
            m = (w0 >= 0) & (w1 >= 0) & (w2 >= 0)
            if not m.any():
                continue
            zz = w0 * z[a] + w1 * z[b] + w2 * z[cc]
            sub = zb[y0:y1 + 1, x0:x1 + 1]
            m &= zz < sub
            if not m.any():
                continue
            u = (w0 * uv[a, 0] + w1 * uv[b, 0] + w2 * uv[cc, 0])
            v = (w0 * uv[a, 1] + w1 * uv[b, 1] + w2 * uv[cc, 1])
            tx = (np.floor(u * tw).astype(int)) % tw
            ty = (np.floor(v * th).astype(int)) % th
            texel = T[ty, tx]
            if flags & 4:
                m &= texel[..., 3] > 0.5
                if not m.any():
                    continue
            nn = n[a] * w0[..., None] + n[b] * w1[..., None] + n[cc] * w2[..., None]
            nn /= np.linalg.norm(nn, axis=-1, keepdims=True) + 1e-9
            vc = c[a, :3] * w0[..., None] + c[b, :3] * w1[..., None] + c[cc, :3] * w2[..., None]
            rgb = texel[..., :3] * col[:3] * vc
            if flags & 1:
                rgb = rgb * np.array(tint)
            if not flags & 2:
                lam = np.clip(nn @ L, 0, 1)
                rgb = rgb * (0.55 + 0.6 * lam[..., None])
            sub[m] = zz[m]
            img[y0:y1 + 1, x0:x1 + 1][m] = rgb[m]
    Image.fromarray((np.clip(img, 0, 1) ** (1 / 1.1) * 255).astype(np.uint8)).save(out)


if __name__ == '__main__':
    a = sys.argv
    render(a[2], a[1], *(float(x) for x in a[3:]))

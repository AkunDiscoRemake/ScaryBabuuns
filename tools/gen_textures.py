#!/usr/bin/env python3
"""Gera texturas procedurais (todas originais) para o Babuun Hollow.

Saídas:
  build/tex/<nome>.png       -> usadas pelo Blender (preview / UV)
  app/assets/tex/<nome>.tex  -> formato do motor: 'BTEX' u16 w, u16 h, RGBA8
"""
import os, struct
import numpy as np
from PIL import Image, ImageDraw, ImageFilter

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT_PNG = os.path.join(ROOT, 'build', 'tex')
OUT_TEX = os.path.join(ROOT, 'app', 'assets', 'tex')
os.makedirs(OUT_PNG, exist_ok=True)
os.makedirs(OUT_TEX, exist_ok=True)
rng = np.random.default_rng(7)


def noise(size, cells, seed=None):
    """Value noise bilinear e repetível (tileable)."""
    r = np.random.default_rng(seed) if seed is not None else rng
    g = r.random((cells, cells))
    t = np.arange(size) * cells / size
    i0 = np.floor(t).astype(int) % cells
    i1 = (i0 + 1) % cells
    f = t - np.floor(t)
    f = f * f * (3 - 2 * f)
    a = g[i0][:, i0] * (1 - f)[None, :] + g[i0][:, i1] * f[None, :]
    b = g[i1][:, i0] * (1 - f)[None, :] + g[i1][:, i1] * f[None, :]
    return a * (1 - f)[:, None] + b * f[:, None]


def fbm(size, base=4, octaves=5, seed=None):
    out = np.zeros((size, size))
    amp, tot = 1.0, 0.0
    for o in range(octaves):
        c = base * (2 ** o)
        if c > size:
            break
        out += noise(size, c, None if seed is None else seed + o) * amp
        tot += amp
        amp *= 0.5
    return out / tot


def stretch_noise(size, cx, cy, seed=None):
    """Ruído anisotrópico (fibras/veios)."""
    r = np.random.default_rng(seed)
    g = r.random((cy, cx))
    img = Image.fromarray((g * 255).astype(np.uint8)).resize((size, size), Image.BICUBIC)
    return np.asarray(img).astype(float) / 255.0


def colorize(v, stops):
    """v em [0,1] -> gradiente de cores [(pos,(r,g,b)),...]"""
    v = np.clip(v, 0, 1)
    out = np.zeros(v.shape + (3,))
    pos = [s[0] for s in stops]
    for c in range(3):
        out[..., c] = np.interp(v, pos, [s[1][c] for s in stops])
    return out


def save(name, rgb, alpha=None):
    rgb = np.clip(rgb, 0, 255).astype(np.uint8)
    h, w = rgb.shape[:2]
    a = np.full((h, w), 255, np.uint8) if alpha is None else np.clip(alpha, 0, 255).astype(np.uint8)
    rgba = np.dstack([rgb, a])
    Image.fromarray(rgba, 'RGBA').save(os.path.join(OUT_PNG, name + '.png'))
    with open(os.path.join(OUT_TEX, name + '.tex'), 'wb') as f:
        f.write(b'BTEX' + struct.pack('<HH', w, h) + rgba.tobytes())
    print('textura', name, w, h)


def fur(size, seed, base_lo, base_hi, streak=0.55):
    """Pelo: muitos fios curtos desenhados + ruído."""
    v = fbm(size, 4, 5, seed) * 0.5
    img = Image.new('L', (size, size), 0)
    d = ImageDraw.Draw(img)
    r = np.random.default_rng(seed)
    for _ in range(size * 14):
        x, y = r.random() * size, r.random() * size
        L = r.uniform(4, 11) * size / 256
        ang = np.pi / 2 + r.normal(0, 0.35)
        c = int(r.uniform(60, 255))
        for ox in (-size, 0, size):
            for oy in (-size, 0, size):
                d.line([(x + ox, y + oy), (x + ox + np.cos(ang) * L, y + oy + np.sin(ang) * L)], fill=c, width=1)
    s = np.asarray(img.filter(ImageFilter.GaussianBlur(0.6))).astype(float) / 255
    v = v + s * streak
    v = (v - v.min()) / (v.max() - v.min())
    return colorize(v, [(0, base_lo), (1, base_hi)])


S = 256
# ---- Pelos ----
# Pelo do jogador: cinza claro (é multiplicado pela cor escolhida no jogo)
save('fur_player', fur(512, 11, (120, 120, 120), (255, 255, 255)))
save('fur_babuun', fur(512, 12, (52, 44, 28), (150, 128, 82)))      # oliva/acastanhado
save('fur_palido', fur(512, 13, (120, 118, 112), (232, 230, 222)))  # pálido doentio
save('fur_mamae', fur(512, 14, (22, 14, 10), (92, 62, 40)))          # escuro com juba

# ---- Peles ----
v = fbm(S, 6, 5, 21)
wr = stretch_noise(S, 6, 60, 22)
save('skin_monkey', colorize(v * 0.7 + wr * 0.3, [(0, (150, 104, 76)), (1, (226, 178, 140))]))
v = fbm(S, 8, 5, 23)
wr = stretch_noise(S, 4, 90, 24)
skin = colorize(v * 0.6 + wr * 0.4, [(0, (92, 46, 46)), (0.55, (170, 92, 92)), (1, (214, 150, 140))])
save('skin_babuun', skin)
save('skin_palido', colorize(v * 0.6 + wr * 0.4, [(0, (140, 136, 140)), (1, (236, 228, 230))]))
save('gums', colorize(fbm(S, 8, 4, 25), [(0, (110, 10, 18)), (1, (200, 50, 60))]))
save('teeth', colorize(fbm(S, 8, 4, 26), [(0, (170, 160, 110)), (1, (240, 232, 200))]))

# Olho (íris central)
yy, xx = np.mgrid[0:S, 0:S] / S - 0.5
r = np.sqrt(xx ** 2 + yy ** 2)
ang = np.arctan2(yy, xx)
streaks = 0.5 + 0.5 * np.sin(ang * 40 + fbm(S, 8, 3, 27) * 10)
eye = colorize(1 - r * 2, [(0, (245, 240, 230)), (0.45, (240, 236, 226)), (0.5, (60, 36, 20)),
                           (0.7, (110, 70, 34)), (0.82, (10, 8, 6)), (1, (0, 0, 0))])
eye *= (0.85 + 0.15 * streaks)[..., None]
save('eye', eye)

# ---- Natureza ----
v = fbm(S, 4, 6, 31)
gr = stretch_noise(S, 128, 128, 32)
ground = colorize(v * 0.75 + gr * 0.25, [(0, (24, 30, 14)), (0.4, (40, 52, 22)), (0.7, (58, 72, 30)), (1, (82, 74, 46))])
dirt = fbm(S, 3, 4, 33)
ground = np.where((dirt > 0.62)[..., None], colorize(v, [(0, (46, 34, 22)), (1, (92, 72, 50))]), ground)
save('ground', ground)

v = stretch_noise(S, 10, 2, 34) * 0.4 + stretch_noise(S, 40, 6, 35) * 0.35 + fbm(S, 8, 4, 36) * 0.25
cracks = (stretch_noise(S, 14, 3, 37) > 0.78).astype(float)
save('bark', colorize(v - cracks * 0.3, [(0, (26, 18, 12)), (0.5, (66, 48, 32)), (1, (110, 86, 60))]))

# Folhagem (com transparência recortada)
leaf = fbm(S, 6, 5, 38)
img = Image.new('L', (S, S), 0)
d = ImageDraw.Draw(img)
rr = np.random.default_rng(39)
for _ in range(260):
    x, y = rr.random() * S, rr.random() * S
    a = rr.random() * np.pi
    L = rr.uniform(10, 22)
    pts = [(x, y), (x + np.cos(a + 0.4) * L * 0.5, y + np.sin(a + 0.4) * L * 0.5),
           (x + np.cos(a) * L, y + np.sin(a) * L), (x + np.cos(a - 0.4) * L * 0.5, y + np.sin(a - 0.4) * L * 0.5)]
    d.polygon(pts, fill=int(rr.uniform(120, 255)))
la = np.asarray(img).astype(float) / 255
save('leaves', colorize(leaf * 0.5 + la * 0.5, [(0, (10, 26, 10)), (0.5, (28, 62, 22)), (1, (70, 110, 40))]), alpha=(la > 0.3) * 255)

# Pedra de templo (blocos + musgo)
v = fbm(S, 6, 6, 41)
by = (np.arange(S) % 64 < 3)
bx = ((np.arange(S)[None, :] + (np.arange(S)[:, None] // 64 % 2) * 48) % 96 < 3)
mortar = by[:, None] | bx
moss = fbm(S, 4, 5, 42)
stone = colorize(v, [(0, (70, 70, 64)), (1, (150, 146, 130))])
stone = np.where(mortar[..., None], stone * 0.45, stone)
stone = np.where((moss > 0.6)[..., None], colorize(moss, [(0.6, (40, 70, 26)), (1, (80, 110, 40))]), stone)
save('stone', stone)

v = fbm(S, 3, 7, 43)
st = stretch_noise(S, 3, 40, 44)
save('rock', colorize(v * 0.7 + st * 0.3, [(0, (40, 38, 36)), (0.5, (86, 82, 76)), (1, (140, 134, 122))]))

# Tábuas de madeira
v = stretch_noise(S, 3, 70, 45) * 0.5 + stretch_noise(S, 6, 200, 46) * 0.3 + fbm(S, 8, 4, 47) * 0.2
pl = np.arange(S) % 51
plank = colorize(v, [(0, (62, 40, 22)), (0.5, (112, 76, 44)), (1, (158, 116, 72))])
plank = np.where((pl < 2)[None, :, None].repeat(S, 0), plank * 0.35, plank)
off = (np.arange(S) // 51)[None, :] * 37
nails = ((((np.arange(S)[:, None] + off) % 128) < 3) & ((pl > 23) & (pl < 27))[None, :])
plank = np.where(nails[..., None], 40, plank)
save('plank', plank.transpose(1, 0, 2))

# Caixote
cr = plank.copy()
m = np.zeros((S, S), bool)
m[:14, :] = m[-14:, :] = m[:, :14] = m[:, -14:] = True
diag = np.abs(np.arange(S)[:, None] - np.arange(S)[None, :]) < 10
m |= diag
cr = np.where(m[..., None], cr * 0.7 + 25, cr)
save('crate', cr)

# Metal enferrujado
v = fbm(S, 6, 6, 51)
rust = fbm(S, 3, 6, 52)
metal = colorize(v, [(0, (60, 64, 66)), (1, (120, 126, 128))])
metal = np.where((rust > 0.55)[..., None], colorize(rust, [(0.55, (100, 50, 20)), (1, (160, 84, 34))]), metal)
corr = (np.arange(S) % 32 < 16)[None, :, None]
save('metal', metal * np.where(corr, 1.0, 0.85))

# Banana dourada
v = fbm(S, 6, 4, 61)
save('banana', colorize(v, [(0, (200, 140, 20)), (0.6, (255, 210, 60)), (1, (255, 246, 170))]))

# Tecido / cosméticos
v = fbm(S, 16, 3, 71)
weave = (np.sin(np.arange(S) * 1.6)[None, :] * np.sin(np.arange(S) * 1.6)[:, None]) * 0.5 + 0.5
save('cloth', colorize(v * 0.6 + weave * 0.4, [(0, (150, 150, 150)), (1, (255, 255, 255))]))
save('gold', colorize(fbm(S, 8, 4, 72), [(0, (150, 100, 20)), (0.5, (230, 180, 50)), (1, (255, 240, 160))]))
save('white', np.full((16, 16, 3), 255.0))
# Lata/papelão p/ lobby
save('poster', colorize(fbm(S, 4, 5, 73), [(0, (150, 128, 96)), (1, (214, 196, 160))]))
# Câmera (plástico preto com grip)
v = fbm(S, 32, 3, 74)
save('plastic', colorize(v, [(0, (20, 20, 22)), (1, (60, 60, 66))]))

# ================= DEPÓSITO (instalação de concreto) =================
# Concreto manchado com marcas de forma
v = fbm(S, 4, 7, 81)
stain = fbm(S, 2, 4, 82)
drip = stretch_noise(S, 30, 3, 83)
conc = colorize(v * 0.8 + drip * 0.2, [(0, (92, 94, 96)), (0.5, (128, 130, 132)), (1, (160, 162, 164))])
conc *= (0.78 + 0.22 * np.clip(stain * 1.6 - 0.2, 0, 1))[..., None]
holes = (noise(S, 64, 84) > 0.93)
conc = np.where(holes[..., None], conc * 0.55, conc)
seam = (np.arange(S) % 128 < 2)
conc = np.where(seam[:, None, None] | seam[None, :, None], conc * 0.7, conc)
save('concrete', conc)
# Piso de ladrilho cinza com rejunte e sujeira
v = fbm(S, 8, 5, 85)
dirt = fbm(S, 3, 5, 86)
tile = colorize(v, [(0, (150, 152, 150)), (1, (196, 198, 196))])
r2 = np.random.default_rng(87).uniform(0.88, 1.08, (4, 4))
tile *= np.kron(r2, np.ones((S // 4, S // 4)))[..., None]
g = (np.arange(S) % (S // 4) < 3)
tile = np.where(g[:, None, None] | g[None, :, None], (80, 82, 82), tile)
tile *= (0.75 + 0.25 * (1 - np.clip(dirt * 1.5 - 0.4, 0, 1)))[..., None]
crk = np.zeros((S, S)); yy = np.random.default_rng(88)
for _ in range(4):
    x, y = yy.integers(0, S, 2)
    for k in range(60):
        x = (x + yy.integers(-1, 3)) % S; y = (y + yy.integers(-1, 2)) % S; crk[y, x] = 1
tile = np.where((crk > 0)[..., None], tile * 0.5, tile)
save('tile', tile)
# Teto de placas
v = fbm(S, 8, 4, 89)
ceil = colorize(v, [(0, (170, 170, 168)), (1, (205, 205, 200))])
g = (np.arange(S) % (S // 2) < 4)
ceil = np.where(g[:, None, None] | g[None, :, None], (110, 110, 110), ceil)
spots = noise(S, 48, 90) > 0.9
ceil = np.where(spots[..., None], ceil * 0.8, ceil)
save('ceiling', ceil)
# Aço pintado (prateleiras/mesas) com arranhões
v = fbm(S, 6, 5, 91)
scr = stretch_noise(S, 200, 8, 92) > 0.82
steel = colorize(v, [(0, (70, 74, 78)), (1, (112, 116, 120))])
steel = np.where(scr[..., None], steel * 1.35, steel)
holes = ((np.arange(S)[:, None] % 32 - 16) ** 2 + (np.arange(S)[None, :] % 32 - 16) ** 2) < 10
save('steel', np.where(holes[..., None], 25, steel))
# Painel fluorescente
p = np.full((64, 64, 3), 255.0)
p[:, :4] = p[:, -4:] = p[:4, :] = p[-4:, :] = 150
for k in range(8, 64, 8): p[:, k] = 230
save('fluor', p)
# Caixa de brinquedo (papelão com logo genérico)
v = fbm(S, 6, 4, 93)
card = colorize(v, [(0, (150, 112, 70)), (1, (196, 156, 104))])
card[100:156, 40:216] = (220, 60, 50)
card[110:146, 60:196] = (250, 220, 60)
card[:, 126:130] = card[:, 126:130] * 0.7
save('toybox', card)
# Vinil de bola (listras coloridas — tingida no jogo)
yy2, xx2 = np.mgrid[0:S, 0:S]
stripes = ((xx2 // (S // 6)) % 3)
ball = np.choose(stripes[..., None].repeat(3, 2), [np.full((S, S, 3), (230, 60, 50.)), np.full((S, S, 3), (60, 120, 230.)), np.full((S, S, 3), (250, 210, 60.))])
save('toyball', ball * (0.85 + 0.15 * fbm(S, 8, 3, 94))[..., None])

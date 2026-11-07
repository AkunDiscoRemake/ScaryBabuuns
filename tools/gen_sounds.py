#!/usr/bin/env python3
"""Sintetiza todos os sons do Babuun Hollow (originais) -> app/assets/snd/*.wav (PCM16 mono 22050 Hz)."""
import os, wave
import numpy as np

SR = 22050
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, 'app', 'assets', 'snd')
os.makedirs(OUT, exist_ok=True)
rng = np.random.default_rng(99)


def t(d):
    return np.arange(int(SR * d)) / SR


def env(n, a=0.005, r=0.2, curve=3.0):
    x = np.arange(n) / SR
    e = np.minimum(1, x / max(a, 1e-4))
    rel = np.clip(1 - (x - a) / max(r, 1e-4), 0, 1) ** curve
    return e * np.where(x > a, rel, 1)


def lp(x, cut):
    """passa-baixa 1 polo (cut pode ser array)."""
    cut = np.broadcast_to(np.asarray(cut, float), x.shape)
    a = np.exp(-2 * np.pi * cut / SR)
    y = np.empty_like(x)
    s = 0.0
    for i in range(len(x)):
        s = (1 - a[i]) * x[i] + a[i] * s
        y[i] = s
    return y


def hp(x, cut):
    return x - lp(x, cut)


def bp(x, f, q=4.0):
    """filtro passa-banda biquad (f pode variar devagar — usa média por blocos)."""
    f = np.broadcast_to(np.asarray(f, float), x.shape)
    y = np.zeros_like(x)
    x1 = x2 = y1 = y2 = 0.0
    for i in range(len(x)):
        w = 2 * np.pi * min(f[i], SR * 0.45) / SR
        al = np.sin(w) / (2 * q)
        b0, b2 = al, -al
        a0, a1, a2 = 1 + al, -2 * np.cos(w), 1 - al
        yi = (b0 * x[i] + b2 * x2 - a1 * y1 - a2 * y2) / a0
        x2, x1 = x1, x[i]
        y2, y1 = y1, yi
        y[i] = yi
    return y


def noise(d):
    return rng.uniform(-1, 1, int(SR * d))


def osc_saw(freq, d):
    ph = np.cumsum(np.broadcast_to(freq, (int(SR * d),)) / SR)
    return 2 * (ph % 1) - 1


def osc_sin(freq, d):
    ph = np.cumsum(np.broadcast_to(freq, (int(SR * d),)) / SR)
    return np.sin(2 * np.pi * ph)


def save(name, x, gain=0.9):
    x = np.asarray(x, float)
    m = np.max(np.abs(x)) or 1
    x = x / m * gain
    # fade curto anti-clique
    f = min(64, len(x) // 4)
    x[:f] *= np.linspace(0, 1, f)
    x[-f:] *= np.linspace(1, 0, f)
    with wave.open(os.path.join(OUT, name + '.wav'), 'wb') as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(SR)
        w.writeframes((x * 32767).astype('<i2').tobytes())
    print('som', name, round(len(x) / SR, 2), 's')


def loopify(x, xf=0.5):
    n = int(SR * xf)
    a, b = x[:n], x[-n:]
    w = np.linspace(0, 1, n)
    y = x[:-n].copy()
    y[:n] = a * w + b * (1 - w)
    return y


# ---------- toques de mão ----------
def tap(body_f, body_q, noise_cut, d=0.18, thump=80):
    n = noise(d) * env(int(SR * d), 0.001, d * 0.6, 4)
    s = bp(n, body_f, body_q) * 3 + lp(n, noise_cut) * 0.6
    s += osc_sin(thump * np.exp(-t(d) * 18), d) * env(int(SR * d), 0.001, 0.08, 3) * 0.9
    return s

save('tap_wood', tap(420, 6, 1800))
save('tap_ground', tap(180, 2, 900, thump=60))
save('tap_stone', tap(1200, 8, 3000, d=0.14, thump=110))
save('tap_metal', tap(900, 1.5, 4000, d=0.4) + osc_sin(1430, 0.4) * env(int(SR * .4), .001, .38, 2) * 0.3
     + osc_sin(2210, 0.4) * env(int(SR * .4), .001, .3, 2) * 0.2)
save('tap_leaf', hp(noise(0.25), 1500) * env(int(SR * .25), .01, .2, 2))
w = noise(0.35)
save('whoosh', bp(w, 300 + 1200 * np.sin(np.linspace(0, np.pi, len(w))), 1.2) * np.sin(np.linspace(0, np.pi, len(w))) ** 2)

# ---------- itens / UI ----------
def chime(notes, d=0.9):
    out = np.zeros(int(SR * d))
    for i, f in enumerate(notes):
        st = int(i * 0.08 * SR)
        s = (osc_sin(f, d) + 0.3 * osc_sin(f * 2.01, d) + 0.1 * osc_sin(f * 3.03, d)) * env(int(SR * d), .002, d * .9, 3)
        out[st:] += s[:len(out) - st]
    return out

save('banana', chime([784, 988, 1175, 1568]))
save('badge', chime([523, 659, 784, 1047, 1319, 1568], 1.6))
save('ui_click', tap(2400, 10, 6000, d=0.06, thump=600), 0.6)
save('ui_error', osc_saw(140, 0.25) * env(int(SR * .25), .005, .2, 1), 0.5)
save('rec_start', np.concatenate([osc_sin(880, .09) * env(int(SR * .09), .002, .08), osc_sin(1320, .12) * env(int(SR * .12), .002, .11)]), 0.6)
save('rec_stop', np.concatenate([osc_sin(1320, .09) * env(int(SR * .09), .002, .08), osc_sin(660, .15) * env(int(SR * .15), .002, .14)]), 0.6)
save('shutter', hp(noise(0.12), 2000) * env(int(SR * .12), .001, .05, 2) + np.pad(tap(1800, 6, 5000, .1, 300), (0, 441)) * 0.5, 0.7)

# portão abrindo: rangido metálico + baque
d = 3.0
creak = osc_saw(70 + 30 * np.sin(t(d) * 3) + 18 * noise(d) * 0.3, d)
creak = bp(creak, 700 + 300 * np.sin(t(d) * 1.7), 6) * (0.5 + 0.5 * (noise(d) > -0.4))
creak *= env(int(SR * d), 0.3, 2.6, 1)
boom = tap(90, 1.2, 400, d=1.2, thump=45)
g = creak.copy(); g[-len(boom):] += boom * 2
save('gate_open', g)

# ---------- vozes do jogador ----------
def hoot(f0, d=0.45):
    tt = t(d)
    f = f0 * (1 + 0.5 * np.sin(np.pi * tt / d))
    v = osc_saw(f, d)
    v = bp(v, 600 + 500 * np.sin(np.pi * tt / d), 3) + bp(v, 1400, 5) * 0.4
    return v * env(len(tt), 0.03, d * 0.8, 1.5)

save('hoot', np.concatenate([hoot(260), hoot(300, .35)]))

# ---------- monstros ----------
def growl(f0, d, rough=0.6, formant=(500, 900)):
    tt = t(d)
    jit = lp(noise(d), 30) * 25
    f = f0 + jit + 10 * np.sin(tt * 7)
    v = osc_saw(f, d) * (1 + rough * lp(noise(d), 60))
    v = bp(v, formant[0], 3) + bp(v, formant[1], 4) * 0.7 + lp(noise(d), 1200) * 0.25
    return v * env(len(tt), 0.08, d * 0.85, 1.2)

def screech(f0, d, f1=None):
    tt = t(d)
    f1 = f1 or f0 * 1.6
    f = f0 + (f1 - f0) * np.sin(np.pi * tt / d) + 40 * lp(noise(d), 40)
    mod = osc_sin(f * 1.5, d) * f * 1.2
    v = osc_sin(f + mod, d) * 0.6 + bp(noise(d), f * 1.1, 2) * 0.8 + osc_saw(f * 0.5, d) * 0.3
    return np.tanh(v * 2) * env(len(tt), 0.02, d * 0.9, 1.3)

save('babuun_growl', growl(85, 1.4))
save('babuun_bark', np.concatenate([growl(140, .25, 1, (700, 1300)), np.zeros(1500), growl(150, .22, 1, (800, 1400))]))
save('babuun_screech', screech(700, 1.1, 1400))
save('palido_breath', lp(noise(2.2), 1800 + 1200 * np.sin(np.linspace(0, 2 * np.pi, int(SR * 2.2)))) *
     (0.4 + 0.6 * np.sin(np.linspace(0, 2 * np.pi, int(SR * 2.2))) ** 2))
save('palido_shriek', screech(1100, 1.4, 2400) + screech(1650, 1.4, 3000) * 0.5)
save('mamae_roar', growl(55, 2.2, 0.9, (300, 650)) + growl(58, 2.2, 0.9, (350, 700)) * 0.6)

def step(f, d=0.3):
    return tap(f, 1.5, f * 4, d=d, thump=f * 0.6)
save('step_light', step(140, .18), 0.7)
save('step_heavy', step(70, .45) * 1.0 + lp(noise(.45), 200) * env(int(SR * .45), .001, .3) * 0.6)

# ---------- jumpscares (estrondo + acorde dissonante + grito) ----------
def jumpscare(scream, base):
    d = 2.2
    n = len(t(d))
    hit = lp(noise(d), 3000) * env(n, 0.001, 0.6, 2) * 1.5
    chord = sum(osc_saw(base * r, d) for r in (1, 1.059, 1.414, 1.888)) * env(n, 0.001, 2.0, 1.5) * 0.4
    sc = np.zeros(n)
    sc[:len(scream)] += scream[:n]
    return np.tanh((hit + chord + sc * 1.4) * 1.8)

save('js_babuun', jumpscare(screech(800, 1.8, 1600), 110), 1.0)
save('js_palido', jumpscare(screech(1300, 2.0, 2800), 155), 1.0)
save('js_mamae', jumpscare(growl(60, 2.0, 1.0, (300, 700)) * 1.5 + np.pad(screech(500, 1.6, 900), (0, 8820)), 65), 1.0)

# ---------- ambiente e música ----------
d = 12.0
n = len(t(d))
wind = lp(noise(d), 250 + 200 * lp(noise(d), 0.3) * 30) * 0.7
crick = np.zeros(n)
for k in range(5):
    f = rng.uniform(3800, 5200)
    rate = rng.uniform(14, 22)
    gate = (np.sin(2 * np.pi * rate * t(d) + rng.random() * 6) > 0.6) * (np.sin(2 * np.pi * rng.uniform(.1, .3) * t(d)) > -0.2)
    crick += osc_sin(f, d) * lp(gate.astype(float), 200) * 0.08
owl = np.zeros(n)
for st in (2.5, 7.8):
    h = osc_sin(380 * (1 - 0.06 * t(.5)), .5) * env(int(SR * .5), .05, .4, 1.5)
    i = int(st * SR); owl[i:i + len(h)] += h * 0.25
    i += int(.7 * SR); owl[i:i + len(h)] += h * 0.2
save('amb_night', loopify(wind + crick + owl), 0.5)

# Lobby: zumbido quente + caixinha de música (melodia original)
d = 16.0
n = len(t(d))
hum = lp(noise(d), 120) * 0.3 + osc_sin(55, d) * 0.05
mel = [0, 4, 7, 9, 7, 4, 2, 4, 0, -3, 0, 2, 4, 2, 0, -5]
mb = np.zeros(n)
for i, s in enumerate(mel):
    f = 523.25 * 2 ** (s / 12)
    st = int(i * SR * 0.9)
    k = chime([f], 1.4) * 0.4
    mb[st:st + len(k)] += k[:max(0, n - st)]
save('amb_lobby', loopify(hum + mb * 0.6), 0.45)

# Perseguição: pulso de tambor + drone grave
d = 8.0
n = len(t(d))
drone = osc_saw(55, d) * 0.3 + osc_saw(55 * 1.06, d) * 0.25
drone = lp(drone, 400 + 300 * (np.sin(t(d) * 2 * np.pi * 0.25) * 0.5 + 0.5))
drum = np.zeros(n)
beat = 60 / 150
for i in range(int(d / beat)):
    k = tap(70, 1, 300, d=.3, thump=55) * (1.0 if i % 2 == 0 else 0.6)
    st = int(i * beat * SR)
    drum[st:st + len(k)] += k[:max(0, n - st)]
    if i % 4 == 3:
        k = hp(noise(.12), 3000) * env(int(SR * .12), .001, .1) * 0.4
        st2 = st + int(beat * SR / 2)
        drum[st2:st2 + len(k)] += k[:max(0, n - st2)]
save('chase', loopify(drone + drum * 1.2, 0.2), 0.7)

d = 0.9
hb = np.zeros(len(t(d)))
for st, a in ((0, 1.0), (0.22, 0.7)):
    k = osc_sin(55 * np.exp(-t(.18) * 6), .18) * env(int(SR * .18), .005, .15, 2) * a
    i = int(st * SR); hb[i:i + len(k)] += k
save('heartbeat', hb, 0.8)
save('escape', chime([523, 659, 784, 1047, 784, 1047, 1319], 2.2))

#!/usr/bin/env python3
"""Converte texturas .tex (BTEX RGBA) em PNG para a versão web. uso: tex_to_png.py <dir_tex> <dir_png>"""
import os, struct, sys
from PIL import Image
src, dst = sys.argv[1], sys.argv[2]
os.makedirs(dst, exist_ok=True)
for f in sorted(os.listdir(src)):
    if not f.endswith('.tex') or f == 'font.tex':
        continue
    d = open(os.path.join(src, f), 'rb').read()
    w, h = struct.unpack('<HH', d[4:8])
    im = Image.frombytes('RGBA', (w, h), d[8:])
    if im.getextrema()[3][0] == 255:
        im = im.convert('RGB')
    im.save(os.path.join(dst, f[:-4] + '.png'), optimize=True)
print('texturas PNG prontas')

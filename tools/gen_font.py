#!/usr/bin/env python3
"""Atlas de fonte (Latin-1, 16x16 células de 64px) -> app/assets/font.tex + font.bin (larguras)."""
import os, struct
from PIL import Image, ImageDraw, ImageFont
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
C = 64
font = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf', 46)
img = Image.new('L', (C * 16, C * 16), 0)
d = ImageDraw.Draw(img)
widths = []
for i in range(256):
    ch = chr(i) if i >= 32 and not (127 <= i < 160) else ' '
    x, y = (i % 16) * C, (i // 16) * C
    w = d.textlength(ch, font=font)
    d.text((x + 4, y + 4), ch, font=font, fill=255)
    widths.append(min(C, int(w + 2)))
rgba = Image.merge('RGBA', [Image.new('L', img.size, 255)] * 3 + [img])
with open(os.path.join(ROOT, 'app/assets/tex/font.tex'), 'wb') as f:
    f.write(b'BTEX' + struct.pack('<HH', *img.size) + rgba.tobytes())
with open(os.path.join(ROOT, 'app/assets/font.bin'), 'wb') as f:
    f.write(bytes(widths))
print('fonte ok')

"""mix.py <set>: tex_<set> (the IGBs' textures) + x4_<set> (Real-ESRGAN x4plus) -> mix_<set>:
the road's (asphalt, lines, walls, rails: the camera's nearest) at 4x up to 1024 a side, the
rest at 2x, the arcade's 1024s left out (pack.py keeps them as they are); a texture's own alpha
upscaled smoothly over the model's (which rings at cut-out edges)"""
import os
import sys
from PIL import Image

B = os.path.dirname(os.path.abspath(__file__)) + "/"
ROAD = ("asph", "line", "road", "wall", "kabe", "hei", "gaido", "guard", "teibou")
s = sys.argv[1]
src, x4, out = B + "tex_%s/" % s, B + "x4_%s/" % s, B + "mix_%s/" % s
os.makedirs(out, exist_ok=True)
n4 = n2 = px = 0
for f in sorted(os.listdir(src)):
    o = Image.open(src + f)
    w, h = o.size
    m = max(w, h)
    if m >= 1024 or not os.path.exists(x4 + f):
        continue
    k = min(4 if (any(r in f.lower() for r in ROAD) and m <= 256) else 2, 1024 // m)
    if k <= 1:
        continue
    # (colour and alpha resized each on its own: an RGBA resize weights the colour by the alpha,
    # and Tokyo's morning sky, alpha 0 with its colour used, came out black)
    u = Image.open(x4 + f).convert("RGB")
    tw, th = w * k, h * k
    if u.size != (tw, th):
        u = u.resize((tw, th), Image.LANCZOS)
    u.putalpha(255)
    if "A" in o.getbands():
        u.putalpha(o.convert("RGBA").getchannel("A").resize((tw, th), Image.BICUBIC))
    u.save(out + f)
    px += tw * th
    n4 += k == 4
    n2 += k == 2
print("%s: 4x %d, 2x %d, %d Mpx" % (s, n4, n2, px // 1000000))

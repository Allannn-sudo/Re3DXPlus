"""Pack IGB scenes into the iPad runtime format (.wpk).

Layout (little-endian, offsets are absolute):
  header   "WPK2" u32 n_tex u32 n_chunk u32 n_batch f32 chunk_size
           u32 tex_off u32 chunk_off u32 batch_off
  texture  char name[48] u16 w u16 h u32 fmt u32 data_off u32 data_size
           fmt 0 = RGB565, 1 = RGBA4444, 2 = RGBA5551 (16-bit, GL_UNSIGNED_SHORT_*)
  chunk    f32 min[3] f32 max[3] u32 first_batch u32 n_batch u32 group
           group: 0xffff a car's shadow volume (--shadow), 0 ground, 1 short-range buildings, 2 long-range buildings (the arcade's
           GroundModel / ShortRangeBuildModel / LongRangeBuildModel, each with its own range);
           3 + k: object k, shown on demand (--objects: each igGroup directly under a "ROOT_"
           group, e.g. the course arrow boards of one junction block); names in out.names
  batch    u16 tex (0xffff = none) u16 flags u32 vtx_off u32 vtx_count u32 idx_off u32 idx_count
           flags: bits 0-1 blend (1 alpha, 2 additive); bits 2-15 a texture flipbook
           (0 = none): 2-4 columns-1, 5-7 rows-1, 8-11 frames-1, 12-15 frames per second*2.
           Flipbook uvs address frame 0; frame i is offset by (i % cols / cols, i / cols / rows).
  vertex   f32 pos[3] f32 uv[2] u8 rgba[4]    (24 bytes)
  index    u16

"WPK3" (--lit, the cars) lights its batches the way the arcade's car programs do: each vertex
adds its normal (s16 normal[3], normalised, s16 pad: 32 bytes) and each batch record its
material (28 bytes): u8 program (0 fixed-function diffuse, 1 car.cg, 2 car_blend.cg), u8 pad,
u16 env texture (EnvMapTexture, unit 1), u16 specular map (unit 2), u16 env power * 10000
(car.cg's uniformEnvMapPower).

Within a chunk, batches are merged by (texture, flags, material) and opaque ones come first.

usage: pack.py out.wpk [--textures textures.igb] [--chunk 4000] [--max-tex 512] [--max-tex-build 256] ground.igb...
               [--short building.igb...] [--long building.igb...] [--place model.igb placements.txt]
               [--objects scene.igb...] [--swap texture=image.tga...]

--swap replaces a texture by an image file, matched by name without extension: a car's
igb carries a plain white body (BaseTexture in car.lua) that the arcade swaps for the chosen
paint (BodyColorTexture), e.g. --swap fd3s_body01_white=normal/color/fd3s_body01_colA01.tga.
--shade bakes sky light into the vertex colours from the normals (brighter facing up, darker
facing down): the renderer is unlit, and a car body with one flat colour reads as a cut-out.

--place copies one model to every placement (ground group), the way the arcade's
clGraphicFrame::setBarricade puts its guardrail across closed junctions; see barricades.py.
"""
import argparse
import hashlib
import math
import os
import struct
from collections import defaultdict

import numpy as np
from PIL import Image

import scene
import tex
from igb import IGB

OBJECTS = 3
GLOSS_MAX = 256  # the gloss maps' largest side (smooth masks; up to 512 in the arcade's data)
CUBE_MAX = 256   # the reflection cubes' (64 to 256 in the arcade's data)
SHADOW = 0xFFFF  # chunk group of a car's shadow volume (render.c render_shadows), never drawn as a model
WHEEL = 0x100   # chunk groups WHEEL + corner (FL, FR, RL, RR): a car's tyre and wheel there (render.h WPK_WHEEL)
LAMP_OFF = 0x110  # a car's lamps unlit (LIGHT_OFF, LAMP_REAR_OFF*): the menus' cars (render.h WPK_LAMP_OFF)
LAMP_ON = 0x111   # lit (LIGHT_ON, LAMP_REAR_HALF*): the race's cars (render.h WPK_LAMP_ON)
BRAKE_RUN = 0x112   # a car's brake lamp model off the brake: RUN (render.h WPK_BRAKE_RUN)
BRAKE_STOP = 0x113  # on it: STOP (render.h WPK_BRAKE_STOP)
BRAKE_LIGHT = 0x114  # the rest of it (LIGHT): the race's cars only (render.h WPK_BRAKE_LIGHT)
# its STOP_HIGH and STOP_WING2, on the brake for the wings whose bits (0..4) the group's low 5
# bits set (render.h WPK_BRAKE_HIGH, WPK_BRAKE_WING2)
BRAKE_HIGH = 0x140
BRAKE_WING2 = 0x160
# a car's dress-up parts, one chunk group per switch child: MIRROR1..2, WING1..4, BONNET1..4
# (clCarModelData's table 0x97773c0); render_params.mirror, wing and bonnet pick (render.h WPK_MIRROR ...)
MIRROR = 0x120
WING = 0x122
BONNET = 0x126
# a car's tyres for a dress-up wheel (TireStatus.scale[2], CustomTireLocation; the wheel comes
# from its own pack): CTIRE + corner like WHEEL (render.h WPK_CTIRE)
CTIRE = 0x12A
VERTEX = np.dtype([("pos", "<f4", 3), ("uv", "<f4", 2), ("rgba", "u1", 4)])
VERTEX_LIT = np.dtype([("pos", "<f4", 3), ("uv", "<f4", 2), ("rgba", "u1", 4), ("normal", "<i2", 3), ("pad", "<i2")])
BLEND_ONE = 1


def load_library(path):
    lib = IGB(path)
    out = {lib.string(o[22]): (lib, o) for o in lib.of_type("igImage") if o[22] is not None}
    # the gloss maps' bindings name them without the artist's path (sk_ginza_build_spc.tga for
    # D:/v363_shutoko/sourceimages/sk_ginza_build_spc.tga): those too, the first one of a name
    for name, v in list(out.items()):
        out.setdefault(name.replace("\\", "/").rsplit("/", 1)[-1], v)
    return out


def load_cubes(path):
    """textures.igb's igTextureCubeAttrs by the name the stage IGBs bind them by (their first
    face's file, e.g. 01ref_other.tga): (library IGB, the six faces' igImages in its order)"""
    lib = IGB(path)
    out = {}
    for c in lib.of_type("igTextureCubeAttr"):
        raw = c[19].raw
        faces = [lib.objects[i] for i in struct.unpack("<%di" % (len(raw) // 4), raw)]
        if len(faces) == 6 and faces[0][22] is not None:
            out.setdefault(lib.string(faces[0][22]).replace("\\", "/").rsplit("/", 1)[-1], (lib, faces))
    return out


def decode_image(f, img):
    data = f.ref(img[13])
    if data is None:
        return None
    try:
        return tex.to_pil(img, data.mem)
    except ValueError as e:
        print("skip texture", f.string(img[22]), e)
        return None


def tex_key(name):
    """A texture's name as car.lua knows it: some IGBs name theirs by the artist's full path
    (F:/.../sourceimages/gg3p_body01_white.tga)"""
    return name.replace("\\", "/").rsplit("/", 1)[-1].rsplit(".", 1)[0]


def load_image_file(path):
    """An image file (.tga, .png) the way IGB images are stored: PIL gives rows top down,
    IGB images and the UVs that map them run bottom up."""
    return Image.open(path).convert("RGBA").transpose(Image.FLIP_TOP_BOTTOM)


def encode16(im, max_size):
    w, h = im.size
    while max(w, h) > max_size:
        w, h = max(1, w // 2), max(1, h // 2)
    if (w, h) != im.size:
        im = im.resize((w, h), Image.LANCZOS)
    a = np.asarray(im, dtype=np.uint16)
    r, g, b, al = a[..., 0], a[..., 1], a[..., 2], a[..., 3]
    amin = int(al.min())
    if amin == 255:
        px = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
        fmt = 0
    elif np.all((al < 8) | (al > 247)):
        px = ((r >> 3) << 11) | ((g >> 3) << 6) | ((b >> 3) << 1) | (al >> 7)
        fmt = 2
    else:
        px = ((r >> 4) << 12) | ((g >> 4) << 8) | ((b >> 4) << 4) | (al >> 4)
        fmt = 1
    return w, h, fmt, px.astype("<u2").tobytes()


# The AI upscale's round trip (Android's packs): WPK_TEX_EXPORT=dir writes every texture as
# the IGB holds it to dir/<tex_file>.png (rows top down, as an image viewer shows them), before
# any shrinking; WPK_TEX_UPSCALED=dir takes dir/<tex_file>.png in its place where there is one,
# up to WPK_TEX_UPSCALED_MAX a side (default 2048) instead of the pack's --max-tex.
TEX_EXPORT = os.environ.get("WPK_TEX_EXPORT")
TEX_UPSCALED = os.environ.get("WPK_TEX_UPSCALED")
TEX_UPSCALED_MAX = int(os.environ.get("WPK_TEX_UPSCALED_MAX", "2048"))


def tex_file(name):
    """a texture's file name in those folders: its key, made safe, and a hash of its full name
    (keys repeat across IGBs: igb#index ones, and the same name over different images)"""
    key = "".join(c if c.isalnum() or c in "-_." else "_" for c in tex_key(name))[:60]
    return "%s_%s" % (key, hashlib.sha1(name.encode("utf-8", "replace")).hexdigest()[:8])


def upscale_round_trip(name, im, size):
    """im (or its upscaled version) and the largest side to keep of it"""
    if TEX_EXPORT:
        os.makedirs(TEX_EXPORT, exist_ok=True)
        out = os.path.join(TEX_EXPORT, tex_file(name) + ".png")
        if not os.path.exists(out):
            im.convert("RGBA").transpose(Image.FLIP_TOP_BOTTOM).save(out)
    if TEX_UPSCALED:
        up = os.path.join(TEX_UPSCALED, tex_file(name) + ".png")
        if os.path.exists(up):
            return load_image_file(up), max(size, TEX_UPSCALED_MAX)
    return im, size


# ASTC (Android's packs, render.c astc_upload): WPK_TEX_ASTC=4, 6 or 8 (the block's side)
# encodes the map's textures with ARM's astcenc (WPK_ASTCENC, its path) into wpk fmt
# WPK_ASTC (3) + 2 * block (0 4x4, 1 6x6, 2 8x8) + alpha, each mip level's 16-byte blocks from
# the base down to 1 x 1 (the base alone for a texture not a power of two)
TEX_ASTC = int(os.environ.get("WPK_TEX_ASTC", "0"))
ASTCENC = os.environ.get("WPK_ASTCENC", "astcenc")
ASTC_BLOCKS = {4: 0, 6: 1, 8: 2}


def astc_level(im, block):
    """one level's blocks (the .astc file's 16-byte header left off)"""
    import subprocess
    import tempfile
    with tempfile.TemporaryDirectory() as d:
        src, dst = os.path.join(d, "l.png"), os.path.join(d, "l.astc")
        im.save(src)
        subprocess.run([ASTCENC, "-cl", src, dst, "%dx%d" % (block, block), "-medium", "-silent"], check=True)
        with open(dst, "rb") as fp:
            return fp.read()[16:]


def resize_rgba(im, size):
    """im resized with Lanczos, colour and alpha each on its own: PIL's RGBA resize weights the
    colour by the alpha, and a texture whose alpha is 0 where its colour is used (Tokyo's
    morning sky, nondxt_ok_iw_sky_*: the colour alone drawn) came out black"""
    rgb = im.convert("RGB").resize(size, Image.LANCZOS)
    rgb.putalpha(im.getchannel("A").resize(size, Image.LANCZOS))
    return rgb


def encode_astc(im, max_size, block=None):
    """encode16's (w, h, fmt, bytes) in ASTC: the rows as encode16 keeps them (bottom up, as GL
    takes them), each mip level made by halving the one above with Lanczos"""
    block = block or TEX_ASTC
    w, h = im.size
    while max(w, h) > max_size:
        w, h = max(1, w // 2), max(1, h // 2)
    im = im.convert("RGBA")
    if (w, h) != im.size:
        im = resize_rgba(im, (w, h))
    alpha = int(np.asarray(im)[..., 3].min()) < 255
    pot = w & (w - 1) == 0 and h & (h - 1) == 0
    base, level, data = (w, h), im, []
    while True:
        data.append(astc_level(level, block))
        lw, lh = level.size
        if not pot or (lw == 1 and lh == 1):
            break
        level = resize_rgba(level, (max(1, lw // 2), max(1, lh // 2)))
    return base[0], base[1], 3 + 2 * ASTC_BLOCKS[block] + alpha, b"".join(data)


def flipbook_flags(mv):
    if mv is None:
        return 0
    fps, cols, rows, frames, _ = mv
    fps2 = min(15, max(1, int(round(fps * 2))))
    return (((cols - 1) & 7) << 2) | (((rows - 1) & 7) << 5) | (((frames - 1) & 15) << 8) | (fps2 << 12)


def placement_matrix(sx, pos, rot):
    """The arcade's barricade posture: v * scale(sx, 2, 1) * Ry * Rx * Rz * translate(pos)
    (Alchemy row vectors; setBarricade builds it with preRotation/preScale)."""
    def rot_m(axis, a):
        c, s = math.cos(a), math.sin(a)
        m = np.eye(4, dtype=np.float64)
        i, j = [(1, 2), (2, 0), (0, 1)][axis]
        m[i, i], m[i, j], m[j, i], m[j, j] = c, s, -s, c
        return m
    sc = np.diag([sx, 2.0, 1.0, 1.0])
    t = np.eye(4)
    t[3, :3] = pos
    return (sc @ rot_m(1, rot[1]) @ rot_m(0, rot[0]) @ rot_m(2, rot[2]) @ t).astype(np.float32)


def load_placements(path):
    """placements.txt: one 'scale_x x y z rot_x rot_y rot_z' per line (radians)"""
    out = []
    for line in open(path):
        v = line.split("#")[0].split()
        if len(v) == 7:
            v = [float(x) for x in v]
            out.append(placement_matrix(v[0], v[1:4], v[4:7]))
    return out


def sky_shade(normals, matrix):
    """Hemisphere light: 1.0 facing up, 0.75 sideways, 0.5 facing down"""
    n = normals[:, :3] @ matrix[:3, :3]
    ny = n[:, 1] / np.maximum(np.linalg.norm(n, axis=1), 1e-6)
    return 0.75 + 0.25 * ny


def collect(paths, library, max_tex, places=(), swaps=None, shade=False, keep=None, opaque=None, bgr=False,
            lit=False, cubes=None, cube_order=None):
    """paths: [(group, igb path)]; max_tex: largest texture side per group;
    places: [(igb path, [matrix] or a function mesh -> [matrix])] instanced into the ground group;
    swaps: {texture name without extension: image file}; keep: mesh -> False to leave it out;
    opaque: mesh -> True to draw it with the solid ones (alpha-tested, writing depth) although
    its IGB blends it; bgr: the IGBs' textures hold blue where red belongs and the other way round
    (data/othercar's bodies; not its brakelamp/ IGBs, whose glow maps are the cars' shared
    effect_cars ones, red as they are);
    lit: keep the normals and each mesh's car program with its env and specular maps (WPK3);
    cubes (load_cubes): keep the gloss program's meshes with their normals, gloss map and cube
    (its index in cube_order, which collect fills with the cubes' names)"""
    textures = {}   # name -> (w, h, fmt, bytes)
    order = []
    meshes = []
    objects = []    # names, by object index
    jobs = [(g, p, None) for g, p in paths] + [(0, p, mats) for p, mats in places]
    for group, path, instances in jobs:
        f, ms = scene.meshes(path)
        for m in ms:
            m["igb"] = f  # for opaque()
        if keep:
            ms = [m for m in ms if keep(m)]
        if callable(instances):  # a placement per mesh, e.g. a car's tyres by corner
            # (matrix, chunk group) puts the copy in a group of its own, e.g. WHEEL + corner
            ms = [dict(m, matrix=mat[0], group=mat[1]) if isinstance(mat, tuple) else dict(m, matrix=mat)
                  for m in ms for mat in instances(m)]
        elif instances is not None:
            ms = [dict(m, matrix=m["matrix"] @ inst) for inst in instances for m in ms]
        for m in ms:
            v = m["verts"]
            if len(v["pos"]) == 0 or len(m["tris"]) == 0:
                continue
            def add(img, size=None):
                """the name of image img (an igImage or a library name) in textures, None if none"""
                name = None
                if isinstance(img, str):
                    if library and img in library:
                        name = img
                        src = library[img]
                elif img is not None:
                    name = f.string(img[22]) or "%s#%d" % (path, img.index)
                    src = (f, img)
                if name is not None and name not in textures:
                    swap = (swaps or {}).get(tex_key(name))
                    if swap and not os.path.exists(swap):
                        swap = None  # car.lua names a file the arcade lacks (38rs_body02_spec.tga): the IGB's own image
                    im = load_image_file(swap) if swap else decode_image(*src)
                    if bgr and "/brakelamp/" not in path and im is not None and not swap:
                        r, g_, b, a_ = im.convert("RGBA").split()
                        im = Image.merge("RGBA", (b, g_, r, a_))
                    size = size or max_tex[min(group, 2)]
                    if im is not None and (TEX_EXPORT or TEX_UPSCALED):
                        im, size = upscale_round_trip(name, im, size)
                    enc = encode_astc if TEX_ASTC else encode16
                    textures[name] = enc(im, size) if im is not None else None
                    order.append(name)
                return name if name is not None and textures[name] is not None else None

            name = add(m["image"])
            n = len(v["pos"])
            vert = np.zeros(n, VERTEX)
            pos = np.c_[v["pos"], np.ones(n, np.float32)] @ m["matrix"]
            vert["pos"] = pos[:, :3]
            st = m["state"]
            if "uv" in v:
                vert["uv"] = v["uv"][:, :2]
                if st.get("movie"):
                    vert["uv"] *= st["movie"][4]
            if "color" in v:
                vert["rgba"] = v["color"][:, [2, 1, 0, 3]]  # BGRA -> RGBA
            else:
                vert["rgba"] = 255
            if shade and "normal" in v:
                k = sky_shade(v["normal"], m["matrix"])[:, None]
                vert["rgba"][:, :3] = np.clip(vert["rgba"][:, :3] * k, 0, 255).astype(np.uint8)
            mat = None
            if lit:
                vert = lit_vertices(vert, v, m["matrix"])
                prog = st.get("car_shader") or 0
                mat = (prog, add(st.get("env_image")) if prog else None, add(st.get("spec_image")) if prog == 2 else None,
                       int(round(st.get("env_power", 0.0) * 10000)) if prog == 1 else 0)
            elif cubes and st.get("gloss") and st["gloss"][1] in cubes and "normal" in v:
                gloss = add(st["gloss"][0], GLOSS_MAX)
                if gloss is not None:
                    vert = lit_vertices(vert, v, m["matrix"])
                    if st["gloss"][1] not in cube_order:
                        cube_order.append(st["gloss"][1])
                    mat = ("G", gloss, cube_order.index(st["gloss"][1]), 0)
            flags = flipbook_flags(st.get("movie")) if name is not None else 0
            if st["blend"] and not (opaque and opaque(m)):
                flags |= 2 if st["blend_func"][1] == BLEND_ONE else 1
            g = m.get("group", group)
            if group == OBJECTS:
                # Osaka's and Hakone's files have no "ROOT_": their groups sit under the top
                # ("yazirushi_all:ok_cousearrow_distribution" -> "yazirushi_all:OsOther")
                roots = [i for i, n in enumerate(m["path"]) if n.startswith("ROOT_")] or [0]
                if roots[-1] + 1 >= len(m["path"]):
                    continue
                obj = m["path"][roots[-1] + 1]
                if obj not in objects:
                    objects.append(obj)
                g = OBJECTS + objects.index(obj)
            meshes.append((g, name, flags, vert, m["tris"], mat))
    return textures, [n for n in order if textures[n] is not None], meshes, objects


def shadow_meshes(path, lit=False):
    """A car's shadow volume (car.lua ShadowIGB, othercar/LOD_*/shadow.igb: a prism from the
    car's floor 8 units down) as meshes of the SHADOW group: positions alone."""
    out = []
    for m in scene.meshes(path)[1]:
        pos = np.c_[m["verts"]["pos"], np.ones(len(m["verts"]["pos"]), np.float32)] @ m["matrix"]
        vert = np.zeros(len(pos), VERTEX_LIT if lit else VERTEX)
        vert["pos"] = pos[:, :3]
        vert["rgba"] = 255
        out.append((SHADOW, None, 0, vert, m["tris"], (0, None, None, 0) if lit else None))
    return out


def lit_vertices(vert, v, matrix):
    """vert with the mesh's normals (turned like its positions; straight up where it has none)"""
    out = np.zeros(len(vert), VERTEX_LIT)
    for k in ("pos", "uv", "rgba"):
        out[k] = vert[k]
    if "normal" in v:
        n = v["normal"][:, :3] @ matrix[:3, :3]
        n /= np.maximum(np.linalg.norm(n, axis=1), 1e-6)[:, None]
    else:
        n = np.tile(np.array([0, 1, 0], np.float32), (len(vert), 1))
    out["normal"] = np.clip(np.round(n * 32767), -32767, 32767).astype("<i2")
    return out


def fully_opaque(m):
    """A mesh its IGB blends although nothing of it is see-through (the traffic truck's body):
    blended without writing depth its far side shows through, so it goes with the solid ones."""
    img = m["image"]
    if img is None or isinstance(img, str) or ("color" in m["verts"] and m["verts"]["color"][:, 3].min() < 255):
        return False
    im = decode_image(m["igb"], img)
    return im is not None and im.getchannel("A").getextrema()[0] == 255


def split_by_chunk(meshes, chunk):
    """Assign each triangle to the chunk containing its centroid (chunk 0: one chunk per group,
    for a model such as a car that is drawn whole)."""
    cells = defaultdict(list)
    for group, name, flags, vert, tris, mat in meshes:
        if chunk <= 0:
            cells[(group, 0, 0)].append((name, flags, vert, tris, mat))
            continue
        cent = vert["pos"][tris].mean(axis=1)
        key = np.floor(cent[:, [0, 2]] / chunk).astype(np.int64)
        for k in np.unique(key, axis=0):
            sel = tris[(key == k).all(axis=1)]
            cells[(group,) + tuple(k)].append((name, flags, vert, sel, mat))
    return cells


def build_batches(parts, tex_index):
    """Merge parts with the same (texture, flags, material); split at 65535 vertices."""
    groups = defaultdict(list)
    for name, flags, vert, tris, mat in parts:
        if mat is not None and mat[0] == "G":
            mat = (mat[0], tex_index.get(mat[1], 0xFFFF), mat[2], 0)
        elif mat is not None:
            mat = (mat[0], tex_index.get(mat[1], 0xFFFF), tex_index.get(mat[2], 0xFFFF), mat[3])
        groups[(tex_index.get(name, 0xFFFF), flags, mat)].append((vert, tris))
    out = []
    for (t, flags, mat), items in sorted(groups.items(), key=lambda kv: (kv[0][1] & 3 != 0, kv[0][0], kv[0][2] or ())):
        vbuf, ibuf, base = [], [], 0
        for vert, tris in items:
            used, inv = np.unique(tris, return_inverse=True)
            if base + len(used) > 65535:
                out.append((t, flags, np.concatenate(vbuf), np.concatenate(ibuf), mat))
                vbuf, ibuf, base = [], [], 0
            vbuf.append(vert[used])
            ibuf.append((inv.reshape(-1) + base).astype("<u2"))
            base += len(used)
        if vbuf:
            out.append((t, flags, np.concatenate(vbuf), np.concatenate(ibuf), mat))
    return out


def write(path, textures, order, cells, chunk, lit=False, cubes=None, cube_order=()):
    tex_index = {n: i for i, n in enumerate(order)}
    chunks = []
    batches = []
    for key in sorted(cells):
        bs = build_batches(cells[key], tex_index)
        pos = np.concatenate([b[2]["pos"] for b in bs])
        chunks.append((pos.min(0), pos.max(0), len(batches), len(bs), key[0]))
        batches += bs

    hdr_size = 32
    tex_off = hdr_size
    chunk_off = tex_off + 64 * len(order)
    batch_off = chunk_off + 36 * len(chunks)
    data_off = batch_off + (28 if lit else 20) * len(batches)
    blob = bytearray()

    def put(b):
        while (data_off + len(blob)) % 4:
            blob.append(0)
        off = data_off + len(blob)
        blob.extend(b)
        return off

    tex_tbl = bytearray()
    for n in order:
        w, h, fmt, px = textures[n]
        off = put(px)
        tex_tbl += struct.pack("<48sHHIII", n.encode("latin1")[:47], w, h, fmt, off, len(px))
    batch_tbl = bytearray()
    gloss = []  # (batch, gloss texture, cube, normals' offset)
    for bi, (t, flags, vert, idx, mat) in enumerate(batches):
        if not lit and mat is not None and mat[0] == "G":
            plain = np.zeros(len(vert), VERTEX)
            for k in ("pos", "uv", "rgba"):
                plain[k] = vert[k]
            nrm = np.zeros((len(vert), 4), "<i2")
            nrm[:, :3] = vert["normal"]
            gloss.append((bi, mat[1], mat[2], put(nrm.tobytes())))
            vert = plain
        voff = put(vert.tobytes())
        ioff = put(idx.tobytes())
        batch_tbl += struct.pack("<HHIIII", t, flags, voff, len(vert), ioff, len(idx))
        if lit:
            batch_tbl += struct.pack("<BBHHH", mat[0], 0, mat[1], mat[2], mat[3])
    footer = b""
    if gloss:
        # the gloss section ("GLS1", found from the file's last 8 bytes: its offset, "GLS1"):
        # u32 n_cube, n_gloss; per cube its six faces in the library's order (GL's +X, -X, +Y,
        # -Y, +Z, -Z) {u16 size, u16 fmt, u32 off, u32 bytes}; per gloss batch {u32 batch,
        # u16 gloss texture, u16 cube, u32 normals' offset (s16 normal[3], s16 pad a vertex)}
        faces = []
        for name in cube_order:
            lib, imgs = cubes[name]
            for img in imgs:
                im = decode_image(lib, img)
                w, h, fmt, px = encode16(im, CUBE_MAX)
                faces.append(struct.pack("<HHII", w, fmt, put(px), len(px)))
        sec = struct.pack("<II", len(cube_order), len(gloss)) + b"".join(faces) + \
            b"".join(struct.pack("<IHHI", *g) for g in gloss)
        footer = struct.pack("<I4s", put(sec), b"GLS1")
        print("  gloss: %d batches, %d cubes (%s)" % (len(gloss), len(cube_order), ", ".join(cube_order)))
    chunk_tbl = bytearray()
    for lo, hi, first, n, group in chunks:
        chunk_tbl += struct.pack("<6fIII", *lo, *hi, first, n, group)
    with open(path, "wb") as fp:
        fp.write(struct.pack("<4sIIIfIII", b"WPK3" if lit else b"WPK2", len(order), len(chunks), len(batches),
                             chunk, tex_off, chunk_off, batch_off))
        fp.write(tex_tbl)
        fp.write(chunk_tbl)
        fp.write(batch_tbl)
        fp.write(blob)
        fp.write(footer)
    tris = sum(len(b[3]) // 3 for b in batches)
    tex_bytes = sum(len(textures[n][3]) for n in order)
    for g in range(OBJECTS):
        sel = [c for c in chunks if c[4] == g]
        if sel:
            print("  group %d: %d chunks, %d triangles" % (g, len(sel), sum(
                len(batches[i][3]) // 3 for c in sel for i in range(c[2], c[2] + c[3]))))
    print("%s: %d textures (%.1f MB), %d chunks, %d batches, %d triangles, %.1f MB total"
          % (path, len(order), tex_bytes / 1e6, len(chunks), len(batches), tris,
             (data_off + len(blob)) / 1e6))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("out")
    ap.add_argument("scenes", nargs="*")
    ap.add_argument("--short", nargs="+", default=[])
    ap.add_argument("--long", nargs="+", default=[])
    ap.add_argument("--objects", nargs="+", default=[])
    ap.add_argument("--swap", nargs="+", default=[], metavar="TEXTURE=IMAGE")
    ap.add_argument("--shade", action="store_true", help="bake sky light from the normals")
    ap.add_argument("--bgr", action="store_true", help="swap the textures' red and blue (data/othercar's)")
    ap.add_argument("--lit", action="store_true", help="keep normals and car programs for the renderer's lighting (WPK3)")
    ap.add_argument("--solid-opaque", action="store_true",
                    help="draw blended meshes whose texture and vertex colours are fully opaque with the solid ones")
    ap.add_argument("--shadow", help="a car's shadow volume IGB, kept as the shadow group")
    ap.add_argument("--textures")
    ap.add_argument("--place", nargs=2, action="append", default=[], metavar=("MODEL", "PLACEMENTS"))
    ap.add_argument("--chunk", type=float, default=4000.0, help="0: one chunk per group")
    ap.add_argument("--max-tex", type=int, default=512)
    ap.add_argument("--max-tex-build", type=int, default=256, help="for --short/--long textures not used by the ground")
    a = ap.parse_intermixed_args()
    library = load_library(a.textures) if a.textures else None
    cubes = load_cubes(a.textures) if a.textures and not a.lit else None
    cube_order = []
    paths = [(0, p) for p in a.scenes] + [(1, p) for p in a.short] + [(2, p) for p in a.long] + \
        [(OBJECTS, p) for p in a.objects]
    places = [(model, load_placements(txt)) for model, txt in a.place]
    textures, order, meshes, objects = collect(paths, library, (a.max_tex, a.max_tex_build, a.max_tex_build), places,
                                       dict(x.split("=", 1) for x in a.swap), a.shade, bgr=a.bgr,
                                       opaque=fully_opaque if a.solid_opaque else None, lit=a.lit,
                                       cubes=cubes, cube_order=cube_order)
    if a.shadow:
        meshes += shadow_meshes(a.shadow, a.lit)
    if objects:
        with open(a.out.rsplit(".", 1)[0] + ".names", "w") as fp:
            fp.write("".join(n + "\n" for n in objects))
        print("%d objects" % len(objects))
    write(a.out, textures, order, split_by_chunk(meshes, a.chunk), a.chunk, a.lit, cubes, cube_order)


if __name__ == "__main__":
    main()

# Babuun Hollow — modela TODOS os assets via Blender CLI e exporta no formato .bm do motor.
# Uso: blender -b -P tools/blender/build_assets.py
#
# Convenções (no Blender): Z para cima, personagens olhando para +Y.
# O exportador converte para o espaço do jogo: (x, y, z)_blender -> (x, z, -y)_jogo  (Y para cima, frente = -Z).
import bpy, bmesh, json, math, os, random, struct, sys
from mathutils import Vector, Matrix, Quaternion, noise
from mathutils.bvhtree import BVHTree

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
OUT = os.path.join(ROOT, 'app', 'assets', 'models')
TEXDIR = os.path.join(ROOT, 'build', 'tex')
os.makedirs(OUT, exist_ok=True)
random.seed(42)

# ---------------------------------------------------------------- materiais
F_TINT, F_UNLIT, F_ALPHA, F_SCREEN, F_DOUBLE, F_WIND = 1, 2, 4, 8, 16, 32
MATS = {
    # nome: (textura, cor rgba, flags, escala UV (m por repetição))
    'fur_player': ('fur_player', (1, 1, 1, 1), F_TINT, 0.35),
    'skin_player': ('skin_monkey', (1, 1, 1, 1), 0, 0.3),
    'palm': ('skin_monkey', (0.55, 0.42, 0.36, 1), 0, 0.2),
    'eye': ('eye', (1, 1, 1, 1), 0, 1),
    'pupil_glow': ('white', (1.0, 0.82, 0.2, 1), F_UNLIT, 1),
    'pupil_red': ('white', (1.0, 0.12, 0.08, 1), F_UNLIT, 1),
    'pupil_white': ('white', (0.95, 0.95, 1.0, 1), F_UNLIT, 1),
    'socket': ('white', (0.02, 0.01, 0.01, 1), F_UNLIT, 1),
    'nose': ('skin_monkey', (0.25, 0.16, 0.13, 1), 0, 0.2),
    'fur_babuun': ('fur_babuun', (1, 1, 1, 1), 0, 0.45),
    'fur_palido': ('fur_palido', (1, 1, 1, 1), 0, 0.5),
    'fur_mamae': ('fur_mamae', (1, 1, 1, 1), 0, 0.6),
    'skin_babuun': ('skin_babuun', (1, 1, 1, 1), 0, 0.3),
    'skin_palido': ('skin_palido', (1, 1, 1, 1), 0, 0.3),
    'gums': ('gums', (1, 1, 1, 1), 0, 0.2),
    'teeth': ('teeth', (1, 1, 1, 1), 0, 0.1),
    'ground': ('ground', (1, 1, 1, 1), 0, 6),
    'bark': ('bark', (1, 1, 1, 1), 0, 2),
    'leaves': ('leaves', (1, 1, 1, 1), F_ALPHA | F_DOUBLE | F_WIND, 2.5),
    'leaves_solid': ('leaves', (0.55, 0.6, 0.55, 1), 0, 2.5),
    'grass': ('leaves', (0.9, 1.0, 0.8, 1), F_ALPHA | F_DOUBLE | F_WIND, 1),
    'stone': ('stone', (1, 1, 1, 1), 0, 4),
    'rock': ('rock', (1, 1, 1, 1), 0, 5),
    'plank': ('plank', (1, 1, 1, 1), 0, 2.5),
    'crate': ('crate', (1, 1, 1, 1), 0, 1.2),
    'metal': ('metal', (1, 1, 1, 1), 0, 2.5),
    'banana': ('banana', (1, 1, 1, 1), F_UNLIT, 0.3),
    'gold': ('gold', (1, 1, 1, 1), 0, 0.3),
    'cloth': ('cloth', (1, 1, 1, 1), F_TINT, 0.3),
    'cloth_red': ('cloth', (0.8, 0.12, 0.1, 1), 0, 0.3),
    'cloth_blue': ('cloth', (0.15, 0.3, 0.8, 1), 0, 0.3),
    'cloth_black': ('cloth', (0.12, 0.12, 0.13, 1), 0, 0.3),
    'cloth_green': ('cloth', (0.2, 0.55, 0.2, 1), 0, 0.3),
    'lens': ('white', (0.04, 0.05, 0.08, 1), 0, 1),
    'glass_dark': ('white', (0.03, 0.03, 0.04, 1), 0, 1),
    'plastic': ('plastic', (1, 1, 1, 1), 0, 0.2),
    'screen': ('white', (1, 1, 1, 1), F_SCREEN | F_UNLIT, 1),
    'reclight': ('white', (1, 0.1, 0.1, 1), F_UNLIT, 1),
    'lamp': ('white', (1.0, 0.8, 0.45, 1), F_UNLIT, 1),
    'mushroom_glow': ('white', (0.35, 0.9, 1.0, 1), F_UNLIT, 1),
    'bone': ('teeth', (1, 0.97, 0.9, 1), 0, 0.2),
    'portal': ('white', (0.0, 0.0, 0.0, 1), F_UNLIT, 1),
    'portal_glow': ('white', (0.3, 1.0, 0.5, 1), F_UNLIT, 1),
    'slate': ('white', (0.08, 0.09, 0.08, 1), 0, 1),
    'button': ('white', (1, 1, 1, 1), F_TINT, 1),
    'rug': ('cloth', (0.55, 0.15, 0.12, 1), 0, 0.8),
    'white': ('white', (1, 1, 1, 1), 0, 1),
    'concrete': ('concrete', (1, 1, 1, 1), 0, 4),
    'tile': ('tile', (1, 1, 1, 1), 0, 2.4),
    'ceiling': ('ceiling', (1, 1, 1, 1), 0, 2.4),
    'steel': ('steel', (1, 1, 1, 1), 0, 1.5),
    'fluor': ('fluor', (1, 1, 1, 1), F_UNLIT, 1.4),
    'toybox': ('toybox', (1, 1, 1, 1), 0, 0.5),
    'toyball': ('toyball', (1, 1, 1, 1), 0, 0.4),
}


# ---------------------------------------------------------------- utilidades
def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)


def link(obj, coll=None):
    (coll or bpy.context.scene.collection).objects.link(obj)
    return obj


def mesh_obj(name, bm, mat, uv='box', smooth=True, coll=None, uvscale=None, tint=None):
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    if smooth:
        me.polygons.foreach_set('use_smooth', [True] * len(me.polygons))
    o = bpy.data.objects.new(name, me)
    o['mat'] = mat
    o['uv'] = uv
    if uvscale:
        o['uvscale'] = uvscale
    if tint:
        o['tint'] = tint
    return link(o, coll)


def T(bm, mat):
    bmesh.ops.transform(bm, matrix=mat, verts=bm.verts)


def bm_sphere(r=1, seg=16, rings=10):
    bm = bmesh.new()
    bmesh.ops.create_uvsphere(bm, u_segments=seg, v_segments=rings, radius=r)
    return bm


def bm_ico(r=1, sub=2):
    bm = bmesh.new()
    bmesh.ops.create_icosphere(bm, subdivisions=sub, radius=r)
    return bm


def bm_cyl(r1, r2, h, seg=12, caps=True):
    bm = bmesh.new()
    bmesh.ops.create_cone(bm, cap_ends=caps, segments=seg, radius1=r1, radius2=r2, depth=h)
    T(bm, Matrix.Translation((0, 0, h / 2)))  # base em z=0
    return bm


def bm_box(sx, sy, sz, bevel=0.0):
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1)
    T(bm, Matrix.Diagonal((sx, sy, sz, 1)))
    if bevel > 0:
        bmesh.ops.bevel(bm, geom=bm.edges[:] + bm.verts[:], offset=bevel, segments=2, affect='EDGES', profile=0.5)
    return bm


def displace(bm, amp, freq, seed=0.0, axis_scale=(1, 1, 1), outward_only=False):
    for v in bm.verts:
        p = v.co * freq + Vector((seed, seed * 1.7, seed * 0.3))
        n = noise.noise(p) + 0.5 * noise.noise(p * 2.1)
        d = v.co.normalized() if v.co.length > 1e-6 else Vector((0, 0, 1))
        if outward_only:
            n = abs(n)
        v.co += Vector((d.x * axis_scale[0], d.y * axis_scale[1], d.z * axis_scale[2])) * n * amp


def meta(name, elems, res=0.03, mat='fur_babuun', uv='box', coll=None, decimate=0.5, uvscale=None, thresh=0.35):
    """elems: lista de dicts {t:'BALL'|'ELLIPSOID'|'CAPSULE', co, r, s:(x,y,z), rot:(w,x,y,z), stiff}"""
    mb = bpy.data.metaballs.new(name + '_mb')
    mb.resolution = res
    mb.render_resolution = res
    mb.threshold = thresh
    for e in elems:
        el = mb.elements.new(type=e.get('t', 'BALL'))
        el.co = e['co']
        el.radius = e.get('r', 0.1)
        s = e.get('s')
        if s:
            el.size_x, el.size_y, el.size_z = s
        if 'rot' in e:
            el.rotation = Quaternion(e['rot'])
        el.stiffness = e.get('stiff', 2.0)
        if e.get('neg'):
            el.use_negative = True
    mo = bpy.data.objects.new(name + '_meta', mb)
    link(mo)
    dg = bpy.context.evaluated_depsgraph_get()
    me = bpy.data.meshes.new_from_object(mo.evaluated_get(dg))
    bpy.data.objects.remove(mo)
    me.name = name
    me.polygons.foreach_set('use_smooth', [True] * len(me.polygons))
    o = bpy.data.objects.new(name, me)
    o['mat'] = mat
    o['uv'] = uv
    if uvscale:
        o['uvscale'] = uvscale
    link(o, coll)
    if decimate < 1:
        m = o.modifiers.new('dec', 'DECIMATE')
        m.ratio = decimate
    return o


def capsule(a, b, r, stiff=2.0):
    """elemento CAPSULE do ponto a até b."""
    a, b = Vector(a), Vector(b)
    d = b - a
    q = Vector((1, 0, 0)).rotation_difference(d.normalized())
    return {'t': 'CAPSULE', 'co': (a + b) / 2, 'r': r, 's': (d.length / 2, 0, 0), 'rot': tuple(q), 'stiff': stiff}


def ball(c, r, stiff=2.0):
    return {'t': 'BALL', 'co': c, 'r': r, 'stiff': stiff}


def ell(c, r, s, rot=(1, 0, 0, 0), stiff=2.0):
    return {'t': 'ELLIPSOID', 'co': c, 'r': r, 's': s, 'rot': rot, 'stiff': stiff}


def upd():
    bpy.context.view_layer.update()


def parent(child, par):
    """Define pai mantendo a transformação de mundo."""
    upd()
    child.parent = par
    child.matrix_parent_inverse = par.matrix_world.inverted()
    return child


def empty(name, loc, coll=None):
    o = bpy.data.objects.new(name, None)
    o.location = loc
    return link(o, coll)


def set_origin(o, pivot):
    """Move a origem (articulação) para o ponto de mundo 'pivot' sem mover a geometria.
    Use ANTES de dar pais aos filhos."""
    upd()
    pv = Vector(pivot)
    off = o.matrix_world.translation - pv
    o.data.transform(Matrix.Translation(off))
    o.location = pv
    upd()
    return o


# ---------------------------------------------------------------- exportador
C3 = Matrix(((1, 0, 0), (0, 0, 1), (0, -1, 0)))
C4 = C3.to_4x4()


def conv_v(v):
    return Vector((v.x, v.z, -v.y))


def compute_uv(mode, p, n, s):
    if mode == 'box':
        a = (abs(n.x), abs(n.y), abs(n.z))
        if a[2] >= a[0] and a[2] >= a[1]:
            return (p.x / s, p.y / s)
        if a[0] >= a[1]:
            return (p.y / s, p.z / s)
        return (p.x / s, p.z / s)
    if mode == 'cyl':  # cilíndrico em torno de Z
        return (math.atan2(p.y, p.x) / (2 * math.pi) * 3, p.z / s)
    if mode == 'front':  # projeção frontal (+Y) para olhos
        r = s
        return (0.5 + p.x / (2 * r), 0.5 + p.z / (2 * r))
    if mode == 'sph':
        d = p.normalized() if p.length > 1e-6 else Vector((0, 0, 1))
        return (math.atan2(d.y, d.x) / (2 * math.pi) * 3, math.acos(max(-1, min(1, d.z))) / math.pi * 2)
    if mode == 'len':  # ao longo de Z (galhos/troncos deitados)
        return (math.atan2(p.y, p.x) / math.pi, p.z / s)
    return (0, 0)


def gather_mesh(o, dg):
    """Retorna lista de (pos_world, normal_world, uv, mat) por canto de triângulo + índices."""
    ev = o.evaluated_get(dg)
    me = ev.to_mesh()
    me.calc_loop_triangles()
    mw = o.matrix_world
    nm = mw.to_3x3().inverted().transposed()
    mode = o.get('uv', 'box')
    mat = o.get('mat', 'white')
    s = o.get('uvscale', MATS[mat][3])
    worlduv = o.get('worlduv', False)
    tint = o.get('tint', None)
    cn = me.corner_normals
    verts = []
    for tri in me.loop_triangles:
        for li in tri.loops:
            vi = me.loops[li].vertex_index
            p = me.vertices[vi].co.copy()
            n = cn[li].vector.copy()
            pw = mw @ p
            nw = (nm @ n).normalized()
            uvp = pw if worlduv else p
            uvn = nw if worlduv else n
            uv = compute_uv(mode, uvp, uvn, s)
            verts.append((pw, nw, uv, tuple(tint) if tint else (1, 1, 1)))
    ev.to_mesh_clear()
    return mat, verts


def bake_ao(entries, rays=10, dist=0.5, strength=0.85):
    """AO por vértice com raycast contra todos os triângulos do modelo."""
    allv, alltri = [], []
    for _, _, vs in entries:
        for i in range(0, len(vs), 3):
            b = len(allv)
            allv.extend([vs[i][0], vs[i + 1][0], vs[i + 2][0]])
            alltri.append((b, b + 1, b + 2))
    if not alltri:
        return {}
    bvh = BVHTree.FromPolygons(allv, alltri, epsilon=0.0)
    dirs = []
    for i in range(rays):  # hemisfério (fibonacci)
        z = (i + 0.5) / rays
        a = i * 2.39996
        rr = math.sqrt(1 - z * z)
        dirs.append(Vector((math.cos(a) * rr, math.sin(a) * rr, z)))
    cache = {}
    for _, _, vs in entries:
        for (p, n, _, _) in vs:
            key = (round(p.x, 3), round(p.y, 3), round(p.z, 3), round(n.x, 1), round(n.y, 1), round(n.z, 1))
            if key in cache:
                continue
            q = n.to_track_quat('Z', 'Y')
            occ = 0.0
            o = p + n * 0.01
            for d in dirs:
                wd = q @ d
                hit = bvh.ray_cast(o, wd, dist)
                if hit[0] is not None:
                    occ += 1 - hit[3] / dist
            cache[key] = 1 - strength * occ / rays
    return cache


def export(filename, objects, ao_dist=0.3, ao=True, chunk=None):
    """Exporta objetos (hierarquia preservada). Se chunk (tamanho em m) for dado, ignora
    hierarquia e funde tudo em nós 'chunk_i_j' por material (para o mapa)."""
    upd()
    dg = bpy.context.evaluated_depsgraph_get()
    entries = []  # (obj, mat, verts)
    for o in objects:
        if o.type == 'MESH':
            mat, vs = gather_mesh(o, dg)
            if vs:
                entries.append((o, mat, vs))
    aoc = bake_ao(entries, dist=ao_dist) if ao else {}
    baked = []
    for o, mat, vs in entries:
        if o.get('noao'):
            baked.append((o, mat, vs)); continue
        nv = []
        for (p, n, uv, tn) in vs:
            k = (round(p.x, 3), round(p.y, 3), round(p.z, 3), round(n.x, 1), round(n.y, 1), round(n.z, 1))
            a = aoc.get(k, 1.0)
            nv.append((p, n, uv, (tn[0] * a, tn[1] * a, tn[2] * a)))
        baked.append((o, mat, nv))
    entries = baked

    def aov(p, n):
        return 1.0

    nodes = []  # (name, parent_idx, matrix_game, sections{mat: (verts)})
    if chunk:
        groups = {}
        for o, mat, vs in entries:
            for i in range(0, len(vs), 3):
                c = (vs[i][0] + vs[i + 1][0] + vs[i + 2][0]) / 3
                key = (int(math.floor(c.x / chunk)), int(math.floor(c.y / chunk)))
                groups.setdefault(key, {}).setdefault(mat, []).extend(vs[i:i + 3])
        for key, secs in sorted(groups.items()):
            nodes.append(('chunk_%d_%d' % key, -1, Matrix.Identity(4), secs, True))
        for o in objects:
            if o.type == 'EMPTY':
                nodes.append((o.name, -1, C4 @ o.matrix_world @ C4.transposed(), {}, False))
    else:
        objs = [o for o in objects]
        idx = {o.name: i for i, o in enumerate(objs)}
        emap = {o.name: (mat, vs) for o, mat, vs in entries}
        for o in objs:
            pi = idx.get(o.parent.name, -1) if o.parent else -1
            local = (o.parent.matrix_world.inverted() @ o.matrix_world) if o.parent else o.matrix_world
            secs = {}
            if o.name in emap:
                mat, vs = emap[o.name]
                inv = o.matrix_world.inverted()
                ninv = o.matrix_world.to_3x3().transposed()  # normal: mundo -> local (rotação)
                loc = [(inv @ p, (ninv @ n).normalized(), uv, tn, p, n) for (p, n, uv, tn) in vs]
                secs[mat] = loc
            nodes.append((o.name, pi, C4 @ local @ C4.transposed(), secs, False))

    with open(os.path.join(OUT, filename), 'wb') as f:
        f.write(b'BMDL' + struct.pack('<II', 1, len(nodes)))
        tris = 0
        for name, pi, m, secs, world in nodes:
            f.write(name.encode()[:31].ljust(32, b'\0'))
            f.write(struct.pack('<i', pi))
            f.write(struct.pack('<16f', *[m[r][c] for c in range(4) for r in range(4)]))  # coluna-major
            f.write(struct.pack('<I', len(secs)))
            for mat, vs in secs.items():
                tex, col, flags, _ = MATS[mat]
                # deduplicação de vértices
                vmap, vbuf, ibuf = {}, bytearray(), []
                for v in vs:
                    if world:
                        p, n, uv, tn = v
                        pw, nw = p, n
                    else:
                        p, n, uv, tn, pw, nw = v
                    a = aov(pw, nw)
                    gp, gn = conv_v(p), conv_v(n)
                    rgba = tuple(int(max(0, min(255, a * tn[k] * 255))) for k in range(3)) + (255,)
                    key = (round(gp.x, 4), round(gp.y, 4), round(gp.z, 4), round(gn.x, 2), round(gn.y, 2),
                           round(gn.z, 2), round(uv[0], 3), round(uv[1], 3), rgba)
                    if key not in vmap:
                        vmap[key] = len(vmap)
                        vbuf += struct.pack('<8f4B', gp.x, gp.y, gp.z, gn.x, gn.y, gn.z, uv[0], 1 - uv[1], *rgba)
                    ibuf.append(vmap[key])
                f.write(tex.encode()[:31].ljust(32, b'\0'))
                f.write(struct.pack('<4fI', *col, flags))
                f.write(struct.pack('<II', len(vmap), len(ibuf)))
                f.write(vbuf)
                f.write(struct.pack('<%dI' % len(ibuf), *ibuf))
                tris += len(ibuf) // 3
    print('exportado %s: %d nós, %d triângulos' % (filename, len(nodes), tris))


# ================================================================= PERSONAGENS
def eye(name, c, r, mat='eye', look=(0, 1, 0)):
    bm = bm_sphere(r, 16, 10)
    o = mesh_obj(name, bm, mat, uv='front', uvscale=r)
    o.location = c
    q = Vector((0, 1, 0)).rotation_difference(Vector(look).normalized())
    o.rotation_mode = 'QUATERNION'
    o.rotation_quaternion = q
    return o


def build_player():
    reset()
    # Corpo SEM PERNAS: torso arredondado em gota, barriga de pele.
    body = meta('body', [
        ell((0, 0, 0), 0.2, (1.0, 0.85, 1.25)),
        ell((0, 0.0, -0.12), 0.2, (1.05, 0.95, 0.8)),
        ell((0, 0.02, 0.17), 0.16, (1.25, 0.9, 0.6)),  # ombros
        ball((0.17, 0, 0.14), 0.09), ball((-0.17, 0, 0.14), 0.09),
    ], res=0.018, mat='fur_player', decimate=0.45)
    belly = meta('belly', [ell((0, 0.12, -0.04), 0.15, (0.9, 0.45, 1.1))], res=0.015, mat='skin_player', decimate=0.5)
    parent(belly, body)
    head = meta('head', [
        ell((0, 0, 0), 0.16, (1.0, 0.95, 0.95)),
        ell((0, -0.02, 0.1), 0.1, (0.9, 0.9, 0.6)),       # topo
        ball((0, -0.03, 0.15), 0.05),                       # topete
        ball((0.03, -0.05, 0.19), 0.03), ball((-0.03, -0.04, 0.185), 0.03),
    ], res=0.014, mat='fur_player', decimate=0.45)
    head.location = (0, 0.02, 0.3)
    head.data.transform(Matrix.Translation((0, 0, 0)))
    face = meta('face', [
        ell((0, 0.105, 0.02), 0.11, (1.1, 0.5, 0.85)),     # máscara
        ell((0, 0.14, -0.06), 0.075, (1.15, 0.7, 0.6)),    # focinho
        ball((0.05, 0.1, 0.05), 0.06), ball((-0.05, 0.1, 0.05), 0.06),
    ], res=0.012, mat='skin_player', decimate=0.5)
    face.location = head.location
    ears = []
    for sx in (1, -1):
        e = meta('ear_%s' % ('r' if sx > 0 else 'l'), [ell((0, 0, 0), 0.06, (0.4, 1.0, 1.0))], res=0.012,
                 mat='fur_player', decimate=0.6)
        e.location = (head.location[0] + 0.155 * sx, head.location[1] - 0.01, head.location[2] + 0.01)
        ein = meta('earin_%s' % ('r' if sx > 0 else 'l'), [ell((0.012 * sx, 0, 0), 0.04, (0.35, 0.8, 0.8))], res=0.01,
                   mat='skin_player', decimate=0.6)
        ein.location = e.location
        parent(ein, e)
        ears.append(e)
    eyes = [eye('eye_r', (0.048, 0.125, 0.33), 0.034, look=(0.15, 1, 0)), eye('eye_l', (-0.048, 0.125, 0.33), 0.034, look=(-0.15, 1, 0))]
    brows = meta('brow', [capsule((-0.075, 0.13, 0.375), (0.075, 0.13, 0.375), 0.022)], res=0.01, mat='fur_player', decimate=0.6)
    nose = meta('nose', [ball((0.012, 0.21, 0.27), 0.012), ball((-0.012, 0.21, 0.27), 0.012)], res=0.006, mat='nose', decimate=0.7)
    mouth = meta('mouth', [capsule((-0.04, 0.188, 0.235), (0.04, 0.188, 0.235), 0.008)], res=0.006, mat='nose', decimate=0.7)
    parent(head, body)
    for o in [face, *ears, *eyes, brows, nose, mouth]:
        parent(o, head)
    objs = [body, belly, head, face, *ears, *[e.children[0] for e in ears], *eyes, brows, nose, mouth]
    # pontos de ancoragem de cosméticos
    hat = empty('anchor_hat', (0, 0.0, 0.46)); parent(hat, head)
    face_a = empty('anchor_face', (0, 0.155, 0.335)); parent(face_a, head)
    neck = empty('anchor_neck', (0, 0.13, 0.17)); parent(neck, body)
    sh_r = empty('shoulder_r', (0.2, 0.0, 0.14)); parent(sh_r, body)
    sh_l = empty('shoulder_l', (-0.2, 0.0, 0.14)); parent(sh_l, body)
    export('monkey.bm', objs + [hat, face_a, neck, sh_r, sh_l], ao_dist=0.12)

    # Mão (luvinha peluda com dedos) — origem no pulso, dedos para +Y
    reset()
    hand = meta('hand', [
        ell((0, 0.0, 0.0), 0.05, (1.0, 1.1, 0.75)),          # punho peludo
    ], res=0.008, mat='fur_player', decimate=0.5)
    palm = meta('palm', [ell((0, 0.06, -0.008), 0.056, (0.95, 0.95, 0.42)),
                         ell((0, 0.035, -0.004), 0.05, (0.9, 0.6, 0.45))], res=0.007, mat='palm', decimate=0.5)
    parts = [hand, palm]
    for i, x in enumerate((-0.033, -0.011, 0.011, 0.033)):
        L = 0.065 if i in (1, 2) else 0.055
        f = meta('finger%d' % i, [capsule((x, 0.095, -0.006), (x * 1.08, 0.095 + L * 0.55, -0.016), 0.0125, stiff=3),
                                  capsule((x * 1.08, 0.095 + L * 0.55, -0.016), (x * 1.12, 0.095 + L, -0.036), 0.011, stiff=3)],
                 res=0.005, mat='palm', decimate=0.6, thresh=0.6)
        parts.append(f)
    th = meta('thumb', [capsule((0.04, 0.04, -0.012), (0.068, 0.075, -0.03), 0.014, stiff=3),
                        capsule((0.068, 0.075, -0.03), (0.075, 0.1, -0.045), 0.012, stiff=3)], res=0.005, mat='palm', decimate=0.6, thresh=0.6)
    parts.append(th)
    for o in parts[1:]:
        parent(o, hand)
    export('hand.bm', parts, ao_dist=0.04)

    # Braço: cilindro afunilado de 0..1 em Z (jogo: Y), escalonado em tempo real
    reset()
    bm = bm_cyl(0.06, 0.045, 1.0, 14, caps=False)
    arm = mesh_obj('arm', bm, 'fur_player', uv='cyl', uvscale=0.3)
    export('arm.bm', [arm], ao=False)


def build_banana():
    reset()
    bm = bm_cyl(0.03, 0.03, 1.0, 10, caps=True)
    for v in bm.verts:
        t = v.co.z  # 0..1
        ang = (t - 0.5) * 1.6
        r = 0.6 * (1 - 0.55 * abs(t - 0.5) * 2)  # afina nas pontas
        x, y = v.co.x * r / 0.6 * 1.5, v.co.y * r / 0.6 * 1.5
        R = 0.22
        v.co = Vector((x + (R - R * math.cos(ang)), y, R * math.sin(ang)))
    T(bm, Matrix.Diagonal((1.2, 1.2, 1.2, 1)))
    b = mesh_obj('banana', bm, 'banana', uv='cyl', uvscale=0.1)
    tip = mesh_obj('stem', bm_cyl(0.012, 0.008, 0.05, 6), 'bark')
    tip.location = (0.06, 0, 0.26)
    parent(tip, b)
    export('banana.bm', [b, tip], ao=False)


# -------------------------------------------------------- MONSTROS (originais)
def quad_monster(prefix, S, furmat, skinmat, pupilmat, mane=False, fat=1.0):
    """Primata quadrúpede corcunda com focinho longo (estilo babuíno, design original).
    S = escala. Retorna lista de objetos."""
    reset()
    s = S
    body = meta('body', [
        ell((0, 0.25 * s, 0.78 * s), 0.3 * s, (1.0, 1.25, 0.95)),             # peito alto/ombros
        ell((0, -0.18 * s, 0.62 * s), 0.26 * s * fat, (0.95, 1.2, 0.9)),      # barriga
        ell((0, -0.45 * s, 0.62 * s), 0.22 * s, (1.0, 0.9, 0.9)),             # quadril
        ell((0, 0.05 * s, 0.98 * s), 0.18 * s, (1.0, 1.4, 0.6)),              # corcunda
    ] + ([ell((0, 0.45 * s, 0.95 * s), 0.3 * s, (1.4, 0.9, 1.2))] if mane else []),
        res=0.03 * s, mat=furmat, decimate=0.45)
    if mane:  # juba espetada
        bm = bmesh.new()
        for i in range(70):
            a = random.random() * math.pi * 2
            e = random.uniform(-0.4, 1.0)
            d = Vector((math.cos(a) * math.cos(e), math.sin(a) * math.cos(e) * 0.6 + 0.2, math.sin(e)))
            c = Vector((0, 0.42 * s, 0.95 * s)) + d * 0.32 * s
            sb = bm_cyl(0.07 * s, 0.0, random.uniform(0.25, 0.45) * s, 5)
            q = Vector((0, 0, 1)).rotation_difference(d + Vector((0, -0.6, 0.3)))
            T(sb, Matrix.Translation(c) @ q.to_matrix().to_4x4())
            me = bpy.data.meshes.new('tmp'); sb.to_mesh(me); sb.free(); bm.from_mesh(me); bpy.data.meshes.remove(me)
        maneo = mesh_obj('mane', bm, furmat, uv='box', uvscale=0.4)
        parent(maneo, body)
    # costelas aparentes (magreza) — sulcos negativos
    head_c = Vector((0, 0.68 * s, 1.0 * s))
    head = meta('head', [
        ell((0, 0, 0), 0.17 * s, (1.0, 1.0, 0.95)),
        ell((0, 0.06 * s, 0.07 * s), 0.13 * s, (1.1, 0.8, 0.5)),                # testa / sobrancelha pesada
        ball((0.08 * s, -0.06 * s, 0.1 * s), 0.07 * s), ball((-0.08 * s, -0.06 * s, 0.1 * s), 0.07 * s),
    ], res=0.02 * s, mat=furmat, decimate=0.5)
    head.location = head_c
    snout = meta('snout', [
        ell((0, 0.2 * s, -0.06 * s), 0.1 * s, (0.75, 1.9, 0.75)),               # focinho longo de cão
        ell((0, 0.36 * s, -0.04 * s), 0.07 * s, (0.95, 0.6, 0.75)),             # ponta do nariz
        ell((0, 0.14 * s, 0.03 * s), 0.08 * s, (1.2, 1.2, 0.5)),                # ponte
    ], res=0.012 * s, mat=skinmat, decimate=0.5)
    snout.location = head_c
    nostr = meta('nostrils', [ball((0.025 * s, 0.42 * s, -0.03 * s), 0.018 * s), ball((-0.025 * s, 0.42 * s, -0.03 * s), 0.018 * s)],
                 res=0.006 * s, mat='socket', decimate=0.7)
    nostr.location = head_c
    # olhos fundos e brilhantes
    eyes = []
    for sx in (1, -1):
        so = eye('socket_%d' % sx, head_c + Vector((0.065 * s * sx, 0.115 * s, 0.045 * s)), 0.032 * s, mat='socket')
        pu = eye('glow_%d' % sx, head_c + Vector((0.065 * s * sx, 0.13 * s, 0.045 * s)), 0.018 * s, mat=pupilmat)
        eyes += [so, pu]
    # maxilar com dentes (nó separado para abrir a boca)
    jaw_piv = head_c + Vector((0, 0.02 * s, -0.08 * s))
    jaw = meta('jaw', [ell((0, 0.2 * s, -0.13 * s), 0.08 * s, (0.75, 1.9, 0.45))], res=0.012 * s, mat=skinmat, decimate=0.5)
    jaw.location = head_c
    gum = meta('gums', [ell((0, 0.22 * s, -0.1 * s), 0.065 * s, (0.7, 1.9, 0.3))], res=0.01 * s, mat='gums', decimate=0.6)
    gum.location = head_c
    bm = bmesh.new()
    for side in (1, -1):
        for i in range(6):
            y = 0.1 + i * 0.045
            big = i == 4
            L = (0.12 if big else 0.035) * s
            for up in (1, -1):
                if up == 1 and not big and i % 2:
                    continue
                tb = bm_cyl(0.012 * s * (1.6 if big else 1), 0.001, L, 5)
                rot = Matrix.Rotation(0 if up == 1 else math.pi, 4, 'X')
                z = (-0.085 if up == 1 else -0.11) * s
                T(tb, Matrix.Translation(head_c + Vector((0.045 * s * side * (1 - i * 0.06), y * s, z))) @ rot)
                me = bpy.data.meshes.new('t'); tb.to_mesh(me); tb.free(); bm.from_mesh(me); bpy.data.meshes.remove(me)
    teeth = mesh_obj('teeth', bm, 'teeth', uv='box', uvscale=0.05)
    set_origin(jaw, jaw_piv)
    parent(gum, jaw); parent(teeth, jaw)
    # orelhas pequenas
    ears = []
    for sx in (1, -1):
        e = meta('ear_%d' % sx, [ell((0, 0, 0), 0.045 * s, (0.5, 0.9, 1.0))], res=0.01 * s, mat=skinmat, decimate=0.6)
        e.location = head_c + Vector((0.15 * s * sx, -0.05 * s, 0.08 * s))
        ears.append(e)
    set_origin(head, head_c - Vector((0, 0.12 * s, 0.02 * s)))  # pivô no pescoço
    for o in [snout, nostr, *eyes, *ears]:
        parent(o, head)
    parent(jaw, head)
    # pernas (4) — cada uma com pivô no ombro/quadril
    legs = []
    for nm, x, y, front in (('leg_fr', 0.17, 0.32, True), ('leg_fl', -0.17, 0.32, True), ('leg_br', 0.16, -0.48, False), ('leg_bl', -0.16, -0.48, False)):
        piv = Vector((x * s, y * s, (0.78 if front else 0.66) * s))
        if front:  # braço longo de primata, apoio nos nós dos dedos
            el = [capsule(piv, piv + Vector((0.02 * s, 0.06 * s, -0.4 * s)), 0.085 * s),
                  capsule(piv + Vector((0.02 * s, 0.06 * s, -0.4 * s)), piv + Vector((0.03 * s, 0.1 * s, -0.72 * s)), 0.065 * s),
                  ell(piv + Vector((0.03 * s, 0.14 * s, -0.75 * s)), 0.06 * s, (1.0, 1.3, 0.55))]
        else:  # perna traseira dobrada
            el = [capsule(piv, piv + Vector((0.02 * s, 0.18 * s, -0.3 * s)), 0.1 * s),
                  capsule(piv + Vector((0.02 * s, 0.18 * s, -0.3 * s)), piv + Vector((0.02 * s, -0.04 * s, -0.6 * s)), 0.07 * s),
                  ell(piv + Vector((0.02 * s, 0.06 * s, -0.63 * s)), 0.06 * s, (0.9, 1.7, 0.45))]
        lg = meta(nm, el, res=0.02 * s, mat=furmat, decimate=0.5)
        set_origin(lg, piv)
        parent(lg, body)
        legs.append(lg)
    # cauda curvada
    tail_piv = Vector((0, -0.6 * s, 0.72 * s))
    pts = [tail_piv + Vector((0, -0.12 * s * i, 0.1 * s * math.sin(i * 0.9) + 0.06 * s * i)) for i in range(6)]
    tail = meta('tail', [capsule(pts[i], pts[i + 1], 0.045 * s * (1 - i * 0.12)) for i in range(5)], res=0.012 * s, mat=furmat, decimate=0.5)
    set_origin(tail, tail_piv)
    parent(tail, body)
    parent(head, body)
    objs = [body] + list(body.children_recursive)
    return objs


def build_monsters():
    objs = quad_monster('babuun', 1.0, 'fur_babuun', 'skin_babuun', 'pupil_glow')
    export('babuun.bm', objs, ao_dist=0.18)
    objs = quad_monster('mamae', 2.0, 'fur_mamae', 'skin_babuun', 'pupil_red', mane=True, fat=1.35)
    export('mamae.bm', objs, ao_dist=0.35)
    build_palido()


def build_palido():
    """Babuíno pálido bípede, alto e esquelético (design original)."""
    reset()
    s = 1.0
    hip = Vector((0, 0, 1.25))
    body = meta('body', [
        ell((0, 0.05, 1.78), 0.26, (1.0, 0.75, 1.0)),    # peito
        ell((0, 0.08, 1.98), 0.22, (1.55, 0.8, 0.6)),    # ombros caídos
        ell((0, 0.02, 1.5), 0.19, (0.85, 0.7, 1.3)),     # cintura fina
        ell((0, -0.02, 1.27), 0.22, (1.05, 0.85, 0.75)), # quadril
        ell((0, 0.16, 2.12), 0.1, (1.0, 1.0, 1.6)),      # pescoço projetado
        ball((0.3, 0.08, 1.97), 0.12), ball((-0.3, 0.08, 1.97), 0.12),
        *[capsule((-0.16, 0.16, 1.62 + i * 0.07), (0.16, 0.16, 1.62 + i * 0.07), 0.035, stiff=1.2) for i in range(4)],
    ], res=0.02, mat='fur_palido', decimate=0.45)
    head_c = Vector((0, 0.28, 2.15))
    head = meta('head', [
        ell((0, 0, 0), 0.15, (0.9, 1.05, 1.15)),
        ball((0, -0.04, 0.1), 0.1),
    ], res=0.016, mat='fur_palido', decimate=0.5)
    head.location = head_c
    face = meta('face', [
        ell((0, 0.13, -0.02), 0.11, (0.95, 0.6, 1.1)),
        ell((0, 0.24, -0.1), 0.07, (0.8, 1.4, 0.7)),
    ], res=0.012, mat='skin_palido', decimate=0.5)
    face.location = head_c
    eyes = []
    for sx in (1, -1):
        eyes.append(eye('socket_%d' % sx, head_c + Vector((0.055 * sx, 0.16, 0.03)), 0.04, mat='socket'))
        eyes.append(eye('glow_%d' % sx, head_c + Vector((0.055 * sx, 0.192, 0.03)), 0.009, mat='pupil_white'))
    jaw_piv = head_c + Vector((0, 0.05, -0.1))
    jaw = meta('jaw', [ell((0, 0.2, -0.2), 0.08, (1.0, 1.3, 0.5))], res=0.012, mat='skin_palido', decimate=0.5)
    jaw.location = head_c
    bm = bmesh.new()
    for i in range(14):  # sorriso largo de dentes finos
        a = (i / 13 - 0.5) * 2.2
        for up in (1, -1):
            tb = bm_cyl(0.007, 0.0005, 0.05, 4)
            rot = Matrix.Rotation(0 if up == 1 else math.pi, 4, 'X')
            T(tb, Matrix.Translation(head_c + Vector((math.sin(a) * 0.08, 0.2 + math.cos(a) * 0.08, -0.14 if up == 1 else -0.17))) @ rot)
            me = bpy.data.meshes.new('t'); tb.to_mesh(me); tb.free(); bm.from_mesh(me); bpy.data.meshes.remove(me)
    teeth = mesh_obj('teeth', bm, 'teeth', uv='box', uvscale=0.05)
    gum = meta('gums', [ell((0, 0.21, -0.165), 0.075, (1.1, 1.2, 0.25))], res=0.01, mat='gums', decimate=0.6)
    gum.location = head_c
    set_origin(jaw, jaw_piv)
    parent(teeth, jaw); parent(gum, jaw)
    set_origin(head, head_c - Vector((0, 0.08, 0.12)))
    for o in [face, *eyes]:
        parent(o, head)
    parent(jaw, head)
    parent(head, body)
    parts = []
    for sx, nm in ((1, 'r'), (-1, 'l')):
        sh = Vector((0.3 * sx, 0.08, 1.97))
        el = [ball(sh, 0.1),
              capsule(sh, sh + Vector((0.05 * sx, 0.05, -0.6)), 0.075),
              ball(sh + Vector((0.05 * sx, 0.05, -0.6)), 0.07),
              capsule(sh + Vector((0.05 * sx, 0.05, -0.6)), sh + Vector((0.06 * sx, 0.15, -1.25)), 0.06),
              ell(sh + Vector((0.06 * sx, 0.18, -1.32)), 0.07, (0.7, 0.9, 1.4))]
        # dedos longos
        for k in range(4):
            b0 = sh + Vector((0.06 * sx + (k - 1.5) * 0.025, 0.18, -1.38))
            el.append(capsule(b0, b0 + Vector((0, 0.02, -0.2)), 0.02))
        arm = meta('arm_' + nm, el, res=0.012, mat='fur_palido', decimate=0.5)
        set_origin(arm, sh)
        parent(arm, body)
        hp = Vector((0.13 * sx, 0, 1.22))
        el = [ball(hp, 0.12),
              capsule(hp, hp + Vector((0.02 * sx, 0.12, -0.6)), 0.09),
              ball(hp + Vector((0.02 * sx, 0.12, -0.6)), 0.075),
              capsule(hp + Vector((0.02 * sx, 0.12, -0.6)), hp + Vector((0.03 * sx, -0.05, -1.15)), 0.065),
              ell(hp + Vector((0.03 * sx, 0.05, -1.18)), 0.055, (0.8, 2.0, 0.4))]
        leg = meta('leg_' + nm, el, res=0.014, mat='fur_palido', decimate=0.5)
        set_origin(leg, hp)
        parent(leg, body)
        parts += [arm, leg]
    objs = [body] + list(body.children_recursive)
    export('palido.bm', objs, ao_dist=0.2)


# ---------------------------------------------------------------- COSMÉTICOS
def build_cosmetics():
    def out(name, objs):
        export('cos_%s.bm' % name, objs, ao_dist=0.04)

    # boné (aba para frente)
    reset()
    bm = bm_sphere(0.165, 20, 10)
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if v.co.z < -0.01], context='VERTS')
    T(bm, Matrix.Diagonal((1, 1.05, 0.8, 1)))
    c = mesh_obj('cap', bm, 'cloth', uv='sph')
    br = mesh_obj('brim', bm_cyl(0.12, 0.12, 0.012, 16), 'cloth')
    br.scale = (1, 0.8, 1); br.location = (0, 0.15, 0.0)
    parent(br, c)
    bt = mesh_obj('button', bm_sphere(0.018, 8, 6), 'cloth')
    bt.location = (0, 0, 0.13); parent(bt, c)
    c.location = (0, 0, -0.08)
    out('cap', [c, br, bt])
    # gorro com pompom
    reset()
    bm = bm_sphere(0.17, 20, 12)
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if v.co.z < -0.02], context='VERTS')
    T(bm, Matrix.Diagonal((1, 1, 1.05, 1)))
    b = mesh_obj('beanie', bm, 'cloth', uv='sph')
    rim = mesh_obj('rim', bm_cyl(0.172, 0.172, 0.05, 20), 'cloth')
    rim.location = (0, 0, -0.03); parent(rim, b)
    pom = meta('pom', [ball((0, 0, 0), 0.05)] + [ball((random.uniform(-.03, .03), random.uniform(-.03, .03), random.uniform(-.03, .03)), 0.025) for _ in range(6)], res=0.008, mat='cloth', decimate=0.6)
    pom.location = (0, 0, 0.19); parent(pom, b)
    b.location = (0, 0, -0.1)
    out('beanie', [b, rim, pom])
    # cartola
    reset()
    t = mesh_obj('tophat', bm_cyl(0.11, 0.115, 0.22, 20), 'cloth_black', smooth=False)
    br = mesh_obj('brim', bm_cyl(0.19, 0.19, 0.012, 20), 'cloth_black'); parent(br, t)
    band = mesh_obj('band', bm_cyl(0.116, 0.116, 0.04, 20), 'cloth_red'); band.location = (0, 0, 0.02); parent(band, t)
    t.location = (0, 0, -0.04)
    out('tophat', [t, br, band])
    # coroa
    reset()
    bm = bm_cyl(0.11, 0.11, 0.06, 24, caps=False)
    cr = mesh_obj('crown', bm, 'gold', smooth=False)
    parts = [cr]
    for i in range(8):
        a = i / 8 * math.pi * 2
        sp = mesh_obj('spike%d' % i, bm_cyl(0.025, 0.0, 0.08, 4), 'gold', smooth=False)
        sp.location = (math.cos(a) * 0.105, math.sin(a) * 0.105, 0.06); parent(sp, cr)
        gm = mesh_obj('gem%d' % i, bm_sphere(0.012, 8, 6), 'cloth_red' if i % 2 else 'cloth_blue')
        gm.location = (math.cos(a) * 0.113, math.sin(a) * 0.113, 0.03); parent(gm, cr)
        parts += [sp, gm]
    cr.location = (0, 0, -0.03)
    out('crown', parts)
    # coroa de folhas
    reset()
    ring = mesh_obj('vine', bm_cyl(0.155, 0.155, 0.025, 20, caps=False), 'bark')
    parts = [ring]
    for i in range(14):
        a = i / 14 * math.pi * 2
        bm = bm_sphere(0.04, 8, 6); T(bm, Matrix.Diagonal((0.4, 1.0, 1.6, 1)))
        lf = mesh_obj('leaf%d' % i, bm, 'cloth_green')
        lf.location = (math.cos(a) * 0.16, math.sin(a) * 0.16, 0.04)
        lf.rotation_euler = (0.3, 0, a); parent(lf, ring); parts.append(lf)
    ring.location = (0, 0, -0.07)
    out('leafcrown', parts)
    # óculos redondos / escuros (âncora no rosto)
    for nm, lensmat in (('glasses', 'lens'), ('shades', 'glass_dark')):
        reset()
        fr = mesh_obj('bridge', bm_cyl(0.006, 0.006, 0.04, 6), 'metal')
        fr.rotation_euler = (0, math.pi / 2, 0); fr.location = (-0.02, 0, 0.0)
        parts = [fr]
        for sx in (1, -1):
            bm = bmesh.new()
            bmesh.ops.create_circle(bm, cap_ends=False, segments=16, radius=0.036)
            rim = mesh_obj('rim%d' % sx, bm_cyl(0.04, 0.04, 0.008, 16, caps=False), 'metal')
            rim.rotation_euler = (math.pi / 2, 0, 0); rim.location = (0.05 * sx, 0, 0); parent(rim, fr)
            ln = mesh_obj('lens%d' % sx, bm_cyl(0.037, 0.037, 0.004, 16), lensmat)
            ln.rotation_euler = (math.pi / 2, 0, 0); ln.location = (0.05 * sx, 0.002, 0); parent(ln, fr)
            arm = mesh_obj('arm%d' % sx, bm_box(0.006, 0.17, 0.006), 'metal')
            arm.location = (0.088 * sx, -0.085, 0.0); parent(arm, fr)
            parts += [rim, ln, arm]
        out(nm, parts)
    # fones de ouvido
    reset()
    bm = bmesh.new()
    bmesh.ops.create_cone(bm, cap_ends=False, segments=24, radius1=0.19, radius2=0.19, depth=0.03)
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if v.co.y < -0.05 or v.co.x * 0 + math.atan2(v.co.y, v.co.x) < 0.0], context='VERTS')
    hb = mesh_obj('band', bm_cyl(0.185, 0.185, 0.03, 24, caps=False), 'plastic')
    hb.rotation_euler = (math.pi / 2, 0, 0); hb.location = (0, 0, -0.1)
    parts = [hb]
    for sx in (1, -1):
        cup = mesh_obj('cup%d' % sx, bm_cyl(0.06, 0.06, 0.05, 16), 'cloth_red')
        cup.rotation_euler = (0, math.pi / 2 * sx, 0); cup.location = (0.15 * sx, 0, -0.14); parts.append(cup)
    out('headphones', parts)
    # gravata borboleta (âncora no pescoço)
    reset()
    k = mesh_obj('knot', bm_sphere(0.022, 10, 8), 'cloth')
    parts = [k]
    for sx in (1, -1):
        bm = bm_sphere(0.05, 10, 8); T(bm, Matrix.Diagonal((1.0, 0.35, 0.65, 1)))
        w = mesh_obj('wing%d' % sx, bm, 'cloth'); w.location = (0.045 * sx, 0, 0); parent(w, k); parts.append(w)
    out('bowtie', parts)
    # chifrinhos de diabo
    reset()
    parts = []
    base = empty('horns', (0, 0, 0))
    for sx in (1, -1):
        bm = bm_cyl(0.035, 0.0, 0.12, 10)
        for v in bm.verts:
            v.co.x += 0.05 * (v.co.z / 0.12) ** 2 * sx
        h = mesh_obj('horn%d' % sx, bm, 'cloth_red'); h.location = (0.08 * sx, 0.02, -0.06); h.rotation_euler = (0, 0.3 * sx, 0)
        parent(h, base); parts.append(h)
    out('horns', [base] + parts)


# ---------------------------------------------------------------- CÂMERA
def build_camera():
    reset()
    body = mesh_obj('cam', bm_box(0.16, 0.07, 0.1, bevel=0.012), 'plastic', smooth=False)
    lens = mesh_obj('lensbarrel', bm_cyl(0.035, 0.03, 0.05, 20), 'metal')
    lens.rotation_euler = (-math.pi / 2, 0, 0); lens.location = (0.02, 0.035, 0); parent(lens, body)
    glass = mesh_obj('glass', bm_cyl(0.026, 0.026, 0.004, 20), 'lens')
    glass.rotation_euler = (-math.pi / 2, 0, 0); glass.location = (0.02, 0.085, 0); parent(glass, body)
    # tela traseira (UV 0..1 para receber a imagem da câmera)
    bm = bmesh.new()
    bmesh.ops.create_grid(bm, x_segments=1, y_segments=1, size=0.5)
    T(bm, Matrix.Rotation(math.pi / 2, 4, 'X'))
    T(bm, Matrix.Diagonal((0.13, 1, 0.073, 1)))
    scr = mesh_obj('screen', bm, 'screen', uv='front', uvscale=0.065, smooth=False)
    scr.rotation_euler = (0, 0, math.pi)
    scr.location = (0, -0.0362, 0); parent(scr, body)
    rl = mesh_obj('reclight', bm_sphere(0.008, 8, 6), 'reclight'); rl.location = (-0.06, 0.03, 0.05); parent(rl, body)
    btn = mesh_obj('shutterbtn', bm_cyl(0.012, 0.012, 0.01, 10), 'cloth_red'); btn.location = (0.05, 0, 0.05); parent(btn, body)
    mount = mesh_obj('mount', bm_cyl(0.015, 0.015, 0.03, 10), 'metal'); mount.location = (0, 0, -0.08); parent(mount, body)
    export('camera.bm', [body, lens, glass, scr, rl, btn, mount], ao_dist=0.03)
    # bastão (0..1 em Z)
    reset()
    st = mesh_obj('stick', bm_cyl(0.012, 0.012, 1.0, 10, caps=False), 'metal', uv='cyl', uvscale=0.2)
    export('stick.bm', [st], ao=False)
    reset()
    gr = meta('grip', [capsule((0, 0, -0.06), (0, 0, 0.06), 0.022)], res=0.006, mat='plastic', decimate=0.6)
    export('grip.bm', [gr], ao=False)


# ---------------------------------------------------------------- UI
def build_ui():
    reset()
    b = mesh_obj('button', bm_cyl(0.05, 0.045, 0.03, 20), 'button')
    b.rotation_euler = (-math.pi / 2, 0, 0)  # frente aponta para +Y blender (= -Z jogo)
    ring = mesh_obj('ring', bm_cyl(0.06, 0.06, 0.01, 20), 'metal'); ring.rotation_euler = (-math.pi / 2, 0, 0); ring.location = (0, -0.002, 0)
    root = empty('btn', (0, 0, 0))
    parent(b, root); parent(ring, root)
    export('button.bm', [root, b, ring], ao=False)
    # quadro: moldura de madeira + lousa 1x1 (escalado em tempo real)
    reset()
    fr = mesh_obj('frame', bm_box(1.06, 0.04, 1.06, bevel=0.01), 'plank', smooth=False)
    sl = mesh_obj('slate', bm_box(1.0, 0.01, 1.0), 'slate', smooth=False); sl.location = (0, 0.02, 0)
    parent(sl, fr)
    export('board.bm', [fr, sl], ao=False)


# ---------------------------------------------------------------- MAPA
def build_map():
    reset()
    with open(os.path.join(ROOT, 'shared', 'map.json')) as fp:
        M = json.load(fp)
    objs = []

    def B(x, y, z):  # jogo -> blender
        return Vector((x, -z, y))

    def add(o, worlduv=True):
        o['worlduv'] = worlduv
        objs.append(o)
        return o

    def boxat(name, mat, pos, size, yaw=0, bevel=0.0, worlduv=True, uvscale=None):
        o = mesh_obj(name, bm_box(size[0], size[1], size[2], bevel=bevel), mat, uv='box', smooth=False)
        if max(size) > 2.5:
            o['noao'] = True
        o.matrix_world = Matrix.Translation(pos) @ Matrix.Rotation(yaw, 4, 'Z')
        if uvscale:
            o['uvscale'] = uvscale
        return add(o, worlduv)

    toymats = ['cloth_red', 'cloth_blue', 'cloth_green', 'gold', 'toyball']

    def toy(pos, scale=1.0):
        """Brinquedo aleatório em cima de uma prateleira (caixa, bola ou bichinho de pelúcia)."""
        k = random.random()
        if k < 0.45:
            s = Vector((random.uniform(0.35, 0.6), random.uniform(0.3, 0.5), random.uniform(0.3, 0.6))) * scale
            o = boxat('toybox', 'toybox', pos + Vector((0, 0, s.z / 2)), s, yaw=random.uniform(-0.3, 0.3), worlduv=False, uvscale=max(s))
            o['uv'] = 'box'
        elif k < 0.75:
            r = random.uniform(0.12, 0.22) * scale
            o = mesh_obj('ball', bm_sphere(r, 12, 8), random.choice(toymats), uv='sph')
            o.location = pos + Vector((0, 0, r)); add(o, False)
        else:  # pelúcia redonda genérica
            r = random.uniform(0.12, 0.18) * scale
            mat = random.choice(['cloth_red', 'cloth_blue', 'cloth_green', 'fur_player'])
            o = meta('plush', [ell((0, 0, r), r, (1, 0.9, 1.1)), ball((0, 0.02, r * 2.1), r * 0.75),
                               ball((r * 0.55, 0, r * 2.6), r * 0.3), ball((-r * 0.55, 0, r * 2.6), r * 0.3)],
                     res=r * 0.25, mat=mat, decimate=0.4)
            o.location = pos; o.rotation_euler = (0, 0, random.uniform(0, 6)); add(o, False)

    # ---- piso de ladrilho em toda a área jogável + fora (corredor de saída)
    fmin, fmax = M['forest']['min'], M['forest']['max']
    W, D = fmax[0] - fmin[0] + 20, fmax[1] - fmin[1] + 4
    bm = bmesh.new()
    bmesh.ops.create_grid(bm, x_segments=40, y_segments=40, size=1)
    T(bm, Matrix.Diagonal((W / 2, D / 2, 1, 1)))
    T(bm, Matrix.Translation(B((fmin[0] + fmax[0]) / 2 + 8, 0.0, (fmin[1] + fmax[1]) / 2)))
    fl = add(mesh_obj('floor', bm, 'tile', uv='box', smooth=False))
    fl['noao'] = True
    H = M['wallH']

    for c in M['colliders']:
        kind = c['kind']
        if c['type'] != 'box':
            continue
        x, y, z = c['c']; sx, sy, sz = c['s']; yaw = c['yaw']
        pos = B(x, y, z)
        if c.get('id') == 'gate':
            continue
        if kind in ('cwall', 'lobbywall', 'pillar'):
            boxat(kind, 'concrete', pos, (sx, sz, sy), yaw, bevel=0.02)
            # rodapé escuro
            if kind != 'pillar':
                boxat('base', 'steel', B(x, 0.1, z), (sx + 0.04, sz + 0.04, 0.2), yaw)
        elif kind in ('ceiling', 'lobbyceil'):
            boxat(kind, 'ceiling', pos, (sx, sz, sy), yaw)
        elif kind == 'lobbyfloor':
            boxat(kind, 'tile', pos, (sx, sz, sy), yaw)
        elif kind == 'shelf':
            alongZ = sz > sx
            L = sz if alongZ else sx
            dep = sx if alongZ else sz
            n = max(2, int(L / 1.5) + 1)
            # postes
            for i in range(n):
                t = -L / 2 + 0.05 + i * (L - 0.1) / (n - 1)
                for side in (-1, 1):
                    lx, lz = (side * (dep / 2 - 0.04), t) if alongZ else (t, side * (dep / 2 - 0.04))
                    boxat('post', 'steel', B(x + lx, sy / 2, z + lz), (0.07, 0.07, sy))
            # prateleiras + brinquedos
            for k, hgt in enumerate((0.12, 0.95, 1.8, 2.6, 3.35)):
                size = (dep, L, 0.05) if alongZ else (L, dep, 0.05)
                boxat('board', 'steel', B(x, hgt, z), size)
                if k < 4:
                    m = int(L / 0.7)
                    for i in range(m):
                        if random.random() < 0.3:
                            continue
                        t = -L / 2 + 0.35 + i * 0.7 + random.uniform(-0.1, 0.1)
                        lx, lz = (random.uniform(-0.15, 0.15), t) if alongZ else (t, random.uniform(-0.15, 0.15))
                        toy(B(x + lx, hgt + 0.03, z + lz), 1.0 if k > 0 else 1.2)
        elif kind == 'crate':
            o = boxat('crate', 'crate', pos, (sx, sz, sy), yaw, bevel=0.02, worlduv=False, uvscale=sx)
        elif kind == 'table':
            boxat('top', 'steel', B(x, sy - 0.03, z), (sx, sz, 0.06))
            for lx in (-sx / 2 + 0.08, sx / 2 - 0.08):
                for lz in (-sz / 2 + 0.08, sz / 2 - 0.08):
                    boxat('leg', 'steel', B(x + lx, sy / 2, z + lz), (0.06, 0.06, sy))
            for i in range(random.randint(1, 3)):
                toy(B(x + random.uniform(-0.7, 0.7), sy, z + random.uniform(-0.3, 0.3)), 0.8)
        elif kind == 'machine':
            boxat('machine', 'steel', pos, (sx, sz, sy), yaw, bevel=0.05)
            boxat('panel', 'metal', B(x, sy * 0.6, z - sz / 2 - 0.01), (sx * 0.6, 0.04, sy * 0.4))
            for i in range(4):
                l = mesh_obj('led', bm_sphere(0.03, 6, 4), 'reclight' if i % 2 else 'mushroom_glow')
                l.location = B(x - 0.3 + i * 0.2, sy * 0.75, z - sz / 2 - 0.04); add(l)
            for i in range(3):  # canos subindo ao teto
                p = mesh_obj('pipe', bm_cyl(0.1, 0.1, H - sy, 10), 'metal', uv='cyl')
                p.location = B(x - sx / 3 + i * sx / 3, sy, z + sz / 4); add(p)
        elif kind == 'toypile':
            for i in range(28):
                r = random.uniform(0.15, 0.3)
                a = random.random() * 6.28; d = random.random() ** 0.5
                hh = (1 - d) * sy * 0.9 + r * 0.5
                o = mesh_obj('ball', bm_sphere(r, 12, 8), random.choice(toymats), uv='sph')
                o.location = B(x + math.cos(a) * d * sx / 2, hh, z + math.sin(a) * d * sz / 2); add(o, False)

    # ---- luzes fluorescentes
    for (lx, ly, lz) in M['lights']:
        fx = mesh_obj('fixture', bm_box(1.5, 0.45, 0.08), 'steel', smooth=False)
        fx.location = B(lx, ly - 0.04, lz); add(fx)
        bm = bm_box(1.35, 0.3, 0.02)
        p = mesh_obj('fluor', bm, 'fluor', uv='box', smooth=False); p['uvscale'] = 1.4
        p.location = B(lx, ly - 0.09, lz); add(p, False)
        add(empty('light', B(lx, ly - 0.4, lz)))

    # ---- canos e conduítes ao longo do teto
    for i in range(14):
        x0 = random.uniform(fmin[0] + 4, fmax[0] - 4); z0 = random.uniform(fmin[1] + 4, fmax[1] - 4)
        L = random.uniform(6, 18)
        p = mesh_obj('ductpipe', bm_cyl(0.12, 0.12, L, 10), 'metal', uv='cyl')
        p.matrix_world = Matrix.Translation(B(x0, H - 0.35, z0)) @ Matrix.Rotation(math.pi / 2, 4, 'Y' if i % 2 else 'X')
        add(p)

    # ---- caixas de papelão espalhadas e papéis no chão
    cols = M['colliders']

    def free(x, z, rad):
        if not (fmin[0] + 1 < x < fmax[0] - 1 and fmin[1] + 1 < z < fmax[1] - 1):
            return False
        for c in cols:
            if c['type'] != 'box' or c['kind'] in ('ceiling',):
                continue
            cx_, _, cz_ = c['c']
            if abs(x - cx_) < c['s'][0] / 2 + rad and abs(z - cz_) < c['s'][2] / 2 + rad:
                return False
        return True

    for i in range(60):
        x, z = random.uniform(fmin[0], fmax[0]), random.uniform(fmin[1], fmax[1])
        if not free(x, z, 0.5):
            continue
        if random.random() < 0.5:
            toy(B(x, 0, z), 1.3)
        else:
            bm = bm_box(random.uniform(0.2, 0.3), random.uniform(0.28, 0.4), 0.003)
            o = mesh_obj('paper', bm, 'white', smooth=False)
            o.location = B(x, 0.005, z); o.rotation_euler = (0, 0, random.uniform(0, 6)); add(o)

    # ---- placa de SAÍDA verde sobre o portão
    g = M['gate']
    sg = mesh_obj('exitsign', bm_box(0.12, 1.0, 0.35), 'portal_glow', smooth=False)
    sg.location = B(g['c'][0] - 0.2, 4.4, g['c'][2]); add(sg)
    add(empty('light', B(g['c'][0] - 1, 3.5, g['c'][2])))

    # ---- lobby: tapete, caixas, portal
    L = M['lobby']
    lx, _, lz = L['center']
    rug = mesh_obj('rug', bm_box(7, 7, 0.02), 'rug', uv='box', smooth=False); rug.location = B(lx, 0.01, lz + 1.5); add(rug)
    for i in range(6):
        cr = mesh_obj('crate', bm_box(0.9, 0.9, 0.9, 0.03), 'crate', uv='box', smooth=False)
        cr['uvscale'] = 0.9
        cr.location = B(lx + 6.2 - (i % 3) * 0.95, 0.45 + (i // 3) * 0.9, lz - 6.8 + (i % 2) * 0.1); objs.append(cr)
    px, py, pz = L['portal']['c']
    dark = mesh_obj('portal', bm_box(3.8, 0.1, 3.8), 'portal', smooth=False); dark.location = B(px, 1.9, pz + 1.6); add(dark)
    for sx in (1, -1):
        gl = mesh_obj('pglow', bm_box(0.1, 0.12, 3.6), 'portal_glow', smooth=False); gl.location = B(px + 1.95 * sx, 1.8, pz - 1.7); add(gl)
    gl = mesh_obj('pglow', bm_box(4.0, 0.12, 0.1), 'portal_glow', smooth=False); gl.location = B(px, 3.6, pz - 1.7); add(gl)

    export('map.bm', objs, ao_dist=1.5, chunk=24)

    # ---- porta de aço da saída (sobe ao abrir)
    reset()
    door = mesh_obj('door', bm_box(0.2, 2.8, 4.0, 0.02), 'steel', smooth=False)
    door['worlduv'] = False
    parts = [door]
    for i in range(6):
        rib = mesh_obj('rib%d' % i, bm_box(0.26, 2.7, 0.06), 'metal', smooth=False)
        rib.location = (0, 0, -1.7 + i * 0.68); parent(rib, door); rib['worlduv'] = False; parts.append(rib)
    hz = mesh_obj('stripe', bm_box(0.25, 2.7, 0.25), 'gold', smooth=False); hz.location = (0, 0, -1.85); parent(hz, door); parts.append(hz)
    export('gate.bm', parts, ao=False)


if __name__ == '__main__':
    what = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else ['all']
    if 'all' in what or 'player' in what:
        build_player(); build_banana()
    if 'all' in what or 'monsters' in what:
        build_monsters()
    if 'all' in what or 'cosmetics' in what:
        build_cosmetics(); build_camera(); build_ui()
    if 'all' in what or 'map' in what:
        build_map()
    print('OK')

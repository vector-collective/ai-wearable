#!/usr/bin/env python3
"""
Build the Kraken Medallion relief from the source Meshy sculpt.

    python3 build_kraken.py path/to/Meshy_AI_Crimson_Kraken.3mf

Writes kraken_wrap_bored.stl next to pendant_case_v7.scad, which imports it.

WHY THIS EXISTS AS CODE AND NOT AS A MESH EDIT
The supplied sculpt is a flat-backed bas-relief - it has no geometry at all
below its own back plane. Enlarging it until the arms reach the case edge
therefore does not make them grip the edge; it makes them overhang into
thin air. So every vertex is decomposed into (footprint, height above the
back plane), the footprint is re-projected onto the case's real outer
surface - flat disc, 2.5mm rim fillet, then the r=35 cylinder - and the
height is re-applied along that surface's local normal. Arms that used to
run off the edge now curl over the fillet and down the side.

The 2.5mm fillet here and rim_fillet_r in pendant_case_v7.scad are the same
number twice. Change one and you must change the other, or the relief's
back face will cut through the shell's corner.
"""
import sys, zipfile, xml.etree.ElementTree as ET
import numpy as np, trimesh, fast_simplification, manifold3d

# ---- case surface the relief is bent onto (must match the SCAD) --------
CASE_R = 35.0
RF     = 2.5             # rim fillet radius  == rim_fillet_r
R0     = CASE_R - RF     # where the flat face ends
MAXZ   = 5.5             # arms must stop short of front_d = 6.5, or the
                         # shells cannot be parted

XY_SCALE    = 1.16       # solved: the largest scale at which all three mic
                         # ports still find clear sculpt. See README.
DEPTH_SCALE = 0.55       # 8.77mm native relief -> 4.82mm
DECIMATE    = 0.90       # target_reduction; ~197k faces out of ~2M

# apertures bored through the relief. The camera one is not optional:
# swept over the whole board-reach window there is no lens position with
# 4.5mm of clear sculpt, so the lens must look through the artwork.
BORES = [("cam",  48.4, 38.8, 9.4),
         ("mic1", 22.8, 41.6, 4.0),
         ("mic2", 44.4, 22.0, 4.0),
         ("mic3", 25.6, 21.6, 4.0)]


def load_3mf(path):
    """Meshy 3MF: one object, vertices in mm, Y up."""
    # Meshy splits the package: 3D/3dmodel.model is only a build item that
    # references 3D/Objects/object_N.model, where the geometry actually is.
    # Scan every .model part and take the first that carries a <mesh>.
    with zipfile.ZipFile(path) as z:
        for name in (n for n in z.namelist() if n.endswith(".model")):
            root = ET.fromstring(z.read(name))
            ns = {"c": root.tag.split("}")[0].strip("{")}
            mesh = root.find(".//c:mesh", ns)
            if mesh is not None:
                break
        else:
            raise SystemExit(f"no <mesh> in any .model part of {path}")
    v = np.array([[float(x.get(a)) for a in "xyz"]
                  for x in mesh.find("c:vertices", ns)], dtype=np.float64)
    f = np.array([[int(t.get(a)) for a in ("v1", "v2", "v3")]
                  for t in mesh.find("c:triangles", ns)], dtype=np.int64)
    return v, f


def wrap(vo, fo, cx, cz, back):
    """Bend the flat relief over the rim. See module docstring."""
    x = (vo[:, 0] - cx) * XY_SCALE + 35.0
    y = (vo[:, 2] - cz) * XY_SCALE + 35.0 - 0.5      # best-fit centring
    z = -(back - vo[:, 1]) * DEPTH_SCALE             # sculpt lives at z<0

    dx, dy = x - 35.0, y - 35.0
    r = np.hypot(dx, dy); r = np.where(r < 1e-9, 1e-9, r)
    ux, uy = dx / r, dy / r
    p = -z                                            # height above the back
    s = r - R0                                        # how far past the flat

    phi     = np.clip(s / RF, 0, np.pi / 2)
    past    = s > RF * np.pi / 2
    base_r  = np.where(s <= 0, r, R0 + RF * np.sin(phi))
    base_z  = np.where(s <= 0, 0.0, RF * (1 - np.cos(phi)))
    base_r  = np.where(past, CASE_R, base_r)
    base_z  = np.minimum(np.where(past, RF + (s - RF * np.pi / 2), base_z), MAXZ)
    nr      = np.where(past, 1.0, np.where(s <= 0,  0.0,  np.sin(phi)))
    nz      = np.where(past, 0.0, np.where(s <= 0, -1.0, -np.cos(phi)))

    nr_, nz_ = base_r + p * nr, base_z + p * nz
    # the Z flip above reverses winding, hence [0,2,1]
    k = trimesh.Trimesh(np.c_[35 + ux * nr_, 35 + uy * nr_, nz_],
                        fo[:, [0, 2, 1]], process=True)
    # decimation leaves zero-volume specks; keep only the real body
    return max(k.split(only_watertight=False), key=lambda m: abs(m.volume))


def to_manifold(m):
    return manifold3d.Manifold(manifold3d.Mesh(
        vert_properties=np.asarray(m.vertices, dtype=np.float32),
        tri_verts=np.asarray(m.faces, dtype=np.uint32)))


def main(src, dst="kraken_wrap_bored.stl"):
    v, f = load_3mf(src)
    print(f"source        {len(f):>8,} faces")
    cx = (v[:, 0].max() + v[:, 0].min()) / 2
    cz = (v[:, 2].max() + v[:, 2].min()) / 2
    back = v[:, 1].max()

    vo, fo = fast_simplification.simplify(v.astype(np.float32),
                                          f.astype(np.int32),
                                          target_reduction=DECIMATE)
    print(f"decimated     {len(fo):>8,} faces")

    k = wrap(vo, fo, cx, cz, back)
    M = to_manifold(k)
    print(f"wrapped       {M.volume()/1000:8.3f} cm3   status {M.status()}")

    for name, x, y, d in BORES:
        before = M.volume()
        M = M - manifold3d.Manifold.cylinder(20.0, d/2, d/2, 96, True).translate([x, y, 0.0])
        print(f"  bore {name:<5} d{d:.1f}  removes {before-M.volume():7.1f} mm3")

    mm = M.to_mesh()
    out = trimesh.Trimesh(np.asarray(mm.vert_properties)[:, :3].astype(np.float64),
                          np.asarray(mm.tri_verts).astype(np.int64), process=True)
    out.merge_vertices()
    out.export(dst)

    r = np.hypot(out.vertices[:, 0] - 35, out.vertices[:, 1] - 35)
    print(f"\n{dst}: {len(out.faces):,} faces, {out.volume/1000:.3f} cm3")
    print(f"  relief    {-out.vertices[:,2].min():.2f} mm proud of the face")
    print(f"  wrap      {out.vertices[:,2].max():.2f} mm down the side, "
          f"{r.max()-CASE_R:.2f} mm proud of the rim")
    print(f"  seam      arm tips clear the 6.50 parting plane by "
          f"{6.5-out.vertices[:,2].max():.2f} mm")
    assert out.vertices[:, 2].max() < 6.5, "arms cross the parting plane"


if __name__ == "__main__":
    main(*sys.argv[1:])

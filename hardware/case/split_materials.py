#!/usr/bin/env python3
"""
Split the kraken relief into two complementary solids for a two-material print:

    kraken_body.stl     the sculpt            -> kraken filament
    kraken_accents.stl  eyes + sucker rings   -> the SHELL filament, so the
                                                 eyes and rings read as the
                                                 case showing through

    python3 split_materials.py [kraken_wrap_bored.stl]

HOW THE FEATURES ARE FOUND
There is no per-feature tagging in a Meshy export, so both are found from the
geometry. The relief is rasterised into a height field, and a mask-normalised
Gaussian high-pass gives the local relief - mask-normalised because a plain
Gaussian bleeds across the silhouette and makes every arm edge look raised.

  sucker rings  a ring matched filter over five radii (0.62-1.15mm) locates
                each sucker; the high-pass then supplies the exact ring SHAPE,
                so obliquely-viewed elliptical rings come out right. The filter
                is only a gate - it says where, not what.
  eyes          fitted directly: brute-force the best rim circle in a window
                over the left eye, then mirror it about the sculpt's symmetry
                axis at x=35.

THE BEAK IS EXCLUDED BY HAND, and that is deliberate. The mouth's radiating
spikes are smooth and carry no suckers (confirmed against the height field),
but the GROOVE between two adjacent spikes is genuinely dish-shaped with
raised flanks - it is a sucker as far as any local detector is concerned.
Five discriminators were tried on it: component elongation, ring closure,
rim-vs-centre on the height rather than the residual, angular isotropy, and
raising the matched-filter threshold. Every one of them removed real suckers
faster than it removed the grooves. An explicit ellipse over the beak is the
honest fix.
"""
import sys
import numpy as np, trimesh, manifold3d as m3
from scipy import ndimage as ndi

RES   = 0.15      # raster pitch, mm
OFF   = 1.0       # raster origin offset so nothing lands on a negative index
SIGMA = 0.50      # high-pass scale; larger merges neighbouring suckers
DEPTH = 1.2       # how far the accent material reaches below the local surface
RIM_R = 32.3      # past this the arms roll over the rim and a top-down height
                  # field stops describing them, so accents stop too
BEAK  = (35.0, 29.6, 9.8, 5.6)      # cx, cy, semi-x, semi-y
EYE_WIN = dict(x=(26.4, 30.0), y=(42.4, 45.8), r=(2.2, 3.4))
SYM_X = 35.0


def height_field(mesh):
    """Top surface height above the face plane, by barycentric rasterisation.
    Rasterising vertices alone leaves holes - the mesh is not dense enough."""
    T = mesh.triangles.astype(np.float32)
    n = int(72 / RES)
    k = 16
    B = np.array([(i/k, j/k, 1-i/k-j/k)
                  for i in range(k+1) for j in range(k+1-i)], dtype=np.float32)
    H = np.full((n, n), -99.0, dtype=np.float32)
    for s in range(0, len(T), 15000):
        P = np.einsum('sk,nkc->nsc', B, T[s:s+15000]).reshape(-1, 3)
        ix = ((P[:, 0]+OFF)/RES).astype(np.int32); iy = ((P[:, 1]+OFF)/RES).astype(np.int32)
        ok = (ix >= 0) & (ix < n) & (iy >= 0) & (iy < n)
        np.maximum.at(H, (iy[ok], ix[ok]), -P[ok, 2])
    solid = H > -90
    H = ndi.grey_closing(np.where(solid, H, 0.0).astype(np.float64), size=3)
    return H, ndi.binary_closing(solid, np.ones((3, 3)))


def highpass(H, solid, sigma_mm):
    """Mask-normalised Gaussian high-pass: only sculpt cells contribute, so
    the silhouette edge stops masquerading as a raised feature."""
    s = solid.astype(float); sg = sigma_mm/RES
    num = ndi.gaussian_filter(H*s, sigma=sg); den = ndi.gaussian_filter(s, sigma=sg)
    return np.where(solid, H - np.where(den > 1e-6, num/np.maximum(den, 1e-6), 0.0), 0.0)


def ring_response(R, solid, radii):
    """Ring matched filter, mask-normalised and rejecting partial support."""
    s = solid.astype(float); Rs = np.where(solid, R, 0.0)
    cv = lambda a, kk: ndi.convolve(a, kk, mode='constant')
    best = np.full(R.shape, -9.0); bestr = np.zeros(R.shape)
    for r0 in radii:
        r0c, wc = r0/RES, 0.26/RES
        k = int(np.ceil(r0c+wc))+1
        y, x = np.mgrid[-k:k+1, -k:k+1]; rr = np.hypot(x, y)
        ring = (np.abs(rr-r0c) <= wc).astype(float); core = (rr <= r0c-wc).astype(float)
        if core.sum() < 4:
            continue
        rn, cn = cv(s, ring), cv(s, core)
        ok = (rn > 0.90*ring.sum()) & (cn > 0.90*core.sum())
        resp = np.where(ok, cv(Rs, ring)/np.maximum(rn, 1e-9)
                            - cv(Rs, core)/np.maximum(cn, 1e-9), -9.0)
        u = resp > best; bestr[u] = r0; best[u] = resp[u]
    return best, bestr


def fit_eye(R, solid, X, Y):
    best = None
    for cx in np.arange(*EYE_WIN['x'], 0.1):
        for cy in np.arange(*EYE_WIN['y'], 0.1):
            d = np.hypot(X-cx, Y-cy)
            for r in np.arange(*EYE_WIN['r'], 0.1):
                rim = (np.abs(d-r) <= 0.35) & solid; core = (d <= r-0.5) & solid
                if rim.sum() < 60 or core.sum() < 60:
                    continue
                sc = R[rim].mean() - R[core].mean()
                if best is None or sc > best[0]:
                    best = (sc, cx, cy, r)
    return best


def de_diagonal(M):
    """manifold3d rejects a prism mesh built over cells that touch only at a
    corner - that is a non-manifold edge. Fill one cell at each such pinch."""
    M = M.copy()
    for _ in range(8):
        a = M[:-1, :-1] & M[1:, 1:] & ~M[:-1, 1:] & ~M[1:, :-1]
        b = M[:-1, 1:] & M[1:, :-1] & ~M[:-1, :-1] & ~M[1:, 1:]
        if not a.any() and not b.any():
            break
        M[:-1, 1:] |= a; M[:-1, :-1] |= b
    return M


def band_mesh(H, M):
    """A closed prism following the local surface: top just above it, bottom
    DEPTH below. Corner-sampled so the surfaces stay continuous across cells."""
    ny, nx = H.shape
    acc = np.zeros((ny+1, nx+1)); cnt = np.zeros((ny+1, nx+1))
    for dy in (0, 1):
        for dx in (0, 1):
            acc[dy:dy+ny, dx:dx+nx] += H; cnt[dy:dy+ny, dx:dx+nx] += 1
    Hc = ndi.maximum_filter(acc/np.maximum(cnt, 1), size=3)
    Zt = -(Hc+0.5); Zb = -np.maximum(Hc-DEPTH, 0.30)
    V = []; F = []; idx = {}
    def vid(i, j, top):
        k = (i, j, top)
        if k not in idx:
            idx[k] = len(V); V.append((j*RES-OFF, i*RES-OFF, (Zt if top else Zb)[i, j]))
        return idx[k]
    for i, j in np.argwhere(M):
        # z is negative going OUT of the case, so the "top" cap is the most
        # negative surface and its outward normal is -z: wind it clockwise
        # seen from +z. Getting this backwards leaves some of the 100-odd
        # separate ring bodies inside-out, and they then cut instead of fill.
        a, b, c, d = vid(i, j, 1), vid(i, j+1, 1), vid(i+1, j+1, 1), vid(i+1, j, 1)
        F += [[a, c, b], [a, d, c]]
        a2, b2, c2, d2 = vid(i, j, 0), vid(i, j+1, 0), vid(i+1, j+1, 0), vid(i+1, j, 0)
        F += [[a2, b2, c2], [a2, c2, d2]]
        for di, dj, e0, e1 in ((-1, 0, (i, j), (i, j+1)), (1, 0, (i+1, j+1), (i+1, j)),
                               (0, -1, (i+1, j), (i, j)), (0, 1, (i, j+1), (i+1, j+1))):
            ii, jj = i+di, j+dj
            if 0 <= ii < ny and 0 <= jj < nx and M[ii, jj]:
                continue
            t0, t1 = vid(*e0, 1), vid(*e1, 1); b0, b1 = vid(*e0, 0), vid(*e1, 0)
            F += [[t0, t1, b1], [t0, b1, b0]]
    t = trimesh.Trimesh(np.array(V, float), np.array(F, np.int64), process=False)
    t.merge_vertices()
    if not t.is_winding_consistent:
        raise SystemExit("band winding is inconsistent - check the cap order")
    return t


def to_manifold(t):
    return m3.Manifold(m3.Mesh(vert_properties=np.asarray(t.vertices, dtype=np.float32),
                               tri_verts=np.asarray(t.faces, dtype=np.uint32)))


def to_trimesh(man):
    mm = man.to_mesh()
    t = trimesh.Trimesh(np.asarray(mm.vert_properties)[:, :3].astype(np.float64),
                        np.asarray(mm.tri_verts).astype(np.int64), process=True)
    t.merge_vertices(); return t


def drop_slivers(t, min_mm3, tag):
    parts = [p for p in t.split(only_watertight=False)]
    keep = [p for p in parts if abs(p.volume) >= min_mm3]
    lost = sum(abs(p.volume) for p in parts if abs(p.volume) < min_mm3)
    if len(keep) != len(parts):
        print(f"  {tag}: dropped {len(parts)-len(keep)} slivers totalling {lost:.3f} mm3")
        t = trimesh.util.concatenate(keep)
    return t


def main(src="kraken_wrap_bored.stl"):
    rel = trimesh.load(src, process=True); rel.merge_vertices()
    H, solid = height_field(rel)
    ny, nx = H.shape
    yy, xx = np.mgrid[0:ny, 0:nx]; X = xx*RES-OFF; Y = yy*RES-OFF
    R = highpass(H, solid, SIGMA)

    bx, by, ax, ay = BEAK
    beak = (((X-bx)/ax)**2 + ((Y-by)/ay)**2) <= 1.0

    sc, ecx, ecy, er = fit_eye(R, solid, X, Y)
    print(f"eye fit: ({ecx:.2f}, {ecy:.2f}) rim r {er:.2f}; mirrored to ({2*SYM_X-ecx:.2f}, {ecy:.2f})")
    m_eyes = np.zeros(H.shape, bool)
    for cx in (ecx, 2*SYM_X-ecx):
        m_eyes |= np.hypot(X-cx, Y-ecy) <= er+0.35
    m_eyes &= solid

    best, bestr = ring_response(R, solid, [0.62, 0.72, 0.84, 0.98, 1.15])
    mx = ndi.maximum_filter(best, size=int(1.35/RES) | 1)
    pk = (best == mx) & (best > 0.07) & ~beak
    ys, xs = np.nonzero(pk); rr = bestr[pk]
    # Two masks per detected sucker, unioned:
    #   gate    a disc, inside which the high-pass supplies the true ring
    #           shape - right for obliquely-viewed elliptical rings
    #   annulus the analytic ring at the detected radius, which CLOSES the
    #           many rings the high-pass only catches an arc of. On its own
    #           it looks mechanical; on its own the high-pass looks scrappy.
    gate = np.zeros(H.shape, bool); annulus = np.zeros(H.shape, bool)
    for gy, gx, r0 in zip(ys, xs, rr):
        rad = r0+0.40; k = int(np.ceil(rad/RES))+1
        a, b = max(gy-k, 0), min(gy+k+1, ny); c, d = max(gx-k, 0), min(gx+k+1, nx)
        dd = np.hypot((yy[a:b, c:d]-gy)*RES, (xx[a:b, c:d]-gx)*RES)
        gate[a:b, c:d] |= dd <= rad
        annulus[a:b, c:d] |= np.abs(dd-r0) <= 0.26
    inner = ndi.binary_erosion(solid, np.ones((int(0.35/RES) | 1,)*2))
    shape = ((R > 0.055) & gate) | annulus
    m_suck = ndi.binary_closing(ndi.binary_opening(
        shape & solid & inner & ~m_eyes & ~beak, np.ones((3, 3))), np.ones((3, 3)))
    print(f"{len(ys)} sucker gates -> rings {m_suck.sum()*RES*RES:.1f} mm2, "
          f"eyes {m_eyes.sum()*RES*RES:.1f} mm2")

    M = (m_suck | m_eyes) & (np.hypot(X-35, Y-35) <= RIM_R)
    M = de_diagonal(ndi.binary_closing(M, np.ones((3, 3))))
    print(f"accent footprint {M.sum()*RES*RES:.1f} mm2 = {100*M.sum()/solid.sum():.1f}% of the sculpt")

    band = band_mesh(H, M)
    B = to_manifold(band)
    print(f"band: {len(band.faces)} faces, {B.volume()/1000:.3f} cm3, status {B.status()}")
    if str(B.status()) != "Error.NoError":
        raise SystemExit("band is not manifold - refusing to produce a bad split")

    Rm = to_manifold(rel)
    A = Rm ^ B; Y_ = Rm - B
    print(f"accents {A.volume()/1000:.3f} cm3 + body {Y_.volume()/1000:.3f} cm3 "
          f"= {(A.volume()+Y_.volume())/1000:.3f} (relief {Rm.volume()/1000:.3f})")
    assert abs(A.volume()+Y_.volume()-Rm.volume()) < 1.0, "split is not complementary"
    ta, ty = to_trimesh(A), to_trimesh(Y_)

    # The cut leaves a scatter of sub-bead slivers along its boundary. Drop
    # them from the body (each is smaller than one extrusion) but keep every
    # accent piece - an individual sucker ring is only 1-2mm3 and dropping
    # one would leave a pinhole rather than a smaller part.
    ty = drop_slivers(ty, 0.5, "body")
    ta = drop_slivers(ta, 0.02, "accents")
    ta.export("kraken_accents.stl"); ty.export("kraken_body.stl")
    print(f"kraken_accents.stl  {len(ta.faces):,} faces, {ta.body_count} bodies, {ta.volume/1000:.3f} cm3")
    print(f"kraken_body.stl     {len(ty.faces):,} faces, {ty.body_count} bodies, {ty.volume/1000:.3f} cm3")


if __name__ == "__main__":
    main(*sys.argv[1:])

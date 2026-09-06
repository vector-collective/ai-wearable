// =====================================================================
// PENDANT POUCH Mk1 - assembled visualisation
//
// This is NOT a printable part. The pouch is sewn; the geometry that
// actually gets made is the flat pattern in pattern.py (sheet1.svg,
// sheet2.svg) and template.dxf. This file exists to check that the
// proportions read right on a body before anyone cuts leather, and to
// see where the apertures land once the panel is no longer flat.
//
// Orientation is as worn: X across the chest, Z up, Y away from you.
// The front panel faces -Y. Panel coordinates from the pattern map in
// as (x - 39, z - 31), so aperture (39, 33) sits at model (0, 2).
//
// CONSTRUCTION MODELLED
//   body     78 x 62 x 24, R14 in the XZ face, 3mm softening front-to-back
//   front    2mm veg-tan skin on the -Y face, apertures cut through
//   flap     the same skin continued over the top and 28mm down the back
//   tabs     D-ring tabs caught in the top of each side seam
//   strap    14mm, hung from the rings, with the nape sleeve shown
// =====================================================================

part = "worn";          // "worn" | "pouch" | "front"

W  = 78;   HB = 62;   D = 24;    // body: width, height, depth
RXZ = 14;  RY = 3;                // corner radius in the face, and front-to-back
LEATHER = 2.0;                    // 4-5 oz veg-tan
FLAP_BACK = 28;                   // how far the flap runs down the back
SNAP_Z = HB/2 - 16;               // snap sits 16mm down from the top

// panel (x, y) -> model (x, z)
function P(p) = [p[0] - W/2, p[1] - HB/2];

APERTURES = [
    ["micA",   [16, 44],   3.0],
    ["micB",   [62, 44],   3.0],
    ["micC",   [39, 15],   3.0],
    ["camera", [39, 33],  10.0],
    ["led",    [39, 53],   5.0],
    ["button", [62, 15],  12.5],
];

$fn = 56;

// Rounded-rect prism, grown uniformly by t. Built from ellipsoids so the
// face keeps R14 while the front-to-back edge stays a soft 3mm - a single
// sphere radius cannot do both.
module shell_solid(t = 0) {
    rx = RXZ + t;  ry = RY + t;
    hull() for (sx = [-1, 1], sy = [-1, 1], sz = [-1, 1])
        translate([sx*(W/2 - RXZ), sy*(D/2 - RY), sz*(HB/2 - RXZ)])
            scale([1, ry/rx, 1]) sphere(r = rx);
}

// The 2mm skin that the leather occupies, before it is masked to a region.
module skin() { difference() { shell_solid(LEATHER); shell_solid(0); } }

module aperture_cuts() {
    for (a = APERTURES) {
        p = P(a[1]);
        translate([p[0], -D/2 - LEATHER - 4, p[1]])
            rotate([-90, 0, 0]) cylinder(d = a[2], h = 12);
    }
}

module front_panel() {
    difference() {
        intersection() {
            skin();
            translate([-W, -D/2 - LEATHER - 6, -HB/2 - RXZ])
                cube([2*W, LEATHER + 6.5, HB + 2*RXZ]);
        }
        aperture_cuts();
    }
}

module flap() {
    intersection() {
        skin();
        union() {
            // over the top, full depth
            translate([-W, -D/2 - 6, HB/2 - 3]) cube([2*W, D + 12, 20]);
            // and down the back
            translate([-W, D/2 - 1, SNAP_Z - 6]) cube([2*W, 12, HB/2 - SNAP_Z + 8]);
        }
    }
}

module d_ring(id = 10, wire = 2.2) {
    rotate([0, 90, 0]) rotate_extrude($fn = 40)
        translate([id/2 + wire/2, 0]) circle(d = wire, $fn = 20);
}

module tabs() {
    for (sx = [-1, 1]) translate([sx*(W/2 - 8), 0, HB/2 - 1]) {
        color([0.42, 0.24, 0.13]) cube([13, D - 3, 7], center = true);
        translate([0, 0, 6.5]) color("goldenrod") d_ring();
    }
}

// Strap: a loop hung from both rings. Drawn as a swept flat section so it
// reads as 14mm strap rather than cord - the difference matters, because a
// cord is what chafes.
module strap(loop = 800) {
    ax = W/2 - 8;  az = HB/2 + 12;      // ring centres
    rise = loop/2 - 40;
    steps = 44;
    pts = [ for (i = [0 : steps])
        let (t = i/steps, a = t*180)
        [ ax*cos(a),
          -sin(a)*sin(a)*18,             // bows away from the chest at the nape
          az + rise*sin(a)*0.62 ] ];
    for (i = [0 : steps - 1]) hull() {
        translate(pts[i])     scale([1, 0.16, 1]) sphere(r = 7, $fn = 16);
        translate(pts[i + 1]) scale([1, 0.16, 1]) sphere(r = 7, $fn = 16);
    }
}

module nape_sleeve() {
    ax = W/2 - 8;  az = HB/2 + 12;  rise = 800/2 - 40;
    steps = 44;
    for (i = [0 : steps - 1]) {
        t0 = i/steps;
        if (t0 > 0.36 && t0 < 0.64) hull() {
            for (j = [i, i + 1]) let (a = (j/steps)*180)
                translate([ax*cos(a), -sin(a)*sin(a)*18, az + rise*sin(a)*0.62])
                    scale([1, 0.26, 1]) sphere(r = 10, $fn = 16);
        }
    }
}

module pouch() {
    color([0.20, 0.21, 0.20]) shell_solid(0);          // laminate back + gusset
    color([0.42, 0.24, 0.13]) front_panel();           // veg-tan front
    color([0.38, 0.21, 0.11]) flap();
    translate([0, D/2 + LEATHER - 0.4, SNAP_Z])
        rotate([-90, 0, 0]) color("goldenrod") cylinder(d = 9, h = 1.4);
    tabs();
}

if (part == "front")      front_panel();
else if (part == "pouch") pouch();
else {
    pouch();
    color([0.40, 0.23, 0.12]) strap();
    color([0.16, 0.17, 0.17]) nape_sleeve();
}

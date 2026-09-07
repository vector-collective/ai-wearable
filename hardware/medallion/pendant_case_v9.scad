// =====================================================================
// KRAKEN MEDALLION v9 - resin, and two cord lugs.
//
// FIRST UNIT PRINTS ON A CREALITY HALOT-MAGE 8K in a rigid photopolymer.
// Three things change because of that, none of them cosmetic:
//
//   1. HEAT-SET INSERTS ARE GONE. They are a thermoplastic technique - the
//      brass melts its way into PLA. Resin is a thermoset and does not melt;
//      an insert set with a soldering iron just chars the boss. The front
//      bosses are now TAPPED M2 directly in the resin (pilot 1.65, tap
//      M2x0.4, resin taps cleanly at this wall thickness). Screws still
//      enter from the back.
//   2. HOLE COMPENSATION SHRINKS. FDM printed holes 0.1-0.4 undersize and the
//      file carried +0.4..+0.6 to cover it. An 8K mono LCD prints holes
//      ~0.05-0.15 undersize from light bleed and nothing else. The
//      allowances are retuned to ~+0.2; keeping the FDM values would have
//      left every gasketed mic disc rattling in its ring.
//   3. THE TWO-MATERIAL SPLIT IS OPTIONAL. A resin printer is one material.
//      Default relief is the single piece (kraken_wrap_bored.stl); paint the
//      eyes and toroids. The body/accents pair is still here for anyone with
//      an MMU (relief = "split").
//
// AND THE BAIL IS REPLACED BY TWO CORD LUGS, one either side of the top, so
// the pendant stops rotating and tilting on its leather cord. Each is an eye
// on the back shell's rim with a 5.0 hole whose axis runs along the rim
// tangent - the cord threads through along the rim, not front-to-back.
// Placed at 50 and 130 deg: 45mm apart, 20 deg clear of the LED window at
// 110, and outside the 45/135 deg bosses that sit inside the wall.
//
// (v8 was: two-material relief.)
//
// The eyes and the rings around every sucker now print in the SHELL
// filament, so they read as the case showing through the creature. That
// makes the relief a two-part object: kraken_body.stl in the kraken
// colour, kraken_accents.stl in the shell colour. They are exactly
// complementary - 4.744 + 0.269 = 5.013 cm3, the whole relief - so the
// slicer treats them as one object with two materials, not as two parts
// that have to be aligned.
//
// Meshy exports carry no per-feature tags, so both features are found from
// the geometry by split_materials.py. See that file for how, and for the
// one place it needed a hand: the beak.
//
// THIS NEEDS AN MMU/AMS OR A TOOLCHANGER. The accents are scattered over
// 100-odd separate islands at many different heights, so a single-extruder
// colour swap cannot produce them - there is no single Z to swap at.
//
// (v7 was: the arms now WRAP THE RIM.)
//
// WHAT CHANGED FROM v6, and why it needed more than a scale factor:
// the Meshy sculpt is a flat-backed bas-relief. There is no geometry
// below its face plane at all, so simply enlarging it makes the arms
// hang off the edge into thin air - they would overhang the rim, not
// grip it. So the mesh is BENT: every vertex is decomposed into
// (footprint position, height above the back plane), the footprint is
// re-projected onto the case's real outer surface - flat disc, then a
// 2.5mm rim fillet, then the r=35 cylinder - and the height is re-applied
// along that surface's local normal. Arms that used to run off the edge
// now curl over the fillet and down the side, standing 0.83mm proud of
// the cylinder and reaching 1.95mm below the glue plane.
//
// THE RIM FILLET IS NOT DECORATION. The bend assumes that exact surface,
// so the front shell's outer edge at z=0 is now a true 2.5mm round-over
// (rim_fillet_r below). Print it sharp and the arms' back faces would cut
// straight through the corner material.
//
// SCALE IS A SOLVED TRADE, not a taste call. Sweeping the sculpt against
// the full opening solve:
//     XY x1.28  56.3% face coverage, 5.50mm wrap, only 2 of 4 openings fit
//     XY x1.22  52.9%,               3.85mm wrap, only 2 of 4 openings fit
//     XY x1.16  47.9%,               1.95mm wrap, ALL FOUR fit  <- taken
// 1.16 is the largest the three mic ports survive. Face coverage is up
// from v6's 41.4%. (48.9% under the old smoothed mask; 47.9% is the
// honest number the true mask reports for the same mesh.)
//
// PRINTING COST: the relief's back is no longer flat, so it can no longer
// print back-down straight onto the bed. See the note at the bottom.
//
// (v6 was: flattened, wider sculpt, no extension cable.)
// v4 did not assemble; this fixes every blocker that review found.
//
// DECISIONS TAKEN (owner's, this revision):
//   camera stays in phase 1; thicker pendant rather than a smaller cell;
//   LED keeps its RGB but moves to the TOP so the wearer can see it;
//   assembly adhesive is E7000, not foam tape.
//
// Z-BUDGET - the blocker that killed v4. Rebuilt on the owner's MEASURED
// board sandwich of 10.0mm to the base of the camera bezel (v4 was drawn
// against a 7.9mm build-up ESTIMATE, which was already 0.4mm short):
//     cell 6.3 + swell 0.5 + puncture guard 0.6 + board 10.0
//   + bezel/lens 2.5 + E7000 bond 0.3            = 20.2mm required
//   front_d 8.0 + back_d 17.0 - 2*wall           = 21.4mm available
//                                                 = +1.2mm margin
// E7000 rather than foam tape is worth 0.7mm of that. The puncture guard
// is NOT optional now: without tape's cushion, the board's solder tails
// sit against a bare pouch.
// Total pendant = 25.0mm case + 7.0mm relief = 32.0mm. It is a thick
// object; that is the honest cost of stacking the board on this cell.
//
// EVERYTHING ON THE RIM. v4 put the button on the art face, where its
// nut could not seat (26.2mm out, only 14.0mm of nut room) and where a
// hug applies 1.5s of even pressure - the exact record gesture. The
// button, USB-C and power switch now live in the rim. Only the bottom
// arc (255-285 deg) clears the battery for a 14.4mm housing, verified by
// sweeping the housing tip against the pocket; the shallower USB-C jack
// and switch fit at 300 and 240 deg on the same test.
//
// LED MOVED TO THE TOP RIM. Measured: light from a 6mm bore in a 1.8mm
// wall spreads ~47-59 deg, the wearer's eye is ~86 deg off that axis and
// a partner at 1.5m is ~30 deg - so the front face aimed the status light
// at exactly the wrong person. In the top rim the wearer sees it looking
// down and the relief shades it from anyone in front.
//
// LAYOUT SOLVED STRUCTURE-FIRST. v4 solved sensors, then discovered the
// bosses. Here the boss ring is fixed first (5 bosses, max angular gap 90
// deg vs v4's 143, all outside the battery pocket, all r+boss <= 33.2),
// then sensors are solved around it against the sculpt's own silhouette.
// Reported module-to-wall clearance is the real one - v4's camera passed
// its check by 0.09mm because the check used the hole, not the module.
//
// STILL YOUR BENCH: charge current (at 100mA a refill outlasts the
// runtime and the device loses ground daily); the camera needs Seeed's
// EXTENSION FPC - the stock cable is meant to fold onto the board and
// will not reach (47.2, 57.6); and the sculpt needs a local ~3mm relief
// pocket at the camera, where mean sculpt height is 0.3mm but peaks 2.8.
// =====================================================================

part = "assembled";

case_r  = 35;
wall    = 1.8;
front_d = 6.5;      // v6: trimmed to the minimum the mic ring+disc stack (3.8) allows
back_d  = 17.8;     // v6: interior 20.7 vs 20.2 needed - margin +0.5
case_d  = front_d + back_d;
inner_r = case_r - wall;
cx = 35; cy = 35;
$fn = 96;

// Rim round-over at the art face. MUST match the RF used by the mesh
// bend (2.5) or the wrapped arms and the case fight for the same space.
rim_fillet_r = 2.5;
rim_fillet_r0 = case_r - rim_fillet_r;   // 32.5

// ---- hardware ----
batt_t = 6.3; batt_w = 34.5; batt_l = 51;      // LP603449 MAX envelope
batt_clear = 0.6;                               // per side - v4 had ZERO
mic_disc_d = 14; led_disc_d = 10;
mic_port_d = 2.2;
ring_bore_extra = 0.25;  // v9 resin: light bleed only, ~0.1 undersize (FDM was 0.6)
ring_wall = 1.4;         // was 1.0 - too fragile to retain
ring_h    = 2.2;         // was 1.2 - now takes a PORON gasket, see below
cam_lens_d = 8.8;        // v9 resin: 8.5 nominal +0.3 (FDM needed 9.0)
cam_mod_half_diag = 6.01;
button_hole_d = 12.3;    // v9 resin: 12.0 thread +0.3 (FDM needed 12.6)
button_depth  = 14.4;
usbc_w = 10.5; usbc_h = 4.0; usbc_depth = 12.0;
sw_d = 7.0; sw_depth = 8.0;
led_window_d = 6.4;

// ---- fasteners: M2 tapped in resin ----
// v5-v8 used heat-set brass inserts, which cannot be set in a thermoset.
// Resin at 50+ MPa takes an M2x0.4 thread directly: pilot 1.65 (tap drill
// is 1.6; the extra 0.05 covers light bleed), boss 7.0 OD for a 2.7mm wall
// around the thread. Tap by hand with a taper tap, no power. If a thread
// ever strips, drill to 2.5 and epoxy in a press-fit M2 insert - epoxy, not
// heat.
boss_od = 7.0; tap_pilot_d = 1.65; screw_clear_d = 2.3; screw_head_d = 4.1;
boss_xy = [[55.5,55.5],[14.5,55.5],[14.5,14.5],[55.5,14.5]];  // v6: true 90deg bolt circle, r=29

// ---- solved layout (structure-first; see header) ----
// v7: re-solved against the TRUE silhouette. v5/v6 rasterised the sculpt
// and then morphologically OPENED the mask by 2 cells, which deleted
// every tendril thinner than 1.6mm - and tendrils are exactly what lay
// over the camera. Measured against real vertices, v6's camera was
// buried 1.90mm under artwork. The mask now closes 1-cell rasterisation
// gaps and re-erodes by the same amount, so nothing real is lost.
// Only mesh at z<=0.3 can block a face opening; the arms on the rim
// cannot, so the solve uses face geometry only.
cam_xy  = [48.4, 38.8];   // module-to-wall 13.26mm
mic1_xy = [22.8, 41.6];   // 10.93mm, sculpt gap 1.20
mic2_xy = [44.4, 22.0];   //  8.76mm, sculpt gap 1.20
mic3_xy = [25.6, 21.6];   //  8.43mm, sculpt gap 1.20
// mic spread 18.8 / 20.2 / 29.2mm

// THE CAMERA LOOKS THROUGH THE ARTWORK, and that is forced, not chosen:
// swept over the whole board-reach window there is NO lens position with
// 4.5mm of clear sculpt gap - the best anywhere is 3.6mm. So the aperture
// is bored through the relief as well (see kraken_wrap_bored.stl).
//   bore 9.4mm dia, 3.27mm deep  +  1.8mm shell wall = 4.89mm tunnel
//   aspect 0.54:1 against a 9.0mm aperture -> no vignetting; an OV2640's
//   65deg cone needs better than 1.6:1.
// Bore depth barely varies across the reach window (3.09 at best, 3.37 at
// worst), so this spot was picked for BOARD SLACK instead: +/-2.0mm of
// travel in every direction before the stock FPC is stressed.
// The three mic ports get a 4.0mm relief bore too. They already cleared
// the artwork - but only by 0.07-0.23mm, which is inside hand-alignment
// error and close enough for E7000 squeeze-out to bridge into a 2.2mm
// port. The bore removes only the grazing tendril edge (3-5mm3 each) and
// takes every port to 0.90mm of clear air.

button_ang = 270;   // dead bottom: the only arc clearing the battery
usbc_ang   = 300;
sw_ang     = 240;
led_ang    = 110;   // top rim, between the two cord lugs

// Cord lugs (v9). Eye centre sits lug_stand from the disc centre so the hole
// clears the rim by 1.0; wall around the hole is lug_r - lug_hole_d/2 = 2.5.
lug_ang    = [50, 130];
lug_hole_d = 5.0;      // 3-4mm round leather cord, knotted
lug_r      = 5.0;      // eye outer radius
lug_w      = 8.0;      // width along the rim tangent
lug_stand  = case_r + 3.5;

// spigot/groove register - v4 butted flat with 87.7mm of unsupported rim
spig_or = 34.2; spig_ir = 33.4; spig_h = 1.0;
spig_clr = 0.06;    // v9 resin: per side (FDM was 0.10); 0.12 diametral

module disc(h)       { translate([cx,cy,0]) cylinder(r=case_r,  h=h); }
// The corner wedge outside the fillet arc. Subtracting it from the disc
// gives the art face a 2.5mm round-over, which is the surface the mesh
// bend was computed against.
module rim_fillet_cut() {
    translate([cx,cy,0]) rotate_extrude()
        difference() {
            translate([rim_fillet_r0, -1]) square([rim_fillet_r + 2, rim_fillet_r + 1]);
            translate([rim_fillet_r0, rim_fillet_r]) circle(r = rim_fillet_r);
        }
}
module disc_inner(h) { translate([cx,cy,0]) cylinder(r=inner_r, h=h); }
module boss_positions() { for (p=boss_xy) translate([p[0],p[1],0]) children(); }

// Radial cut through the rim at `ang`, `depth` inward from the surface.
module rim_cyl(ang, dia, depth, zc) {
    translate([cx+case_r*cos(ang), cy+case_r*sin(ang), zc])
        rotate([0,0,ang]) rotate([0,90,0])
            translate([0,0,-depth]) cylinder(d=dia, h=depth+2);
}
// Solid pad growing INWARD from the inner wall, axis along the radius -
// gives a flat face perpendicular to the radius for a disc to sit on.
module led_pad(ang, dia, depth, zc) {
    translate([cx+inner_r*cos(ang), cy+inner_r*sin(ang), zc])
        rotate([0,0,ang]) rotate([0,90,0])
            translate([0,0,-depth]) cylinder(d=dia, h=depth);
}
module led_pocket(ang, dia, depth, zc) {
    translate([cx+inner_r*cos(ang), cy+inner_r*sin(ang), zc])
        rotate([0,0,ang]) rotate([0,90,0])
            translate([0,0,-3.0]) cylinder(d=dia, h=depth);
}
// One cord lug: an eye whose axis is the rim tangent at `ang`, hulled to
// a thin slab buried in the wall so it blends into the rim rather than
// butting onto it. The slab is kept inside the wall (r 34.0..34.2) so the
// hull never reaches the cavity. The hole is cut afterwards.
module lug_eye(ang, rr, r, w) {
    translate([cx + rr*cos(ang), cy + rr*sin(ang), back_d/2])
        rotate([0,0,ang]) rotate([90,0,0]) cylinder(r=r, h=w, center=true);
}
module cord_lug(ang) {
    hull() {
        lug_eye(ang, lug_stand, lug_r, lug_w);
        translate([cx + (case_r-0.9)*cos(ang), cy + (case_r-0.9)*sin(ang), back_d/2])
            rotate([0,0,ang]) cube([0.2, lug_w, 2*lug_r], center=true);
    }
}
module cord_lug_hole(ang) {
    translate([cx + lug_stand*cos(ang), cy + lug_stand*sin(ang), back_d/2])
        rotate([0,0,ang]) rotate([90,0,0]) cylinder(d=lug_hole_d, h=lug_w+2, center=true);
}
module rim_box(ang, w, h, depth, zc) {
    translate([cx+case_r*cos(ang), cy+case_r*sin(ang), zc])
        rotate([0,0,ang]) rotate([0,90,0])
            translate([-w/2,-h/2,-depth]) cube([w,h,depth+2]);
}

// Locating ring, sunk 0.3 into the floor so it unions into one solid.
// Taller than v4 so a PORON gasket seals each mic to its own front
// volume: unsealed 2.2mm ports onto the shared cavity form a Helmholtz
// resonator at 299Hz - the middle of the speech band. Gasketed, 24.8kHz.
module disc_ring(p, dd) {
    translate([p[0],p[1],wall-0.3]) difference() {
        cylinder(r=dd/2+ring_bore_extra/2+ring_wall, h=ring_h+0.3);
        translate([0,0,-0.5]) cylinder(d=dd+ring_bore_extra, h=ring_h+1.5);
    }
}

// ---------------------------------------------------------------------
// FRONT SHELL - art face. No screw holes, no button, no LED.
// ---------------------------------------------------------------------
module front_shell() {
    difference() {
        union() {
            difference() {
                disc(front_d);
                rim_fillet_cut();
                translate([0,0,wall]) disc_inner(front_d);
                translate([cam_xy[0],cam_xy[1],-1]) cylinder(d=cam_lens_d, h=wall+2);
                for (p=[mic1_xy,mic2_xy,mic3_xy])
                    translate([p[0],p[1],-1]) cylinder(d=mic_port_d, h=wall+2);
            }
            // bosses AFTER the cavity cut - unioned before, disc_inner
            // swallows them whole, which is what happened in v4.
            boss_positions() translate([0,0,wall-0.3])
                cylinder(d=boss_od, h=front_d-wall+0.3);
            disc_ring(mic1_xy, mic_disc_d);
            disc_ring(mic2_xy, mic_disc_d);
            disc_ring(mic3_xy, mic_disc_d);
            // register spigot on the rim
            translate([cx,cy,front_d]) difference() {
                cylinder(r=spig_or, h=spig_h);
                translate([0,0,-0.5]) cylinder(r=spig_ir, h=spig_h+1);
            }
            // camera seat: 9.2 square pocket wall so the module locates
            translate([cam_xy[0],cam_xy[1],wall-0.3])
                difference() {
                    cylinder(d=cam_lens_d+2*ring_wall+1.2, h=2.0+0.3);
                    translate([0,0,-0.5]) cube([9.4,9.4,6], center=true);
                }
        }
        boss_positions() translate([0,0,wall+0.2]) cylinder(d=tap_pilot_d, h=front_d);
    }
}

// ---------------------------------------------------------------------
// BACK SHELL - battery, bail, and every rim-mounted control.
// ---------------------------------------------------------------------
module back_shell() {
    lip_h = 3.0;                       // was 2.0: retained only 32% of the cell
    bx = cx - (batt_l+2*batt_clear)/2;
    by = cy - (batt_w+2*batt_clear)/2;
    difference() {
        union() {
            difference() {
                union() {
                    disc(back_d);
                    for (a = lug_ang) cord_lug(a);
                }
                translate([0,0,wall]) disc_inner(back_d);
                // register groove, 0.2 diametral clearance on the spigot
                translate([cx,cy,back_d-spig_h-0.1]) difference() {
                    cylinder(r=spig_or+spig_clr, h=spig_h+0.2);
                    translate([0,0,-0.5]) cylinder(r=spig_ir-spig_clr, h=spig_h+1.2);
                }
                for (a = lug_ang) cord_lug_hole(a);
                // rim controls
                rim_cyl(button_ang, button_hole_d, button_depth+2, back_d/2);
                rim_box (usbc_ang,  usbc_w, usbc_h, usbc_depth,   back_d/2);
                rim_cyl (sw_ang,    sw_d,   sw_depth,             back_d/2);
                rim_cyl (led_ang,   led_window_d, 6.0,            back_d/2);
            }
            boss_positions() translate([0,0,wall-0.3])
                cylinder(d=boss_od, h=back_d-wall+0.3);
            // battery bay: real clearance, taller lip, wire-exit notch
            translate([bx,by,wall-0.3]) difference() {
                cube([batt_l+2*batt_clear+1.2, batt_w+2*batt_clear+1.2, lip_h+0.3]);
                translate([0.6,0.6,-1]) cube([batt_l+2*batt_clear, batt_w+2*batt_clear, lip_h+2]);
                translate([-1, (batt_w+2*batt_clear+1.2)/2-3.5, -1]) cube([3, 7, lip_h+2]);
            }
            // LED seat: a RADIAL pad on the inner wall, so the 10mm disc
            // mounts flat facing outward at the top rim port. Placed on the
            // floor at r=32.2 it overhung the outer wall and got sliced.
            led_pad(led_ang, led_disc_d+2*ring_wall+1.0, 3.0, back_d/2);
        }
        // insert bores + screw clearance from the back exterior
        boss_positions() {
            translate([0,0,-1]) cylinder(d=screw_clear_d, h=back_d+2);
            translate([0,0,-0.01]) cylinder(d=screw_head_d, h=1.4);   // cap-head counterbore
        }
        led_pocket(led_ang, led_disc_d+0.6, 1.8, back_d/2);
        rim_cyl(led_ang, led_window_d, 6.0, back_d/2);
    }
}

// relief = "single": one piece, kraken_wrap_bored.stl - the resin default,
//                    paint the eyes and toroids.
// relief = "split":  body + accents, geometrically the same union, for an
//                    MMU FDM printer.
relief = "single";
module kraken_body()    { import("kraken_body.stl"); }
module kraken_accents() { import("kraken_accents.stl"); }
module kraken_single()  { import("kraken_wrap_bored.stl"); }
module kraken() { if (relief == "split") { kraken_body(); kraken_accents(); } else kraken_single(); }

// ---------------------------------------------------------------------
// PRINTING THE RELIEF (part="kraken")
// The back is no longer a plane: the outer ~3mm annulus curls down 1.95mm
// to grip the rim. Sat sculpt-up on the bed it now rests on eight arm
// tips with the glue face 1.95mm in the air, and the fillet band goes
// vertical at its outer edge. Two workable answers:
//   FDM - print sculpt-up and paint supports on the outer annulus only
//     (block them everywhere else). The scarring lands on the glue face
//     and on the fillet band, both hidden once bonded; E7000 is gap-
//     filling and tolerates the texture.
//   Resin / SLS / MJF - free. This is the better part for a service.
// ---------------------------------------------------------------------

if      (part=="front")     front_shell();
else if (part=="back")      back_shell();
else if (part=="kraken")    mirror([0,0,1]) kraken();
else if (part=="kraken_body")    mirror([0,0,1]) kraken_body();
else if (part=="kraken_accents") mirror([0,0,1]) kraken_accents();
else if (part=="assembled") {
    color("gainsboro") front_shell();
    color("indianred") kraken_body();
    color("gainsboro") kraken_accents();     // same material as the shell
}
else { front_shell(); translate([2*case_r+12,0,0]) back_shell(); }

// =====================================================================
// KRAKEN MEDALLION v7 - the arms now WRAP THE RIM.
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
ring_bore_extra = 0.6;   // FDM shrinks holes 0.1-0.4; v4 allowed 0.4 total
ring_wall = 1.4;         // was 1.0 - too fragile to retain
ring_h    = 2.2;         // was 1.2 - now takes a PORON gasket, see below
cam_lens_d = 9.0;        // was 8.5 nominal -> printed ~8.25, negative clearance
cam_mod_half_diag = 6.01;
button_hole_d = 12.6;    // was 12.2 -> printed ~11.95 against a 12.0 thread
button_depth  = 14.4;
usbc_w = 10.5; usbc_h = 4.0; usbc_depth = 12.0;
sw_d = 7.0; sw_depth = 8.0;
led_window_d = 6.4;

// ---- fasteners: heat-set inserts, not self-tapped plastic ----
// v4 self-tapped M2 into 4.0mm of plastic on a case opened every battery
// swap - that strips in 5-10 cycles. M2 brass inserts need a 3.4mm bore,
// so the boss grows to 7.5 OD for a 2.05mm wall.
boss_od = 7.5; insert_bore_d = 3.4; screw_clear_d = 2.6; screw_head_d = 4.2;
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
led_ang    = 110;   // top rim, offset clear of the bail

bail_cy = 70; bail_w = 15; bail_top = 79; bail_hole_d = 4.5; bail_hole_cy = 74;

// spigot/groove register - v4 butted flat with 87.7mm of unsupported rim
spig_or = 34.2; spig_ir = 33.4; spig_h = 1.0;

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
        boss_positions() translate([0,0,wall+0.2]) cylinder(d=insert_bore_d, h=front_d);
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
                    hull() {
                        translate([cx, bail_cy-4, 0]) cylinder(d=bail_w, h=back_d);
                        translate([cx, bail_top-bail_w/2, 0]) cylinder(d=bail_w, h=back_d);
                    }
                }
                translate([0,0,wall]) disc_inner(back_d);
                // register groove, 0.2 diametral clearance on the spigot
                translate([cx,cy,back_d-spig_h-0.1]) difference() {
                    cylinder(r=spig_or+0.1, h=spig_h+0.2);
                    translate([0,0,-0.5]) cylinder(r=spig_ir-0.1, h=spig_h+1.2);
                }
                translate([cx-bail_w, bail_hole_cy, back_d/2]) rotate([0,90,0])
                    cylinder(d=bail_hole_d, h=2*bail_w);
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

module kraken() { import("kraken_wrap_bored.stl"); }  // XY x1.16, depth x0.55, bent over the rim, apertures bored

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
else if (part=="assembled") { color("tan") front_shell(); color("indianred") kraken(); }
else { front_shell(); translate([2*case_r+12,0,0]) back_shell(); }

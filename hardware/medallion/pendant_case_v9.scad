// =====================================================================
// KRAKEN MEDALLION v9.1 - resin, and two cord lugs.
//
// FIRST UNIT PRINTS ON A CREALITY HALOT-MAGE 8K in a rigid photopolymer.
// Three things change because of that, none of them cosmetic:
//
//   1. HEAT-SET INSERTS ARE GONE. They are a thermoplastic technique - the
//      brass melts its way into PLA. Resin is a thermoset and does not melt;
//      an insert set with a soldering iron just chars the boss. Tapping the
//      resin was rejected by both reviews (strips in 3-5 reopenings). Each
//      front boss now captures a DIN 934 M2 nut in a side-entry slot under
//      a resin ceiling; screws still enter from the back. (v9.1: the v9
//      hex pocket was open to the parting face and could not clamp the
//      front shell - see the fasteners block.)
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
// runtime and the device loses ground daily). v6's other two blockers are
// gone: the camera sits over the board with the STOCK FPC folded (see
// board_xy) and the aperture is bored through the relief.
// =====================================================================

part = "assembled";

case_r  = 35;
wall    = 1.8;
front_d = 6.5;      // v6: trimmed to the minimum the mic ring+disc stack (3.8) allows
back_d  = 18.1;     // v9: interior 21.0 vs 20.2. Resin post-cure shrinks 0.3-1%;
                    // at 1% the old 20.7 interior became 20.49 against a stack
                    // that is itself +/-0.3 - negative. Grown here, not the front:
                    // front_d is what the arm-wrap bend was computed against.
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
ring_bore_extra = 0.25;  // v9 resin: bore 14.25 prints ~14.15, ~0.08/side (FDM was 0.6)
ring_wall = 1.4;         // was 1.0 - too fragile to retain
ring_h    = 2.2;         // was 1.2 - now takes a PORON gasket, see below
cam_lens_d = 9.0;        // an OPTICAL aperture, not a fit: the 8.5 square module
                         // has a 12.0 diagonal and never passes through it
cam_mod_half_diag = 6.01;
button_hole_d = 12.3;    // v9 resin: 12.0 thread +0.3 (FDM needed 12.6)
button_depth  = 14.4;
usbc_w = 10.5; usbc_h = 4.0; usbc_depth = 12.0;
sw_d = 7.2; sw_depth = 8.0;     // v9: 7.0 was zero clearance on an M7 bushing
led_window_d = 6.4;

// ---- fasteners: captured M2 nuts in the front bosses ----
// v5-v8 used heat-set brass inserts, which cannot be set in a thermoset.
// Tapping M2 into rigid resin was the next idea and both reviews rejected
// it for a case opened every battery swap: 3-5 cycles and it strips or
// cracks. So: a DIN 934 M2 nut (4.0 AF x 1.6) is captured in each front
// boss and an M2x20 comes through from the back. Load on the resin is pure
// compression. No thread, no adhesive, and a spare nut fixes anything.
//
// v9.1: THE NUT SITS IN A SIDE-ENTRY SLOT UNDER A RESIN CEILING, not in a
// hex pocket open to the parting face. The v9 pocket could not clamp the
// front shell at all: a screw from the back pulls its nut TOWARD the head,
// so a nut in a pocket open to the parting face simply lifts its 0.2 of
// play and bears on the BACK shell's boss face. Head, back shell and nut
// became the whole load path and the front shell was held by spigot
// friction alone - and it would have felt tight. With a ceiling the nut
// pulls up against front-shell resin and the shells are clamped together.
// The slot enters from the boss's cavity side (radially inward), 4.25 wide
// x 1.8 tall; the nut slides in flats-first and cannot turn.
boss_od = 8.0;            // 1.9 wall either side of the slot
nut_af = 4.25;            // 4.0 nut + 0.25, across the slot
nut_ac = 4.0/cos(30);     // 4.62 across corners, along the slot
nut_slot_h = 1.8;         // 1.6 nut + 0.2 (a 1.5 nut has 0.3 of play; fine)
nut_slot_z = 2.4;         // slot floor: front wall 1.8 + 0.6
nut_ceiling = front_d - nut_slot_z - nut_slot_h;   // 2.3 of resin the nut clamps against
nut_slot_in = 5.0;        // slot runs this far past the boss's inner face into the cavity
nut_clear_d = 2.4;        // screw tip clearance bore below the slot floor
nut_clear_z = 1.9;        // its floor: front wall 1.8 + 0.1
screw_clear_d = 2.3; screw_head_d = 4.0;   // ISO 7380 M2 button head, 3.8 x 1.3 tall
screw_cbore_h = 2.5;      // v9.1: was 1.5. Head 1.2 sub-flush. Deeper so an M2x20
                          // reaches through the ceiling and the whole nut:
                          // head underside 15.6 behind the parting face, tip 4.4
                          // ahead of it = z 2.1 in the front shell, 0.5 past a
                          // 1.6 nut's far face (2.6) and 0.2 into the clearance bore
boss_xy = [[56.57,56.57],[13.43,56.57],[13.43,13.43],[56.57,13.43]];  // v9: r=30.5. At r=29 an
                          // 8.0 boss bit 0.75 into a max-envelope cell; at 30.5 it clears by 0.3
                          // and fuses 1.3 into the wall, which is fine in resin.

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

// BOARD KEEP-OUT (v9). The stock FPC folds 180 deg so the camera module
// lies ON the Sense board, centred 2-4mm off board centre toward the SD
// end, with +/-2mm play along the FPC and +/-0.5 across. So the board sits
// with its LONG AXIS ALONG X and its centre 3mm from the lens in X. Long
// axis along Y overlaps mic2's disc by ~5mm - the board's front face is at
// z=4.3 in front-shell terms and the mic stack tops out at 5.6, so the
// board may not overlap any disc. Long-axis-X clears mic2 by 0.9 in Y.
board_xy = [45.4, 38.8];
board_l = 21.0; board_w = 17.5; board_h = 10.0;   // measured, to bezel base

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
usbc_ang   = 295;   // v9: at 300 the jack, now lying flat (10.5 wide), cut 1.4 into the 315 boss
sw_ang     = 243;   // v9: at 240 the switch nut hit the 225 deg boss
led_ang    = 110;   // top rim, between the two cord lugs

// Cord ears (v9). A vertical cylinder either side of the top, full back_d
// height, with the cord hole running along the rim tangent. The root is a
// TRUE r=3 concave fillet made in 2D (offset out, then back in) before the
// extrusion - the earlier hull-to-slab lug left a sharp internal corner at
// the rim, which is exactly where a rigid resin starts a crack. Sized so
// the cord is the fuse: 4mm leather breaks at 300-450N, the ear is good
// for 600N at >= 3x on a 20MPa notched allowable.
// 45/135: chord 55mm between holes, the root sits over the 45/135 boss
// column inside the wall, and the 135 ear ends 14 deg clear of the LED.
lug_ang    = [45, 135];
lug_hole_d = 5.5;      // 3-4mm round leather cord; an overhand knot in
                       // either will not pass it
ear_d      = 11.0;     // 2.75 wall around the hole
ear_stand  = 39.0;     // ear centre radius: inner edge at 33.5, clear of the
                       // 33.2 cavity; hole inner edge at 36.25, 1.25 off the rim
ear_fillet = 3.0;

// spigot/groove register - v4 butted flat with 87.7mm of unsupported rim
spig_or = 34.2; spig_ir = 33.2; spig_h = 1.0;   // v9: ir = inner_r. At 33.4 the groove
                                                // left a 0.1 x 1.2 fin the wash would snap off.
spig_clr = 0.15;    // per side, 0.3 diametral. MORE than FDM, not less: light
                    // bleed narrows a 0.8 slot by 0.1-0.2, and a 0.2% cure
                    // mismatch between two separately printed 68mm parts is
                    // 0.14. The old 0.2 diametral went to zero.

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
// Starts 0.5 INSIDE the pad's inner face rather than on it. v5-v8 began the
// pocket exactly at r=30.2, coincident with the pad face it opens onto, and
// CGAL left 20 non-manifold edges around the mouth. Closed, but not clean.
module led_pocket(ang, dia, depth, zc) {
    translate([cx+inner_r*cos(ang), cy+inner_r*sin(ang), zc])
        rotate([0,0,ang]) rotate([0,90,0])
            translate([0,0,-3.5]) cylinder(d=dia, h=depth+0.5);
}
// The ears and their root fillets, as the part of the 2D outline that lies
// OUTSIDE the disc. offset(+f) then offset(-f) rounds every concave corner
// of the union to radius f and touches nothing else; subtracting the disc
// leaves just the ears and the two webs at each root, all at r >= 35.
module ears_2d() {
    difference() {
        offset(r=-ear_fillet) offset(r=ear_fillet) {
            translate([cx,cy]) circle(r=case_r);
            for (a = lug_ang)
                translate([cx + ear_stand*cos(a), cy + ear_stand*sin(a)]) circle(d=ear_d);
        }
        translate([cx,cy]) circle(r=case_r-0.02);
    }
}
module ears() { linear_extrude(back_d) ears_2d(); }
// Cord hole along the tangent, both mouths chamfered 1.0 x 45 so the cord
// bends over an edge, not a corner.
module ear_hole(ang) {
    translate([cx + ear_stand*cos(ang), cy + ear_stand*sin(ang), back_d/2])
        rotate([0,0,ang]) rotate([90,0,0]) {
            cylinder(d=lug_hole_d, h=ear_d+2, center=true);
            for (sgn=[-1,1]) mirror([0,0,sgn<0?1:0])
                translate([0,0,ear_d/2-1.0]) cylinder(d1=lug_hole_d, d2=lug_hole_d+2.4, h=1.2);
        }
}
// Chest-side round-over on the back shell's outer edge. Same construction
// as the art face's rim fillet; applied to the disc only, before the ears
// are added, so it never carves into an ear's foot.
module edge_fillet_cut(r) {
    translate([cx,cy,0]) rotate_extrude()
        difference() {
            translate([case_r - r, -1]) square([r + 2, r + 1]);
            translate([case_r - r, r]) circle(r = r);
        }
}
// Flat seat on the INSIDE of the rim for a panel-mount nut or flange. The
// inner wall is a concave r=33.2 cylinder; a 14.4 nut across it has 0.75mm
// of sagitta and bears on two edges. PETG yielded to that. Resin cracks.
// The pad thickens the wall to pad_wall at the seat centre and presents a
// plane; at the seat's edge the wall is still >= 1.8.
module rim_pad(ang, w, h, zc, pw) {
    translate([cx+(case_r-pw)*cos(ang), cy+(case_r-pw)*sin(ang), zc])
        rotate([0,0,ang]) translate([0, -w/2, -h/2]) cube([pw+0.5, w, h]);
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
                // 0.3 x 45 lead-in on the outer edge: the tongue finds the
                // rebate instead of catching its lip
                translate([0,0,spig_h-0.3]) difference() {
                    cylinder(r=spig_or+1, h=0.4);
                    cylinder(r1=spig_or-0.3, r2=spig_or+0.1, h=0.4);
                }
            }
            // camera seat: 9.2 square pocket wall so the module locates
            translate([cam_xy[0],cam_xy[1],wall-0.3])
                difference() {
                    cylinder(d=15.0, h=2.0+0.3);   // v9: 9.0 pocket has a 6.36 half-diagonal;
                                                    // the old 13.0 seat left 0.14 at the corners
                    translate([0,0,-0.5]) cube([9.0,9.0,6], center=true);   // v9: 0.25/side, ~0.15 after bleed
                }
        }
        nut_slot_cut();
    }
}

// v9.1 captured-nut slot in each front boss. Local +x points at the case
// centre, so the slot's open mouth is on the cavity side and its closed end
// sits 0.2 past the nut's outer corner, 2.0 inside the outer surface.
module nut_slot_cut() {
    for (p = boss_xy) {
        a = atan2(cy - p[1], cx - p[0]);
        translate([p[0], p[1], 0]) rotate([0, 0, a]) {
            translate([-(nut_ac/2 + 0.2), -nut_af/2, nut_slot_z])
                cube([nut_ac/2 + 0.2 + boss_od/2 + nut_slot_in, nut_af, nut_slot_h]);
        }
        // screw clearance through the ceiling, and tip clearance under the floor
        translate([p[0], p[1], nut_slot_z + nut_slot_h - 0.01]) cylinder(d=screw_clear_d, h=nut_ceiling + 1);
        translate([p[0], p[1], nut_clear_z]) cylinder(d=nut_clear_d, h=nut_slot_z - nut_clear_z + 0.01);
    }
}

// ---------------------------------------------------------------------
// BACK SHELL - battery, bail, and every rim-mounted control.
// ---------------------------------------------------------------------
module back_shell() {
    lip_h = 3.0;                       // was 2.0: retained only 32% of the cell
    lip_w = 1.0;                        // v9: was 0.6, brittle in resin
    bx = cx - (batt_l+2*batt_clear)/2 - lip_w;   // pocket centred (v8 sat 0.6 off)
    by = cy - (batt_w+2*batt_clear)/2 - lip_w;
    difference() {
        union() {
            difference() {
                union() {
                    difference() {
                        union() {
                            difference() { disc(back_d); edge_fillet_cut(2.0); }   // v9: skin side
                            ears();
                        }
                        translate([0,0,wall]) disc_inner(back_d);
                    }
                    // seats AFTER the cavity cut, BEFORE the control cuts
                    // wall at the seat's edge = case_r - hypot(case_r-pw, w/2):
                    //   button 15 wide, pw 2.8 -> 1.94   usbc 18, 3.2 -> 1.95
                    //   switch 10, 2.6 -> 2.22. A 17-wide button seat at 2.6
                    //   thinned the edge to 1.50.
                    rim_pad(button_ang, 15.0, 14.0, back_d/2, 2.8);
                    rim_pad(usbc_ang,   18.0, 14.0, back_d/2, 3.2);   // flange ~17.5, still unmeasured
                    rim_pad(sw_ang,     10.0, 14.0, back_d/2, 2.6);
                }
                // register groove, 0.2 diametral clearance on the spigot
                translate([cx,cy,back_d-spig_h-0.2]) difference() {
                    cylinder(r=spig_or+spig_clr, h=spig_h+0.3);
                    translate([0,0,-0.5]) cylinder(r=spig_ir-spig_clr, h=spig_h+1.3);
                }
                for (a = lug_ang) ear_hole(a);
                // rim controls
                rim_cyl(button_ang, button_hole_d, button_depth+2, back_d/2);
                // v9: w along the rim, h vertical - the jack lies flat. rim_box's
                // first size runs along Z, so the args are swapped here on purpose.
                rim_box (usbc_ang,  usbc_h, usbc_w, usbc_depth,   back_d/2);
                rim_cyl (sw_ang,    sw_d,   sw_depth,             back_d/2);
                rim_cyl (led_ang,   led_window_d, 6.0,            back_d/2);
            }
            boss_positions() translate([0,0,wall-0.3])
                cylinder(d=boss_od, h=back_d-wall+0.3);
            // battery bay: real clearance, taller lip, wire-exit notch
            translate([bx,by,wall-0.3]) difference() {
                cube([batt_l+2*batt_clear+2*lip_w, batt_w+2*batt_clear+2*lip_w, lip_h+0.3]);
                translate([lip_w,lip_w,-1]) cube([batt_l+2*batt_clear, batt_w+2*batt_clear, lip_h+2]);
                // wire-exit notch, left end
                translate([-1, (batt_w+2*batt_clear+2*lip_w)/2-3.5, -1]) cube([3, 7, lip_h+2]);
                // button notch, bottom wall: the 12.25 body at 270 deg reaches
                // r=17.8 from its seat, the lip spans r 17.85-18.85. 14 wide
                // leaves 19.5 of wall either side; the cell is 51 long.
                translate([(batt_l+2*batt_clear+2*lip_w)/2-7.0, -1, -1]) cube([14, lip_w+2, lip_h+2]);
            }
            // LED seat: a RADIAL pad on the inner wall, so the 10mm disc
            // mounts flat facing outward at the top rim port. Placed on the
            // floor at r=32.2 it overhung the outer wall and got sliced.
            led_pad(led_ang, led_disc_d+2*ring_wall+1.0, 3.0, back_d/2);
        }
        // insert bores + screw clearance from the back exterior
        boss_positions() {
            translate([0,0,-1]) cylinder(d=screw_clear_d, h=back_d+2);
            translate([0,0,-0.01]) cylinder(d=screw_head_d, h=screw_cbore_h);
        }
        led_pocket(led_ang, led_disc_d+0.3, 1.8, back_d/2);   // v9 resin
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
    color("indianred") kraken();
    color("slategray") translate([0,0,front_d+back_d]) mirror([0,0,1]) back_shell();
    // ghost stack, in assembled coordinates (face at z=0, back floor at
    // front_d+back_d-wall). Cell against the back floor, board on the cell,
    // bezel reaching into the front cavity. The board spans the parting plane.
    zb = front_d + back_d - wall;                     // back floor inner face
    %translate([cx-batt_l/2, cy-batt_w/2, zb-batt_t]) cube([batt_l, batt_w, batt_t]);
    %translate([board_xy[0]-board_l/2, board_xy[1]-board_w/2, zb-batt_t-0.5-0.6-board_h])
        cube([board_l, board_w, board_h]);
    %translate([cam_xy[0]-4.25, cam_xy[1]-4.25, zb-batt_t-0.5-0.6-board_h-2.5]) cube([8.5, 8.5, 2.5]);
}
else { front_shell(); translate([2*case_r+12,0,0]) back_shell(); }

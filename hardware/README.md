# Hardware

Two containers for the same electronics. They are alternatives, not stages of
one thing — but the pouch is the one to build first.

| | [`medallion/`](medallion/) | [`pouch/`](pouch/) |
|---|---|---|
| What | 3D-printed disc case, kraken relief | Sewn leather-and-jersey pouch |
| Size | Ø70 × 29.4 mm, 63 across the ears | 78 × 62 × 24 mm |
| Made with | Halot-Mage 8K resin (v9); FDM with MMU still supported | Singer CG590, ~3 h for the first unit |
| Access to the cell | 4 × M2 into captured nuts | Flap and a snap |
| Mic mounting | Gasketed to a rigid faceplate | Ports through leather, mesh-backed |
| Cord | two ears, 55 mm apart | two D-ring tabs |
| Mic spread | 18.8 / 20.2 / 29.2 mm | 46 / 37 / 37 mm |

**Build the pouch first.** It gets the thing on your chest in an evening,
which is the only way to find out whether you stop noticing it — and that
question decides whether the project is worth continuing at all. The
medallion is the better object and the better acoustics, but it is a week of
printing and finishing to answer a question the pouch answers on Saturday.

**The medallion is still the better container**, for one reason worth
stating plainly: its mics are gasketed to rigid plastic. A 2.2 mm port
venting into a soft cavity resonates near 299 Hz — the middle of the speech
band — where a gasketed port sits at 24.8 kHz and out of the way. A fabric
front also flexes, which moves the mics relative to each other every time you
lean. `pouch/` §Mk2 covers cutting a Ø72 window in the leather so the printed
front shell drops in as a faceplate, which is how these two converge.

## Shared facts

Both are drawn against the same measured stack: LP603449 cell 51 × 34.5 ×
6.3 mm, board assembly 10.0 mm to the base of the camera bezel, 2.5 mm of
bezel and lens, **20.2 mm total**. All connections hand-soldered, no pin
headers anywhere. Assembly adhesive is E7000.

Still needs a bench measurement, for either container: charge current,
heat-set insert OD, USB-C panel flange dimensions, and whether the camera
module is an OV2640 or an OV3660.

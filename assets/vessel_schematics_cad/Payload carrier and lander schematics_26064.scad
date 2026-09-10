// ---------------------------------------------------------
// Tethered Deep-Sea Drop-Frame - Complete OpenSCAD Model
// (Battery operated, internal data logging, winch-deployed)
// ---------------------------------------------------------

$fn = 64; // High resolution rendering

// --- GLOBALS & DIMENSIONS ---
w = 40;            // Square tubing width (mm)
L = 1000;          // Frame base width (mm)
H = 600;           // Main frame height (Lowered for tether stability)
h_leg = 250;       // Mud legs height (mm)
tL = 800;          // Top platform width (mm)

// --- GEOMETRIC POINTS ---
// Mud feet locations (Z=0)
l00 = [-L/2, -L/2, 0]; l10 = [ L/2, -L/2, 0]; l11 = [ L/2, L/2, 0]; l01 = [-L/2, L/2, 0];

// Main chassis base (Z=h_leg)
b00 = [-L/2, -L/2, h_leg]; b10 = [ L/2, -L/2, h_leg]; 
b11 = [ L/2,  L/2, h_leg]; b01 = [-L/2,  L/2, h_leg];

// Main chassis top platform
t00 = [-tL/2, -tL/2, h_leg+H]; t10 = [ tL/2, -tL/2, h_leg+H];
t11 = [ tL/2,  tL/2, h_leg+H]; t01 = [-tL/2,  tL/2, h_leg+H];

// Apex Lifting Point
apex = [0, 0, h_leg+H+250];

// --- UTILITY MODULES ---
module strut(p1, p2, width) {
    color("silver")
    hull() {
        translate(p1) cube([width, width, width], center=true);
        translate(p2) cube([width, width, width], center=true);
    }
}

module cable(p1, p2, p3) {
    color("darkorange") {
        hull() { translate(p1) sphere(d=12); translate(p2) sphere(d=12); }
        hull() { translate(p2) sphere(d=12); translate(p3) sphere(d=12); }
    }
}

// --- MAIN COMPONENTS ---

// 1. Reinforced Chassis
module heavy_frame() {
    difference() {
        union() {
            // Legs to feet
            strut(l00, b00, w); strut(l10, b10, w); strut(l11, b11, w); strut(l01, b01, w);
            // Lower perimeter
            strut(b00, b10, w); strut(b10, b11, w); strut(b11, b01, w); strut(b01, b00, w);
            // Upper perimeter
            strut(t00, t10, w); strut(t10, t11, w); strut(t11, t01, w); strut(t01, t00, w);
            // Vertical pillars
            strut(b00, t00, w); strut(b10, t10, w); strut(b11, t11, w); strut(b01, t01, w);
            // Diagonal bracing (tension members)
            strut(b00, t10, w/1.5); strut(b10, t11, w/1.5); 
            strut(b11, t01, w/1.5); strut(b01, t00, w/1.5);
            // Internal payload mounting rails
            strut([-L/2, 200, h_leg], [L/2, 200, h_leg], w);
            strut([-L/2, 0, h_leg], [L/2, 0, h_leg], w);
            // Triangulated A-Frame Lifting Bridle
            strut(t00, apex, w*1.2); strut(t10, apex, w*1.2);
            strut(t11, apex, w*1.2); strut(t01, apex, w*1.2);
        }
        // Weep holes for free-flooding
        for (p = [b00, b10, b11, b01, t00, t10, t11, t01]) {
            translate(p + [0,0,30]) rotate([0,90,0]) cylinder(h=w*3, d=12, center=true);
        }
    }
}

// 2. Mud Feet (Geometric anti-suction)
module mud_feet() {
    color("silver")
    for (pos = [l00, l10, l11, l01]) {
        translate(pos) {
            difference() {
                cylinder(h=15, d=250, center=false);
                for (a = [0:45:315]) {
                    rotate([0, 0, a]) translate([70, 0, -5]) cylinder(h=30, d=60);
                }
            }
        }
    }
}

// 3. Static Ballast (Bolted to bottom rails to keep tether taut)
module static_ballast() {
    color("darkslategray")
    for (y = [-L/2+60, L/2-60]) {
        translate([0, y, h_leg + 25])
        cube([800, 80, 50], center=true);
    }
}

// 4. Heavy-Duty Lifting Shackle
module lifting_shackle() {
    translate(apex + [0, 0, 30])
    rotate([90, 0, 0]) {
        color("dimgray") difference() {
            cylinder(h=30, d=100, center=true);
            cylinder(h=35, d=60, center=true);
            translate([0, -50, 0]) cube([110, 50, 40], center=true);
        }
        // Shackle Pin
        color("silver") translate([0, -20, 0]) cylinder(h=50, d=20, center=true);
    }
}

// 5. Electronics Housing (Battery, SD Logging, MCU, IMU)
module electronics_cylinder() {
    c_len = 500; c_od = 160; c_id = 120;
    
    translate([0, 100, h_leg + 100]) 
    rotate([0, 90, 0]) {
        // Main cylinder with cutaway
        color("dimgray") difference() {
            cylinder(h=c_len, d=c_od, center=true);
            cylinder(h=c_len + 10, d=c_id, center=true);
            translate([0, c_od/2, 0]) cube([c_od, c_od, c_len+20], center=true); // Cutaway
        }
        
        // End-caps with O-rings
        color("silver")
        for (z = [-c_len/2 + 25, c_len/2 - 25]) {
            translate([0, 0, z]) {
                difference() {
                    union() {
                        cylinder(h=50, d=c_id - 1, center=true);
                        translate([0, 0, (z > 0 ? 15 : -15)]) cylinder(h=20, d=c_od, center=true);
                    }
                    translate([0, c_od/2, 0]) cube([c_od, c_od, 100], center=true);
                }
            }
        }
        
        // Internal Sled (Dry payload)
        color("dodgerblue") translate([0, -15, 0]) cube([80, 5, c_len - 120], center=true);
        
        // Component Layout
        color("limegreen") translate([-15, 10, -120]) cube([40, 20, 100], center=true); // Battery
        color("purple") translate([15, 5, -20]) cube([30, 8, 60], center=true);     // MCU
        color("red") translate([15, 5, 40]) cube([20, 4, 30], center=true);         // MicroSD
        color("gold") translate([-15, 5, 60]) cube([15, 3, 15], center=true);       // IMU
    }
}

// 6. External Sensors & Cables
module sensors_and_cables() {
    // Pulse Induction (PI) Coil
    color("black") translate([-150, -150, h_leg - 50]) cylinder(h=15, d=250, center=true);
    
    // PT100 Probe
    color("silver") translate([L/2 + 20, -100, h_leg]) rotate([0, 90, 0]) cylinder(h=150, d=8, center=true);
    
    // Downward Camera & LEDs
    color("lightblue", 0.4) translate([150, -150, h_leg - 20]) sphere(d=120);
    color("black") translate([150, -150, h_leg - 20]) cylinder(h=60, d=40, center=true);
    color("white") for (dx = [50, 250]) translate([dx, -150, h_leg - 20]) cylinder(h=80, d=30, center=true);
    
    // Bulkhead Penetrators & Wiring
    color("gold") translate([275, 100, h_leg + 100]) rotate([0, 90, 0])
    for (a = [0, 120, 240]) {
        rotate([0, 0, a]) translate([30, 0, 0]) cylinder(h=20, d=15, center=true);
    }
    
    cable([-150, -150, h_leg - 40], [-100, 100, h_leg + 50], [280, 110, h_leg + 120]);
    cable([150, -150, h_leg + 40], [150, 50, h_leg + 50], [280, 90, h_leg + 80]);
    cable([L/2, -100, h_leg + 10], [L/2 - 100, 0, h_leg + 30], [280, 90, h_leg + 110]);
}

// --- ASSEMBLE MODEL ---
heavy_frame();
mud_feet();
static_ballast();
lifting_shackle();
electronics_cylinder();
sensors_and_cables();
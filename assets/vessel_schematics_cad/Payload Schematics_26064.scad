// ---------------------------------------------------------
// Deep-Sea Benthic Lander - Pressure Housing & Sled
// ---------------------------------------------------------

$fn = 100; // High resolution for smooth O-rings and cylinders

// --- DIMENSIONS ---
c_len = 450;       // Total length of the pressure tube (mm)
c_od = 160;        // Outer Diameter (mm) - 20mm thick walls (7075-T6)
c_id = 120;        // Inner Diameter (mm)

// Sled Geometry (Derived graphically via chord length)
// Dropping the sled 25mm below the center axis creates a chord width of ~114mm.
sled_z_offset = -25; 
sled_w = 110;      // Slightly undersized for clearance
sled_len = 380;    // Length of the acrylic mounting board
sled_thick = 5;    // Thickness of the sled board

// --- MODULES ---

// 1. The Main 7075-T6 Aluminum Tube (With Cutaway)
module pressure_tube() {
    color("dimgray")
    difference() {
        // Outer tube
        rotate([0, 90, 0]) cylinder(h=c_len, d=c_od, center=true);
        // Inner bore
        rotate([0, 90, 0]) cylinder(h=c_len + 10, d=c_id, center=true);
        
        // 120-degree cutaway to view the internal sled
        translate([0, c_od/2 + 10, c_od/2 + 10])
        cube([c_len + 20, c_od, c_od], center=true);
    }
}

// 2. Piston End-Caps with Radial Seals
module end_caps() {
    for (x = [-c_len/2 + 25, c_len/2 - 25]) {
        translate([x, 0, 0]) {
            rotate([0, 90, 0]) {
                difference() {
                    union() {
                        // The inner plug that slides into the bore
                        cylinder(h=40, d=c_id - 0.5, center=true);
                        // The outer retaining flange
                        translate([0, 0, (x > 0 ? 15 : -15)]) 
                        cylinder(h=15, d=c_od, center=true);
                    }
                    // Cutaway slice to match the tube
                    translate([c_od/2 + 10, c_od/2 + 10, 0])
                    cube([c_od, c_od, 100], center=true);
                }
                
                // Viton O-Rings (Black)
                color("black")
                for (g = [-8, 8]) {
                    translate([0, 0, g])
                    rotate_extrude()
                    translate([(c_id-4)/2, 0, 0]) circle(d=4.5);
                }
            }
        }
    }
    
    // Front End-Cap Bulkhead Penetrators (Gold)
    // Placed only on the front cap (x > 0)
    color("gold")
    translate([c_len/2 - 5, 0, 0])
    rotate([0, 90, 0]) {
        for (a = [0, 120, 240]) {
            rotate([0, 0, a])
            translate([35, 0, 0])
            cylinder(h=30, d=12, center=true);
        }
    }
}

// 3. The Electronics Sled and Runner Rails
module electronics_sled() {
    translate([0, 0, sled_z_offset]) {
        // Main Delrin/Acrylic Tray
        color("ghostwhite")
        cube([sled_len, sled_w, sled_thick], center=true);
        
        // Side Runner Rails (Smooth standoffs to prevent scratching the bore)
        color("silver")
        for (y = [-sled_w/2, sled_w/2]) {
            translate([0, y, sled_thick/2 + 3])
            rotate([0, 90, 0])
            cylinder(h=sled_len, d=6, center=true);
        }
    }
}

// 4. Component Layout on the Sled
module payload_components() {
    // Base Z height for components sitting on the sled
    z_base = sled_z_offset + (sled_thick / 2);
    
    // REAR: Heavy Li-Ion Battery Pack (Isolated mass)
    color("limegreen")
    translate([-120, 0, z_base + 15])
    cube([100, 50, 30], center=true);
    
    // CENTER-REAR: Microcontroller Unit (MCU)
    color("purple")
    translate([-20, 0, z_base + 6])
    cube([60, 40, 12], center=true);
    
    // CENTER: MPU9250 IMU (Dead center, mounted perfectly flat)
    color("gold")
    translate([30, 0, z_base + 2])
    cube([15, 15, 4], center=true);
    
    // CENTER-FRONT: MicroSD Card Breakout (Adjacent to MCU for short SPI clock lines)
    color("crimson")
    translate([20, 30, z_base + 3])
    cube([25, 20, 6], center=true);
    
    // FRONT: MAX31865 RTD Amplifier (Closest to the front penetrators)
    color("royalblue")
    translate([100, 0, z_base + 4])
    cube([35, 25, 8], center=true);
    
    // Wiring Harness Representation (Orange SPI and Power lines)
    color("darkorange")
    translate([25, 15, z_base + 2])
    rotate([0, 90, 0]) cylinder(h=80, d=2, center=true); // SPI Bus
    
    color("darkorange")
    translate([-60, 10, z_base + 6])
    rotate([0, 90, 0]) cylinder(h=60, d=3, center=true); // Main Power
}

// --- ASSEMBLE MODEL ---
pressure_tube();
end_caps();
electronics_sled();
payload_components();
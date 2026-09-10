# Sensor Fusion & Directional Drift Estimation

This document describes the mathematical formulation of the attitude estimation, tilt-compensated heading, and current-aligned drift sector fusion algorithm.

> [!IMPORTANT]
> **CRITICAL CONCEPTUAL CLARIFICATION:**
> The fusion algorithm implements a **statistical directional uncertainty sector model**, NOT a dead-reckoning positioning system. Due to sensor noise and double integration error, MEMS accelerometers cannot estimate long-duration underwater position. The drift direction is constrained primarily by hydrodynamic fin alignment measured by the magnetometer.

---

## 1. Frame Conventions

```
              BODY FRAME CONVENTION (Cylindrical Vessel)
              
                      +X (Forward / Fin Nose)
                        ^
                        |
            +Y (Port) <--+--> -Y (Starboard)
                        |
                        v
                      +Z (Down / Bottom)
```

- **Body Frame ($B$):**
  - $+X$: Forward facing along vessel longitudinal axis.
  - $+Y$: Port (Left).
  - $+Z$: Downward along cylinder axis.
- **Global Geographic Frame ($G$):**
  - $X_G$: Magnetic North ($0^\circ$).
  - $Y_G$: Magnetic East ($90^\circ$).
  - $Z_G$: Downward toward gravity vector.

---

## 2. Mathematical Formulation

### A. IMU Attitude Complementary Filter
Pitch ($\theta$) and Roll ($\phi$) are updated by combining gyroscope integration with accelerometer gravity vector sensing:

$$\phi_{\text{accel}} = \text{atan2}(a_y, a_z)$$
$$\theta_{\text{accel}} = \text{atan2}(-a_x, \sqrt{a_y^2 + a_z^2})$$

$$\phi(k) = \alpha \cdot (\phi(k-1) + \omega_x \Delta t) + (1 - \alpha) \cdot \phi_{\text{accel}}$$
$$\theta(k) = \alpha \cdot (\theta(k-1) + \omega_y \Delta t) + (1 - \alpha) \cdot \theta_{\text{accel}}$$

where $\alpha = 0.98$ (configurable).

### B. Tilt-Compensated Magnetic Heading
Raw magnetometer readings $(B_x, B_y, B_z)$ are calibrated using hard-iron offsets $(V_x, V_y, V_z)$ and soft-iron scale multipliers $(S_x, S_y, S_z)$, then projected into the horizontal plane:

$$B_{x,c} = (B_x - V_x) \cdot S_x, \quad B_{y,c} = (B_y - V_y) \cdot S_y, \quad B_{z,c} = (B_z - V_z) \cdot S_z$$

$$X_h = B_{x,c} \cos\theta + B_{y,c} \sin\phi \sin\theta + B_{z,c} \cos\phi \sin\theta$$
$$Y_h = B_{y,c} \cos\phi - B_{z,c} \sin\phi$$

$$\text{Heading} (\psi) = \text{atan2}(-Y_h, X_h) \times \frac{180}{\pi} \pmod{360^\circ}$$

### C. Fin-Aligned Drift Sector Calculation
1. Mechanical current-alignment fins align the vessel $+X$ axis with the ocean current vector $\vec{V}_{\text{current}}$.
2. The magnetometer directly measures this aligned orientation $\psi_{\text{fin}}$.
3. Short-term IMU motion direction $\psi_{\text{imu}}$ is extracted from horizontal acceleration components:

$$a_{N} = a_x \cos\theta \cos\psi - a_y \sin\psi + a_z \sin\theta \cos\psi$$
$$a_{E} = a_x \cos\theta \sin\psi + a_y \cos\psi + a_z \sin\theta \sin\psi$$

$$\psi_{\text{imu}} = \text{atan2}(a_E, a_N) \times \frac{180}{\pi}$$

4. The estimated drift bearing $\theta_{\text{drift}}$ is computed via weighted circular angular fusion:

$$\theta_{\text{drift}} = \text{atan2}\left( w_{\text{mag}} \sin\psi_{\text{fin}} + w_{\text{imu}} \sin\psi_{\text{imu}}, w_{\text{mag}} \cos\psi_{\text{fin}} + w_{\text{imu}} \cos\psi_{\text{imu}} \right)$$

where default weights are $w_{\text{mag}} = 0.80, w_{\text{imu}} = 0.20$.
The uncertainty sector half-angle $\Delta\theta$ defaults to $30.0^\circ$, producing a $60.0^\circ$ directional search cone.

# Pulse Induction (PI) Tuning & Waveform Guide

This document details the operation, parameter tuning, baseline tracking, and anomaly detection algorithms of the parameterized Pulse Induction (PI) metal detector module.

---

## 1. Timing Sequence & Waveform

```
                      PI PULSE TIMING SEQUENCE
                      
MOSFET GATE (GPIO33):
      +-----------------+
      |                 |
------+                 +----------------------------------------
      <-- pulse_width -->
      (150 us default)

COIL DECAY RESPONSE (GPIO32):
----+\                                 
      \  (Switching Transient)         
       \=======+                       
               \                       
                +--. (Sample Window: 16 microsecond samples)
                    `----------------. 
      <------->                        
      settling_delay                   
      (30 us default)                  
      
      <------------------------------->
              pulse_period (1000 us default)
```

---

## 2. Parameter Matrix

| Parameter Macro | Provisional Default | Unit | Description | Bench Calibration Notes |
| :--- | :--- | :--- | :--- | :--- |
| `PI_PULSE_WIDTH_US` | `150` | $\mu\text{s}$ | Duration MOSFET gate is held HIGH to energize coil. | Increase for deeper detection; check MOSFET thermal dissipation. |
| `PI_SETTLING_DELAY_US` | `30` | $\mu\text{s}$ | Wait time after MOSFET turn-OFF to allow back-EMF voltage spike to clamp below 3.3V. | **CRITICAL:** Do NOT sample before 30us to prevent damaging ESP32 ADC pin! |
| `PI_PULSE_PERIOD_US` | `1000` | $\mu\text{s}$ | Repetition period between consecutive pulse bursts (1 kHz rep rate). | Ensures coil magnetic flux collapses completely. |
| `PI_SAMPLE_COUNT` | `16` | Samples | Number of microsecond-spaced ADC decay curve samples per pulse. | Captures decay curve signature. |
| `PI_AVERAGING_COUNT` | `20` | Pulses | Number of consecutive pulse waveforms averaged per measurement frame. | Reduces background electrical noise and ADC jitter. |
| `PI_BASELINE_ALPHA` | `0.05` | Float | Exponential Moving Average (EMA) baseline tracking rate ($0 < \alpha \le 1$). | Slow adaptation rate to track background mineralized seabed baseline. |
| `PI_DEFAULT_THRESHOLD`| `50.0` | ADC Counts | Signal threshold above baseline required to trigger `pi_detected = true`. | Adjust based on benchmark target (ferrous/non-ferrous anomaly spikes). |

> [!WARNING]
> **PROVISIONAL STATUS:** All pulse timings and thresholds are provisional engineering defaults. They MUST be calibrated on an oscilloscope with the physical PI coil and damping resistor.

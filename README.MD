# Sensorless BLDC Motor Controller (STM32G431RBT6)

![Motor Demonstration](./Results/step_response.mp4)

## 📌 Overview
This repository contains the complete hardware and firmware design for a **Sensorless 6-Step Trapezoidal BLDC Motor Controller** built around the STM32G4 microcontroller. The project demonstrates advanced embedded motor control techniques, including Back-EMF (BEMF) Zero-Crossing Detection (ZCD) via Injected ADCs, hardware-synchronized timers, and a custom-tuned PI speed controller.

The hardware was designed from scratch (KiCad) and tested rigorously to evaluate both physical limits (Voltage Saturation/Kv limits) and embedded software constraints (Timer Quantization limits).

---

## ⚙️ Firmware Architecture & Control Theory

The firmware avoids blocking delays and polling. It relies entirely on a deterministic, interrupt-driven architecture utilizing STM32 Advanced Timers and ADC peripherals.

### 1. Robust State-Machine
The motor control flow is managed by a strict state machine to ensure safe start-up and running conditions:
* **IDLE:** PWM outputs are disabled (Soft Stop / Coasting).
* **ALIGN:** A static DC magnetic field is applied to the stator to force the mechanical rotor into a known starting position.
* **RAMP-UP:** Open-loop commutation with progressively decreasing delay to generate enough initial BEMF for the ADCs to read.
* **CLOSED-LOOP:** Sensorless commutation takes over. The system relies purely on ZCD events and the PI controller manages the PWM duty cycle to reach the target RPM.
* **FAULT:** Emergency hard-stop triggered by stall detection or hardware limits.

### 2. Injected ADC & Timer Synchronization
To avoid high-frequency PWM switching noise (ringing), BEMF is **not** polled continuously in the main loop. Instead, the STM32's **Injected ADC** channels are hardware-triggered by `TIM1`'s Update/Compare events. 
This ensures that the BEMF voltage is sampled exactly at the optimal moment (typically the PWM ON state or center-aligned peak), providing a clean signal for the ZCD algorithm without requiring heavy software filtering.

### 3. PI Speed Control & Super-Sampling (MAF)
Speed calculation at high RPMs suffers from timer resolution limits (Quantization Error). To mitigate this without using floating-point math on every cycle, a **Moving Average Filter (MAF)** is implemented.
The PI controller runs deterministically in a dedicated 1kHz timer interrupt (`TIM7`). It compares the filtered RPM against the target and smoothly adjusts the duty cycle, featuring anti-windup clamping to protect the system.

---

## 🔬 Key Hardware Verifications

Theoretical models were strictly validated against physical hardware behavior using a 4-channel oscilloscope and RAM data logging. *(For full oscilloscope captures, including Dead-Time verification, please see the [Results/](./Results) folder).*

### 1. Hardware RC Filter Phase Shift & ZCD Smoothing
![ZCD Proof](./Results/Figure1.png)
> **Analysis:** The Yellow trace shows the raw Phase 1 voltage. The Blue trace shows the signal at the STM32 ADC pin after the resistor divider and RC filter. The filter successfully eliminates PWM noise. Crucially, the inherent RC time delay acts as a natural "Phase Advance," optimizing high-speed commutation timing directly in hardware, eliminating the need for 30-degree software compensation.

### 2. System Limit Discovery (RAM Datalogging)
![Step Response](./Results/step_response.png)
> **Analysis (Step Response & Limitations):** Extracted via an 8KB internal RAM "Flight Recorder" to avoid UART bottlenecking. The graph reveals two physical realities:
> * **Voltage Saturation:** The PI controller pushes the Duty Cycle (Green) to its ~95% limit to reach 14200 RPM, but the motor plateaus at ~9500 RPM (Orange). The motor has hit its BEMF ceiling for a 12V supply ($\Delta V \approx 0$).
> * **Quantization Artifacts:** The step-like jumps at peak speeds are not physical. They are artifacts of the 50us timer resolution; at extreme speeds, a 1-tick variation in commutation measurement results in massive calculated RPM swings, proving the necessity of the MAF implemented in the code.

---

## 🔌 Quick Start & Hardware Setup

### Custom Hardware Notes (v1.0 Prototyping)
* **MP1482 Bypass:** Due to a layout errata on the v1.0 PCB, the onboard 12V-to-3.3V buck converter is bypassed. Logic power is supplied externally. (See `Hardware/ERRATA.md` for details).
* **Future-Proofing (DNP):** The board includes footprints for inline current shunts, op-amps, and encoder headers. These are currently marked as DNP (Do Not Populate) as they are reserved for the upcoming FOC (Field Oriented Control) revision.

## 📁 Repository Structure

📦 STM32-Sensorless-BLDC-Control
 ┣ 📂 Firmware         # STM32CubeIDE Project (C source, headers, .ioc)
 ┣ 📂 Hardware         # KiCad project, Schematics (PDF), BOM, and Errata
 ┣ 📂 Results          # Oscilloscope captures and Step Response graphs
 ┣ 📜 .gitignore       # Excludes IDE debug/build binaries
 ┣ 📜 README.md        # You are here
 ┗ 📜 LICENSE          # MIT License
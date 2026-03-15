# Hardware Errata and Implementation Notes (v1.0 Prototype)

This document outlines the known hardware issues, temporary workarounds, and unpopulated components for the v1.0 revision of the Sensorless BLDC Motor Controller board. 

As this is an initial lab-tested R&D prototype, some sections have been modified post-production to ensure stable operation of the core motor control algorithms.

## 1. MP1482 Buck Converter Circuit (Bypassed)
* **Description:** A layout/design error was identified in the onboard MP1482 step-down converter section during the initial board bring-up.
* **Workaround:** The onboard voltage regulation section is currently bypassed to prevent power instability. System logic voltage (3.3V for MCU) is temporarily provided via external laboratory power supplies using direct flying leads (bodge wires) soldered to the respective power planes.
* **Impact:** This issue only affects the standalone power capability of the board. It does **not** affect the MCU performance, gate driver switching logic, or the BEMF sensing circuitry. The sensorless 6-step commutation algorithms function perfectly under this bypassed configuration.
* **Resolution:** The buck converter layout will be completely revised in the v1.1 hardware update.

## 2. DNP (Do Not Populate) Components - Future FOC Prep
* **Description:** The PCB was strategically designed to be forward-compatible with advanced motor control techniques. It includes footprints for phase current sensing (inline/low-side shunt resistors and operational amplifiers) as well as header pins for quadrature encoders.
* **Current Status:** Since this specific repository focuses entirely on **Sensorless 6-Step Trapezoidal Control** utilizing Back-EMF Zero-Crossing Detection (ZCD), these specific components are intentionally left unpopulated (DNP) to reduce noise and BOM cost.
* **Future Use:** These analog measurement sections are reserved and will be populated for the upcoming Field Oriented Control (FOC) phase of the project.

## 3. General Prototyping Notes
* Visible flux residues and external wire modifications on the physical board are normal artifacts of the debugging and tuning process for this v1.0 iteration. The primary focus of this hardware version is to validate the STM32G4's ADC Injected Conversion, Timer synchronizations, and hardware limits.
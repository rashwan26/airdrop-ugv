# Hardware and wiring

## Pin allocation

The source pin allocation is preserved for a classic ESP32 WROOM board. GPIO16 and GPIO17 may be unavailable on modules that use them for PSRAM, including some WROVER boards. Do not apply this table to an ESP32-S3, C3, or another variant without remapping.

| Signal | ESP32 GPIO | Direction | Connection |
|---|---:|---|---|
| HC-SR04 TRIG | 23 | Output | Sensor TRIG; verify 3.3 V trigger compatibility |
| HC-SR04 ECHO | 34 | Input only | Through a suitable 5 V-to-3.3 V level shifter or divider |
| GPS TX | 35 | Input only | GPS TX to ESP32 UART1 RX; verify 3.3 V logic |
| GPS RX | 4 | Output | ESP32 UART1 TX to GPS RX; optional for receive-only use |
| Release servo | 13 | PWM output | Servo signal; 50 Hz |
| Left front M1A / M1B | 16 / 17 | PWM outputs | First MDD3A logic input pair |
| Left rear M2A / M2B | 18 / 19 | PWM outputs | First MDD3A logic input pair |
| Right front M1A / M1B | 25 / 26 | PWM outputs | Second MDD3A logic input pair |
| Right rear M2A / M2B | 27 / 32 | PWM outputs | Second MDD3A logic input pair |

Distinguish the small MDD3A logic input headers from the similarly named motor output screw terminals. GPIO pins connect to logic inputs only. Configure GPS NMEA output at 9600 baud, including GGA and RMC sentences. The firmware accepts standard GP/GN GGA quality fields; other talker IDs require an adapter update.

## Electrical interfaces

For a conventional 5 V HC-SR04, use for example a 10 kOhm resistor from ECHO to GPIO34 and a 15 kOhm resistor from GPIO34 to ground. This yields approximately 3.0 V from a 5 V echo. Verify the actual waveform and component tolerances. Do not connect a 5 V echo directly to the ESP32. GPIO34 and GPIO35 do not provide normal internal output drive or pull resistors.

Add external 10 kOhm pulldowns on all eight MDD3A input signals to hold them low while the ESP32 boots or resets. Software starts with both inputs low, then attaches PWM. The stop command sets both inputs low; characterize the driver's resulting stop behavior and the vehicle's stopping distance on the actual assembly.

MDD3A is a dual-input PWM driver. Forward in the source document is A low / B active. This firmware uses A=0 and B=driveDuty, or swaps the active input when the motor's `reversed` flag is true. Verify the polarity of each motor with wheels raised. Board placement and motor mounting can require different reversal flags.

## Breadboard prototype and power distribution

The original assembly used a breadboard. Use it for low-current signal prototyping only; route motor power directly through suitably rated wires, connectors, and a physical disconnect. Supply the servo from a regulator sized for its stall current. Do not power the servo or DC motors from an ESP32 GPIO or the board's 3.3 V rail. Do not assume the MDD3A auxiliary 5 V output can supply a servo.

Tie ESP32, GPS, ultrasonic sensor, servo-supply and driver logic grounds together. Keep noisy motor-current return paths separate from sensor wiring until a suitable common grounding point. Verify battery voltage, regulator capacity, motor stall current, driver ratings, fusing, decoupling, and USB power interaction against the actual part specifications. These component details were absent from the supplied record.

## Mechanical calibration

The documented 90-degree closed / 180-degree open servo positions are nominal. The adapter uses a 1000-2000 microsecond command range at 50 Hz. Calibrate with the latch unloaded; do not force the servo into a mechanical stop. There is no latch-position feedback. A commanded release does not prove separation.

Mount the sensor facing the ground. Measure actual clearance and revise the 2-10 cm landing band accordingly. A nearby object can satisfy this band while the assembly is airborne; a single range sensor cannot establish that a load-bearing landing has occurred. The code does not control the drone and cannot verify flight clearance or tether separation.

Servo uses LEDC high-speed channel 0 / timer 0 through ESP32Servo 1.1.1. Motor outputs use channels 4-11 at 1 kHz: high-speed timers 2/3 and low-speed timers 0/1. The high-speed and low-speed timer groups are separate hardware resources in the pinned classic ESP32 Arduino implementation. Any added PWM outputs or library/core upgrades need a timer-allocation review.

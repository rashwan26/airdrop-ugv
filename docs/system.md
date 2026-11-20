# Engineering description (English)

## 1. Mission objective

The earlier project describes an ESP32-controlled unmanned ground vehicle carried by a drone. The vehicle detects departure from the ground, monitors its return, commands a release mechanism after a stable landing indication, drives forward, and stops after nominally 20 m of GPS displacement. The drone is externally controlled. The ground controller has no target coordinate, aerial navigation function, or obstacle-avoidance capability.

## 2. Inputs and outputs

An HC-SR04 estimates the distance to the surface beneath the vehicle from echo pulse duration. A NEO-6M/8M-class GPS supplies NMEA location and quality information over UART at 9600 baud. A 50 Hz servo commands the latch, and two Cytron MDD3A drivers control four motors. The full source pin map is preserved in [hardware.md](hardware.md).

The range adapter samples about every 70 ms and waits at most 25 ms for an echo. A missing echo or a reading outside 2-400 cm is invalid. Each sample is timestamped; samples older than 250 ms cannot support ground or takeoff decisions. State timers are elapsed-time checks rather than multi-second blocking delays. This adapter is bounded-blocking, not fully asynchronous.

## 3. Mission states

| State | Entry / exit condition | Motor command | Servo command |
|---|---|---|---|
| DISARMED | Boot; `ARM` requires fresh 2-10 cm reading | Off | Closed |
| PRE_TAKEOFF | Valid height above 30 cm for 3 s | Off | Closed |
| IN_AIR | Wait for fresh 2-10 cm reading | Off | Closed |
| LANDED_CHECKING | 10 s in landing band; invalid/out-of-band reading resets to IN_AIR | Off | Closed |
| RELEASE_CAR | Command open; wait 3 s while ground remains valid | Off | Open |
| WAIT_GPS | Gather 5 distinct quality fixes, at least 800 ms apart; 60 s timeout | Off | Open |
| DRIVING_20M | Drive until target confirmation, GPS fault or 120 s timeout | Forward PWM | Open |
| MISSION_DONE | Terminal state after 3 fixes at or beyond 20 m | Off | Open |
| FAULT | Terminal operator stop or detected fault | Off | Preserve release status |

## 4. GPS policy

A usable fix must have finite latitude/longitude in bounds, HDOP greater than zero and at most 2.5, at least six satellites, fresh position data, fresh HDOP/satellite data, and a fresh nonzero GGA fix-quality field. Freshness is limited to 1500 ms. A receiver that reports a no-fix GGA sentence causes invalidation even if TinyGPSPlus retains an older valid coordinate.

After release, the controller averages five distinct, time-spaced fixes while stopped. Poor quality resets the origin accumulator. These samples are an averaging heuristic; the firmware has no independent stationary detector and does not reject a slowly drifting origin. Longitude averaging assumes a local mission away from the antimeridian.

During driving, each distinct fix is compared with the previous position. A step exceeding `5 m + 3 m/s * elapsed seconds` faults the mission. These are uncalibrated plausibility limits. They can reject real movement or admit GPS drift and need field characterization. Position displacement is computed using a Haversine formula with a 6,371,000 m Earth radius.

Loss of fix quality, stale data, an implausible jump, or 120 s elapsed drive time stops and latches a fault. Recovery is manual. The firmware does not resume movement when GPS returns. Latitude/longitude are not printed in routine telemetry.

## 5. Changes from the surviving document

| Source record | Reconstruction |
|---|---|
| Called its declarations a complete program | Supplies state-machine implementation, sensor adapter, setup and main loop |
| Counted zero/no echo as possible takeoff evidence | Requires valid above-threshold range; no echo is unknown |
| Started autonomous sequencing on power-up | Adds explicit ground-only ARM command |
| Described full-power digital forward command | Adds adjustable PWM duty and per-motor polarity |
| Mentioned only HDOP for GPS protection | Adds fix validity, quality-field freshness, satellites, distinct updates and jump checks |
| Did not define signal-loss recovery or duration limit | Latches faults and limits acquisition/drive duration |
| Claimed 100% safe operation | Documents single-sensor ambiguity and absence of hardware validation |
| Described forward 20 m movement as accurate | Defines approximate GPS displacement and missing heading feedback |

## 6. Limits and next engineering work

No available evidence establishes successful flight, release reliability, straight-line travel, obstacle detection, or stop accuracy. Complete software tests do not establish these properties. The controller cannot detect motor stall, servo jam, low battery, an obstructed route, or continued drone/tether attachment. A watchdog resets a stuck task after roughly 3 seconds; reset forces a disarmed restart but also commands the latch closed. Hardware must remain mechanically secure through a reset.

Next hardware revisions should consider independent ground contact/load sensing, latch feedback, wheel encoders, heading estimation, battery monitoring and a physical emergency stop. These are future requirements, not features implemented in this repository.

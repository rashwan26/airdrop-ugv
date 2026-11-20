# Validation and commissioning

## Software checks

The portable host suite checks 17 mission scenarios plus geodesy calculations:

1. Boot inhibits driving; invalid/stale ground readings reject arming.
2. Missing echo cannot confirm takeoff.
3. Interrupted takeoff evidence resets confirmation.
4. Interrupted landing indication resets its timer.
5. Missing GPS never starts motors and eventually times out.
6. Reusing a single GPS update cannot complete origin acquisition.
7. Stale GPS during driving latches a fault.
8. Poor HDOP stops driving and signal recovery cannot restart it.
9. An excessive GPS jump faults the mission.
10. Target requires distinct confirming fixes and completion is terminal.
11. Drive duration is bounded.
12. Operator abort stops movement and preserves release status.
13. Timer arithmetic crosses the 32-bit millisecond rollover.
14. Ground loss after release faults the mission.
15. Nonfinite coordinates cannot command driving.
16. Stale origin data resets the sample accumulator.
17. A gap in range sampling cannot count toward continuous takeoff confirmation.

The host suite tests the actual portable state machine, not a separate model. It does not emulate the UART parser, servo mechanics, LEDC waveform, watchdog, or motor driver. The ESP32 target compilation covers adapter API compatibility, not physical behavior. GitHub Actions runs both checks on pushes and pull requests.

## Hardware acceptance procedure (not yet executed)

| Stage | Procedure | Evidence to record |
|---|---|---|
| Electrical inspection | Verify actual board variant, ECHO voltage, regulated rails, grounds, pulldowns and current ratings | Wiring diagram, measured voltages and part numbers |
| Unloaded bench | Disconnect motor power; boot and confirm DISARMED; exercise ARM and STOP | Serial log; no unintended motor commands |
| Output characterization | Wheels raised and latch unloaded; verify all four motor polarities, PWM frequency/duty and servo travel | Waveforms, direction map, servo pulse limits |
| Range tests | Move a flat target through ground, takeoff and invalid ranges, including interruptions | Transition timings and invalid-echo behavior |
| Parser / signal loss | Feed recorded NMEA, no-fix GGA, missing GGA, stale HDOP, poor satellites and position jumps | Motors off on fault; no automatic restart |
| GPS stationary test | Log drift in the intended environment while the vehicle remains still | Fix quality, origin drift, false finish behavior |
| Ground-only run | Use a clear controlled area, low duty and accessible physical disconnect | Measured displacement, route, speed and stopping distance |
| Fault injection | Remove GPS during driving; stall software in a controlled bench setup; restart at each stage | Output inhibition, watchdog response, latch behavior |
| Integrated test | Only after electrical, ground and mechanical acceptance, evaluate the full carrier sequence | Independent clearance/landing evidence, latch separation and measured outcome |

Do not infer real stopping distance from the nominal 20 m threshold. At a 1 Hz GPS rate, several seconds can pass between crossing the threshold and satisfying three updates. Measure overshoot and tune the completion policy only with known speed and braking behavior.

## Evidence boundaries

The source PDF has four pages; page four ends after global declarations. The project creator confirmed a breadboard assembly and stated that the original code was written using Arduino IDE. No original source files, schematics, motor/servo specifications, photographs or field logs were available. The Arabic PDF itself is not republished here; its engineering content is reconstructed in English with uncertainties identified.

Build output and host-test results for this reconstruction are recorded in `build-results.txt` after local verification. A successful build is not a flight or hardware certification.

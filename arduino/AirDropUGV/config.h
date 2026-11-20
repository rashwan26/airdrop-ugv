#pragma once
#include <stdint.h>

namespace cfg {
constexpr int trig = 23, echo = 34, gpsRx = 35, gpsTx = 4, servo = 13;
struct MotorPins { int a, b; bool reversed; };
// A=LOW/B=PWM follows the source document; calibrate each motor on a stand.
constexpr MotorPins motors[] = {{16,17,false}, {18,19,false},
                                {25,26,false}, {27,32,false}};
constexpr int lockedAngle = 90, releasedAngle = 180;
constexpr uint8_t driveDuty = 90;  // 0..255; bench starting point, not calibrated
constexpr uint32_t rangeIntervalMs = 70, echoTimeoutUs = 25000;
constexpr uint32_t rangeFreshMs = 250, gpsFreshMs = 1500;
constexpr uint32_t takeoffMs = 3000, landingMs = 10000, releaseMs = 3000;
constexpr uint32_t gpsWaitMs = 60000, driveTimeoutMs = 120000;
constexpr uint32_t originSpacingMs = 800;
constexpr unsigned originSamples = 5, finishSamples = 3;
constexpr double rangeMinCm = 2.0, rangeMaxCm = 400.0;
constexpr double takeoffCm = 30.0, landingMaxCm = 10.0;
constexpr double maxHdop = 2.5, targetM = 20.0;
constexpr double maxGpsSpeedMps = 3.0, jumpAllowanceM = 5.0;
constexpr uint32_t minSatellites = 6;
}  // namespace cfg

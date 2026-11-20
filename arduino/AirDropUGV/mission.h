#pragma once
#include <cmath>
#include <stdint.h>
#include "config.h"

namespace ugv {
inline uint32_t elapsed(uint32_t now, uint32_t then) { return now - then; }
enum class State { Disarmed, PreTakeoff, InAir, LandingCheck, Release,
                   WaitGps, Driving, Done, Fault };
inline const char* name(State s) {
  switch (s) {
    case State::Disarmed: return "DISARMED";
    case State::PreTakeoff: return "PRE_TAKEOFF";
    case State::InAir: return "IN_AIR";
    case State::LandingCheck: return "LANDED_CHECKING";
    case State::Release: return "RELEASE_CAR";
    case State::WaitGps: return "WAIT_GPS";
    case State::Driving: return "DRIVING_20M";
    case State::Done: return "MISSION_DONE";
    case State::Fault: return "FAULT";
  }
  return "UNKNOWN";
}
struct Range { bool valid = false; double cm = 0; uint32_t at = 0; };
struct Fix {
  bool valid = false;
  double lat = 0, lon = 0, hdop = 99;
  uint32_t satellites = 0, at = 0, sequence = 0;
};
inline double distance(double a, double b, double c, double d) {
  constexpr double rad = 0.017453292519943295;
  double p = (c-a)*rad, q = (d-b)*rad;
  double h = std::sin(p/2)*std::sin(p/2) + std::cos(a*rad)*
             std::cos(c*rad)*std::sin(q/2)*std::sin(q/2);
  h = h < 0 ? 0 : (h > 1 ? 1 : h);
  return 6371000.0 * 2 * std::atan2(std::sqrt(h), std::sqrt(1-h));
}
class Mission {
 public:
  State state = State::Disarmed;
  const char* reason = "Boot: send ARM while on the ground";
  bool released = false;
  double displacementM = 0;

  bool ground(uint32_t now, const Range& r) const {
    return rangeOk(now,r) && r.cm <= cfg::landingMaxCm;
  }
  bool gpsOk(uint32_t now, const Fix& f) const {
    return f.valid && elapsed(now,f.at) <= cfg::gpsFreshMs &&
      std::isfinite(f.lat) && std::isfinite(f.lon) &&
      f.lat >= -90 && f.lat <= 90 && f.lon >= -180 && f.lon <= 180 &&
      std::isfinite(f.hdop) && f.hdop > 0 && f.hdop <= cfg::maxHdop &&
      f.satellites >= cfg::minSatellites;
  }
  bool arm(uint32_t now, const Range& r) {
    if (state != State::Disarmed || !ground(now,r)) return false;
    enter(State::PreTakeoff,now,"Armed on valid ground reading");
    return true;
  }
  void abort(uint32_t now) { fail(now,"Operator STOP"); }
  bool drive() const { return state == State::Driving; }

  void update(uint32_t now, const Range& r, const Fix& f) {
    if (rangeSeen_ && r.at != lastRangeAt_ &&
        elapsed(r.at,lastRangeAt_) > cfg::rangeFreshMs) {
      timing_ = false;
      if (state == State::LandingCheck)
        enter(State::InAir,now,"Range sampling interrupted");
    }
    if (!rangeSeen_ || r.at != lastRangeAt_) {
      rangeSeen_ = true; lastRangeAt_ = r.at;
    }
    switch(state) {
      case State::Disarmed: case State::Done: case State::Fault: return;
      case State::PreTakeoff:
        // An invalid/no-echo reading never counts as evidence of flight.
        if (rangeOk(now,r) && r.cm > cfg::takeoffCm) {
          if (!timing_) { timing_ = true; since_ = now; }
          if (elapsed(now,since_) >= cfg::takeoffMs)
            enter(State::InAir,now,"Takeoff confirmed");
        } else timing_ = false;
        break;
      case State::InAir:
        if (ground(now,r)) enter(State::LandingCheck,now,"Ground detected");
        break;
      case State::LandingCheck:
        if (!ground(now,r)) enter(State::InAir,now,"Landing interrupted");
        else if (elapsed(now,entered_) >= cfg::landingMs) {
          released = true;
          enter(State::Release,now,"Release commanded; no latch feedback");
        }
        break;
      case State::Release:
        if (!ground(now,r)) { fail(now,"Ground lost after release"); break; }
        if (elapsed(now,entered_) >= cfg::releaseMs) {
          samples_ = 0; latSum_ = lonSum_ = 0;
          sequence_ = f.sequence; lastOriginAt_ = now;
          enter(State::WaitGps,now,"Acquiring stationary GPS origin");
        }
        break;
      case State::WaitGps:
        if (!ground(now,r)) { fail(now,"Ground lost before drive"); break; }
        if (elapsed(now,entered_) >= cfg::gpsWaitMs) {
          fail(now,"GPS acquisition timeout"); break;
        }
        if (!gpsOk(now,f)) {
          samples_ = 0; latSum_ = lonSum_ = 0;
          sequence_ = f.sequence; break;
        }
        if (f.sequence == sequence_) break;
        sequence_ = f.sequence;
        if (elapsed(f.at,lastOriginAt_) < cfg::originSpacingMs) break;
        lastOriginAt_ = f.at;
        latSum_ += f.lat; lonSum_ += f.lon;
        if (++samples_ >= cfg::originSamples) {
          originLat_ = latSum_/samples_; originLon_ = lonSum_/samples_;
          previous_ = f; reached_ = 0;
          enter(State::Driving,now,"Driving from averaged GPS origin");
        }
        break;
      case State::Driving:
        if (!gpsOk(now,f)) { fail(now,"GPS invalid, stale or low quality"); break; }
        if (elapsed(now,entered_) >= cfg::driveTimeoutMs) {
          fail(now,"Maximum drive duration exceeded"); break;
        }
        if (f.sequence == sequence_) break;
        sequence_ = f.sequence;
        {
          double step = distance(previous_.lat,previous_.lon,f.lat,f.lon);
          double seconds = elapsed(f.at,previous_.at)/1000.0;
          if (seconds <= 0 || step > cfg::jumpAllowanceM + cfg::maxGpsSpeedMps*seconds) {
            fail(now,"Implausible GPS position jump"); break;
          }
          previous_ = f;
          displacementM = distance(originLat_,originLon_,f.lat,f.lon);
          if (displacementM >= cfg::targetM) {
            if (++reached_ >= cfg::finishSamples)
              enter(State::Done,now,"Target displacement confirmed");
          } else reached_ = 0;
        }
        break;
    }
  }
 private:
  uint32_t entered_ = 0, since_ = 0, sequence_ = 0, lastOriginAt_ = 0;
  bool timing_ = false;
  bool rangeSeen_ = false;
  uint32_t lastRangeAt_ = 0;
  unsigned samples_ = 0, reached_ = 0;
  double latSum_ = 0, lonSum_ = 0, originLat_ = 0, originLon_ = 0;
  Fix previous_;
  bool rangeOk(uint32_t now, const Range& r) const {
    return r.valid && elapsed(now,r.at) <= cfg::rangeFreshMs &&
      std::isfinite(r.cm) && r.cm >= cfg::rangeMinCm && r.cm <= cfg::rangeMaxCm;
  }
  void enter(State s, uint32_t now, const char* why) {
    state = s; entered_ = now; timing_ = false; reason = why;
  }
  void fail(uint32_t now, const char* why) { enter(State::Fault,now,why); }
};
}  // namespace ugv

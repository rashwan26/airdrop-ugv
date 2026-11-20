#include "mission.h"
#include <cassert>
#include <iostream>
#include <limits>
using namespace ugv;
struct Rig {
  Mission m; uint32_t t=100, seq=0;
  Range r; Fix f;
  void tick(uint32_t dt, double cm=5, bool valid=true) {
    do {
      uint32_t step=dt>70 ? 70 : dt;
      t+=step; dt-=step; r.valid=valid; r.cm=cm; r.at=t;
      m.update(t,r,f);
    } while (dt);
  }
  void gps(double northM=0, bool valid=true) {
    f.valid=valid; f.lat=30.0+northM/111194.9266; f.lon=31.0;
    f.hdop=1.2; f.satellites=9; f.at=t; f.sequence=++seq;
  }
  void arm() { tick(0); assert(m.arm(t,r)); }
  void airborne() {
    arm(); tick(70,50); tick(3000,50); assert(m.state==State::InAir);
  }
  void release() {
    airborne(); tick(70); tick(10000); assert(m.released);
    tick(3000); assert(m.state==State::WaitGps);
  }
  void drive() {
    release();
    for (unsigned i=0;i<cfg::originSamples;++i) {t+=1000; gps(); tick(0);}
    assert(m.drive());
  }
};
int main() {
  unsigned passed=0;
  { Rig x; x.tick(0); assert(!x.m.drive()); x.r.valid=false;
    assert(!x.m.arm(x.t,x.r)); x.r.valid=true; x.r.at=x.t-1000;
    assert(!x.m.arm(x.t,x.r)); ++passed; }
  { Rig x; x.arm(); x.tick(1,0,false); x.tick(4000,0,false);
    assert(x.m.state==State::PreTakeoff); ++passed; }
  { Rig x; x.arm(); x.tick(1,50); x.tick(2000,50); x.tick(1,0,false);
    x.tick(1,50); x.tick(2999,50); assert(x.m.state==State::PreTakeoff);
    x.tick(1,50); assert(x.m.state==State::InAir); ++passed; }
  { Rig x; x.airborne(); x.tick(1); x.tick(9999);
    assert(!x.m.released); x.tick(1,15); assert(x.m.state==State::InAir);
    x.tick(1); x.tick(10000); assert(x.m.released); ++passed; }
  { Rig x; x.release(); for(int i=0;i<100;++i) x.tick(10);
    assert(!x.m.drive()); x.tick(60000); assert(x.m.state==State::Fault); ++passed; }
  { Rig x; x.release(); x.tick(1000); x.gps(); x.tick(0);
    for(int i=0;i<10;++i) { x.tick(10); }
    assert(!x.m.drive()); ++passed; }
  { Rig x; x.drive(); x.tick(cfg::gpsFreshMs+1);
    assert(x.m.state==State::Fault && !x.m.drive()); ++passed; }
  { Rig x; x.drive(); x.gps(); x.f.hdop=2.6; x.tick(0);
    assert(x.m.state==State::Fault); x.gps(); x.tick(0);
    assert(!x.m.drive()); assert(!x.m.arm(x.t,x.r)); ++passed; }
  { Rig x; x.drive(); x.t+=1000; x.gps(100); x.tick(0);
    assert(x.m.state==State::Fault); ++passed; }
  { Rig x; x.drive();
    for(int n=2;n<=22;n+=2) {x.t+=1000; x.gps(n); x.tick(0);}
    assert(x.m.drive()); x.t+=1000; x.gps(22); x.tick(0);
    assert(x.m.state==State::Done && !x.m.drive());
    x.tick(1000); assert(x.m.state==State::Done); ++passed; }
  { Rig x; x.drive(); x.t+=cfg::driveTimeoutMs; x.gps(); x.tick(0);
    assert(x.m.state==State::Fault); ++passed; }
  { Rig x; x.drive(); x.m.abort(x.t); assert(!x.m.drive() && x.m.released); ++passed; }
  { Rig x; x.t=std::numeric_limits<uint32_t>::max()-2000;
    x.airborne(); assert(x.m.state==State::InAir); ++passed; }
  { Rig x; x.release(); x.tick(10,0,false);
    assert(x.m.state==State::Fault && x.m.released); ++passed; }
  { Rig x; x.drive(); x.gps(); x.f.lat=std::numeric_limits<double>::quiet_NaN();
    x.tick(0); assert(!x.m.drive()); ++passed; }
  { Rig x; x.release(); x.tick(1000); x.gps(); x.tick(0);
    x.tick(1600); // repeated data becomes stale and clears the origin accumulator
    for(unsigned i=0;i<cfg::originSamples-1;++i) {x.t+=1000; x.gps(); x.tick(0);}
    assert(!x.m.drive()); x.t+=1000; x.gps(); x.tick(0); assert(x.m.drive()); ++passed; }
  { Rig x; x.arm(); x.tick(1,50); x.tick(2000,50);
    x.t+=1000; x.tick(0,50); assert(x.m.state==State::PreTakeoff);
    x.tick(2999,50); assert(x.m.state==State::PreTakeoff);
    x.tick(1,50); assert(x.m.state==State::InAir); ++passed; }
  assert(distance(30,31,30,31)==0);
  assert(std::abs(distance(0,0,0,1)-111194.9266)<0.01);
  std::cout << passed << " mission scenarios + geodesy checks passed\n";
}

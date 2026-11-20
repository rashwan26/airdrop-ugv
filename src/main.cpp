#include <Arduino.h>
#include <TinyGPS++.h>
#include <ESP32Servo.h>
#include <esp_task_wdt.h>
#include "mission.h"

TinyGPSPlus gps;
HardwareSerial gpsSerial(1);
Servo latch;
ugv::Mission mission;
ugv::Range range;
ugv::Fix fix;
uint32_t lastRange = 0, lastLog = 0;
uint32_t hdopAt = 0, satellitesAt = 0;
bool hdopSeen = false, satellitesSeen = false;
constexpr unsigned motorChannelBase = 4; // Servo library uses other LEDC timers.

void motors(bool run) {
  for (unsigned i=0; i<4; ++i) {
    const bool reverse = cfg::motors[i].reversed;
    ledcWrite(motorChannelBase+2*i, run && reverse ? cfg::driveDuty : 0);
    ledcWrite(motorChannelBase+2*i+1, run && !reverse ? cfg::driveDuty : 0);
  }
}
void readGps() {
  while (gpsSerial.available()) {
    if (!gps.encode(static_cast<char>(gpsSerial.read()))) continue;
    const uint32_t now = millis();
    if (gps.hdop.isUpdated()) {
      fix.hdop = gps.hdop.hdop(); hdopAt = now; hdopSeen = gps.hdop.isValid();
    }
    if (gps.satellites.isUpdated()) {
      fix.satellites = gps.satellites.value(); satellitesAt = now;
      satellitesSeen = gps.satellites.isValid();
    }
    if (gps.location.isUpdated()) {
      fix.lat = gps.location.lat(); fix.lon = gps.location.lng();
      fix.at = now - static_cast<uint32_t>(gps.location.age());
      ++fix.sequence;
    }
  }
  // The custom library extension exposes fix quality; a GGA quality of zero
  // invalidates retained location data immediately after a no-fix sentence.
}
TinyGPSCustom ggaQuality(gps, "GPGGA", 6);
TinyGPSCustom gnQuality(gps, "GNGGA", 6);
bool receiverFix = false;
uint32_t qualityAt = 0;
void updateQuality(uint32_t now) {
  if (ggaQuality.isUpdated()) {
    receiverFix = atoi(ggaQuality.value()) > 0; qualityAt = now;
  }
  if (gnQuality.isUpdated()) {
    receiverFix = atoi(gnQuality.value()) > 0; qualityAt = now;
  }
  fix.valid = receiverFix && gps.location.isValid() && hdopSeen && satellitesSeen &&
    ugv::elapsed(now,qualityAt) <= cfg::gpsFreshMs &&
    ugv::elapsed(now,hdopAt) <= cfg::gpsFreshMs &&
    ugv::elapsed(now,satellitesAt) <= cfg::gpsFreshMs;
}
void readRange() {
  digitalWrite(cfg::trig,LOW); delayMicroseconds(2);
  digitalWrite(cfg::trig,HIGH); delayMicroseconds(10);
  digitalWrite(cfg::trig,LOW);
  // Bounded blocking: at most 25 ms. FSM deadlines never use delay().
  unsigned long pulse = pulseIn(cfg::echo,HIGH,cfg::echoTimeoutUs);
  range.at = millis(); range.cm = pulse * 0.0343 / 2.0;
  range.valid = pulse != 0 && range.cm >= cfg::rangeMinCm && range.cm <= cfg::rangeMaxCm;
}
void commands(uint32_t now) {
  static char command[12]; static unsigned count = 0;
  static bool overflow = false;
  while (Serial.available()) {
    char c = static_cast<char>(Serial.read());
    if (c=='\n' || c=='\r') {
      command[count]='\0';
      if (!overflow && count) {
        if (strcmp(command,"STOP")==0) mission.abort(now);
        else if (strcmp(command,"ARM")==0)
          Serial.println(mission.arm(now,range) ? "ARM accepted" : "ARM rejected: check state and ground reading");
        else Serial.println("Commands: ARM, STOP (uppercase, newline)");
      }
      count=0; overflow=false;
    } else if (count < sizeof(command)-1) command[count++]=c;
    else overflow=true;
  }
}
void setup() {
  // External 10k pulldowns hold driver inputs low before firmware starts.
  for (const auto& m : cfg::motors) {
    pinMode(m.a,OUTPUT); pinMode(m.b,OUTPUT);
    digitalWrite(m.a,LOW); digitalWrite(m.b,LOW);
  }
  Serial.begin(115200);
  pinMode(cfg::trig,OUTPUT); digitalWrite(cfg::trig,LOW);
  pinMode(cfg::echo,INPUT);
  gpsSerial.begin(9600,SERIAL_8N1,cfg::gpsRx,cfg::gpsTx);
  // Servo takes high-speed channel 0. Motors use high-speed timers 2/3
  // and low-speed timers 0/1; these are separate hardware timer groups.
  ESP32PWM::allocateTimer(0);
  latch.setPeriodHertz(50);
  latch.attach(cfg::servo,1000,2000);
  latch.write(cfg::lockedAngle);
  for (unsigned i=0; i<4; ++i) {
    ledcSetup(motorChannelBase+2*i,1000,8);
    ledcSetup(motorChannelBase+2*i+1,1000,8);
    ledcAttachPin(cfg::motors[i].a,motorChannelBase+2*i);
    ledcAttachPin(cfg::motors[i].b,motorChannelBase+2*i+1);
  }
  motors(false);
  esp_task_wdt_init(3,true);
  esp_task_wdt_add(nullptr);
  Serial.println("Air-Drop UGV reconstruction | DISARMED | ARM / STOP");
}
void loop() {
  esp_task_wdt_reset();
  readGps();
  uint32_t now=millis();
  if (ugv::elapsed(now,lastRange) >= cfg::rangeIntervalMs) {
    lastRange=now; readRange();
  }
  // Refresh time after pulseIn; never compare a new sample to an older now.
  readGps(); now=millis(); updateQuality(now);
  commands(now);
  ugv::State before=mission.state;
  mission.update(now,range,fix);
  latch.write(mission.released ? cfg::releasedAngle : cfg::lockedAngle);
  motors(mission.drive());
  if (before != mission.state || ugv::elapsed(now,lastLog)>=1000) {
    lastLog=now;
    Serial.printf("%lu %s range=%.1f valid=%d gps=%d displacement=%.2f reason=%s\n",
      static_cast<unsigned long>(now),ugv::name(mission.state),range.cm,
      range.valid,mission.gpsOk(now,fix),mission.displacementM,mission.reason);
  }
}

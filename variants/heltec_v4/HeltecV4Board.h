#pragma once

#include <Arduino.h>
#include <helpers/RefCountedDigitalPin.h>
#include <helpers/ESP32Board.h>
#include <driver/rtc_io.h>

class HeltecV4Board : public ESP32Board {

public:
  RefCountedDigitalPin periph_power;

  HeltecV4Board() : periph_power(PIN_VEXT_EN,PIN_VEXT_EN_ACTIVE) { }

  void begin();
  void onBeforeTransmit(void) override;
  void onAfterTransmit(void) override;
  void enterDeepSleep(uint32_t secs, int pin_wake_btn = -1);
  void powerOff() override;
  uint16_t getBattMilliVolts() override;
  const char* getManufacturerName() const override ;

#if defined(ENABLE_PACKET_TOA)
  /** Start TOA timer, PPS capture (CAP0) and DIO1 packet-arrival capture (CAP1). Call once from setup. */
  void toaBegin();
  /** Last PPS rising-edge timestamp in timer ticks (80 MHz). */
  uint64_t toaGetLastPpsTicks() const;
  /** Last DIO1 (packet arrival) rising-edge capture in timer ticks; 0 if none yet. */
  uint32_t toaGetLastDio1CaptureTicks() const;
  uint32_t getLastToaDio1CaptureTicks() const override;
  /** Smoothed clock error vs 80 MHz from PPS interval, in ppm (0 until at least two PPS edges). */
  float getToaClockErrorPpm() const override;
#endif

};

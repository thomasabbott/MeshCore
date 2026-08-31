#pragma once

#include <MeshCore.h>
#include <Arduino.h>
#include <helpers/KeyValueStore.h>

#if defined(RP2040_LOW_POWER) || defined(RP2040_BATTERY_PROTECT)
// USB VBUS detect (GPIO 24), sampled once at boot
#define PIN_VBUS_DET  24
#endif
#ifdef RP2040_LOW_POWER
// Pico board: SMPS PFM select (GPIO 23)
#define PIN_SMPS_MODE 23
#endif

#ifdef RP2040_BATTERY_PROTECT
// LiFePO4-style thresholds on Pico VSYS (mV); 100 mV hysteresis
#define BATTERY_PROTECT_LOW_MV    2850
#define BATTERY_PROTECT_RESUME_MV 2950
#define BATTERY_PROTECT_SLEEP_MS  5000
#endif

// built-ins — Pico onboard VSYS/3 on ADC3 (GPIO29, not on 40-pin header)
#ifndef PIN_VBAT_READ
#define PIN_VBAT_READ    29
#endif
#ifndef ADC_MULTIPLIER
#define ADC_MULTIPLIER   (3.0f * 3.3f * 1000)
#endif
#define  PIN_LED_BUILTIN  LED_BUILTIN

class PicoWBoard : public mesh::MainBoard {
protected:
  uint8_t startup_reason;
#ifdef RP2040_BATTERY_PROTECT
  bool on_battery_power;
#endif

public:
  void begin();
  uint8_t getStartupReason() const override { return startup_reason; }

#ifdef RP2040_LOW_POWER
  void applyPowerClocks(bool usb_connected);
#endif
#ifdef RP2040_BATTERY_PROTECT
  bool isOnBatteryPower() const { return on_battery_power; }
  void lowPowerSleep();
#endif

  void attachDynamicPrefs(KeyValueStore* prefs) { }  // no-op

  void onBeforeTransmit() override {
    digitalWrite(LED_BUILTIN, HIGH);   // turn TX LED on
  }

  void onAfterTransmit() override {
    digitalWrite(LED_BUILTIN, LOW);   // turn TX LED off
  }

  #define BATTERY_SAMPLES 8

  uint16_t getBattMilliVolts() override {
    analogReadResolution(12);

    uint32_t raw = 0;
    for (int i = 0; i < BATTERY_SAMPLES; i++) {
      raw += analogRead(PIN_VBAT_READ);
    }
    raw = raw / BATTERY_SAMPLES;

    return (ADC_MULTIPLIER * raw) / 4096;
  }

  float getMCUTemperature() override {
    return analogReadTemp();
  }

  const char* getManufacturerName() const override {
    return "Pico";
  }

  void reboot() override {
    //NVIC_SystemReset();
    rp2040.reboot();
  }

  uint32_t getIRQGpio() override { return P_LORA_DIO_1; }
  void sleep(uint32_t secs) override;

  bool startOTAUpdate(const char* id, char reply[]) override;
};

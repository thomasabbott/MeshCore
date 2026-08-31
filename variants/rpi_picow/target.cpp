#include <Arduino.h>
#include "target.h"
#include <helpers/ArduinoHelpers.h>

PicoWBoard board;

RADIO_CLASS radio = new Module(P_LORA_NSS, P_LORA_DIO_1, P_LORA_RESET, P_LORA_BUSY, SPI1);

WRAPPER_CLASS radio_driver(radio, board);

VolatileRTCClock fallback_clock;
AutoDiscoverRTCClock rtc_clock(fallback_clock);
SensorManager sensors;

bool radio_init() {
  rtc_clock.begin(Wire);
  
  return radio.std_init(&SPI1);
}

mesh::LocalIdentity radio_new_identity() {
  RadioNoiseListener rng(radio);
  return mesh::LocalIdentity(&rng);  // create new random identity
}

static void radio_shutdown_for_protect() {
  radio.clearPacketReceivedAction();
  radio_driver.powerOff();  // cold sleep — sleep(false), TCXO off
}

void battery_protect_loop_check() {
#ifdef RP2040_BATTERY_PROTECT
  if (!board.isOnBatteryPower()) {
    return;
  }
  if (board.getBattMilliVolts() >= BATTERY_PROTECT_LOW_MV) {
    return;
  }

  radio_shutdown_for_protect();

  while (true) {
    board.lowPowerSleep();
    if (board.getBattMilliVolts() >= BATTERY_PROTECT_RESUME_MV) {
      break;
    }
  }
  board.reboot();
#else
  (void)0;
#endif
}

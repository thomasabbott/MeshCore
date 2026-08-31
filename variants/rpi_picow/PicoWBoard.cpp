#include <Arduino.h>
#include "PicoWBoard.h"

#if defined(RP2040_LOW_POWER) || defined(RP2040_BATTERY_PROTECT)
#include "pico/stdlib.h"
#include "pico/time.h"
#endif
#ifdef RP2040_LOW_POWER
#include "hardware/vreg.h"
#include "hardware/clocks.h"
#include "hardware/pll.h"
#include "hardware/structs/pll.h"
#include "hardware/structs/clocks.h"
#endif

#include <Wire.h>

//static BLEDfu bledfu;

static void connect_callback(uint16_t conn_handle) {
  (void)conn_handle;
  MESH_DEBUG_PRINTLN("BLE client connected");
}

static void disconnect_callback(uint16_t conn_handle, uint8_t reason) {
  (void)conn_handle;
  (void)reason;

  MESH_DEBUG_PRINTLN("BLE client disconnected");
}

#ifdef RP2040_LOW_POWER
void PicoWBoard::applyPowerClocks(bool usb_connected) {
  if (usb_connected) {
    set_sys_clock_khz(48000, true);

    clock_configure(clk_usb,
                    0,
                    CLOCKS_CLK_USB_CTRL_AUXSRC_VALUE_CLKSRC_PLL_USB,
                    48 * MHZ,
                    48 * MHZ);

    clock_configure(clk_adc,
                    0,
                    CLOCKS_CLK_ADC_CTRL_AUXSRC_VALUE_CLKSRC_PLL_USB,
                    48 * MHZ,
                    48 * MHZ);
  } else {
    clock_configure(clk_adc,
                    0,
                    CLOCKS_CLK_ADC_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS,
                    18 * MHZ,
                    18 * MHZ);

    set_sys_clock_khz(18000, true);

    clock_stop(clk_usb);
    pll_deinit(pll_usb);

    vreg_set_voltage(VREG_VOLTAGE_0_95);
  }

  clock_configure(clk_peri,
                  0,
                  CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS,
                  usb_connected ? 48000 * 1000 : 18000 * 1000,
                  usb_connected ? 48000 * 1000 : 18000 * 1000);
}
#endif

void PicoWBoard::begin() {
#if defined(RP2040_LOW_POWER) || defined(RP2040_BATTERY_PROTECT)
  pinMode(PIN_VBUS_DET, INPUT);
  bool usb_connected = digitalRead(PIN_VBUS_DET);
#ifdef RP2040_BATTERY_PROTECT
  on_battery_power = !usb_connected;
#endif
#else
  bool usb_connected = false;
#endif
#ifdef RP2040_LOW_POWER
  pinMode(PIN_SMPS_MODE, OUTPUT);
  digitalWrite(PIN_SMPS_MODE, LOW);
  applyPowerClocks(usb_connected);
#endif

  startup_reason = BD_STARTUP_NORMAL;
#if defined(PIN_VBAT_READ)
  pinMode(PIN_VBAT_READ, INPUT);
#endif
#ifdef PIN_USER_BTN
  pinMode(PIN_USER_BTN, INPUT_PULLUP);
#endif

#if defined(PIN_BOARD_SDA) && defined(PIN_BOARD_SCL)
  Wire.setPins(PIN_BOARD_SDA, PIN_BOARD_SCL);
#endif

  Wire.begin();

  delay(10);   // give sx1262 some time to power up
}

bool PicoWBoard::startOTAUpdate(const char* id, char reply[]) {
  return false;
}

void PicoWBoard::sleep(uint32_t secs) {
  (void)secs;
  __wfi();
}

#ifdef RP2040_BATTERY_PROTECT
static volatile bool protect_sleep_done;

static int64_t protect_sleep_alarm(__unused alarm_id_t id, __unused void* user_data) {
  protect_sleep_done = true;
  return 0;
}

void PicoWBoard::lowPowerSleep() {
  digitalWrite(LED_BUILTIN, LOW);
  protect_sleep_done = false;
  alarm_id_t alarm = add_alarm_in_ms(BATTERY_PROTECT_SLEEP_MS, protect_sleep_alarm, NULL, true);
  if (alarm >= 0) {
    while (!protect_sleep_done) {
      __wfi();
    }
  } else {
    sleep_ms(BATTERY_PROTECT_SLEEP_MS);
  }
}
#endif

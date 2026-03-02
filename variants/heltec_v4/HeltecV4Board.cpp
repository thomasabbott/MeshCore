#include "HeltecV4Board.h"

#if defined(ENABLE_PACKET_TOA)
#include "esp_err.h"
#include "driver/gpio.h"
#include "driver/mcpwm.h"

// TOA state: updated by MCPWM capture callbacks (same 80 MHz timer for CAP0 and CAP1).
// Clock error: PPS callback stores uint32_t interval (handles 32-bit wrap); EMA in getToaClockErrorPpm().
#define TOA_PPS_TICKS_PER_SEC      80000000u
#define TOA_PPS_TICKS_HALF_SEC     40000000u
#define TOA_PPS_TICKS_ONE_HALF    120000000u   // 1.5 s - reject intervals outside 0.5..1.5 s
#define TOA_PPM_EMA_OLD            0.8f
#define TOA_PPM_EMA_NEW            0.2f
#define TOA_PREVIOUS_NONE          0xFFFFFFFFu

struct ToaState {
  volatile uint64_t last_pps_ticks;
  volatile uint32_t last_dio1_ticks;
  volatile uint32_t previous_pps_ticks;
  volatile uint32_t last_delta_ticks;  // unsigned: correct modular interval across 32-bit wrap
  volatile uint8_t have_new_delta;
  float smoothed_ppm;
};
static ToaState s_toa = { 0, 0, TOA_PREVIOUS_NONE, 0, 0, 0.0f };

static bool IRAM_ATTR pps_capture_cb(mcpwm_unit_t, mcpwm_capture_channel_id_t,
                                     const cap_event_data_t* edata, void*) {
  uint32_t new_val = edata->cap_value;
  if (s_toa.previous_pps_ticks != TOA_PREVIOUS_NONE) {
    s_toa.last_delta_ticks = new_val - s_toa.previous_pps_ticks;  // uint32_t: wrap-correct
    s_toa.have_new_delta = 1;
  }
  s_toa.previous_pps_ticks = new_val;
  s_toa.last_pps_ticks = (uint64_t)new_val;
  return false;
}

static bool IRAM_ATTR dio1_capture_cb(mcpwm_unit_t, mcpwm_capture_channel_id_t,
                                      const cap_event_data_t* edata, void*) {
  s_toa.last_dio1_ticks = edata->cap_value;
  return false;
}
#endif

void HeltecV4Board::begin() {
    ESP32Board::begin();

    pinMode(PIN_ADC_CTRL, OUTPUT);
    digitalWrite(PIN_ADC_CTRL, LOW); // Initially inactive

    // Check if waking from deep sleep
    esp_reset_reason_t reason = esp_reset_reason();
    if (reason == ESP_RST_DEEPSLEEP) {
      long wakeup_source = esp_sleep_get_ext1_wakeup_status();
      if (wakeup_source & (1 << P_LORA_DIO_1)) {  // received a LoRa packet (while in deep sleep)
        startup_reason = BD_STARTUP_RX_PACKET;
      }

      // Release RTC holds - pins retain their state, no need to reconfigure
      rtc_gpio_hold_dis((gpio_num_t)P_LORA_NSS);
      rtc_gpio_hold_dis((gpio_num_t)P_LORA_PA_POWER);
      rtc_gpio_hold_dis((gpio_num_t)P_LORA_PA_EN);
      rtc_gpio_deinit((gpio_num_t)P_LORA_DIO_1);
    } else {
      // Cold boot: Configure GC1109 FEM pins
      // Control logic (from GC1109 datasheet):
      //   Receive LNA:  CSD=1, CTX=0, CPS=X  (17dB gain, 2dB NF)
      //   Transmit PA:  CSD=1, CTX=1, CPS=1  (full PA enabled)
      // Pin mapping: CTX->DIO2 (auto), CSD->GPIO2, CPS->GPIO46, VFEM->GPIO7

      // VFEM_Ctrl (GPIO7): Power enable for GC1109 LDO
      pinMode(P_LORA_PA_POWER, OUTPUT);
      digitalWrite(P_LORA_PA_POWER, HIGH);

      // CSD (GPIO2): Chip enable - must be HIGH for GC1109 to work
      pinMode(P_LORA_PA_EN, OUTPUT);
      digitalWrite(P_LORA_PA_EN, HIGH);
    }

    periph_power.begin();

    // Note: GPIO46 (CPS) is a strapping pin - do NOT configure it here.
    // TX handlers are fully responsible for GPIO46 (see onBeforeTransmit/onAfterTransmit)

#if defined(ENABLE_PACKET_TOA)
    toaBegin();
#endif
}

  void HeltecV4Board::onBeforeTransmit(void) {
    // GPIO46 is a strapping pin - only drive it when actively transmitting
    pinMode(P_LORA_PA_TX_EN, OUTPUT);
    digitalWrite(P_LORA_PA_TX_EN, HIGH);   // CPS=1: Enable full PA mode
    digitalWrite(P_LORA_TX_LED, HIGH);
  }

  void HeltecV4Board::onAfterTransmit(void) {
    digitalWrite(P_LORA_PA_TX_EN, LOW);
    pinMode(P_LORA_PA_TX_EN, INPUT);       // Release strapping pin
    digitalWrite(P_LORA_TX_LED, LOW);
  }

  void HeltecV4Board::enterDeepSleep(uint32_t secs, int pin_wake_btn) {
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);

    // Make sure the DIO1 and NSS GPIOs are hold on required levels during deep sleep
    rtc_gpio_set_direction((gpio_num_t)P_LORA_DIO_1, RTC_GPIO_MODE_INPUT_ONLY);
    rtc_gpio_pulldown_en((gpio_num_t)P_LORA_DIO_1);

    rtc_gpio_hold_en((gpio_num_t)P_LORA_NSS);

    // Hold GC1109 FEM pins during sleep for RX wake capability
    // State: CSD=1, CTX=0 (DIO2), CPS=X -> Receive LNA mode
    rtc_gpio_hold_en((gpio_num_t)P_LORA_PA_POWER);   // VFEM_Ctrl - keep LDO powered
    rtc_gpio_hold_en((gpio_num_t)P_LORA_PA_EN);      // CSD=1 - chip enabled
    // Note: GPIO46 (CPS) is NOT an RTC GPIO, cannot hold - but CPS is don't care for RX

    if (pin_wake_btn < 0) {
      esp_sleep_enable_ext1_wakeup( (1L << P_LORA_DIO_1), ESP_EXT1_WAKEUP_ANY_HIGH);  // wake up on: recv LoRa packet
    } else {
      esp_sleep_enable_ext1_wakeup( (1L << P_LORA_DIO_1) | (1L << pin_wake_btn), ESP_EXT1_WAKEUP_ANY_HIGH);  // wake up on: recv LoRa packet OR wake btn
    }

    if (secs > 0) {
      esp_sleep_enable_timer_wakeup(secs * 1000000);
    }

    // Finally set ESP32 into sleep
    esp_deep_sleep_start();   // CPU halts here and never returns!
  }

  void HeltecV4Board::powerOff()  {
    enterDeepSleep(0);
  }

  uint16_t HeltecV4Board::getBattMilliVolts()  {
    analogReadResolution(10);
    digitalWrite(PIN_ADC_CTRL, HIGH);
    delay(10);
    uint32_t raw = 0;
    for (int i = 0; i < 8; i++) {
      raw += analogRead(PIN_VBAT_READ);
    }
    raw = raw / 8;

    digitalWrite(PIN_ADC_CTRL, LOW);

    return (5.42 * (3.3 / 1024.0) * raw) * 1000;
  }

  const char* HeltecV4Board::getManufacturerName() const {
  #ifdef HELTEC_LORA_V4_TFT
    return "Heltec V4 TFT";
  #else
    return "Heltec V4 OLED";
  #endif
  }

#if defined(ENABLE_PACKET_TOA)
void HeltecV4Board::toaBegin() {
  // CAP0 = PPS (GPS), CAP1 = DIO1 (packet arrival). Same MCPWM unit so timestamps are comparable.
  mcpwm_pin_config_t pin_cfg = {};
  pin_cfg.mcpwm0a_out_num = -1;
  pin_cfg.mcpwm0b_out_num = -1;
  pin_cfg.mcpwm1a_out_num = -1;
  pin_cfg.mcpwm1b_out_num = -1;
  pin_cfg.mcpwm2a_out_num = -1;
  pin_cfg.mcpwm2b_out_num = -1;
  pin_cfg.mcpwm_sync0_in_num = -1;
  pin_cfg.mcpwm_sync1_in_num = -1;
  pin_cfg.mcpwm_sync2_in_num = -1;
  pin_cfg.mcpwm_fault0_in_num = -1;
  pin_cfg.mcpwm_fault1_in_num = -1;
  pin_cfg.mcpwm_fault2_in_num = -1;
  pin_cfg.mcpwm_cap0_in_num = (gpio_num_t)PIN_GPS_PPS;
  pin_cfg.mcpwm_cap1_in_num = (gpio_num_t)P_LORA_DIO_1;
  pin_cfg.mcpwm_cap2_in_num = -1;

  esp_err_t err = mcpwm_set_pin(MCPWM_UNIT_0, &pin_cfg);
  if (err != ESP_OK) {
    MESH_DEBUG_PRINTLN("TOA mcpwm_set_pin failed: %s", esp_err_to_name(err));
    return;
  }

  mcpwm_capture_config_t cap_conf = {};
  cap_conf.cap_edge = MCPWM_POS_EDGE;
  cap_conf.cap_prescale = 1;
  cap_conf.capture_cb = &pps_capture_cb;
  cap_conf.user_data = nullptr;

  err = mcpwm_capture_enable_channel(MCPWM_UNIT_0, MCPWM_SELECT_CAP0, &cap_conf);
  if (err != ESP_OK) {
    MESH_DEBUG_PRINTLN("TOA mcpwm_capture_enable_channel(CAP0) failed: %s", esp_err_to_name(err));
    return;
  }

  cap_conf.capture_cb = &dio1_capture_cb;
  err = mcpwm_capture_enable_channel(MCPWM_UNIT_0, MCPWM_SELECT_CAP1, &cap_conf);
  if (err != ESP_OK) {
    MESH_DEBUG_PRINTLN("TOA mcpwm_capture_enable_channel(CAP1) failed: %s", esp_err_to_name(err));
    return;
  }

  MESH_DEBUG_PRINTLN("TOA initialized: CAP0=PPS pin %d, CAP1=DIO1 pin %d (rising edge)", PIN_GPS_PPS, P_LORA_DIO_1);
}

uint64_t HeltecV4Board::toaGetLastPpsTicks() const {
  return s_toa.last_pps_ticks;
}

uint32_t HeltecV4Board::toaGetLastDio1CaptureTicks() const {
  return s_toa.last_dio1_ticks;
}

uint32_t HeltecV4Board::getLastToaDio1CaptureTicks() const {
  return toaGetLastDio1CaptureTicks();
}

float HeltecV4Board::getToaClockErrorPpm() const {
  if (s_toa.have_new_delta) {
    s_toa.have_new_delta = 0;
    uint32_t d = s_toa.last_delta_ticks;
    // Only use plausible 1 s interval (0.5..1.5 s) so wrap glitches and missed PPS don't spike ppm
    if (d >= TOA_PPS_TICKS_HALF_SEC && d <= TOA_PPS_TICKS_ONE_HALF) {
      float raw_ppm = (float)((int64_t)d - (int64_t)TOA_PPS_TICKS_PER_SEC) / 80.0f;
      s_toa.smoothed_ppm = TOA_PPM_EMA_OLD * s_toa.smoothed_ppm + TOA_PPM_EMA_NEW * raw_ppm;
    }
  }
  return s_toa.smoothed_ppm;
}
#endif

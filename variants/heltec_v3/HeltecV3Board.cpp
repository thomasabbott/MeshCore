#include "HeltecV3Board.h"

#if defined(ENABLE_PACKET_TOA)
#include "esp_err.h"
#include "driver/gpio.h"
#include "driver/mcpwm.h"

// TOA state: updated by MCPWM capture callbacks (same 80 MHz timer for CAP0 and CAP1).
struct ToaState {
  volatile uint64_t last_pps_ticks;
  volatile uint32_t last_dio1_ticks;
};
static ToaState s_toa = {};

static bool IRAM_ATTR pps_capture_cb(mcpwm_unit_t, mcpwm_capture_channel_id_t,
                                     const cap_event_data_t* edata, void*) {
  s_toa.last_pps_ticks = (uint64_t)edata->cap_value;
  return false;
}

static bool IRAM_ATTR dio1_capture_cb(mcpwm_unit_t, mcpwm_capture_channel_id_t,
                                      const cap_event_data_t* edata, void*) {
  s_toa.last_dio1_ticks = edata->cap_value;
  return false;
}
#endif

void HeltecV3Board::begin() {
  ESP32Board::begin();

  // Auto-detect correct ADC_CTRL pin polarity (different for boards >3.2)
  pinMode(PIN_ADC_CTRL, INPUT);
  adc_active_state = !digitalRead(PIN_ADC_CTRL);

  pinMode(PIN_ADC_CTRL, OUTPUT);
  digitalWrite(PIN_ADC_CTRL, !adc_active_state); // Initially inactive

  periph_power.begin();

  esp_reset_reason_t reason = esp_reset_reason();
  if (reason == ESP_RST_DEEPSLEEP) {
    long wakeup_source = esp_sleep_get_ext1_wakeup_status();
    if (wakeup_source & (1 << P_LORA_DIO_1)) {  // received a LoRa packet (while in deep sleep)
      startup_reason = BD_STARTUP_RX_PACKET;
    }

    rtc_gpio_hold_dis((gpio_num_t)P_LORA_NSS);
    rtc_gpio_deinit((gpio_num_t)P_LORA_DIO_1);
  }

#if defined(ENABLE_PACKET_TOA)
  toaBegin();
#endif
}

#if defined(ENABLE_PACKET_TOA)
void HeltecV3Board::toaBegin() {
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

uint64_t HeltecV3Board::toaGetLastPpsTicks() const {
  return s_toa.last_pps_ticks;
}

uint32_t HeltecV3Board::toaGetLastDio1CaptureTicks() const {
  return s_toa.last_dio1_ticks;
}

uint32_t HeltecV3Board::getLastToaDio1CaptureTicks() const {
  return toaGetLastDio1CaptureTicks();
}
#endif

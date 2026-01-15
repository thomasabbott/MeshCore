#pragma once

#define RADIOLIB_STATIC_ONLY 1
#include <RadioLib.h>
#include <helpers/radiolib/RadioLibWrappers.h>
#include <HeltecV3Board.h>
#include <helpers/radiolib/CustomSX1262Wrapper.h>
#include <helpers/AutoDiscoverRTCClock.h>
#include <helpers/SensorManager.h>
#include <helpers/sensors/EnvironmentSensorManager.h>
#ifdef DISPLAY_CLASS
  #include <helpers/ui/SSD1306Display.h>
  #include <helpers/ui/MomentaryButton.h>
#endif

extern HeltecV3Board board;
extern WRAPPER_CLASS radio_driver;
extern AutoDiscoverRTCClock rtc_clock;
//extern EnvironmentSensorManager sensors;

// from Gemini
class HeltecV3SensorManager : public EnvironmentSensorManager {
  public:
    using EnvironmentSensorManager::EnvironmentSensorManager; // Use existing constructor
    bool begin() override;
    void loop() override;
    int getNumSettings() const override;
    const char* getSettingName(int i) const override;
    const char* getSettingValue(int i) const override;
    bool setSettingValue(const char* name, const char* value) override;
  private:
    //bool gps_active = true; // Force active for now
};

extern HeltecV3SensorManager sensors;
// end from Gemini

#ifdef DISPLAY_CLASS
  extern DISPLAY_CLASS display;
  extern MomentaryButton user_btn;
#endif

bool radio_init();
uint32_t radio_get_rng_seed();
void radio_set_params(float freq, float bw, uint8_t sf, uint8_t cr);
void radio_set_tx_power(uint8_t dbm);
mesh::LocalIdentity radio_new_identity();

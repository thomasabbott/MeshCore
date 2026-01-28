#pragma once

#ifdef ARDUINO_ARCH_RP2040
  #define ENV_INCLUDE_RP2040_TEMP 1
#endif

#include <Mesh.h>
#include <helpers/SensorManager.h>
#include <helpers/sensors/LocationProvider.h>
#include <time.h> // Include for system clock

class EnvironmentSensorManager : public SensorManager {
protected:
  int next_available_channel = TELEM_CHANNEL_SELF + 1;

  bool AHTX0_initialized = false;
  bool BME280_initialized = false;
  bool BMP280_initialized = false;
  bool INA3221_initialized = false;
  bool INA219_initialized = false;
  bool INA260_initialized = false;
  bool INA226_initialized = false;
  bool SHTC3_initialized = false;
  bool LPS22HB_initialized = false;
  bool MLX90614_initialized = false;
  bool VL53L0X_initialized = false;
  bool SHT4X_initialized = false;
  bool BME680_initialized = false;
  bool BMP085_initialized = false;
  bool RP2040_TEMP_initialized = false;
  bool Solar_initialized = false;
  uint32_t startup_time = 0;
  bool gps_detected = false;
  bool gps_active = false;


// --- NEW: Statistics & History ---
  #ifdef ENABLE_SOLAR_STATS
  
  // Time Tracking
  uint32_t last_stats_update = 0;
  long last_day_index = -1;
  const long TIMEZONE_OFFSET = -8 * 3600; // UTC-8 (PST)

  // Live Accumulators
  float current_mA_live = 0.0;      // Current reading (updated every 1s)
  double accumulated_mAs = 0.0;     // Milliamp-Seconds (for mAh calc)
  
  // Today's Stats
  float today_max_mA = 0.0;
  float today_min_temp = 100.0;
  float today_max_temp = -100.0;
  
  // Yesterday's Stats
  float yest_max_mA = 0.0;
  float yest_total_mAh = 0.0;
  float yest_min_temp = 0.0;
  float yest_max_temp = 0.0;

  // Internal Helpers
  float getPrimaryTemperature();
  float readBatteryVoltage();
  float readSolarCurrentStatistical(); // New robust sampler
  #endif
  // --------------------------------

  #if ENV_INCLUDE_GPS
  LocationProvider* _location;
  void start_gps();
  void stop_gps();
  void initBasicGPS();
  #ifdef RAK_BOARD
  void rakGPSInit();
  bool gpsIsAwake(uint8_t ioPin);
  #endif
  #endif


public:
  #if ENV_INCLUDE_GPS
  EnvironmentSensorManager(LocationProvider &location): _location(&location){};
  LocationProvider* getLocationProvider() { return _location; }
  #else
  EnvironmentSensorManager(){};
  #endif

  bool begin() override;
  bool querySensors(uint8_t requester_permissions, CayenneLPP& telemetry) override;
  void loop() override;     // Removed #if GPS check so loop always runs for stats
  int getNumSettings() const override;
  const char* getSettingName(int i) const override;
  const char* getSettingValue(int i) const override;
  bool setSettingValue(const char* name, const char* value) override;
};

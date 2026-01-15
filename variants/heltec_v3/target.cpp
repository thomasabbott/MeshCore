#include <Arduino.h>
#include "target.h"

HeltecV3Board board;

#if defined(P_LORA_SCLK)
  static SPIClass spi;
  RADIO_CLASS radio = new Module(P_LORA_NSS, P_LORA_DIO_1, P_LORA_RESET, P_LORA_BUSY, spi);
#else
  RADIO_CLASS radio = new Module(P_LORA_NSS, P_LORA_DIO_1, P_LORA_RESET, P_LORA_BUSY);
#endif

WRAPPER_CLASS radio_driver(radio, board);

ESP32RTCClock fallback_clock;
AutoDiscoverRTCClock rtc_clock(fallback_clock);

#define HAS_GPS 1
#define ENV_INCLUDE_GPS 1
#define PIN_GPS_RX 48
#define PIN_GPS_TX 26
#define PIN_GPS_EN 47
#define GPS_EN 47
#define PIN_GPS_EN_ACTIVE LOW


#if ENV_INCLUDE_GPS
  #include <helpers/sensors/MicroNMEALocationProvider.h>
  
  // Create the NMEA parser using Serial1
  //MicroNMEALocationProvider nmea = MicroNMEALocationProvider(Serial1);
  MicroNMEALocationProvider nmea = MicroNMEALocationProvider(Serial1, &rtc_clock, -1, 47);
  // Use your new custom manager
  HeltecV3SensorManager sensors(nmea); 
#else
  EnvironmentSensorManager sensors;
#endif


#ifdef DISPLAY_CLASS
  DISPLAY_CLASS display;
  MomentaryButton user_btn(PIN_USER_BTN, 1000, true);
#endif

bool radio_init() {
  fallback_clock.begin();
  rtc_clock.begin(Wire);
  
#if defined(P_LORA_SCLK)
  return radio.std_init(&spi);
#else
  return radio.std_init();
#endif
}

uint32_t radio_get_rng_seed() {
  return radio.random(0x7FFFFFFF);
}

void radio_set_params(float freq, float bw, uint8_t sf, uint8_t cr) {
  radio.setFrequency(freq);
  radio.setSpreadingFactor(sf);
  radio.setBandwidth(bw);
  radio.setCodingRate(cr);
}

void radio_set_tx_power(uint8_t dbm) {
  radio.setOutputPower(dbm);
}

mesh::LocalIdentity radio_new_identity() {
  RadioNoiseListener rng(radio);
  return mesh::LocalIdentity(&rng);  // create new random identity
}



// --------------------------------------------------------------------------
// HELTEC V3 SENSOR MANAGER IMPLEMENTATION by Gemini on 14 Jan 2026
// --------------------------------------------------------------------------

bool HeltecV3SensorManager::begin() {
  // 1. Start Serial1. 
  // NOTE: We use the pins you confirmed work. 
  // If "GPS Tx" is on Pin 26, then 26 is the ESP32's RX pin (3rd arg).
  // If "GPS Rx" is on Pin 48, then 48 is the ESP32's TX pin (4th arg).
  Serial1.begin(9600, SERIAL_8N1, PIN_GPS_TX, PIN_GPS_RX); 
  
  MESH_DEBUG_PRINTLN("GPS: Manager Begin. Serial1 started on RX=%d, TX=%d", PIN_GPS_TX, PIN_GPS_RX);
  
  // 2. Start the location provider (flips the Enable pin LOW)
  if (_location) {
      _location->begin();
  }

  gps_active = true;    // Set the PARENT class variable to true
  gps_detected = true;  // Tell parent we found hardware
  
  // 3. IMPORTANT: Call the base class begin() if it exists/does setup
  // (Safe to omit if EnvironmentSensorManager::begin is empty, but good practice)
  return true;
}

void HeltecV3SensorManager::loop() {
  // 1. Process Raw GPS Data (Feed the NMEA parser)
  if (gps_active && _location) {
     _location->loop(); 
  }
  // 2. CRITICAL FIX: Call the Parent Class Loop!
  // This is likely where the data is grabbed from _location and sent to the Mesh/Web App.
  EnvironmentSensorManager::loop();

  // 3. Debug Print (Optional - keep this to verify it's still alive)
  static uint32_t last_print = 0;
  if (millis() - last_print > 5000 && _location && _location->isValid()) {
    MESH_DEBUG_PRINTLN("GPS: Fix Valid. Lat: %f", _location->getLatitude()/1000000.0);
    last_print = millis();
  }
}

// --- Web App Settings Logic ---

int HeltecV3SensorManager::getNumSettings() const {
  return 1; 
}

const char* HeltecV3SensorManager::getSettingName(int i) const {
  return (i == 0) ? "gps" : NULL;
}

const char* HeltecV3SensorManager::getSettingValue(int i) const {
  return (i == 0) ? (gps_active ? "1" : "0") : NULL;
}

bool HeltecV3SensorManager::setSettingValue(const char* name, const char* value) {
  if (strcmp(name, "gps") == 0) {
    bool new_state = (strcmp(value, "1") == 0);
    
    // Only act if state changed
    if (gps_active != new_state) {
        gps_active = new_state;
        
        // ACTUALLY Turn Hardware On/Off
        if (gps_active) {
            MESH_DEBUG_PRINTLN("GPS: Web App requested ON");
            if (_location) _location->begin(); // Output LOW
        } else {
            MESH_DEBUG_PRINTLN("GPS: Web App requested OFF");
            if (_location) _location->stop();  // Output HIGH
        }
    }
    return true;
  }
  return false;
}


//////////////////////////////////////
/*
// all of this added by Gemini on 12 Jan 2026 to enable GPS on Heltec V3

bool HeltecV3SensorManager::begin() {
  // 1. Start Serial1 with Heltec V3 GPS Pins
//  Serial1.begin(9600, SERIAL_8N1, 47, 48); 
  Serial1.begin(9600, SERIAL_8N1, PIN_GPS_TX, PIN_GPS_RX);   // bloody inconsistent naming
  MESH_DEBUG_PRINTLN("GPS: Started Serial1 on defined pins, which are in order:",PIN_GPS_TX,PIN_GPS_RX);
  _location->begin();
  return true;
}

void HeltecV3SensorManager::loop() {
  _location->loop(); // Feed data to NMEA parser
  
  static uint32_t last_print = 0;
  if (millis() - last_print > 5000 && _location->isValid()) {
    MESH_DEBUG_PRINTLN("GPS: Fix Found! Lat: %f", _location->getLatitude()/1000000.0);
    last_print = millis();
  }
}

int HeltecV3SensorManager::getNumSettings() const {
  return 1; // Tell Web App there is 1 setting
}

const char* HeltecV3SensorManager::getSettingName(int i) const {
  return (i == 0) ? "gps" : NULL;
}

const char* HeltecV3SensorManager::getSettingValue(int i) const {
  return (i == 0) ? (gps_active ? "1" : "0") : NULL;
}

bool HeltecV3SensorManager::setSettingValue(const char* name, const char* value) {
  if (strcmp(name, "gps") == 0) {
    gps_active = (strcmp(value, "1") == 0);
    return true;
  }
  return false;
}

*/

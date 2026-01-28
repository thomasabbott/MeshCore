#include "WaveshareBoard.h"

#include <Arduino.h>
#include <Wire.h>

// RP2040 Specific Includes for Clock/Power
#include "pico/stdlib.h"
#include "hardware/vreg.h"
#include "hardware/clocks.h"
#include "hardware/pll.h"
#include "hardware/structs/pll.h"
#include "hardware/structs/clocks.h"

// Pico Standard Pins
#define PIN_SMPS_MODE 23
#define PIN_VBUS_DET  24

void WaveshareBoard::begin() {
  // 1. Force SMPS to PFM (Power Save) Mode
  pinMode(PIN_SMPS_MODE, OUTPUT);
  digitalWrite(PIN_SMPS_MODE, LOW);

  // 2. Detect Power Source
  pinMode(PIN_VBUS_DET, INPUT);
  bool usb_connected = digitalRead(PIN_VBUS_DET);

  if (usb_connected) {
    // --- USB MODE (Performance) ---
    // Must run at 48MHz+ for USB to work
    set_sys_clock_khz(48000, true); 
    
    // Ensure USB Clock is enabled
    clock_configure(clk_usb,
                    0,
                    CLOCKS_CLK_USB_CTRL_AUXSRC_VALUE_CLKSRC_PLL_USB,
                    48 * MHZ,
                    48 * MHZ);
    
    // Ensure ADC runs from USB PLL (Standard 48MHz)
    clock_configure(clk_adc,
                    0,
                    CLOCKS_CLK_ADC_CTRL_AUXSRC_VALUE_CLKSRC_PLL_USB,
                    48 * MHZ,
                    48 * MHZ);

  } else {
    // --- BATTERY MODE (Deep Power Saving) ---
    
    // A. Move ADC to System Clock BEFORE killing USB PLL
    // This keeps the ADC alive, even though USB PLL is about to die.
    // It will run at 18MHz (Slower, but fine for battery reading).
    clock_configure(clk_adc,
                    0,
                    CLOCKS_CLK_ADC_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS,
                    18 * MHZ,
                    18 * MHZ);

    // B. Drop System Clock to 18 MHz
    set_sys_clock_khz(18000, true); 
    
    // C. AGGRESSIVE: Disable USB Clock & PLL
    // The USB PLL consumes ~2-3mA. We kill it here.
    clock_stop(clk_usb);
    pll_deinit(pll_usb);
    
    // D. Lower Core Voltage to 0.95V
    vreg_set_voltage(VREG_VOLTAGE_0_95);
  }

  // 3. Reconfigure Peripheral Clock to match System Clock
  // This ensures UART/SPI timings stay correct at the new 18MHz speed
  clock_configure(clk_peri,
                  0,
                  CLOCKS_CLK_ADC_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS, // Run peripherals from Sys Clock
                  usb_connected ? 48000 * 1000 : 18000 * 1000,
                  usb_connected ? 48000 * 1000 : 18000 * 1000);

  // 4. Standard Init
  startup_reason = BD_STARTUP_NORMAL;

#ifdef P_LORA_TX_LED
  pinMode(P_LORA_TX_LED, OUTPUT);
  digitalWrite(P_LORA_TX_LED, LOW);
#endif

// --- I2C DISABLED ---
// #if defined(PIN_BOARD_SDA) && defined(PIN_BOARD_SCL)
//   Wire.setSDA(PIN_BOARD_SDA);
//   Wire.setSCL(PIN_BOARD_SCL);
// #endif
//   Wire.begin();
// --------------------

  // Note: Serial output will NOT work in Battery Mode now because USB PLL is dead.
  // Plug in USB to debug (logic automatically restores 48MHz + PLL).
  delay(10); 
}

bool WaveshareBoard::startOTAUpdate(const char *id, char reply[]) {
  return false;
}

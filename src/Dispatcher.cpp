#include "Dispatcher.h"
#include "helpers/radiolib/CustomSX1262Wrapper.h" 
#include <Arduino.h>
#include <math.h>

// [DEBUG] Ensure these are defined for RadioLib interactions
#define SX126X_IRQ_HEADER_VALID      0x0010
#define SX126X_IRQ_PREAMBLE_DETECTED 0x0004
#define SX126X_IRQ_RX_DONE           0x0002
#define SX126X_IRQ_ALL               0xFFFF

// HARDCODED OVERRIDES (North America Presets)
#undef LORA_FREQ
#undef LORA_BW
#undef LORA_SF
#undef LORA_CR

#define LORA_FREQ   910.525
#define LORA_BW     62.5
#define LORA_SF     7
#define LORA_CR     5 

namespace mesh {

SX1262* getRawRadio(mesh::Radio* radio_wrapper) {
    return (SX1262*) ((RadioLibWrapper*)radio_wrapper)->getPhysicalLayer();
}

void Dispatcher::begin() {
  n_sent_flood = n_sent_direct = 0;
  n_recv_flood = n_recv_direct = 0;
  _err_flags = 0;
  radio_nonrx_start = _ms->getMillis();

  #ifdef P_LORA_TX_LED
    pinMode(P_LORA_TX_LED, OUTPUT);
    digitalWrite(P_LORA_TX_LED, LOW);
  #endif

  _radio->begin();
  delay(100); 

  SX1262* sx = getRawRadio(_radio);
  if (sx) {
      sx->standby();
      sx->setFrequency(LORA_FREQ);
      sx->setBandwidth(LORA_BW);
      sx->setSpreadingFactor(LORA_SF);
      sx->setCodingRate(LORA_CR);
      sx->setOutputPower(22);
      
      // 1. Initial Interrupt Config
      sx->setDioIrqParams(SX126X_IRQ_HEADER_VALID, SX126X_IRQ_HEADER_VALID, 0, 0);
      
      // 2. Start RX
      sx->startReceive(RADIOLIB_SX126X_RX_TIMEOUT_INF);
      
      #ifdef MESH_DEBUG
        Serial.println(">>> RADIO RE-INIT (NA PRESETS) <<<");
        Serial.print("Freq: "); Serial.print(LORA_FREQ, 3); Serial.println(" MHz");
        Serial.print("BW:   "); Serial.print(LORA_BW, 1);   Serial.println(" kHz");
        Serial.print("SF:   "); Serial.println(LORA_SF);
        Serial.print("CR:   4/"); Serial.println(LORA_CR);
      #endif
  }
}

float Dispatcher::getAirtimeBudgetFactor() const { return 2.0; }
int Dispatcher::calcRxDelay(float score, uint32_t air_time) const { return 0; }
uint32_t Dispatcher::getCADFailRetryDelay() const { return 200; }
uint32_t Dispatcher::getCADFailMaxDuration() const { return 4000; }

void Dispatcher::loop() {
  checkRecv();
}

void Dispatcher::checkRecv() {
  SX1262* sx = getRawRadio(_radio);
  if (!sx) return;

  uint16_t flags = sx->getIrqFlags();

  if (flags & SX126X_IRQ_HEADER_VALID) {
    
    // --- 1. PULSE START ---
    #ifdef P_LORA_TX_LED
      digitalWrite(P_LORA_TX_LED, HIGH);
    #endif

    // Drop DIO1 Low (Falling Edge on Scope)
    sx->clearIrqStatus(SX126X_IRQ_HEADER_VALID);

    #ifdef MESH_DEBUG
      Serial.println("!!! HEADER DETECTED !!!");
    #endif

    // --- 2. HOLD ---
    delay(30);

    // --- 3. PULSE END ---
    #ifdef P_LORA_TX_LED
      digitalWrite(P_LORA_TX_LED, LOW);
    #endif

    // --- 4. RE-ARM ---
    // RadioLib's startReceive() resets DIO1 to "RxDone".
    // We must call startReceive, AND THEN immediately override DIO1 again.
    
    sx->standby();
    sx->startReceive(RADIOLIB_SX126X_RX_TIMEOUT_INF);
    
    // [FIX] RE-APPLY OUR CUSTOM INTERRUPT MAPPING
    // This ensures DIO1 is mapped back to HEADER_VALID for the next packet.
    sx->setDioIrqParams(SX126X_IRQ_HEADER_VALID, SX126X_IRQ_HEADER_VALID, 0, 0);
  }
}

// ----------------------------------------------------------------------
// DISABLED FUNCTIONS
// ----------------------------------------------------------------------
void Dispatcher::processRecvPacket(Packet* pkt) { _mgr->free(pkt); }
void Dispatcher::checkSend() { return; }
Packet* Dispatcher::obtainNewPacket() { return _mgr->allocNew(); }
void Dispatcher::releasePacket(Packet* packet) { _mgr->free(packet); }
void Dispatcher::sendPacket(Packet* packet, uint8_t priority, uint32_t delay_millis) { _mgr->free(packet); }
bool Dispatcher::millisHasNowPassed(unsigned long timestamp) const { return (long)(_ms->getMillis() - timestamp) > 0; }
unsigned long Dispatcher::futureMillis(int millis_from_now) const { return _ms->getMillis() + millis_from_now; }

}
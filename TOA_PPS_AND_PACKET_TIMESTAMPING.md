# TOA: PPS Capture and Packet Timestamping

Brief notes on the time-of-arrival (TOA) feature: GPS PPS capture, DIO1 packet-arrival capture, and MQTT reporting.

## What was done

- **PPS capture**: MCPWM unit 0, CAP0, captures the GPS PPS pin (rising edge) and stores the 80 MHz timer value.
- **DIO1 capture**: Same MCPWM unit, CAP1, captures the LoRa DIO1 pin (packet-arrival rising edge). Same timer as CAP0 so PPS and packet timestamps are comparable.
- **Packet metadata**: Each received packet gets `toa_capture_ticks` set from the last DIO1 capture. This is local metadata only (not on the wire).
- **MQTT packets**: RX packets can include `timestamp_precise` (nanoseconds since the most recent PPS, **corrected for CPU clock error**). Wrap at 1 s is handled. The value is compensated using the measured clock ppm so it represents time vs the true second.
- **Clock error (ppm)**: Successive PPS tick intervals are used to measure how much the 80 MHz timer deviates from nominal. Raw ppm = (delta_ticks - 80e6) / 80; smoothed with EMA 0.8·old + 0.2·new. Starts at 0 ppm until at least two PPS edges. Positive ppm = clock fast.
- **MQTT status**: TOA stats: `toa_enabled` and `toa_clock_ppm` (smoothed clock error, 2 dp) for recordkeeping.

DIO1 interrupt behaviour is unchanged: RadioLib still uses its DIO1 callback for packet handling. MCPWM CAP1 runs in parallel and only records the timer value.

---

## Methods and files

### `variants/heltec_v4/HeltecV4Board.h`
- **`toaBegin()`** – Start MCPWM CAP0 (PPS) and CAP1 (DIO1); call once from setup.
- **`toaGetLastPpsTicks()`** – Last PPS rising-edge timestamp (80 MHz ticks).
- **`toaGetLastDio1CaptureTicks()`** – Last DIO1 rising-edge capture; 0 if none.
- **`getLastToaDio1CaptureTicks()`** – Override of `MainBoard`; returns same as above.
- **`getToaClockErrorPpm()`** – Override of `MainBoard`; returns smoothed clock error in ppm (0 until two PPS edges).

(Removed from this board: `toaPoll`, `toaGetPpsPolledCount`, `toaGetPpsPinLevel`, `toaGetMcpwmCapValue`, `toaDebugPrint`.)

### `variants/heltec_v4/HeltecV4Board.cpp`
- TOA state in a single **`ToaState`** struct: `last_pps_ticks`, `last_dio1_ticks` (updated by MCPWM callbacks).
- **`pps_capture_cb`** / **`dio1_capture_cb`** – Static MCPWM capture callbacks (IRAM_ATTR).

### `src/MeshCore.h`
- **`MainBoard::getLastToaDio1CaptureTicks()`** – Virtual, default `0`; Heltec V4 overrides.

### `src/Packet.h` / `src/Packet.cpp`
- **`Packet::toa_capture_ticks`** – `uint32_t`; 0 = not set. Initialized to 0 in constructor; not serialized in wire format.

### `src/Dispatcher.h` / `src/Dispatcher.cpp`
- **`Radio::getLastToaCaptureTicks()`** – Virtual, default `0`.
- In **`Dispatcher::checkRecv()`**: after building a received packet, `pkt->toa_capture_ticks = _radio->getLastToaCaptureTicks()`.

### `src/helpers/radiolib/RadioLibWrappers.h` / `.cpp`
- **`RadioLibWrapper::getLastToaCaptureTicks()`** – Override; returns `_board->getLastToaDio1CaptureTicks()`.

### `src/helpers/MQTTMessageBuilder.h` / `.cpp`
- **`buildStatusMessage(...)`** – TOA params: **`toa_enabled`**, **`toa_clock_ppm`** (float, omit if NAN).
- **`buildPacketMessage(..., timestamp_precise_ns = -1)`** – When `timestamp_precise_ns >= 0`, adds **`timestamp_precise`** to the packet JSON.
- **`buildPacketJSON(..., timestamp_precise_ns = -1)`** – Forwards `timestamp_precise_ns` to `buildPacketMessage`.
- **`buildPacketJSONFromRaw(..., timestamp_precise_ns = -1)`** – Same.

### `src/helpers/bridges/MQTTBridge.cpp`
- **Status**: Passes **`toa_enabled`** and **`toa_clock_ppm`** (from `board.getToaClockErrorPpm()`) into `buildStatusMessage` (main and analyzer).
- **`publishPacket()`**: For RX when TOA is enabled, computes raw `timestamp_precise_ns` from packet and last PPS ticks (with wrap), then **compensates for clock error**: `timestamp_precise_ns = raw_ns * (1 - ppm/1e6)`, and passes the corrected value into the packet JSON.

### `examples/simple_repeater/main.cpp`
- Removed the `ENABLE_PACKET_TOA` block that called `board.toaPoll()` and `board.toaDebugPrint()`.

---

## Timer and units

- MCPWM capture timer: APB clock, ~80 MHz → 1 tick ≈ 12.5 ns.
- **`timestamp_precise`**: nanoseconds since the last PPS rising edge; formula `delta_ticks * 125 / 10` with wrap (if `delta_ticks < 0`, add 80 000 000).

---

## Scope

TOA and PPS/DIO1 capture are implemented for **Heltec V3** and **Heltec V4** (`ENABLE_PACKET_TOA` + `HELTEC_LORA_V3` or `HELTEC_LORA_V4`). Same ESP32 + SX1262 + pinout for timing; V3 uses `HeltecV3Board.cpp` with the same MCPWM logic as V4. Other boards and variants are unchanged.

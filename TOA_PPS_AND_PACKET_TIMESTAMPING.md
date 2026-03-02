# TOA: PPS Capture and Packet Timestamping

Brief notes on the time-of-arrival (TOA) feature: GPS PPS capture, DIO1 packet-arrival capture, and MQTT reporting.

## What was done

- **PPS capture**: MCPWM unit 0, CAP0, captures the GPS PPS pin (rising edge) and stores the 80 MHz timer value.
- **DIO1 capture**: Same MCPWM unit, CAP1, captures the LoRa DIO1 pin (packet-arrival rising edge). Same timer as CAP0 so PPS and packet timestamps are comparable.
- **Packet metadata**: Each received packet gets `toa_capture_ticks` set from the last DIO1 capture. This is local metadata only (not on the wire).
- **MQTT packets**: RX packets can include `timestamp_precise` (nanoseconds since the most recent PPS). Wrap at 1 s is handled (if delta is negative, add 80e6 ticks then convert to ns).
- **MQTT status**: TOA stats reduced to a single flag: `toa_enabled` (no PPS/poll/debug fields).

DIO1 interrupt behaviour is unchanged: RadioLib still uses its DIO1 callback for packet handling. MCPWM CAP1 runs in parallel and only records the timer value.

---

## Methods and files

### `variants/heltec_v4/HeltecV4Board.h`
- **`toaBegin()`** – Start MCPWM CAP0 (PPS) and CAP1 (DIO1); call once from setup.
- **`toaGetLastPpsTicks()`** – Last PPS rising-edge timestamp (80 MHz ticks).
- **`toaGetLastDio1CaptureTicks()`** – Last DIO1 rising-edge capture; 0 if none.
- **`getLastToaDio1CaptureTicks()`** – Override of `MainBoard`; returns same as above.

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
- **`buildStatusMessage(...)`** – TOA parameter reduced to **`toa_enabled`** (removed PPS/poll/debug params).
- **`buildPacketMessage(..., timestamp_precise_ns = -1)`** – When `timestamp_precise_ns >= 0`, adds **`timestamp_precise`** to the packet JSON.
- **`buildPacketJSON(..., timestamp_precise_ns = -1)`** – Forwards `timestamp_precise_ns` to `buildPacketMessage`.
- **`buildPacketJSONFromRaw(..., timestamp_precise_ns = -1)`** – Same.

### `src/helpers/bridges/MQTTBridge.cpp`
- **Status**: Only passes **`toa_enabled`** into `buildStatusMessage` (both main status and analyzer status).
- **`publishPacket()`**: For RX when TOA is enabled and board is Heltec V4, computes `timestamp_precise_ns` from `packet->toa_capture_ticks` and `board.toaGetLastPpsTicks()` (with 80e6-tick wrap), then passes it into `buildPacketJSON` / `buildPacketJSONFromRaw`.

### `examples/simple_repeater/main.cpp`
- Removed the `ENABLE_PACKET_TOA` block that called `board.toaPoll()` and `board.toaDebugPrint()`.

---

## Timer and units

- MCPWM capture timer: APB clock, ~80 MHz → 1 tick ≈ 12.5 ns.
- **`timestamp_precise`**: nanoseconds since the last PPS rising edge; formula `delta_ticks * 125 / 10` with wrap (if `delta_ticks < 0`, add 80 000 000).

---

## Scope

TOA and PPS/DIO1 capture are implemented only for the **Heltec V4** variant (`ENABLE_PACKET_TOA` + `HELTEC_LORA_V4`). Other boards and variants are unchanged.

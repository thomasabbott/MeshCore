# Project Context: Adding precise TOA to existing Meshcore MQTT Bridge

## Overview
I am working on a fork of MeshCore (originally from agessaman) that implements precise time of arrival (TOA) tagging functionality to this version of the meshcore MQTT repeater bridge, by tagging the packets precise time of arrival using the GPS PPS output

## Key Files for this Feature
- `src/mqtt/MqttBridge.cpp` & `.h`: Core logic for the bridge.
- `src/main.cpp`: Check how the bridge is initialized in the setup loop.
- `src/configuration/Config.h`: Defines the MQTT credentials and server settings.
- `src/mesh/MeshManager.cpp`: How the bridge intercepts mesh packets to send them to MQTT.
- `src/Packet.cpp`: Packet processing
- `variants/heltec_v4/HeltecV4Board.cpp`: board support
- `variants/heltec_v4/HeltecV4Board.h`: board support
- `variants/heltec_v4/target.cpp`: board support
- `variants/heltec_v4/target.h`: board support
- `variants/heltec_v4/platformio.ini`: board definitions
- `src/helpers/MQTTMessageBuilder.cpp`: mqtt
- `src/helpers/radiolib/CustomSX1262Wrapper.h`: radio management
- `src/helpers/radiolib/CustomSX1262.h`: radio management
- `src/helpers/radiolib/RadioLibWrappers.cpp`: radio management
- `src/helpers/radiolib/RadioLibWrappers.h`: radio management

## Hardware Context
- Primary Target: **ESP32** 
- Hardware variant: **HELTEC V4**
- Radio variant: **Semtec SX1262**
- Using **PlatformIO** with `espressif32 @ 6.11.0`.
- The project uses many `#ifdef` blocks for different boards; prioritize the MQTT-enabled definitions.

## My Current Task
- configure one of the on board capture timers to run at full CPU clock speed, about 60 MHz, for 14 nanosecond resolution timestamping.
- configure the timer to timestamp the GPS PPS pin, perhaps writing a new ISR, and store the most recent GPS PPS timestamp value somewhere appropriate for later use. Print it to debug.

## future tasks, not to be started yet but useful context
- configure the same or a different timer to timestamp the radio DIO1 pin which signals that packet reception is complete. Store this timestamp along with the rest of the packet data in the appropriate structure
- add an MQTT field to the current list of fields, reporting the diference between packet timestamp and most recent PPS timestamp. Convert to integer nanoseconds. Add this value to the MQTT JSON, called TOA_NS or something, so packet arrival time can be collated by an external program later.

## Known Constraints
- pin definitions need to be done in variants/heltec_v4/platformio.ini
- the function of the DIO1 pin must not be changed, it must still cause an interrupt that allows processing of the packet. The timestamping feature is secondary.
- if no GPS PPS is received (eg. GPS is off), or the GPS PPS timestamp is more than 2 seconds before the packet arrival timestamp, then the MQTT output should be suppressed.

## Rules for AI Assistant
1. Ensure any new code is compatible with the `arduino_base` defined in `platformio.ini`.
2. Do not suggest libraries that require Arduino Core v3.0+, as we are pinned to v2.0.17 (@6.11.0).
3. Ensure that new code does not affect the working of any other variant or board defined in the codebase. This is to be a surgical addition, if possible. Board-specific changes must stay in the variant/heltec_v4/ folder.
4. Changes to the packet engine are inevitable, they must be wrapped in an IFDEF so they don't attempt to do this timing on other boards or in variants other than the heltec_v4_repeater_observer_mqtt.
5. Create the definition ENABLE_PACKET_TOA=1 or something in platformio.ini and use it to wrap the other code (that's not inside the variant)
6. definitely no need to edit the contents of radiolib or other drivers.
7. There is a large amount of irrelevant code that can be excluded by the .aiexclude file, but if necessary you may find specific files within the excluded folders.

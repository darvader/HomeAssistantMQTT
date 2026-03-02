# HomeAssistantMQTT

ESP8266/ESP32 library for Home Assistant MQTT auto-discovery with EEPROM configuration and optional web UI.

## Features

- Home Assistant MQTT auto-discovery for light and select entities
- Configurable device info, modes/effects, and topic prefix
- EEPROM-based MQTT broker configuration
- Optional web UI component (`HAMQTTConfig`) for browser-based MQTT setup

## Usage

```cpp
#include <HomeAssistantMQTT.h>

HomeAssistantMQTT ha;

const char* modeNames[] = {"Mode A", "Mode B", "Mode C"};
const int modeValues[] = {1, 2, 3};

void setup() {
    ha.setDevice("My Device", "Maker", "Model X");
    ha.setDevicePrefix("mydevice");
    ha.setModes(modeNames, modeValues, 3);

    ha.onState([](bool on) { /* ... */ });
    ha.onBrightness([](uint8_t b) { /* ... */ });
    ha.onMode([](int m) { /* ... */ });

    ha.setup();
}

void loop() {
    ha.loop();
}
```

## Dependencies

- [PubSubClient](https://github.com/knolleary/pubsubclient) ^2.8

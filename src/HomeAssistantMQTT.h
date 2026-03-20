#ifndef HOME_ASSISTANT_MQTT_H
#define HOME_ASSISTANT_MQTT_H

#include <Arduino.h>
#include <PubSubClient.h>
#ifdef ESP8266
#include <ESP8266WiFi.h>
#else
#include <WiFi.h>
#endif
#include <EEPROM.h>

#ifndef MQTT_BUFFER_SIZE
#define MQTT_BUFFER_SIZE 512
#endif

class HomeAssistantMQTT {
public:
    // Callback types for controlling the device
    typedef void (*StateCallback)(bool on);
    typedef void (*BrightnessCallback)(uint8_t brightness);
    typedef void (*ModeCallback)(int mode);
    typedef void (*NumberCallback)(float value);

    HomeAssistantMQTT();

    // Device configuration (call before setup())
    void setDevice(const char* name, const char* manufacturer, const char* model);
    void setDevicePrefix(const char* prefix);
    void setModes(const char** names, const int* values, int count);
    void setNumber(const char* name, const char* unit, float min, float max, float step);

    void setup();
    void loop();
    bool isConnected();

    // Set callbacks
    void onState(StateCallback cb) { stateCb = cb; }
    void onBrightness(BrightnessCallback cb) { brightnessCb = cb; }
    void onMode(ModeCallback cb) { modeCb = cb; }
    void onNumber(NumberCallback cb) { numberCb = cb; }

    // Sensor configuration (call before setup(), max 4 sensors)
    void addSensor(const char* name, const char* unit, const char* deviceClass);

    // Publish current state to Home Assistant
    void publishState(bool on, uint8_t brightness, const char* effect);
    void publishNumberState(float value);
    void publishSensorState(int index, float value);

    // MQTT broker configuration
    void setBroker(const char* host, uint16_t port = 1883);
    void setCredentials(const char* user, const char* password);

    // EEPROM storage for MQTT config
    void saveMQTTConfig(String host, uint16_t port, String user, String password);
    bool loadMQTTConfig(String& host, uint16_t& port, String& user, String& password);

    // EEPROM layout for MQTT config (starting at address 600)
    static const int eepromMQTTBaseAddr = 600;
    static const int maxHostLen = 40;
    static const int maxUserLen = 32;
    static const int maxMQTTPassLen = 32;
    // Total: 1 (configured flag) + 40 (host) + 2 (port) + 32 (user) + 32 (pass) = 107 bytes

private:
    WiFiClient wifiClient;
    PubSubClient mqttClient;

    StateCallback stateCb = nullptr;
    BrightnessCallback brightnessCb = nullptr;
    ModeCallback modeCb = nullptr;
    NumberCallback numberCb = nullptr;

    char mqttHost[41] = "";
    uint16_t mqttPort = 1883;
    char mqttUser[33] = "";
    char mqttPass[33] = "";
    bool configured = false;

    unsigned long lastReconnectAttempt = 0;

    String deviceId;

    // Configurable device info
    const char* deviceName = "LED Matrix";
    const char* deviceManufacturer = "DIY";
    const char* deviceModel = "ESP LED Matrix";
    const char* devicePrefix = "ledmatrix";

    // Configurable modes
    const char** modeNames = nullptr;
    const int* modeValues = nullptr;
    int numModes = 0;

    // Configurable number entity
    const char* numberName = nullptr;
    const char* numberUnit = "";
    float numberMin = 0;
    float numberMax = 100;
    float numberStep = 1;

    // Sensor entities (max 4)
    static const int MAX_SENSORS = 4;
    int numSensors = 0;
    const char* sensorNames[4] = {};
    const char* sensorUnits[4] = {};
    const char* sensorDeviceClasses[4] = {};

    void mqttCallback(char* topic, byte* payload, unsigned int length);
    bool reconnect();
    void publishDiscovery();
    void publishSelectDiscovery();
    void publishNumberDiscovery();
    void publishSensorDiscovery();
};

#endif

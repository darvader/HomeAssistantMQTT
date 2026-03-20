#include "HomeAssistantMQTT.h"
#include <functional>

HomeAssistantMQTT::HomeAssistantMQTT() : mqttClient(wifiClient) {
}

void HomeAssistantMQTT::setDevice(const char* name, const char* manufacturer, const char* model) {
    deviceName = name;
    deviceManufacturer = manufacturer;
    deviceModel = model;
}

void HomeAssistantMQTT::setDevicePrefix(const char* prefix) {
    devicePrefix = prefix;
}

void HomeAssistantMQTT::setModes(const char** names, const int* values, int count) {
    modeNames = names;
    modeValues = values;
    numModes = count;
}

void HomeAssistantMQTT::addSensor(const char* name, const char* unit, const char* deviceClass) {
    if (numSensors >= MAX_SENSORS) return;
    sensorNames[numSensors] = name;
    sensorUnits[numSensors] = unit;
    sensorDeviceClasses[numSensors] = deviceClass;
    numSensors++;
}

void HomeAssistantMQTT::setNumber(const char* name, const char* unit, float min, float max, float step) {
    numberName = name;
    numberUnit = unit;
    numberMin = min;
    numberMax = max;
    numberStep = step;
}

void HomeAssistantMQTT::setup() {
    // Generate unique device ID from MAC address (deferred to setup so WiFi is ready)
    uint8_t mac[6];
    WiFi.macAddress(mac);
    char macStr[13];
    snprintf(macStr, sizeof(macStr), "%02X%02X%02X%02X%02X%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    deviceId = String(devicePrefix) + "_" + macStr;

    String host;
    uint16_t port;
    String user, pass;

    if (loadMQTTConfig(host, port, user, pass)) {
        host.toCharArray(mqttHost, sizeof(mqttHost));
        mqttPort = port;
        user.toCharArray(mqttUser, sizeof(mqttUser));
        pass.toCharArray(mqttPass, sizeof(mqttPass));
        configured = true;

        mqttClient.setServer(mqttHost, mqttPort);
        mqttClient.setBufferSize(MQTT_BUFFER_SIZE);
        mqttClient.setCallback([this](char* topic, byte* payload, unsigned int length) {
            this->mqttCallback(topic, payload, length);
        });

        Serial.print("MQTT configured for: ");
        Serial.print(mqttHost);
        Serial.print(":");
        Serial.println(mqttPort);
    } else {
        Serial.println("MQTT not configured");
    }
}

void HomeAssistantMQTT::loop() {
    if (!configured) return;

    if (!mqttClient.connected()) {
        unsigned long now = millis();
        if (now - lastReconnectAttempt > 5000) {
            lastReconnectAttempt = now;
            if (reconnect()) {
                lastReconnectAttempt = 0;
            }
        }
    } else {
        mqttClient.loop();
    }
}

bool HomeAssistantMQTT::isConnected() {
    return mqttClient.connected();
}

bool HomeAssistantMQTT::reconnect() {
    String clientId = deviceId;

    Serial.print("Attempting MQTT connection to ");
    Serial.print(mqttHost);
    Serial.print("...");

    bool connected = false;
    if (strlen(mqttUser) > 0) {
        connected = mqttClient.connect(clientId.c_str(), mqttUser, mqttPass);
    } else {
        connected = mqttClient.connect(clientId.c_str());
    }

    if (connected) {
        Serial.println("connected");

        // Publish Home Assistant discovery messages
        publishDiscovery();
        publishSelectDiscovery();
        publishNumberDiscovery();
        publishSensorDiscovery();

        // Subscribe to command topics
        String prefix = String(devicePrefix);
        String cmdTopic = prefix + "/" + deviceId + "/light/set";
        mqttClient.subscribe(cmdTopic.c_str());

        String modeTopic = prefix + "/" + deviceId + "/mode/set";
        mqttClient.subscribe(modeTopic.c_str());

        if (numberName != nullptr) {
            String numberTopic = prefix + "/" + deviceId + "/number/set";
            mqttClient.subscribe(numberTopic.c_str());
        }

        Serial.print("Subscribed to: ");
        Serial.println(cmdTopic);

        return true;
    } else {
        Serial.print("failed, rc=");
        Serial.println(mqttClient.state());
        return false;
    }
}

void HomeAssistantMQTT::publishDiscovery() {
    // Build discovery topic
    String discoveryTopic = "homeassistant/light/" + deviceId + "/config";
    String prefix = String(devicePrefix);

    // Build discovery payload
    String payload = "{";
    payload += "\"name\":\"" + String(deviceName) + "\",";
    payload += "\"unique_id\":\"" + deviceId + "_light\",";
    payload += "\"command_topic\":\"" + prefix + "/" + deviceId + "/light/set\",";
    payload += "\"state_topic\":\"" + prefix + "/" + deviceId + "/light/state\",";
    payload += "\"schema\":\"json\",";

    if (brightnessCb) {
        payload += "\"brightness\":true,";
        payload += "\"brightness_scale\":255,";
    }

    if (numModes > 0 && modeNames != nullptr) {
        payload += "\"effect\":true,";

        String effectList = "[";
        for (int i = 0; i < numModes; i++) {
            if (i > 0) effectList += ",";
            effectList += "\"" + String(modeNames[i]) + "\"";
        }
        effectList += "]";
        payload += "\"effect_list\":" + effectList + ",";
    }

    payload += "\"device\":{";
    payload += "\"identifiers\":[\"" + deviceId + "\"],";
    payload += "\"name\":\"" + String(deviceName) + "\",";
    payload += "\"manufacturer\":\"" + String(deviceManufacturer) + "\",";
    payload += "\"model\":\"" + String(deviceModel) + "\"";
    payload += "}";
    payload += "}";

    mqttClient.publish(discoveryTopic.c_str(), payload.c_str(), true);
    Serial.println("Published light discovery");
}

void HomeAssistantMQTT::publishSelectDiscovery() {
    if (numModes == 0 || modeNames == nullptr) return;

    // Also publish a select entity for direct mode selection
    String discoveryTopic = "homeassistant/select/" + deviceId + "_mode/config";
    String prefix = String(devicePrefix);

    // Build options list
    String options = "[";
    for (int i = 0; i < numModes; i++) {
        if (i > 0) options += ",";
        options += "\"" + String(modeNames[i]) + "\"";
    }
    options += "]";

    String payload = "{";
    payload += "\"name\":\"" + String(deviceName) + " Mode\",";
    payload += "\"unique_id\":\"" + deviceId + "_mode\",";
    payload += "\"command_topic\":\"" + prefix + "/" + deviceId + "/mode/set\",";
    payload += "\"state_topic\":\"" + prefix + "/" + deviceId + "/mode/state\",";
    payload += "\"options\":" + options + ",";
    payload += "\"device\":{";
    payload += "\"identifiers\":[\"" + deviceId + "\"]";
    payload += "}";
    payload += "}";

    mqttClient.publish(discoveryTopic.c_str(), payload.c_str(), true);
    Serial.println("Published mode select discovery");
}

void HomeAssistantMQTT::publishNumberDiscovery() {
    if (numberName == nullptr) return;

    String discoveryTopic = "homeassistant/number/" + deviceId + "_number/config";
    String prefix = String(devicePrefix);

    String payload = "{";
    payload += "\"name\":\"" + String(deviceName) + " " + String(numberName) + "\",";
    payload += "\"unique_id\":\"" + deviceId + "_number\",";
    payload += "\"command_topic\":\"" + prefix + "/" + deviceId + "/number/set\",";
    payload += "\"state_topic\":\"" + prefix + "/" + deviceId + "/number/state\",";
    payload += "\"min\":" + String(numberMin, 0) + ",";
    payload += "\"max\":" + String(numberMax, 0) + ",";
    payload += "\"step\":" + String(numberStep, 0) + ",";
    if (strlen(numberUnit) > 0) {
        payload += "\"unit_of_measurement\":\"" + String(numberUnit) + "\",";
    }
    payload += "\"device\":{";
    payload += "\"identifiers\":[\"" + deviceId + "\"]";
    payload += "}";
    payload += "}";

    mqttClient.publish(discoveryTopic.c_str(), payload.c_str(), true);
    Serial.println("Published number discovery");
}

void HomeAssistantMQTT::publishNumberState(float value) {
    if (!mqttClient.connected()) return;

    String prefix = String(devicePrefix);
    String stateTopic = prefix + "/" + deviceId + "/number/state";
    mqttClient.publish(stateTopic.c_str(), String(value, 0).c_str(), true);
}

void HomeAssistantMQTT::mqttCallback(char* topic, byte* payload, unsigned int length) {
    // Null-terminate the payload
    char message[length + 1];
    memcpy(message, payload, length);
    message[length] = '\0';

    Serial.print("MQTT message [");
    Serial.print(topic);
    Serial.print("]: ");
    Serial.println(message);

    String topicStr = String(topic);

    // Handle light commands (JSON schema)
    if (topicStr.endsWith("/light/set")) {
        // Simple JSON parsing (avoid heavy ArduinoJson dependency)
        String msg = String(message);

        // Check for state
        if (msg.indexOf("\"state\":\"ON\"") >= 0 || msg.indexOf("\"state\": \"ON\"") >= 0) {
            if (stateCb) stateCb(true);
        } else if (msg.indexOf("\"state\":\"OFF\"") >= 0 || msg.indexOf("\"state\": \"OFF\"") >= 0) {
            if (stateCb) stateCb(false);
        }

        // Check for brightness
        int briIdx = msg.indexOf("\"brightness\":");
        if (briIdx >= 0) {
            int start = briIdx + 13;
            while (start < (int)msg.length() && (msg[start] == ' ' || msg[start] == ':')) start++;
            int end = start;
            while (end < (int)msg.length() && isdigit(msg[end])) end++;
            if (end > start) {
                int brightness = msg.substring(start, end).toInt();
                if (brightnessCb) brightnessCb((uint8_t)brightness);
            }
        }

        // Check for effect
        int effIdx = msg.indexOf("\"effect\":\"");
        if (effIdx >= 0) {
            int start = effIdx + 10;
            int end = msg.indexOf("\"", start);
            if (end > start) {
                String effect = msg.substring(start, end);
                // Find matching mode
                for (int i = 0; i < numModes; i++) {
                    if (effect == modeNames[i]) {
                        if (modeCb) modeCb(modeValues[i]);
                        break;
                    }
                }
            }
        }
    }

    // Handle number input
    if (topicStr.endsWith("/number/set")) {
        float value = String(message).toFloat();
        if (numberCb) numberCb(value);
        // Publish number state back
        String prefix = String(devicePrefix);
        String stateTopic = prefix + "/" + deviceId + "/number/state";
        mqttClient.publish(stateTopic.c_str(), message, true);
    }

    // Handle direct mode selection
    if (topicStr.endsWith("/mode/set")) {
        String modeName = String(message);
        String prefix = String(devicePrefix);
        for (int i = 0; i < numModes; i++) {
            if (modeName == modeNames[i]) {
                if (modeCb) modeCb(modeValues[i]);
                // Publish mode state
                String stateTopic = prefix + "/" + deviceId + "/mode/state";
                mqttClient.publish(stateTopic.c_str(), modeNames[i], true);
                break;
            }
        }
    }
}

void HomeAssistantMQTT::publishState(bool on, uint8_t brightness, const char* effect) {
    if (!mqttClient.connected()) return;

    String prefix = String(devicePrefix);
    String stateTopic = prefix + "/" + deviceId + "/light/state";

    String payload = "{";
    payload += "\"state\":\"" + String(on ? "ON" : "OFF") + "\",";
    payload += "\"brightness\":" + String(brightness);
    if (effect && strlen(effect) > 0) {
        payload += ",\"effect\":\"" + String(effect) + "\"";
    }
    payload += "}";

    mqttClient.publish(stateTopic.c_str(), payload.c_str(), true);

    // Also publish mode state
    if (effect && strlen(effect) > 0) {
        String modeTopic = prefix + "/" + deviceId + "/mode/state";
        mqttClient.publish(modeTopic.c_str(), effect, true);
    }
}

void HomeAssistantMQTT::setBroker(const char* host, uint16_t port) {
    strncpy(mqttHost, host, sizeof(mqttHost) - 1);
    mqttHost[sizeof(mqttHost) - 1] = '\0';
    mqttPort = port;
    configured = true;
    mqttClient.setServer(mqttHost, mqttPort);
}

void HomeAssistantMQTT::setCredentials(const char* user, const char* password) {
    strncpy(mqttUser, user, sizeof(mqttUser) - 1);
    mqttUser[sizeof(mqttUser) - 1] = '\0';
    strncpy(mqttPass, password, sizeof(mqttPass) - 1);
    mqttPass[sizeof(mqttPass) - 1] = '\0';
}

void HomeAssistantMQTT::saveMQTTConfig(String host, uint16_t port, String user, String password) {
    int addr = eepromMQTTBaseAddr;

    // Write configured flag
    EEPROM.write(addr++, 1);

    // Write host
    for (int i = 0; i < maxHostLen; i++) {
        EEPROM.write(addr++, i < (int)host.length() ? host[i] : 0);
    }

    // Write port (2 bytes, little endian)
    EEPROM.write(addr++, port & 0xFF);
    EEPROM.write(addr++, (port >> 8) & 0xFF);

    // Write user
    for (int i = 0; i < maxUserLen; i++) {
        EEPROM.write(addr++, i < (int)user.length() ? user[i] : 0);
    }

    // Write password
    for (int i = 0; i < maxMQTTPassLen; i++) {
        EEPROM.write(addr++, i < (int)password.length() ? password[i] : 0);
    }

    EEPROM.commit();
}

bool HomeAssistantMQTT::loadMQTTConfig(String& host, uint16_t& port, String& user, String& password) {
    int addr = eepromMQTTBaseAddr;

    // Check configured flag
    if (EEPROM.read(addr++) != 1) {
        return false;
    }

    // Read host
    host = "";
    for (int i = 0; i < maxHostLen; i++) {
        char c = EEPROM.read(addr++);
        if (c == 0) {
            addr += (maxHostLen - i - 1);
            break;
        }
        host += c;
    }

    // Read port
    port = EEPROM.read(addr++);
    port |= EEPROM.read(addr++) << 8;

    // Read user
    user = "";
    for (int i = 0; i < maxUserLen; i++) {
        char c = EEPROM.read(addr++);
        if (c == 0) {
            addr += (maxUserLen - i - 1);
            break;
        }
        user += c;
    }

    // Read password
    password = "";
    for (int i = 0; i < maxMQTTPassLen; i++) {
        char c = EEPROM.read(addr++);
        if (c == 0) break;
        password += c;
    }

    return host.length() > 0;
}

void HomeAssistantMQTT::publishSensorDiscovery() {
    if (numSensors == 0) return;

    String prefix = String(devicePrefix);

    for (int i = 0; i < numSensors; i++) {
        String sensorId = deviceId + "_sensor_" + String(i);
        String discoveryTopic = "homeassistant/sensor/" + sensorId + "/config";

        String payload = "{";
        payload += "\"name\":\"" + String(deviceName) + " " + String(sensorNames[i]) + "\",";
        payload += "\"unique_id\":\"" + sensorId + "\",";
        payload += "\"state_topic\":\"" + prefix + "/" + deviceId + "/sensor/" + String(i) + "/state\",";
        payload += "\"unit_of_measurement\":\"" + String(sensorUnits[i]) + "\",";
        payload += "\"device_class\":\"" + String(sensorDeviceClasses[i]) + "\",";
        payload += "\"state_class\":\"measurement\",";
        payload += "\"device\":{";
        payload += "\"identifiers\":[\"" + deviceId + "\"]";
        payload += "}";
        payload += "}";

        mqttClient.publish(discoveryTopic.c_str(), payload.c_str(), true);
        Serial.print("Published sensor discovery: ");
        Serial.println(sensorNames[i]);
    }
}

void HomeAssistantMQTT::publishSensorState(int index, float value) {
    if (!mqttClient.connected() || index < 0 || index >= numSensors) return;

    String prefix = String(devicePrefix);
    String stateTopic = prefix + "/" + deviceId + "/sensor/" + String(index) + "/state";
    mqttClient.publish(stateTopic.c_str(), String(value, 1).c_str(), true);
}

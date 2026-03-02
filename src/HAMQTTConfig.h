#ifndef HA_MQTT_CONFIG_H
#define HA_MQTT_CONFIG_H

#include <Arduino.h>
#include "HomeAssistantMQTT.h"

#ifdef ESP8266
#include <ESP8266WebServer.h>
#else
#include <WebServer.h>
#endif

class HAMQTTConfig {
public:
    HAMQTTConfig();

    // Attach MQTT config routes to an existing web server
    template<typename ServerType>
    void attachToServer(ServerType& server, const char* savePath = "/savemqtt");

    // Generate the HTML form for MQTT configuration
    String generateMQTTHTML();

    // Reconnect flag management
    bool isMqttReconnectNeeded() const { return mqttReconnectNeeded; }
    void clearMqttReconnectFlag() { mqttReconnectNeeded = false; }

    // Set whether we're in config mode (existing connection) vs setup mode (AP)
    void setConfigMode(bool mode) { configMode = mode; }

private:
    bool mqttReconnectNeeded = false;
    bool configMode = false;

    template<typename ServerType>
    void handleSaveMQTT(ServerType& server);
};

// Template implementations must be in the header

template<typename ServerType>
void HAMQTTConfig::attachToServer(ServerType& server, const char* savePath) {
    server.on(savePath, HTTP_POST, [this, &server]() {
        this->handleSaveMQTT(server);
    });
}

template<typename ServerType>
void HAMQTTConfig::handleSaveMQTT(ServerType& server) {
    String mqttHost = server.arg("mqtt_host");
    String mqttPortStr = server.arg("mqtt_port");
    String mqttUser = server.arg("mqtt_user");
    String mqttPass = server.arg("mqtt_pass");

    if (mqttHost.length() == 0) {
        server.send(400, "text/html", "<h1>Error: MQTT host required</h1><a href='/'>Back</a>");
        return;
    }

    uint16_t mqttPort = mqttPortStr.length() > 0 ? mqttPortStr.toInt() : 1883;
    if (mqttPort == 0) mqttPort = 1883;

    HomeAssistantMQTT ha;
    ha.saveMQTTConfig(mqttHost, mqttPort, mqttUser, mqttPass);

    if (configMode) {
        mqttReconnectNeeded = true;
        String html = "<!DOCTYPE html><html><head><title>MQTT Saved</title>";
        html += "<style>body{font-family:sans-serif;margin:20px;}</style></head><body>";
        html += "<h1>MQTT configuration saved!</h1>";
        html += "<p>The MQTT connection will be re-established shortly.</p>";
        html += "<a href='/'>Back to configuration</a>";
        html += "</body></html>";
        server.send(200, "text/html", html);
    } else {
        server.send(200, "text/html", "<h1>MQTT configuration saved! Rebooting...</h1>");
        delay(2000);
        ESP.restart();
    }
}

#endif

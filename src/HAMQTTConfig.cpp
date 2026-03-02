#include "HAMQTTConfig.h"

HAMQTTConfig::HAMQTTConfig() {
}

String HAMQTTConfig::generateMQTTHTML() {
    String html;

    html += "<h2>Home Assistant MQTT Configuration</h2>";

    // Check if MQTT is already configured
    HomeAssistantMQTT ha;
    String mqttHost;
    uint16_t mqttPort;
    String mqttUser, mqttPass;
    bool mqttConfigured = ha.loadMQTTConfig(mqttHost, mqttPort, mqttUser, mqttPass);

    if (mqttConfigured) {
        html += "<p><strong>Current MQTT Broker:</strong> " + mqttHost + ":" + String(mqttPort);
        if (mqttUser.length() > 0) {
            html += " (User: " + mqttUser + ")";
        }
        html += "</p>";
    }

    html += "<form action='/savemqtt' method='POST'>";
    html += "MQTT Broker Host/IP: <input type='text' name='mqtt_host' value='" +
            (mqttConfigured ? mqttHost : String("")) + "' placeholder='192.168.1.100' required><br>";
    html += "MQTT Port: <input type='number' name='mqtt_port' value='" +
            String(mqttConfigured ? mqttPort : 1883) + "' placeholder='1883'><br>";
    html += "MQTT Username: <input type='text' name='mqtt_user' value='" +
            (mqttConfigured ? mqttUser : String("")) + "' placeholder='(optional)'><br>";
    html += "MQTT Password: <input type='password' name='mqtt_pass' placeholder='(optional)'><br>";
    html += "<input type='submit' value='Save MQTT Config' class='btn btn-add'>";
    html += "</form>";

    html += "<p><small><strong>Note:</strong> Configure your Home Assistant MQTT broker first. ";
    html += "The device will appear as a 'Light' entity with multiple effects/modes.</small></p>";

    return html;
}

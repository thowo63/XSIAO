#pragma once

void setupMqtt();
void handleMqtt();
bool mqttIsConnected();
void mqttPublishTextOut(const char* text);
void publishMicLevel();
void publishStatus();
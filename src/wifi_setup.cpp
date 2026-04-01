#include "wifi_setup.h"
#include <WiFi.h>
#include <WiFiManager.h>
#include "app_config.h"
#include "display_ui.h"

static WiFiManager wm;

static void configModeCallback(WiFiManager *myWiFiManager) {
  drawLines(
    "Setup-Modus",
    String("SSID: ") + myWiFiManager->getConfigPortalSSID(),
    "Open: 192.168.4.1",
    "WLAN auswaehlen"
  );
}

void setupWifi() {
  WiFi.mode(WIFI_STA);

  wm.setDebugOutput(true);
  wm.setAPCallback(configModeCallback);
  wm.setConfigPortalTimeout(180);
  wm.setHostname(DEVICE_NAME);

  IPAddress apIP(192, 168, 4, 1);
  IPAddress gateway(192, 168, 4, 1);
  IPAddress subnet(255, 255, 255, 0);
  wm.setAPStaticIPConfig(apIP, gateway, subnet);

  drawLines("WLAN verbinden...", "oder Setup-AP...");

  bool ok = wm.autoConnect("Xiaozhi-Setup");
  if (!ok) {
    drawLines("Kein WLAN", "Timeout", "Neustart...");
    delay(3000);
    ESP.restart();
  }
}
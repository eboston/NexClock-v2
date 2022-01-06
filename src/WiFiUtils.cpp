#include <Arduino.h>
#include <WiFi.h>

#include "WiFiUtils.h"

bool bWiFiConnected = false;

// Convert the WiFi (error) response to a string we can understand
const char *wl_status_to_string(wl_status_t status)
{
	switch (status)
	{
	case WL_NO_SHIELD:
		return "WL_NO_SHIELD";
	case WL_IDLE_STATUS:
		return "WL_IDLE_STATUS";
	case WL_NO_SSID_AVAIL:
		return "WL_NO_SSID_AVAIL";
	case WL_SCAN_COMPLETED:
		return "WL_SCAN_COMPLETED";
	case WL_CONNECTED:
		return "WL_CONNECTED";
	case WL_CONNECT_FAILED:
		return "WL_CONNECT_FAILED";
	case WL_CONNECTION_LOST:
		return "WL_CONNECTION_LOST";
	case WL_DISCONNECTED:
		return "WL_DISCONNECTED";
	default:
		return "UNKNOWN";
	}
}


// Connect to WiFi
bool connectToWiFi()
{
    log_w("Start WiFi.");
//	log_w("Connecting to SSID: %s\n", ClockSettings.szSSID);
	log_w("Connecting to SSID: %s", "Oxford-HASP");

	// Ensure we disconnect WiFi first to stop connection problems
	if (WiFi.status() == WL_CONNECTED)
	{
		log_w("Disconnecting from WiFi");
		WiFi.disconnect(false, true);
	}
	bWiFiConnected = true;

	// We want to be a client not a server
	WiFi.mode(WIFI_STA);

	// Don't store the SSID and password
	WiFi.persistent(false);

	// Don't allow WiFi sleep
	WiFi.setSleep(false);

	WiFi.setAutoReconnect(false);

	// Lock down the WiFi stuff - not mormally necessary unless you need a static IP in AP mode
	// IPAddress local_IP(192, 168, 1, 102);
	// IPAddress gateway(192, 168, 1, 254);
	// IPAddress subnet(255, 255, 255, 0);
	// IPAddress DNS1(8, 8, 8, 8);
	// IPAddress DNS2(8, 8, 4, 4);
	// WiFi.config(local_IP, gateway, subnet, DNS1, DNS2);

	//Connect to the required WiFi
//	WiFi.begin(ClockSettings.szSSID, ClockSettings.szPWD);
	WiFi.begin("Oxford-HASP", "HermanHASP");

	// Give things a chance to settle, avoid problems
	delay(2000);

	uint8_t wifiStatus = WiFi.waitForConnectResult();

	// Successful connection?
	if ((wl_status_t)wifiStatus != WL_CONNECTED)
	{
		log_w("WiFi Status: %s, exiting", wl_status_to_string((wl_status_t)wifiStatus));
		return false;
	}

	WiFi.setTxPower(WIFI_POWER_MINUS_1dBm);

	Serial.printf("WiFi connected with (local) IP address of: %s\n", WiFi.localIP().toString().c_str());
	bWiFiConnected = true;

  return true;
}

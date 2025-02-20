#include "DEV_Config.h"
#include "EPD.h"
#include "GUI_Paint.h"
#include <WiFi.h>
#include <ESP32Ping.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <HTTPClient.h>
#include <Update.h>
#include <ArduinoJson.h>

#define FIRMWARE_URL1 "http://192.168.2.33/firmware/firmware_metadata.json"  // URL to JSON metadata file
String current_version = "1.0.2";

// Deep sleep settings
#define uS_TO_S_FACTOR 1000000   /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP 1785       // sleep 29.5 minutes
#define TIME_TO_SLEEP_NoWIFI 30  // sleep only 2 minutes if no wifi

// Network credentials
const char *ssid = "ssid";
const char *password = "password";
const char *MyHostName = "ESP32ServerMonitor";

// NTP Client settings
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

// Server addresses
IPAddress mediaHost(192, 168, 2, 2);
IPAddress fireHost(192, 168, 2, 102);
IPAddress unraidHost(192, 168, 2, 101);
IPAddress requestHost(192, 168, 2, 17);
IPAddress vpnHost(192, 168, 2, 99);
IPAddress vmHost(192, 168, 2, 250);

// Server statuses
bool pingMedia, pingFire, pingUnraid, pingVPN, pingVM, pingRequest;

void setup() {
  delay(500);
  Serial.begin(115200);
  wifiInit();
  delay(5000);
  checkForFirmwareUpdate();
  analogRead(33);  // Battery voltage read
  delay(500);
  int batteryLevel = map(analogRead(33), 3412.0f, 4095.0f, 0, 100);
  String batLevel = String(batteryLevel) + "%";
  ePaperInit();

  if (WiFi.status() == WL_CONNECTED) {
    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
    pingServers();
    drawMainServerStatus(batteryLevel, batLevel);
  } else {
    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP_NoWIFI * uS_TO_S_FACTOR);
    drawBatteryAndNoWifi(batteryLevel, batLevel);
  }

  // Sleep after display
  Serial.println("Going to sleep now");
  delay(500);
  Serial.flush();
  esp_deep_sleep_start();
}


//function refactoring
void wifiInit() {
  WiFi.setHostname(MyHostName);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  //while (WiFi.status() != WL_CONNECTED) {
  // delay(500);
  // Serial.print(".");
  //}
}

void ePaperInit() {
  if (DEV_Module_Init() != 0) {
    while (1)
      ;
  }
  EPD_2IN9B_V4_Init();
  EPD_2IN9B_V4_Clear();
  DEV_Delay_ms(500);
}


void drawBatteryAndNoWifi(int batteryLevel, String batLevel) {
  UBYTE *BlackImage, *RYImage;
  UWORD Imagesize = ((EPD_2IN9B_V4_WIDTH % 8 == 0) ? (EPD_2IN9B_V4_WIDTH / 8) : (EPD_2IN9B_V4_WIDTH / 8 + 1)) * EPD_2IN9B_V4_HEIGHT;
  allocateMemoryForImages(&BlackImage, &RYImage, Imagesize);

  Paint_NewImage(BlackImage, EPD_2IN9B_V4_WIDTH, EPD_2IN9B_V4_HEIGHT, 270, WHITE);
  Paint_NewImage(RYImage, EPD_2IN9B_V4_WIDTH, EPD_2IN9B_V4_HEIGHT, 270, WHITE);

  // *----------- Draw black image ------------*
  Paint_SelectImage(BlackImage);
  Paint_Clear(WHITE);
  drawBatteryBlack(batteryLevel, batLevel);
  Paint_DrawRectangle(7, 17, 290, 113, BLACK, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);

  // *----------- Draw red image ------------*
  Paint_SelectImage(RYImage);
  Paint_Clear(WHITE);

  String titleIPString = "Not connected";
  Paint_DrawString_EN(0, 0, titleIPString.c_str(), &Font12, BLACK, WHITE);

  Paint_DrawString_EN(10, 50, "WiFi connection is not", &Font16, BLACK, WHITE);
  Paint_DrawString_EN(10, 65, "available", &Font16, BLACK, WHITE);
  drawBatteryRed(batteryLevel, batLevel);
  EPD_2IN9B_V4_Display_Base(BlackImage, RYImage);
  //EPD_2IN9B_V4_Display(BlackImage, RYImage);
  DEV_Delay_ms(2000);
  EPD_2IN9B_V4_Sleep();
  free(BlackImage);
  free(RYImage);
  DEV_Delay_ms(2000);  //important, at least 2s
}

void allocateMemoryForImages(UBYTE **BlackImage, UBYTE **RYImage, UWORD Imagesize) {
  *BlackImage = (UBYTE *)malloc(Imagesize);
  *RYImage = (UBYTE *)malloc(Imagesize);
  if (*BlackImage == NULL || *RYImage == NULL) {
    Serial.println("Memory allocation failed");
    while (1)
      ;
  }
}

void drawMainServerStatus(int batteryLevel, String batLevel) {
  UBYTE *BlackImage, *RYImage;
  UWORD Imagesize = ((EPD_2IN9B_V4_WIDTH % 8 == 0) ? (EPD_2IN9B_V4_WIDTH / 8) : (EPD_2IN9B_V4_WIDTH / 8 + 1)) * EPD_2IN9B_V4_HEIGHT;
  allocateMemoryForImages(&BlackImage, &RYImage, Imagesize);
  const char *serverText[] = {
    pingFire ? "pfsense is ONLINE" : "pfsense is OFFLINE",
    pingUnraid ? "UNRAID is ONLINE" : "UNRAID is OFFLINE",
    pingMedia ? "Media is ONLINE" : "OASIS Media is OFFLINE",
    pingRequest ? "Request is ONLINE" : "OASIS Request is OFFLINE",
    pingVPN ? "VPN Servers are ONLINE" : "VPN Servers are OFFLINE",
    pingVM ? "VMs are ONLINE" : "VMs are OFFLINE"
  };

  Paint_NewImage(BlackImage, EPD_2IN9B_V4_WIDTH, EPD_2IN9B_V4_HEIGHT, 270, WHITE);
  Paint_NewImage(RYImage, EPD_2IN9B_V4_WIDTH, EPD_2IN9B_V4_HEIGHT, 270, WHITE);
  // *----------- Draw black image ------------*
  Paint_SelectImage(BlackImage);
  Paint_Clear(WHITE);

  //draw box
  Paint_DrawRectangle(7, 17, 290, 113, BLACK, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);

  // Draw online server status
  drawServerStatusONLINE(serverText);

  // Draw time and battery in black
  drawBatteryBlack(batteryLevel, batLevel);
  drawTimeDate();


  // *----------- Draw red image ------------*
  Paint_SelectImage(RYImage);
  Paint_Clear(WHITE);

  // Draw offline server status
  drawServerStatusOFFLINE(serverText);

  //draw red battery level
  drawBatteryRed(batteryLevel, batLevel);

  //draw red wifi ip top
  Paint_DrawString_EN(0, 0, WiFi.localIP().toString().c_str(), &Font12, BLACK, WHITE);

  EPD_2IN9B_V4_Display_Base(BlackImage, RYImage);
  free(BlackImage);
  free(RYImage);
  DEV_Delay_ms(2000);
  EPD_2IN9B_V4_Sleep();
}

void drawServerStatusONLINE(const char *serverText[]) {
  for (int i = 0; i < 6; i++) {
    if (String(serverText[i]).indexOf("ONLINE") > 0) {
      Paint_DrawString_EN(10, 20 + i * 15, serverText[i], &Font16, WHITE, BLACK);
    }
  }
}

void drawServerStatusOFFLINE(const char *serverText[]) {
  for (int i = 0; i < 6; i++) {
    if (String(serverText[i]).indexOf("OFFLINE") > 0) {
      Paint_DrawString_EN(10, 20 + i * 15, serverText[i], &Font16, BLACK, WHITE);
    }
  }
}

void drawTimeDate() {
  timeClient.begin();
  timeClient.setTimeOffset(-18000);
  while (!timeClient.update()) {
    timeClient.forceUpdate();
  }
  String Formatted_date = timeClient.getFormattedDate();
  String hour = Formatted_date.substring(11, 13);
  String minute = Formatted_date.substring(14, 16);
  String second = Formatted_date.substring(17, 19);
  if ((hour.toInt() == 0) or (hour.toInt() == 12)) {  // Use 12 instead of 0
    hour = "12";
  } else {
    hour = hour.toInt() % 12;  // Use String
  }
  String Time = hour + ":" + minute + ":" + second;
  int splitT = Formatted_date.indexOf("T");
  String date = Formatted_date.substring(0, splitT);
  String dateTime = date + " " + Time;
  Paint_DrawString_EN(159, 115, dateTime.c_str(), &Font12, BLACK, WHITE);
}

void drawBatteryBlack(int batteryLevel, String batLevel) {
  //draw black empty battery icon
  Paint_DrawRectangle(7, 117, 25, 125, BLACK, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
  Paint_DrawRectangle(25, 120, 27, 123, BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);

  if (batteryLevel > 20) {
    Paint_DrawString_EN(28, 116, batLevel.c_str(), &Font12, WHITE, BLACK);  //battery percentage readout black
  }

  if (batteryLevel >= 90) {
    Paint_DrawRectangle(7, 117, 25, 125, BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);  //90-100%
  } else if (batteryLevel >= 80) {
    Paint_DrawRectangle(7, 117, 22, 125, BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);  //80-90%
  } else if (batteryLevel >= 65) {
    Paint_DrawRectangle(7, 117, 20, 125, BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);  //65-80%
  } else if (batteryLevel >= 50) {
    Paint_DrawRectangle(7, 117, 18, 125, BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);  //50-65%
  } else if (batteryLevel >= 40) {
    Paint_DrawRectangle(7, 117, 16, 125, BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);  //40-50%
  } else if (batteryLevel >= 30) {
    Paint_DrawRectangle(7, 117, 14, 125, BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);  //30-40%
  } else if (batteryLevel > 20) {
    Paint_DrawRectangle(7, 117, 12, 125, BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);  //20-30%
  }
}

void drawBatteryRed(int batteryLevel, String batLevel) {
  if (batteryLevel <= 20) {
    Paint_DrawString_EN(28, 116, batLevel.c_str(), &Font12, WHITE, BLACK);  //battery percentage readout red
  }
  if (batteryLevel > 10 && batteryLevel <= 20) {
    Paint_DrawRectangle(7, 117, 10, 125, BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);  // 10-20%
  } else if (batteryLevel <= 10) {
    Paint_DrawRectangle(7, 117, 8, 125, BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);  // less than 10%
  }
}

void pingServers() {
  pingFire = pingServer(fireHost);
  pingUnraid = pingServer(unraidHost);
  pingMedia = pingServer(mediaHost);
  pingRequest = pingServer(requestHost);
  pingVPN = pingServer(vpnHost);
  pingVM = pingServer(vmHost);
}

bool pingServer(IPAddress server) {
  Serial.println("Pinging server...");

  // Using the ping function directly, it returns a boolean
  bool isReachable = Ping.ping(server, 2);  // 2 retries

  if (isReachable) {
    Serial.println("Server is online");
    return true;
  } else {
    Serial.println("Server is offline");
    return false;
  }
}

void checkForFirmwareUpdate() {
  HTTPClient http;
  http.begin(FIRMWARE_URL1);
  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, payload);
    String firmware_version = doc["version"].as<String>();
    String firmware_url = doc["url"].as<String>();
    if (firmware_version != current_version && firmware_version.toInt() > current_version.toInt()) {
      Serial.println("Updating firmware...");
      // Start the firmware update process
      performOTAUpdate(firmware_url);
    } else {
      Serial.println("No new firmware available.");
    }
  } else {
    Serial.println("Failed to fetch metadata from server. HTTP Code: " + String(httpCode));
  }
  http.end();
}

void performOTAUpdate(const String &url) {
  WiFiClient client;
  HTTPClient http;
  http.begin(client, url);

  // Get firmware file
  int httpCode = http.GET();
  if (httpCode == 200) {
    int contentLength = http.getSize();
    if (contentLength <= 0) {
      Serial.println("Invalid content length.");
      return;
    }

    // Create a stream to write the firmware
    WiFiClient *stream = http.getStreamPtr();

    Serial.println("Starting OTA update...");

    if (Update.begin(contentLength)) {
      size_t written = Update.writeStream(*stream);
      if (written == contentLength) {
        Serial.println("Written: " + String(written) + " successfully.");
      } else {
        Serial.println("Write failed.");
      }

      if (Update.end()) {
        Serial.println("OTA update completed. Rebooting...");
        ESP.restart();
      } else {
        Serial.println("Error during update: " + String(Update.getError()));
      }
    } else {
      Serial.println("Not enough space for OTA.");
    }
  } else {
    Serial.println("Firmware download failed. HTTP Code: " + String(httpCode));
  }

  http.end();
}

void loop() {
  // Your loop code goes here.
}

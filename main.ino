/* Ryan's Server Status */
#include "DEV_Config.h"
#include "EPD.h"
#include "GUI_Paint.h"
#include <stdlib.h>
#include <WiFi.h>
#include <ESP32Ping.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <HTTPClient.h>
#include <Update.h>
#include <ArduinoJson.h>

#define FIRMWARE_URL1  "http://192.168.2.33/firmware_metadata.json"  // URL to JSON metadata file
String current_version = "1.7";

// deep sleep
#define uS_TO_S_FACTOR 1000000    /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP 1785        // sleep 30 minutes
#define TIME_TO_SLEEP_NoWIFI 30  // sleep only 2 minutes if no wifi

// Replace with your network credentials
const char *ssid = "ssid";
const char *password = "password";
const char *MyHostName = "ESP32ServerMonitor";

String firmware_url = "";
String firmware_version = "";

// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

//top bar title & IP string
String titleIPString = "Ryan's Server Status      IP:";

// Variables to save date and time
String hour, minute, second;
String Formatted_date, date, Time;

//text and ip of servers
IPAddress mediaHost(192, 168, 2, 2);
const char *mediaTextOnline = "Media is ONLINE";
const char *mediaTextOffline = "Media is OFFLINE";
IPAddress fireHost(192, 168, 2, 102);
const char *fireTextOnline = "pfsense is ONLINE";
const char *fireTextOffline = "pfsense is OFFLINE";
IPAddress unraidHost(192, 168, 2, 101);
const char *unraidTextOnline = "UNRAID is ONLINE";
const char *unraidTextOffline = "UNRAID is OFFLINE";
IPAddress requestHost(192, 168, 2, 17);
const char *requestTextOnline = "Request is ONLINE";
const char *requestTextOffline = "Request is OFFLINE";
IPAddress vpnHost(192, 168, 2, 99);
const char *vpnTextOnline = "VPN Servers are ONLINE";
const char *vpnTextOffline = "VPN Servers are OFFLINE";
IPAddress vmHost(192, 168, 2, 250);
const char *vmTextOnline = "VMs are ONLINE";
const char *vmTextOffline = "VMs are OFFLINE";

bool pingMedia, pingFire, pingUnraid, pingVPN, pingVM, pingRequest;

/* Entry point ----------------------------------------------------------------*/
void setup() {
  //Serial.begin(115200);
  wifiInit();
  delay(5000);
  checkForFirmwareUpdate();
  analogRead(33);  //esp32 pin 33 battery voltage stepped down from 4.2v to 3.2v
  ePaperInit();

#if 1
  if (WiFi.status() != WL_CONNECTED) {
    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP_NoWIFI * uS_TO_S_FACTOR);
    drawNoConnection();
  } else {
    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
    pingServers();
    drawMainServerStatus();
  }

#endif
  //finally sleep
  Serial.println("Going to sleep now");
  delay(500);
  Serial.flush();
  esp_deep_sleep_start();
}


void wifiInit() {  //init wifi
  WiFi.setHostname(MyHostName);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  delay(500);
  //if (WiFi.status() == WL_CONNECTED) {
  //}
}

void ePaperInit() {  // init the epaper display and clear it
  if (DEV_Module_Init() != 0) {
    while (1)
      ;
  }
  EPD_2IN9B_V4_Init();
  EPD_2IN9B_V4_Clear();
  DEV_Delay_ms(500);
}

void drawNoConnection() { //draw no connection
  //Create a new image cache named IMAGE_BW and fill it with white
  UBYTE *BlackImage, *RYImage;  // Red or Yellow
  UWORD Imagesize = ((EPD_2IN9B_V4_WIDTH % 8 == 0) ? (EPD_2IN9B_V4_WIDTH / 8) : (EPD_2IN9B_V4_WIDTH / 8 + 1)) * EPD_2IN9B_V4_HEIGHT;
  if ((BlackImage = (UBYTE *)malloc(Imagesize)) == NULL) {
    printf("Failed to apply for black memory...\r\n");
    while (1)
      ;
  }
  if ((RYImage = (UBYTE *)malloc(Imagesize)) == NULL) {
    printf("Failed to apply for red memory...\r\n");
    while (1)
      ;
  }
  Paint_NewImage(BlackImage, EPD_2IN9B_V4_WIDTH, EPD_2IN9B_V4_HEIGHT, 270, WHITE);
  Paint_NewImage(RYImage, EPD_2IN9B_V4_WIDTH, EPD_2IN9B_V4_HEIGHT, 270, WHITE);
  //draw black image
  Paint_SelectImage(BlackImage);
  Paint_Clear(WHITE);
  Paint_DrawRectangle(7, 17, 290, 113, BLACK, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
  //battery percentage readout
  int batteryLevel = map(analogRead(33), 3412, 4095, 0, 100);
  String batLevel = String(batteryLevel) + "%";
  //draw empty battery battery icon
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
  //draw red strings or shapes
  //2.Draw red image
  Paint_SelectImage(RYImage);
  Paint_Clear(WHITE);

  titleIPString = titleIPString + "Not connected";
  Paint_DrawString_EN(0, 0, titleIPString.c_str(), &Font12, BLACK, WHITE);

  Paint_DrawString_EN(10, 50, "WiFi connection is not", &Font16, BLACK, WHITE);
  Paint_DrawString_EN(10, 65, "available", &Font16, BLACK, WHITE);
  if (batteryLevel > 10 && batteryLevel <= 20) {
    Paint_DrawString_EN(28, 116, batLevel.c_str(), &Font12, WHITE, BLACK);       //battery percentage readout red
    Paint_DrawRectangle(7, 117, 10, 125, BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);  // 10-20%
  } else if (batteryLevel <= 10) {
    Paint_DrawString_EN(28, 116, batLevel.c_str(), &Font12, WHITE, BLACK);      //battery percentage readout red
    Paint_DrawRectangle(7, 117, 8, 125, BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);  // less than 10%
  }

  EPD_2IN9B_V4_Display_Base(BlackImage, RYImage);
  //EPD_2IN9B_V4_Display(BlackImage, RYImage);
  DEV_Delay_ms(2000);
  EPD_2IN9B_V4_Sleep();
  free(BlackImage);
  free(RYImage);
  BlackImage = NULL;
  RYImage = NULL;
  DEV_Delay_ms(2000);  //important, at least 2s
  // close 5V
}

void drawMainServerStatus() {  //draw strings or shapes
  //Create a new image cache named IMAGE_BW and fill it with white
  UBYTE *BlackImage, *RYImage;  // Red or Yellow
  UWORD Imagesize = ((EPD_2IN9B_V4_WIDTH % 8 == 0) ? (EPD_2IN9B_V4_WIDTH / 8) : (EPD_2IN9B_V4_WIDTH / 8 + 1)) * EPD_2IN9B_V4_HEIGHT;
  if ((BlackImage = (UBYTE *)malloc(Imagesize)) == NULL) {
    printf("Failed to apply for black memory...\r\n");
    while (1)
      ;
  }
  if ((RYImage = (UBYTE *)malloc(Imagesize)) == NULL) {
    printf("Failed to apply for red memory...\r\n");
    while (1)
      ;
  }
  Paint_NewImage(BlackImage, EPD_2IN9B_V4_WIDTH, EPD_2IN9B_V4_HEIGHT, 270, WHITE);
  Paint_NewImage(RYImage, EPD_2IN9B_V4_WIDTH, EPD_2IN9B_V4_HEIGHT, 270, WHITE);

  //draw black image
  Paint_SelectImage(BlackImage);
  Paint_Clear(WHITE);


  if (pingFire == true) {
    Paint_DrawString_EN(10, 20, fireTextOnline, &Font16, WHITE, BLACK);
  }
  if (pingUnraid == true) {
    Paint_DrawString_EN(10, 35, unraidTextOnline, &Font16, WHITE, BLACK);
  }
  if (pingMedia == true) {
    Paint_DrawString_EN(10, 50, mediaTextOnline, &Font16, WHITE, BLACK);
  }
  if (pingRequest == true) {
    Paint_DrawString_EN(10, 65, requestTextOnline, &Font16, WHITE, BLACK);
  }
  if (pingVPN == true) {
    Paint_DrawString_EN(10, 80, vpnTextOnline, &Font16, WHITE, BLACK);
  }
  if (pingVM == true) {
    Paint_DrawString_EN(10, 95, vmTextOnline, &Font16, WHITE, BLACK);
  }

  // Extract datetime
  // Initialize a NTPClient to get time
  timeClient.begin();
  timeClient.setTimeOffset(-18000);
  while (!timeClient.update()) {
    timeClient.forceUpdate();
  }
  Formatted_date = timeClient.getFormattedDate();
  hour = Formatted_date.substring(11, 13);
  minute = Formatted_date.substring(14, 16);
  second = Formatted_date.substring(17, 19);
  if ((hour.toInt() == 0) or (hour.toInt() == 12)) {  // Use 12 instead of 0
    hour = "12";
  } else {
    hour = hour.toInt() % 12;  // Use String
  }
  Time = hour + ":" + minute + ":" + second;
  int splitT = Formatted_date.indexOf("T");
  date = Formatted_date.substring(0, splitT);
  Paint_DrawRectangle(7, 17, 290, 113, BLACK, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
  String dateTime = date + " " + Time;
  Paint_DrawString_EN(159, 115, dateTime.c_str(), &Font12, BLACK, WHITE);

  //battery percentage readout
  int batteryLevel = map(analogRead(33), 3412, 4095, 0, 100);
  String batLevel = String(batteryLevel) + "%";

  //draw empty battery battery icon
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


  //draw red strings or shapes
  //2.Draw red image
  Paint_SelectImage(RYImage);
  Paint_Clear(WHITE);

  titleIPString = titleIPString + WiFi.localIP().toString();
  Paint_DrawString_EN(0, 0, titleIPString.c_str(), &Font12, BLACK, WHITE);

  if (pingFire == false) {
    Paint_DrawString_EN(10, 20, fireTextOffline, &Font16, BLACK, WHITE);
  }
  if (pingUnraid == false) {
    Paint_DrawString_EN(10, 35, unraidTextOffline, &Font16, BLACK, WHITE);
  }
  if (pingMedia == false) {
    Paint_DrawString_EN(10, 50, mediaTextOffline, &Font16, BLACK, WHITE);
  }
  if (pingRequest == false) {
    Paint_DrawString_EN(10, 65, requestTextOffline, &Font16, BLACK, WHITE);
  }
  if (pingVPN == false) {
    Paint_DrawString_EN(10, 80, vpnTextOffline, &Font16, BLACK, WHITE);
  }
  if (pingVM == false) {
    Paint_DrawString_EN(10, 95, vmTextOffline, &Font16, BLACK, WHITE);
  }

  if (batteryLevel > 10 && batteryLevel <= 20) {
    Paint_DrawString_EN(28, 116, batLevel.c_str(), &Font12, WHITE, BLACK);       //battery percentage readout red
    Paint_DrawRectangle(7, 117, 10, 125, BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);  // 10-20%
  } else if (batteryLevel <= 10) {
    Paint_DrawString_EN(28, 116, batLevel.c_str(), &Font12, WHITE, BLACK);      //battery percentage readout red
    Paint_DrawRectangle(7, 117, 8, 125, BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);  // less than 10%
  }

  EPD_2IN9B_V4_Display_Base(BlackImage, RYImage);
  //EPD_2IN9B_V4_Display(BlackImage, RYImage);
  DEV_Delay_ms(2000);

  EPD_2IN9B_V4_Sleep();
  free(BlackImage);
  free(RYImage);
  BlackImage = NULL;
  RYImage = NULL;
  DEV_Delay_ms(2000);  //important, at least 2s
  // close 5V
}

void pingServers() {  //ping the servers once multiple times if failed to speed up results
  delay(500);
  pingFire = Ping.ping(fireHost, 1);
  if (pingFire == false) {
    pingFire = Ping.ping(fireHost, 1);
    if (pingFire == false) {
      pingFire = Ping.ping(fireHost, 2);
    }
  }
  delay(500);
  pingUnraid = Ping.ping(unraidHost, 1);
  if (pingUnraid == false) {
    pingUnraid = Ping.ping(unraidHost, 1);
    if (pingUnraid == false) {
      pingUnraid = Ping.ping(unraidHost, 2);
    }
  }
  delay(500);
  pingMedia = Ping.ping(mediaHost, 1);
  if (pingMedia == false) {
    pingMedia = Ping.ping(mediaHost, 1);
    if (pingMedia == false) {
      pingMedia = Ping.ping(mediaHost, 2);
    }
  }
  delay(500);
  pingRequest = Ping.ping(requestHost, 1);
  if (pingRequest == false) {
    pingRequest = Ping.ping(requestHost, 1);
    if (pingRequest == false) {
      pingRequest = Ping.ping(requestHost, 2);
    }
  }
  delay(500);
  pingVPN = Ping.ping(vpnHost, 1);
  if (pingVPN == false) {
    pingVPN = Ping.ping(vpnHost, 1);
    if (pingVPN == false) {
      pingVPN = Ping.ping(vpnHost, 2);
    }
  }
  delay(500);
  pingVM = Ping.ping(vmHost, 1);
  if (pingVM == false) {
    pingVM = Ping.ping(vmHost, 1);
    if (pingVM == false) {
      pingVM = Ping.ping(vmHost, 2);
    }
  }
}

void checkForFirmwareUpdate() {
  HTTPClient http;
  http.begin(FIRMWARE_URL1);
  // Make an HTTP request to get the firmware metadata JSON
  int httpCode = http.GET();
  if (httpCode == 200) {
    String payload = http.getString();
    Serial.println("Metadata JSON received:");

    // Parse JSON
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, payload);
    firmware_version = doc["version"].as<String>();
    firmware_url = doc["url"].as<String>();

    Serial.println("Firmware Version: " + firmware_version);
    Serial.println("Firmware URL: " + firmware_url);

    // Check if the firmware version is newer than the current one
    if (isNewVersionAvailable(firmware_version)) {
      Serial.println("New firmware available. Starting update...");

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

bool isNewVersionAvailable(const String &new_version) {
  // Compare the current firmware version with the new version
  // You can replace this with a more complex version comparison logic if needed
    // Get the current firmware version (you can use other methods)
  
  return (new_version != current_version);
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

/* The main loop -------------------------------------------------------------*/
void loop() {
}

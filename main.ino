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
String current_version = "1.1.3";

// Deep sleep settings
#define uS_TO_S_FACTOR 1000000   /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP 180        // For testing, sleep 180 seconds (adjust as needed)
#define TIME_TO_SLEEP_NoWIFI 30  // Sleep only 30 seconds if no WiFi
#define TIMEOUT_MS 30000         // 30-second inactivity timeout

// Button configuration
#define BUTTON_PIN 39          // Pin for button input
#define DEBOUNCE_DELAY 50      // milliseconds
#define DOUBLE_PRESS_TIME 500  // maximum time (ms) to detect a double press

UBYTE *partialBuffer = NULL;
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

// Selected server index (0 to 5)
int selectedServer = 0;

// Server statuses
bool pingMedia, pingFire, pingUnraid, pingVPN, pingVM, pingRequest;


void wifiInit();
void ePaperInit();
void allocateMemoryForImages(UBYTE **BlackImage, UBYTE **RYImage, uint16_t Imagesize);
void drawBatteryAndNoWifi(int batteryLevel, String batLevel);
void drawMainServerStatus(int batteryLevel, String batLevel);
void drawTimeDate();
void drawBatteryBlack(int batteryLevel, String batLevel);
void drawBatteryRed(int batteryLevel, String batLevel);
void drawServerStatusONLINE(const char *serverText[], int offsetX);
void drawServerStatusOFFLINE(const char *serverText[]);
void pingServers();
bool pingServer(IPAddress server);
void checkForFirmwareUpdate();
void performOTAUpdate(const String &url);
bool waitForButtonPress(unsigned long &pressTime);
bool isDoublePress(unsigned long firstPressTime);
void updateSelection(int selectedServer, int batteryLevel, String batLevel);
void drawServerTemplate(int selectedServer, int batteryLevel, String batLevel);




//function refactoring
void wifiInit() {
  WiFi.setHostname(MyHostName);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
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

// --- Button Functions ---
bool waitForButtonPress(unsigned long &pressTime) {
  unsigned long start = millis();
  while (millis() - start < TIMEOUT_MS) {
    if (digitalRead(BUTTON_PIN) == HIGH) {  // HIGH means pressed
      delay(DEBOUNCE_DELAY);
      if (digitalRead(BUTTON_PIN) == HIGH) {
        pressTime = millis();
        // Wait until the button is released
        while (digitalRead(BUTTON_PIN) == HIGH) { delay(10); }
        return true;
      }
    }
    delay(10);
  }
  Serial.println("No button press detected for 30 seconds. Going to sleep.");
  //esp_deep_sleep_start();
  return false;  // Not reached
}

bool isDoublePress(unsigned long firstPressTime) {
  unsigned long start = millis();
  while (millis() - start < DOUBLE_PRESS_TIME) {
    if (digitalRead(BUTTON_PIN) == HIGH) {  // HIGH means pressed
      delay(DEBOUNCE_DELAY);
      if (digitalRead(BUTTON_PIN) == HIGH) {
        // Wait for release
        while (digitalRead(BUTTON_PIN) == HIGH) { delay(10); }
        return true;
      }
    }
    delay(10);
  }
  return false;
}


// --- updateSelection ---
// Re-draws the full-screen main status using the partial update buffer.
// It draws the server list (all 6 servers) with server texts at x = 10 (as in base refresh)
// and overlays a selector on top the currently selected row.
void updateSelection(int selectedServer, int batteryLevel, String batLevel) {
  uint16_t Imagesize = ((EPD_2IN9B_V4_WIDTH % 8 == 0) ? (EPD_2IN9B_V4_WIDTH / 8) : (EPD_2IN9B_V4_WIDTH / 8 + 1)) * EPD_2IN9B_V4_HEIGHT;
  Paint_NewImage(partialBuffer, EPD_2IN9B_V4_WIDTH, EPD_2IN9B_V4_HEIGHT, 270, WHITE);
  Paint_SelectImage(partialBuffer);
  Paint_Clear(WHITE);
  Serial.println("update section");
  const char *serverText[] = {
    pingFire ? "pfsense is ONLINE" : "pfsense is OFFLINE",
    pingUnraid ? "UNRAID is ONLINE" : "UNRAID is OFFLINE",
    pingMedia ? "Media is ONLINE" : "Media is OFFLINE",
    pingRequest ? "Request is ONLINE" : "Request is OFFLINE",
    pingVPN ? "VPN Servers are ONLINE" : "VPN Servers are OFFLINE",
    pingVM ? "VMs are ONLINE" : "VMs are OFFLINE"
  };

  for (int i = 0; i < 6; i++) {
    if (i == selectedServer) {
      /*if (blink) { //removed blinking idea for now      
        //Paint_DrawString_EN(5, 20 + selectedServer * 15, ".", &Font16, BLACK, WHITE);
        Paint_DrawString_EN(10, 20 + selectedServer * 15, serverText[selectedServer], &Font16, BLACK, WHITE);
      }*/
      Paint_DrawString_EN(10, 20 + selectedServer * 15, serverText[selectedServer], &Font16, BLACK, WHITE);
    } else {
      Paint_DrawString_EN(10, 20 + i * 15, serverText[i], &Font16, WHITE, BLACK);
    }
  }

  Paint_DrawRectangle(7, 17, 290, 113, BLACK, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);


  // Draw time and battery in black
  drawBatteryBlack(batteryLevel, batLevel);
  drawTimeDate();

  //EPD_2IN9B_V4_Init();
  EPD_2IN9B_V4_Display_Partial(partialBuffer, 0, 0, EPD_2IN9B_V4_WIDTH, EPD_2IN9B_V4_HEIGHT);
  DEV_Delay_ms(500);
}

// Performs a full refresh to display a dummy template for the selected server.
void drawServerTemplate(int selectedServer, int batteryLevel, String batLevel) {
  const char *serverText[] = { "pfsense Stats", "UNRAID Stats", "Media Stats", "Request Stats", "VPN Servers Info", "Virtual Machines" };
  uint16_t Imagesize = ((EPD_2IN9B_V4_WIDTH % 8 == 0) ? (EPD_2IN9B_V4_WIDTH / 8) : (EPD_2IN9B_V4_WIDTH / 8 + 1)) * EPD_2IN9B_V4_HEIGHT;
  Paint_NewImage(partialBuffer, EPD_2IN9B_V4_WIDTH, EPD_2IN9B_V4_HEIGHT, 270, WHITE);
  Paint_SelectImage(partialBuffer);
  Paint_Clear(WHITE);

  Paint_DrawString_EN(10, 20, serverText[selectedServer], &Font24, BLACK, WHITE);
  Paint_DrawString_EN(10, 50, "CPU USAGE", &Font12, BLACK, WHITE);
  Paint_DrawString_EN(110, 50, "RAM USAGE", &Font12, BLACK, WHITE);
  Paint_DrawString_EN(210, 50, "DISK SPACE", &Font12, BLACK, WHITE);


  // cpu usage control later with api json data
  Paint_DrawCircle(40, 87, 22, BLACK, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
  Paint_DrawString_EN(25, 79, "10%", &Font16, WHITE, BLACK);

  // ram usage control later with api json data
  Paint_DrawCircle(145, 87, 22, BLACK, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
  Paint_DrawString_EN(130, 79, "50%", &Font16, WHITE, BLACK);

  // disk usage control later with api json data
  Paint_DrawCircle(250, 87, 22, BLACK, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
  Paint_DrawString_EN(228, 79, "100%", &Font16, WHITE, BLACK);

  // Draw time and battery in black
  Paint_DrawRectangle(7, 17, 290, 113, BLACK, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
  drawBatteryBlack(batteryLevel, batLevel);
  drawTimeDate();

  //EPD_2IN9B_V4_Init();
  EPD_2IN9B_V4_Display_Partial(partialBuffer, 0, 0, EPD_2IN9B_V4_WIDTH, EPD_2IN9B_V4_HEIGHT);
  DEV_Delay_ms(500);


  // Loop waiting for button presses
  //int selectedServer = -1;

  // In drawMainServerStatus(), after the base refresh:
  Serial.println("Entering selection loop. Waiting for button press...");

  while (true) {
    unsigned long pressTime = 0;
    // waitForButtonPress() waits for a press up to TIMEOUT_MS.
    if (!waitForButtonPress(pressTime))
      break;  // deep sleep will be triggered on timeout

    // consider polling here until the display update is finished.

    if (isDoublePress(pressTime)) {
      Serial.println("Double press detected. Showing dummy template.");
      drawServerTemplate(selectedServer, batteryLevel, batLevel);
      break;
    } else {
      // Single press: increment the selection.
      selectedServer = (selectedServer + 1) % 6;
      updateSelection(selectedServer, batteryLevel, batLevel);
      // Note: updateSelection() is blocking until the epaper is ready.
    }
  }

  //free(BlackImage);
  //free(RYImage);
}

//-------------------- Integrated mainServerStatus --------------------
// This function draws the main server status (full refresh) and then
// checks for a button press. If no button is pressed within the timeout,
// it goes to deep sleep. If a button press is detected, it enters selection mode
// (partial update) to allow cycling the selector.
void drawMainServerStatus(int batteryLevel, String batLevel) {
  uint16_t Imagesize = ((EPD_2IN9B_V4_WIDTH % 8 == 0) ? (EPD_2IN9B_V4_WIDTH / 8) : (EPD_2IN9B_V4_WIDTH / 8 + 1)) * EPD_2IN9B_V4_HEIGHT;
  UBYTE *BlackImage = (UBYTE *)malloc(Imagesize);
  UBYTE *RYImage = (UBYTE *)malloc(Imagesize);
  if (BlackImage == NULL || RYImage == NULL) {
    Serial.println("Memory allocation failed for base refresh");
    while (1)
      ;
  }

  // Base refresh: create full-screen images.
  Paint_NewImage(BlackImage, EPD_2IN9B_V4_WIDTH, EPD_2IN9B_V4_HEIGHT, 270, WHITE);
  Paint_NewImage(RYImage, EPD_2IN9B_V4_WIDTH, EPD_2IN9B_V4_HEIGHT, 270, WHITE);

  Paint_SelectImage(BlackImage);
  Paint_Clear(WHITE);
  // Draw a border box.
  Paint_DrawRectangle(7, 17, 290, 113, BLACK, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);

  const char *serverText[] = {
    pingFire ? "pfsense is ONLINE" : "pfsense is OFFLINE",
    pingUnraid ? "UNRAID is ONLINE" : "UNRAID is OFFLINE",
    pingMedia ? "Media is ONLINE" : "Media is OFFLINE",
    pingRequest ? "Request is ONLINE" : "Request is OFFLINE",
    pingVPN ? "VPN Servers are ONLINE" : "VPN Servers are OFFLINE",
    pingVM ? "VMs are ONLINE" : "VMs are OFFLINE"
  };

  for (int i = 0; i < 6; i++) {
    Paint_DrawString_EN(10, 20 + i * 15, serverText[i], &Font16, WHITE, BLACK);
  }

  // Draw battery and time.
  drawBatteryBlack(batteryLevel, batLevel);
  drawTimeDate();

  // Draw red image elements.
  Paint_SelectImage(RYImage);
  Paint_Clear(WHITE);
  drawServerStatusOFFLINE(serverText);
  drawBatteryRed(batteryLevel, batLevel);
  Paint_DrawString_EN(0, 0, (String("IP:") + WiFi.localIP().toString() + "     Server Monitor v" + current_version).c_str(), &Font12, BLACK, WHITE);

  EPD_2IN9B_V4_Init();
  EPD_2IN9B_V4_Display_Base(BlackImage, RYImage);
  DEV_Delay_ms(3000);

  // Free base refresh buffers if no button press.
  unsigned long firstPressTime = 0;

  free(BlackImage);
  free(RYImage);
  partialBuffer = (UBYTE *)malloc(Imagesize);
  if (partialBuffer == NULL) {
    Serial.println("Failed to allocate partial update buffer");
    while (1)
      ;
  }

  // Loop waiting for button presses
  int selectedServer = -1;

  // In drawMainServerStatus(), after the base refresh:
  Serial.println("Entering selection loop. Waiting for button press...");

  while (true) {
    unsigned long pressTime = 0;
    // waitForButtonPress() waits for a press up to TIMEOUT_MS.
    if (!waitForButtonPress(pressTime))
      break;  // deep sleep will be triggered on timeout

    // Optionally: If you have a busy flag or can check the display state,
    // consider polling here until the display update is finished.

    if (isDoublePress(pressTime)) {
      Serial.println("Double press detected. Showing dummy template.");
      drawServerTemplate(selectedServer, batteryLevel, batLevel);
      break;  // Exit loop after handling double press.
    } else {
      // Single press: increment the selection.
      selectedServer = (selectedServer + 1) % 6;
      updateSelection(selectedServer, batteryLevel, batLevel);
      // Note: updateSelection() is blocking until the epaper is ready.
      // If needed, consider reducing the update region or using an interrupt-driven approach.
    }
  }

  //}

  Serial.println("MainServerStatus complete. Going to sleep.");
  DEV_Delay_ms(2000);
  EPD_2IN9B_V4_Sleep();
  esp_deep_sleep_start();
}


void drawServerStatusONLINE(const char *serverText[], int offsetX) {
  for (int i = 0; i < 6; i++) {
    if (String(serverText[i]).indexOf("ONLINE") > 0) {
      Paint_DrawString_EN(offsetX, 20 + i * 15, serverText[i], &Font16, WHITE, BLACK);
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
  // Expected format: "yyyy-mm-ddTHH:MM:SSZ"
  String formatted = timeClient.getFormattedDate();
  
  String year = formatted.substring(0, 4);
  String month = formatted.substring(5, 7);
  String day = formatted.substring(8, 10);
  String hour = formatted.substring(11, 13);
  String minute = formatted.substring(14, 16);
  String second = formatted.substring(17, 19);
  
  // Convert to 12-hour format if desired
  if ((hour.toInt() == 0) || (hour.toInt() == 12)) {
    hour = "12";
  } else {
    hour = String(hour.toInt() % 12);
  }
  
  String timeStr = hour + ":" + minute + ":" + second;
  String dateStr = month + "-" + day + "-" + year;  // mm-dd-yyyy
  String dateTime = dateStr + " " + timeStr;
  
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
    Paint_DrawRectangle(8, 117, 10, 125, BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);  // 10-20%
  } else if (batteryLevel <= 10) {
    Paint_DrawRectangle(8, 117, 8, 125, BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);  // less than 10%
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
    String firmware_checksum = doc["checksum"].as<String>();  // New checksum field
    if (firmware_version != current_version) {
      Serial.println("Updating firmware...");
      performOTAUpdate(firmware_url, firmware_checksum);
    } else {
      Serial.println("No new firmware available.");
    }
  } else {
    Serial.println("Failed to fetch metadata from server. HTTP Code: " + String(httpCode));
  }
  http.end();
}


void performOTAUpdate(const String &url, const String &firmware_md5) {
  WiFiClient client;
  HTTPClient http;
  http.begin(client, url);
  int httpCode = http.GET();
  if (httpCode == 200) {
    int contentLength = http.getSize();
    if (contentLength <= 0) {
      Serial.println("Invalid content length.");
      return;
    }
    WiFiClient *stream = http.getStreamPtr();
    Serial.println("Starting OTA update...");
    if (Update.begin(contentLength)) {
      if (firmware_md5.length() > 0) {
        Update.setMD5(firmware_md5.c_str());
      }
      size_t written = Update.writeStream(*stream);
      if (written == contentLength) {
        Serial.println("Written: " + String(written) + " bytes successfully.");
      } else {
        Serial.println("Write failed.");
      }
      if (Update.end()) {
        Serial.println("OTA update completed. Rebooting...");
        ESP.restart();
      } else {
        String errStr = Update.errorString();  // Use errorString() instead of getErrorString()
        if (errStr.indexOf("MD5") != -1) {
          Serial.println("MD5 checksum mismatch. OTA update failed.");
        } else {
          Serial.println("Error during update: " + errStr);
        }
      }
    } else {
      Serial.println("Not enough space for OTA.");
    }
  } else {
    Serial.println("Firmware download failed. HTTP Code: " + String(httpCode));
  }
  http.end();
}


//-------------------- Setup & Loop --------------------
void setup() {
  delay(500);
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT);
  esp_sleep_enable_ext0_wakeup((gpio_num_t)BUTTON_PIN, HIGH);
  wifiInit();
  delay(5000);
  checkForFirmwareUpdate();
  analogRead(33);  // Battery voltage read
  delay(500);
  int batteryLevel = map(analogRead(33), 3412, 4095, 0, 100);
  String batLevel = String(batteryLevel) + "%";
  ePaperInit();

  if (WiFi.status() == WL_CONNECTED) {
    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
    pingServers();
    // Draw the main server status with integrated selection logic.
    drawMainServerStatus(batteryLevel, batLevel);
  } else {
    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP_NoWIFI * uS_TO_S_FACTOR);
    drawBatteryAndNoWifi(batteryLevel, batLevel);
  }
}

void loop() {
  // Loop left empty; all logic is executed in setup() and drawMainServerStatus().
}

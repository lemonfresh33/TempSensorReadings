#include <Wire.h>
#include <Arduino.h>
#include "Arduino_GFX_Library.h"
#include "TouchDrvGT911.hpp"
#include <SPI.h>

#include <WiFi.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Arduino_GFX_Library.h>

#include "HWCDC.h"

HWCDC USBSerial;

TouchDrvGT911 GT911;
int16_t x[5], y[5];
uint8_t gt911_i2c_addr = 0;

Arduino_DataBus *bus = new Arduino_SWSPI(
  GFX_NOT_DEFINED /* DC */, 42 /* CS */,
  2 /* SCK */, 1 /* MOSI */, GFX_NOT_DEFINED /* MISO */);

Arduino_ESP32RGBPanel *rgbpanel = new Arduino_ESP32RGBPanel(
  40 /* DE */, 39 /* VSYNC */, 38 /* HSYNC */, 41 /* PCLK */,
  46 /* R0 */, 3 /* R1 */, 8 /* R2 */, 18 /* R3 */, 17 /* R4 */,
  14 /* G0 */, 13 /* G1 */, 12 /* G2 */, 11 /* G3 */, 10 /* G4 */, 9 /* G5 */,
  5 /* B0 */, 45 /* B1 */, 48 /* B2 */, 47 /* B3 */, 21 /* B4 */,
  1 /* hsync_polarity */, 10 /* hsync_front_porch */, 8 /* hsync_pulse_width */, 60 /* hsync_back_porch */,
  1 /* vsync_polarity */, 10 /* vsync_front_porch */, 8 /* vsync_pulse_width */, 30 /* vsync_back_porch */);
Arduino_RGB_Display *gfx = new Arduino_RGB_Display(
  480 /* width */, 480 /* height */, rgbpanel, 2 /* rotation */, true /* auto_flush */,
  bus, GFX_NOT_DEFINED /* RST */, st7701_type1_init_operations, sizeof(st7701_type1_init_operations));


// --- MULTI-WIFI SETUP ---
WiFiMulti wifiMulti;
#define WIFI_PASS1 "welcome1"  // Assuming shared pass, otherwise add individually
#define WIFI_PASS2 "Welcome1!"  // Assuming shared pass, otherwise add individually
#define DATA_URL "http://192.168.1.172/getSensorData"
#define UPDATE_INTERVAL 60000

// --- UI & SPINNER CONSTANTS ---
#define SPIN_X 440
#define SPIN_Y 440
#define SPIN_RADIUS 12
#define COL_BLACK 0x0000
#define COL_WHITE 0xFFFF
#define COL_RED 0xF800
#define COL_GREEN 0x07E0
#define COL_BLUE 0x001F
#define COL_AXIS 0x7BEF
#define SCREEN_W        480
#define SCREEN_H        480
#define MARGIN_LEFT     60
#define MARGIN_RIGHT    60
#define GRAPH_Y         140
#define GRAPH_H         280
#define GRAPH_W         (SCREEN_W - MARGIN_LEFT - MARGIN_RIGHT)
#define MAX_HUMIDITY    101
volatile bool isFetching = false;
String globalJsonData = "";
bool newDataAvailable = false;
TaskHandle_t FetchTaskHandle = NULL;



// --- SPINNER ---
void drawSpinnerFrame(int frame) {
  gfx->fillRect(SPIN_X - 20, SPIN_Y - 20, 40, 40, COL_BLACK);
  for (int i = 0; i < 8; i++) {
    float angle = (i * 45 + (frame * 25)) * PI / 180.0;
    int x = SPIN_X + (int)(cos(angle) * SPIN_RADIUS);
    int y = SPIN_Y + (int)(sin(angle) * SPIN_RADIUS);
    uint16_t color = (i == 0) ? COL_WHITE : COL_AXIS;
    gfx->fillCircle(x, y, 3, color);
  }
}

// --- DATA RENDERING ENGINE ---
void performRender(String json) {
  DynamicJsonDocument doc(45000);
  if (deserializeJson(doc, json)) return;

  gfx->fillScreen(COL_BLACK);

  // 1. Header & Table Logic
  JsonObject s = doc["sensorData"];
  gfx->setTextSize(2);
  gfx->setTextColor(COL_WHITE);
  gfx->setCursor(20, 20);
  gfx->printf("%s", (const char *)s["time"]);

  const char *labels[] = { "Inside", "Greenh", "Outside" };
  float temps[] = { s["insideTemp"], s["greenhouseTemp"], s["outsideTemp"] };
  int hums[] = { s["insideHumidity"], s["greenhouseHumidity"], s["outsideHumidity"] };
  uint16_t rowColors[] = { COL_RED, COL_GREEN, COL_WHITE };

  for (int i = 0; i < 3; i++) {
    gfx->setTextColor(rowColors[i]);
    gfx->setCursor(40, 60 + (i * 25));
    gfx->printf("%-8s %4.1f C  %d%%", labels[i], temps[i], hums[i]);
  }

  // 2. Multi-Array Scaling Logic
  const char* tKeys[] = {"temp1", "temp2", "temp3"};
  const char* hKeys[] = {"hum1", "hum2", "hum3"};
  uint16_t hColors[] = { 0x001F, 0x07FF, 0xF81F }; // Blue, Cyan, Magenta for Humidities

  float minT = 100, maxT = -100, minH = 100, maxH = 0;
  int count = doc["temp1"].size(); 
  if (count < 2) return;

  // Scan all 3 arrays to find global min/max for the Y-axis
  for (int j = 0; j < 3; j++) {
    JsonArray tArr = doc[tKeys[j]];
    JsonArray hArr = doc[hKeys[j]];
    for (int i = 0; i < tArr.size(); i++) {
      float t = tArr[i]; float h = hArr[i];
      if (t < minT) minT = t; if (t > maxT) maxT = t;
      if (h < MAX_HUMIDITY) { 
        if (h < minH) minH = h; 
        if (h > maxH) maxH = h; 
      }
    }
  }
  
  // Add padding to margins
  minT -= 1.0; maxT += 1.0;
  minH -= 5.0; maxH += 5.0;

  // 3. Draw Graph Box & Axis Labels
  gfx->drawRect(MARGIN_LEFT, GRAPH_Y, GRAPH_W, GRAPH_H, COL_AXIS);
  gfx->setTextSize(1);
  gfx->setTextColor(COL_RED);
  gfx->setCursor(MARGIN_LEFT - 45, GRAPH_Y); gfx->printf("%.1f", maxT);
  gfx->setCursor(MARGIN_LEFT - 45, GRAPH_Y + GRAPH_H - 10); gfx->printf("%.1f", minT);
  
  gfx->setTextColor(COL_BLUE);
  gfx->setCursor(MARGIN_LEFT + GRAPH_W + 5, GRAPH_Y); gfx->printf("%d%%", (int)maxH);
  gfx->setCursor(MARGIN_LEFT + GRAPH_W + 5, GRAPH_Y + GRAPH_H - 10); gfx->printf("%d%%", (int)minH);

  // 4. Plot All Lines
  for (int j = 0; j < 3; j++) {
    JsonArray tHist = doc[tKeys[j]];
    JsonArray hHist = doc[hKeys[j]];
    uint16_t tCol = rowColors[j];
    uint16_t hCol = hColors[j];

    for (int i = 0; i < count - 1; i++) {
      int x0 = MARGIN_LEFT + (i * GRAPH_W / (count - 1));
      int x1 = MARGIN_LEFT + ((i + 1) * GRAPH_W / (count - 1));

      // Plot Temperature
      int yt0 = (GRAPH_Y + GRAPH_H) - (int)((float(tHist[i]) - minT) * GRAPH_H / (maxT - minT));
      int yt1 = (GRAPH_Y + GRAPH_H) - (int)((float(tHist[i+1]) - minT) * GRAPH_H / (maxT - minT));
      gfx->drawLine(x0, yt0, x1, yt1, tCol);

      // Plot Humidity (with error check)
      if (hHist[i] < MAX_HUMIDITY && hHist[i+1] < MAX_HUMIDITY) {
        int yh0 = (GRAPH_Y + GRAPH_H) - (int)((float(hHist[i]) - minH) * GRAPH_H / (maxH - minH));
        int yh1 = (GRAPH_Y + GRAPH_H) - (int)((float(hHist[i+1]) - minH) * GRAPH_H / (maxH - minH));
        gfx->drawLine(x0, yh0, x1, yh1, hCol);
      }
    }
  }

  // 5. X-Axis Labels
  gfx->setTextColor(COL_WHITE);
  gfx->setCursor(MARGIN_LEFT, GRAPH_Y + GRAPH_H + 15);
  gfx->print((const char*)doc["labels"][0]);
  gfx->setCursor(SCREEN_W - MARGIN_RIGHT - 80, GRAPH_Y + GRAPH_H + 15);
  gfx->print((const char*)doc["labels"][count-1]);
}

// --- CORE 0: NETWORK TASK ---
void fetchTask(void *pvParameters) {
  for (;;) {
    // Attempt to connect/reconnect to strongest WiFi
    if (wifiMulti.run() == WL_CONNECTED) {
      isFetching = true;
      HTTPClient http;
      http.begin(DATA_URL);
      http.setTimeout(10000);

      int httpCode = http.GET();
      if (httpCode == HTTP_CODE_OK) {
        globalJsonData = http.getString();
        newDataAvailable = true;
      }
      http.end();
      isFetching = false;
    }
    vTaskDelay(pdMS_TO_TICKS(UPDATE_INTERVAL));
  }
}






void i2c_scan() {
  USBSerial.println("Scanning I2C bus...");
  byte error, address;
  int nDevices = 0;

  for (address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    if (error == 0) {
      USBSerial.print("I2C device found at address 0x");
      if (address < 16) {
        USBSerial.print("0");
      }
      USBSerial.println(address, HEX);
      nDevices++;

      if (address == GT911_SLAVE_ADDRESS_L || address == GT911_SLAVE_ADDRESS_H) {
        gt911_i2c_addr = address;
        USBSerial.print("Found GT911 candidate address: 0x");
        USBSerial.println(address, HEX);
      }
    } else if (error == 4) {
    }
  }

  if (nDevices == 0) {
    USBSerial.println("No I2C devices found");
  } else {
    USBSerial.println("I2C scan completed");
  }
}

bool init_gt911_with_probe(int sda_pin, int scl_pin) {
  Wire.begin(sda_pin, scl_pin);
  delay(100);

  i2c_scan();

  if (gt911_i2c_addr == 0) {
    USBSerial.println("GT911 not found in I2C scan");
    return false;
  }

  GT911.setPins(-1, -1);
  if (GT911.begin(Wire, gt911_i2c_addr, sda_pin, scl_pin)) {
    USBSerial.print("GT911 initialized successfully at address 0x");
    USBSerial.println(gt911_i2c_addr, HEX);
    return true;
  } else {
    USBSerial.print("Failed to initialize GT911 at address 0x");
    USBSerial.println(gt911_i2c_addr, HEX);
    return false;
  }
}

void setup() {
  USBSerial.begin(115200);

  Wire.begin(15, 7);

  Wire.beginTransmission(0x24);
  Wire.write(0x03);
  Wire.write(0x3a);
  Wire.endTransmission();

  if (!init_gt911_with_probe(15, 7)) {
    while (1) {
      USBSerial.println("Failed to find GT911 - check your wiring!");
      delay(1000);
    }
  }

  GT911.setHomeButtonCallback([](void *user_data) {
    USBSerial.println("Home button pressed!");
  },
                              NULL);
  GT911.setMaxTouchPoint(1);  // max is 5

  gfx->begin();
  gfx->fillScreen(COL_BLACK);

  // Register WiFi Networks
  wifiMulti.addAP("Ollie7", WIFI_PASS2);
  wifiMulti.addAP("Ollie8", WIFI_PASS1);
  wifiMulti.addAP("Ollie10", WIFI_PASS1);

  gfx->setCursor(20, 200);
  gfx->setTextColor(COL_WHITE);
  gfx->setTextSize(2);
  gfx->print("Searching for strongest WiFi...");

  // Initial Connection
  int f = 0;
  while (wifiMulti.run() != WL_CONNECTED) {
    drawSpinnerFrame(f++);
    delay(100);
  }

  // Launch Background Task on Core 0
  xTaskCreatePinnedToCore(fetchTask, "FetchData", 10000, NULL, 1, &FetchTaskHandle, 0);
}

// --- LOOP (CORE 1) ---
int spinnerFrame = 0;
void loop() {
    if (newDataAvailable) {
        performRender(globalJsonData);
        newDataAvailable = false;
    }

    if (isFetching) {
        drawSpinnerFrame(spinnerFrame++);
        delay(60); 
    } else {
        delay(200);
    }
}


/* Sample Sensor Data Returned by getSensorData URL


{"sensorData":{"insideTemp":21.7,"insideHumidity":63,"greenhouseTemp":6.8,"greenhouseHumidity":254,"outsideTemp":6.3,"outsideHumidity":88,"time":"Monday, January 26 2026 11:41:22"},"temp1":[20.6,20.8,20.8,20.8,20.8,21,21.2,21.2,21.4,21.4,21.5,21.4,21.6,21.6,21.7,21.7,21.8,21.9,21.9,21.9,21.9,21.9,21.8,21.5,21.6,21.5,21.4,21.5,21.5,21.5,21.5,21.6,21.6,21.7,21.7,21.8,21.7,21.8,21.8,21.8,21.8,21.9,21.9,21.9,21.9,21.9,21.8,21.5,21.4,21.1,20.9,20.7,20.5,20.4,20.2,20,19.9,19.8,19.7,19.6,19.5,19.3,19.3,19.2,19.1,19,19,18.8,18.7,18.7,18.6,18.5,18.3,18.3,18.2,18.1,18.1,18,17.9,17.9,18,18.3,18.7,19.1,19.3,19.5,19.6,19.8,20,20.1,20.2,20.4,20.6,20.6,20.7,20.8,20.9,21,21,21.1,21.1,20.9,20.7,20.6,20.5,20.4,20.5,20.5,20.6,20.6,20.7,20.8,20.9,21,21.1,21.1,21.2,21.3,21.4,21.4,21.4,21.5,21.5,21.6,21.6,21.6,21.7,21.8,21.9,21.9,21.8,21.7,21.7,21.8,21.8,21.9,21.7,21.9,21.9,21.8,21.9,21.8,21.5,21.4,21.2,21,20.7,20.6,20.3,20.2,20,19.9,19.8,19.7,19.6,19.4,19.3,19.3,19.1,19.1,18.9,18.9,18.7,18.7,18.6,18.5,18.4,18.3,18.2,18.1,18,17.9,17.8,17.8,18.3,18.7,19.4,20.2,20.6,21.1,21.4,21.5,21.7,21.9,21.9,21.8,21.8,21.9,21.9,21.8,21.9,21.7],"temp2":[11.1,12.3,10.3,11.1,10.6,10.9,10.2,9.8,9.7,9.9,9.9,9.9,10.2,10.2,10.3,10,9.8,9.7,9.3,8.9,8.7,8.6,8.6,8.5,8.5,8.5,8.5,8.6,8.4,8.3,8.1,8.3,7.9,7.7,7.6,7.4,7.4,7.4,7.4,7.3,7.4,7.5,7.5,7.5,7.5,7.6,7.6,7.6,7.6,7.7,7.7,7.6,7.4,7.1,7.1,7.1,7,6.8,6.6,6.6,6.8,7,7,7,6.9,6.9,6.9,6.9,6.9,6.9,7,7,7,6.9,6.9,6.9,6.8,6.7,6.5,6.7,6.3,6.1,6.6,6.3,6,5.9,6.1,6.5,6.9,7.3,7.8,8.9,10,12.7,14,16.6,14.5,15.5,13.5,23.3,13.8,15.2,14,14.3,21.3,16.3,14.8,12.3,10.6,10.1,9.8,9.5,9.6,9.2,9,9,8.6,8.3,8.3,8,7.9,8,8.1,8.1,8,8,7.8,7.7,7.7,7.6,7.7,7.7,7.6,7.6,7.6,7.5,7.5,7.5,7.4,7.1,7.2,7.2,7.1,7,6.9,6.9,6.9,6.8,6.7,6.4,6.5,7.1,6.3,6.4,6.3,6.3,6.1,5.8,5.8,5.6,5.5,5.5,5.4,5.5,5.5,5.5,5.4,5.5,5.4,5.4,5.4,5.2,5.3,5.4,5.2,5.2,5.1,5.2,5.1,5.4,5.5,5.6,5.6,5.6,5.9,6.2,6.1,6,6.1,6.4,6.5,6.6],"temp3":[9.9,10.2,9.8,10.3,9.9,9.9,9.5,9.4,9.4,9.6,9.5,9.6,9.7,9.8,9.8,9.8,9.6,9.7,9.1,8.8,8.5,8.8,8.6,8.7,8.8,8.9,8.9,8.9,8.9,8.9,8.9,8.9,8.4,8.4,8.3,8.3,8.3,8.4,8.3,8.3,8.3,8.2,8.1,8,8.2,8.1,8.2,8.2,8.2,8.2,8.1,8.1,8.1,7.9,7.8,7.9,7.8,7.7,7.5,7.6,7.7,7.7,7.7,7.6,7.5,7.5,7.6,7.6,7.5,7.6,7.5,7.6,7.6,7.5,7.6,7.4,7.2,7.1,7.3,7.2,6.8,7,7.1,6.7,6.7,6.6,6.8,6.9,7.1,7.3,7.7,8.5,9,11,11.4,12.1,11.5,12.7,10.9,20.1,10.1,13.2,12.1,11.8,12.5,12.4,12.2,10.9,10.4,10.1,10,9.8,9.7,9.7,20,9.5,9.2,9.1,9.1,8.7,8.8,8.9,9,8.8,8.8,8.7,8.6,8.6,8.6,8.5,8.5,8.3,8.4,8.4,8.4,8.4,8.3,8.4,8.3,8.2,8.2,8,8,8.1,7.9,7.9,7.9,7.8,7.8,7.8,7.7,7.6,7.6,7.6,7.5,7.4,7.4,7.3,7.1,7,7,6.9,6.9,6.9,6.9,6.7,6.7,6.7,6.5,6.6,6.5,6.4,6.5,6.4,6.2,6.3,6.2,6.1,6.1,6.2,6.2,6.2,6.2,6.1,6.3,6.3,6.2,6.1,6.3,6.3,20,6.3],"labels":["24 Jan 10:58","24 Jan 11:13","24 Jan 11:28","24 Jan 11:43","24 Jan 11:59","24 Jan 12:14","24 Jan 12:29","24 Jan 12:44","24 Jan 13:00","24 Jan 13:15","24 Jan 13:30","24 Jan 13:45","24 Jan 14:01","24 Jan 14:16","24 Jan 14:31","24 Jan 14:46","24 Jan 15:02","24 Jan 15:17","24 Jan 15:32","24 Jan 15:47","24 Jan 16:03","24 Jan 16:18","24 Jan 16:33","24 Jan 16:48","24 Jan 17:04","24 Jan 17:19","24 Jan 17:34","24 Jan 17:49","24 Jan 18:05","24 Jan 18:20","24 Jan 18:35","24 Jan 18:50","24 Jan 19:06","24 Jan 19:21","24 Jan 19:36","24 Jan 19:51","24 Jan 20:07","24 Jan 20:22","24 Jan 20:37","24 Jan 20:52","24 Jan 21:08","24 Jan 21:23","24 Jan 21:38","24 Jan 21:53","24 Jan 22:09","24 Jan 22:24","24 Jan 22:39","24 Jan 22:54","24 Jan 23:10","24 Jan 23:25","24 Jan 23:40","24 Jan 23:55","25 Jan 00:11","25 Jan 00:26","25 Jan 00:41","25 Jan 00:56","25 Jan 01:12","25 Jan 01:27","25 Jan 01:42","25 Jan 01:57","25 Jan 02:13","25 Jan 02:28","25 Jan 02:43","25 Jan 02:58","25 Jan 03:13","25 Jan 03:29","25 Jan 03:44","25 Jan 03:59","25 Jan 04:15","25 Jan 04:30","25 Jan 04:45","25 Jan 05:00","25 Jan 05:15","25 Jan 05:31","25 Jan 05:46","25 Jan 06:01","25 Jan 06:16","25 Jan 06:32","25 Jan 06:47","25 Jan 07:02","25 Jan 07:17","25 Jan 07:33","25 Jan 07:48","25 Jan 08:03","25 Jan 08:18","25 Jan 08:34","25 Jan 08:49","25 Jan 09:04","25 Jan 09:19","25 Jan 09:35","25 Jan 09:50","25 Jan 10:05","25 Jan 10:20","25 Jan 10:36","25 Jan 10:51","25 Jan 11:06","25 Jan 11:21","25 Jan 11:37","25 Jan 11:52","25 Jan 12:07","25 Jan 12:22","25 Jan 12:38","25 Jan 12:53","25 Jan 13:08","25 Jan 13:23","25 Jan 13:39","25 Jan 13:54","25 Jan 14:09","25 Jan 14:24","25 Jan 16:59","25 Jan 14:50","25 Jan 15:06","25 Jan 15:21","25 Jan 15:36","25 Jan 15:51","25 Jan 16:07","25 Jan 16:23","25 Jan 16:38","25 Jan 16:54","25 Jan 17:09","25 Jan 17:24","25 Jan 17:39","25 Jan 17:55","25 Jan 18:10","25 Jan 18:25","25 Jan 18:40","25 Jan 18:56","25 Jan 19:11","25 Jan 19:26","25 Jan 19:41","25 Jan 19:57","25 Jan 20:12","25 Jan 20:27","25 Jan 20:42","25 Jan 20:58","25 Jan 21:13","25 Jan 21:28","25 Jan 21:43","25 Jan 21:59","25 Jan 22:14","25 Jan 22:29","25 Jan 22:44","25 Jan 22:59","25 Jan 23:15","25 Jan 23:30","25 Jan 23:45","26 Jan 00:00","26 Jan 00:16","26 Jan 00:31","26 Jan 00:46","26 Jan 01:01","26 Jan 01:17","26 Jan 01:32","26 Jan 01:47","26 Jan 02:02","26 Jan 02:18","26 Jan 02:33","26 Jan 02:48","26 Jan 03:03","26 Jan 03:19","26 Jan 03:34","26 Jan 03:49","26 Jan 04:04","26 Jan 04:20","26 Jan 04:35","26 Jan 04:50","26 Jan 05:05","26 Jan 05:21","26 Jan 05:36","26 Jan 05:51","26 Jan 06:06","26 Jan 06:22","26 Jan 06:37","26 Jan 06:52","26 Jan 07:07","26 Jan 07:23","26 Jan 07:38","26 Jan 07:53","26 Jan 08:08","26 Jan 08:24","26 Jan 08:39","26 Jan 08:54","26 Jan 09:09","26 Jan 09:25","26 Jan 09:40","26 Jan 09:55","26 Jan 10:10","26 Jan 10:25","26 Jan 10:41","26 Jan 10:56","26 Jan 11:12","26 Jan 11:27"],"hum1":[56,56,56,56,56,58,57,57,57,57,57,57,57,57,57,57,57,57,57,57,57,58,58,58,58,58,58,57,58,57,57,57,57,57,57,57,57,57,57,58,58,58,58,57,57,57,57,57,57,57,56,56,56,56,56,57,57,57,57,57,57,57,57,57,58,58,58,58,58,58,58,59,59,59,59,59,59,59,58,58,61,61,60,59,58,58,58,57,57,57,57,56,57,53,54,54,53,55,56,58,57,57,57,57,57,58,57,58,58,59,58,58,58,59,59,59,60,61,61,61,60,59,59,59,59,58,58,58,58,58,58,58,58,58,57,58,57,58,59,59,58,58,58,58,58,58,57,57,57,57,57,57,58,58,58,58,58,58,58,58,58,58,58,58,58,59,59,59,59,59,59,58,58,59,60,59,61,61,60,63,62,61,61,61,61,60,61,62,62,63,62,61],"hum2":[89,90,88,89,88,88,88,88,88,88,88,88,88,88,88,88,88,88,77,254,254,254,254,254,254,254,254,254,254,254,254,254,254,254,254,254,254,254,17,254,254,254,254,254,254,254,254,254,254,254,254,254,254,254,254,254,254,254,254,254,254,254,254,254,254,254,254,254,254,254,254,254,254,254,254,254,254,254,254,254,254,254,9,21,254,254,254,254,254,254,254,254,254,90,79,92,91,91,91,95,91,91,91,91,94,92,91,90,254,254,254,254,254,254,254,254,254,254,254,5,254,254,255,254,254,254,254,5,254,254,254,254,254,254,254,254,254,254,254,254,254,254,254,254,254,254,21,11,254,254,254,254,254,254,254,254,254,254,254,254,254,254,254,254,254,254,254,254,254,254,254,254,254,254,254,254,254,254,254,254,254,254,254,254,254,254,254,254,254,254,254,254],"hum3":[88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,89,89,90,89,90,88,93,88,91,90,89,90,90,90,88,88,88,88,88,88,88,20,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,20,88]}
*/
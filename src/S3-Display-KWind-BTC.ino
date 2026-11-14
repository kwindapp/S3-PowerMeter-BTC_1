#include <WiFiManager.h>   // FIX: include WiFiManager first to avoid FS conflict
#include <TFT_eSPI.h>      // version 2.5.0 library
#include "Latin_Hiragana_24.h"
#include "NotoSansBold15.h"
#include "NotoSansMonoSCB20.h"
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include "time.h"

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite sprite = TFT_eSprite(&tft);

#define latin Latin_Hiragana_24
#define small NotoSansBold15
#define digits NotoSansMonoSCB20

#define c1 0xE73C // white
#define c2 0x11C3 // gray
#define c3 0x9986 // red
#define c4 0x2CAB // green
#define c5 0xBDEF // gold
#define c6 0x9DAB // gold

int brightnesses[7] = {35, 70, 105, 140, 175, 210, 250};
int bright = 5;
int deb = 0;
int deb2 = 0;
bool scheme = 0;

const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600;
const int daylightOffset_sec = 3600;

char timeHour[3];
char timeMin[3];
char timeSec[3];
char day[3];
char month[6];
char year[5];
char timeWeekDay[16];   // FIX: buffer was 3, too small for "%A"
String dayInWeek;

int fromTop = 34;
int left = 40;
int width = 240;
int heigth = 80;
int longLine = 4;
uint16_t grays[60] = {0};
uint16_t lines[11] = {0};
int sec = 0;
int pos = 0;
int animationTimer = 0;
double fps = 0;

const char* websocket_server = "iot.kwind.app";
const uint16_t websocket_port = 443;
const char* websocket_path = "/?channels=bitcoin_halfing,bitcoin";
WebSocketsClient webSocket;
bool isCon = false;

float price;
String unit = "";
float percentChange24h = 0.0;
float currentBlockHeight;
float blocksUntilHalving;
String halvingDate = "";

void webSocketEvent(WStype_t type, uint8_t *payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.println("WebSocket Disconnected");
      break;

    case WStype_CONNECTED:
      Serial.println("WebSocket Connected");
      webSocket.sendTXT("Connected to WebSocket");
      break;

    case WStype_TEXT: {
      String message = String((char *)payload);
      Serial.println("WebSocket Message: " + message);

      // FIX: use JsonDocument (ArduinoJson 7)
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, message);
      if (error) {
        Serial.print("deserializeJson() failed: ");
        Serial.println(error.f_str());
        return;
      }

      currentBlockHeight = doc["currentBlockHeight"].as<float>();
      blocksUntilHalving = doc["blocksUntilHalving"].as<float>();
      halvingDate = doc["halvingDate"].as<String>();

      // FIX: containsKey is deprecated -> use is<T>()
      if (doc["price"].is<float>()) {
        price = doc["price"].as<float>();
      }

      if (doc["percent_change_24h"].is<float>()) {
        percentChange24h = doc["percent_change_24h"].as<float>();
      }

      if (doc["unit"].is<String>()) {
        unit = doc["unit"].as<String>();
      }

      break;
    }

    case WStype_BIN:
      Serial.println("WebSocket Binary Message");
      break;
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(15, OUTPUT);
  digitalWrite(15, 1);
  pinMode(0, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);

  sprite.createSprite(320, 180);
  sprite.setTextSize(2);
  sprite.drawString("search wifi with phone or", 0, 0);
  sprite.drawString("PC and connect KWindGadget", 0, 20);
  sprite.drawString("hit configure wifi and", 0, 40);
  sprite.drawString("connect to your local wifi", 0, 60);
  sprite.drawString("button 1 change station", 0, 85);
  sprite.drawString("button 2 brightness", 0, 105);
  sprite.drawString("cheers enjoy ;-) KWind App", 0, 130);
  sprite.pushSprite(0, 0);

  randomSeed(analogRead(0));
  for (int i = 0; i < 50; i++)
    grays[i] = tft.color565(i * 5, i * 5, i * 5);

  lines[0] = grays[5]; lines[1] = grays[10]; lines[2] = grays[20]; lines[3] = grays[30];
  lines[4] = grays[40]; lines[5] = grays[49]; lines[6] = grays[40]; lines[7] = grays[30];
  lines[8] = grays[20]; lines[9] = grays[10]; lines[10] = grays[5];

  sprite.createSprite(320, 170);
  sprite.setTextDatum(4);

  // FIX: new LEDC API (ESP32 Arduino core 3.x)
  ledcAttach(38, 10000, 8);
  ledcWrite(38, brightnesses[bright]);   // FIX: new API uses pin, not channel

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
}

void printLocalTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return;

  strftime(timeHour, 3, "%H", &timeinfo);
  strftime(timeMin, 3, "%M", &timeinfo);
  strftime(timeSec, 3, "%S", &timeinfo);
  strftime(timeWeekDay, 16, "%A", &timeinfo);   // FIX: buffer size
  dayInWeek = String(timeWeekDay);
  strftime(day, 3, "%d", &timeinfo);
  strftime(month, 6, "%b", &timeinfo);
  strftime(year, 5, "%Y", &timeinfo);
}

void draw() {
  sprite.fillSprite(c1);
  sprite.fillSmoothRoundRect(left + 30, fromTop - 20, width - 60, heigth, 8, c2, c1);
  sprite.fillSmoothRoundRect(left + 32, fromTop - 18, width - 64, heigth, 8, c1, c2);
  sprite.fillRect(left + 30, fromTop - 3, width - 60, 3, c1);
  sprite.fillRect(left + 80, fromTop - 21, width - 160, 4, c1);
  sprite.fillSmoothRoundRect(left, fromTop, width, heigth, 8, c2, c1);

  for (int i = 0; i < 6; i++) {
    sprite.fillRectHGradient(left + 20 + (i * 36), fromTop + 12, 15, 44, TFT_BLACK, grays[1]);
    sprite.fillRectHGradient(left + 35 + (i * 36), fromTop + 12, 15, 44, grays[2], TFT_BLACK);
    for (int j = 0; j < 11; j++) {
      int x1 = left + (j == 5 ? 40 : 43) + (i * 36);
      int x2 = left + 47 + (i * 36);
      int y = fromTop + 14 + (j * 4);
      sprite.drawLine(x1, y, x2, y, lines[j]);
    }
  }

  sprite.loadFont(latin);
  sprite.setTextColor(c2, c1);
  sprite.drawString("Bitcoin", 160, 18);
  sprite.unloadFont();

  sprite.loadFont(small);
  sprite.setTextColor(c1, c2);
  sprite.drawString(String(halvingDate), 160, fromTop + 68);
  sprite.setTextColor(c2);
  sprite.drawString(String(percentChange24h) + " %24h", 275, fromTop + 117);
  sprite.drawString("Current Block", 56, fromTop + 110);
  sprite.drawString("Blocks until 1/2", 60, fromTop + 125);
  sprite.drawString(String(currentBlockHeight, 0), 160, fromTop + 110);
  sprite.drawString(String(blocksUntilHalving, 0), 158, fromTop + 125);

  sprite.drawString(String(timeHour) + ":" + String(timeMin), 295, 15);
  sprite.drawString(String(month) + "/" + String(day), 30, 15);

  sprite.unloadFont();
  sprite.loadFont(digits);
  sprite.setTextColor(c1, c1);
  sprite.drawString(String(price, 0), 152, 130);

  int priceInt = static_cast<int>(price);
  String priceString = String(priceInt);
  while (priceString.length() < 6) priceString = "0" + priceString;

  sprite.drawString(String(priceString[0]), 70, fromTop + 35);
  sprite.drawString(String(priceString[1]), 106, fromTop + 35);
  sprite.drawString(String(priceString[2]), 142, fromTop + 35);
  sprite.drawString(String(priceString[3]), 178, fromTop + 35);
  sprite.drawString(String(priceString[4]), 214, fromTop + 35);
  sprite.drawString(String(priceString[5]), 250, fromTop + 35);

  sprite.unloadFont();

  sprite.fillSmoothRoundRect(80, fromTop + 83, 160, 17, 10, c4, c6);
  sprite.drawRect(100, fromTop + 89, 120, 8, TFT_BLACK);
  sprite.fillRectHGradient(101, fromTop + 90, 59, 6, grays[1], grays[44]);
  sprite.fillRectHGradient(160, fromTop + 90, 59, 6, grays[44], grays[1]);

  for (int i = 0; i < 11; i++) {
    sprite.fillRect(102 + (i * 10) + pos, fromTop + 90, 2, 4, c2);
  }

  sprite.pushSprite(0, 0);
}

void loop() {
  if (digitalRead(0) == 0) {
    if (deb == 0) {
      deb = 1;
      bright++;
      if (bright == 7) bright = 0;
      ledcWrite(38, brightnesses[bright]);   // FIX: new API uses pin
    }
  } else {
    deb = 0;
  }

  if (!isCon) {
    WiFiManager wifiManager;
    wifiManager.setConfigPortalTimeout(300);
    if (!wifiManager.autoConnect("KWind IoT", "")) {
      Serial.println("Failed to connect and hit timeout");
      delay(3000);
      ESP.restart();
    }

    Serial.println("Connected.");
    isCon = true;
    webSocket.beginSSL(websocket_server, websocket_port, websocket_path);
    webSocket.onEvent(webSocketEvent);
    webSocket.setReconnectInterval(5000);
  }

  webSocket.loop();
  printLocalTime();   // FIX: update time BEFORE drawing
  draw();

  sec++;
  pos++;
  if (pos == 10) pos = 0;
  animationTimer++;
  if (animationTimer == 4) {
    animationTimer = 0;
    longLine++;
    if (longLine == 11) longLine = 0;
  }
}
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <XPT2046_Touchscreen.h>
#include <RF24.h>

#define TFT_CS   7
#define TFT_DC   6
#define TFT_RST  5
#define TFT_BL   4
#define TOUCH_CS 14
#define RF_CE    15
#define RF_CSN   16

Adafruit_ILI9341 tft = Adafruit_ILI9341(&SPI, TFT_DC, TFT_CS, TFT_RST);
XPT2046_Touchscreen ts(TOUCH_CS);
RF24 radio(RF_CE, RF_CSN);

uint8_t mode = 0;
bool rfReady = false;
const uint8_t* pkt = (const uint8_t*)"\xFF\x00\x55\xAA\xFF\x00\x55\xAA\xFF\x00\x55\xAA\xFF\x00\x55\xAA\xFF\x00\x55\xAA\xFF\x00\x55\xAA\xFF\x00\x55\xAA\xFF\x00\x55\xAA";
void drawBtn(int x, const char* txt, uint16_t col, bool act) {
  tft.fillRoundRect(x, 184, 72, 48, 6, act ? col : 0x2124);
  tft.drawRoundRect(x, 184, 72, 48, 6, 0xFFFF);
  tft.setTextColor(act ? 0x0000 : 0xFFFF);
  tft.setTextSize(1);
  tft.setCursor(x + 18, 204);
  tft.print(txt);
}

void drawUI() {
  tft.fillScreen(0x0841);
  tft.fillRect(0, 0, 320, 26, 0x10A2);
  tft.drawFastHLine(0, 26, 320, 0x39E7);
  tft.setTextColor(0x07FF);
  tft.setTextSize(2);
  tft.setCursor(8, 6);
  tft.print("ESP32-S3 RF STUDIO");

  tft.setTextSize(1);
  tft.setCursor(265, 9);
  tft.setTextColor(rfReady ? 0x07E0 : 0xF800);
  tft.print(rfReady ? "RF:OK" : "RF:ER");

  tft.fillRoundRect(8, 34, 304, 142, 8, 0x10A2);
  tft.drawRoundRect(8, 34, 304, 142, 8, 0x39E7);

  tft.setCursor(20, 50);
  tft.setTextSize(2);
  tft.setTextColor(0xFFFF);
  if (mode == 0) {
    tft.print("MODE: STANDBY / IDLE");
    tft.setTextSize(1);
    tft.setTextColor(0x07FF);
    tft.setCursor(20, 85);
    tft.print("System Ready. Tap a button below.");
  } else if (mode == 1) {
    tft.setTextColor(0xFD20);
    tft.print("MODE: 2.4GHz SWEEP");
    tft.setTextSize(1);
    tft.setTextColor(0xFFFF);
    tft.setCursor(20, 85);
    tft.print("Sweeping Channels 2 - 80...");
  } else if (mode == 2) {
    tft.setTextSize(1);
    tft.setTextColor(0x07FF);
    tft.print("2.4GHz SPECTRUM ANALYZER");
  } else if (mode == 3) {
    tft.setTextColor(0x07E0);
    tft.print("MODE: PACKET TX");
    tft.setTextSize(1);
    tft.setTextColor(0xFFFF);
    tft.setCursor(20, 85);
    tft.print("Transmitting RF Packets on CH 40");
  }

  drawBtn(8,   "IDLE",  0x07FF, mode == 0);
  drawBtn(85,  "SWEEP", 0xFD20, mode == 1);
  drawBtn(162, "SCAN",  0x07E0, mode == 2);
  drawBtn(239, "PKT",   0xF800, mode == 3);
}
void setup() {
  Serial.begin(115200);
  ledcAttach(TFT_BL, 5000, 8);
  ledcWrite(TFT_BL, 220);

  SPI.begin(12, 13, 11);
  tft.begin();
  tft.setRotation(1);
  ts.begin();
  ts.setRotation(1);

  if (radio.begin()) {
    rfReady = true;
    radio.setAutoAck(false);
    radio.setPALevel(RF24_PA_MAX);
    radio.setDataRate(RF24_2MBPS);
    radio.openWritingPipe(0xE8E8F0F0E1LL);
  }
  drawUI();
}

void loop() {
  if (ts.touched()) {
    TS_Point p = ts.getPoint();
    int x = constrain(map(p.x, 200, 3800, 0, 320), 0, 319);
    int y = constrain(map(p.y, 200, 3800, 0, 240), 0, 239);
    if (y >= 180) {
      if (x < 82) mode = 0;
      else if (x < 160) mode = 1;
      else if (x < 238) mode = 2;
      else mode = 3;
      if (rfReady) {
        if (mode == 2) radio.startListening();
        else radio.stopListening();
      }
      drawUI();
      delay(250);
    }
  }

  if (rfReady) {
    if (mode == 1) {
      for (uint8_t ch = 2; ch <= 80; ch++) {
        radio.setChannel(ch);
        radio.writeFast(pkt, 32);
      }
      radio.txStandBy();
    } else if (mode == 2) {
      tft.fillRect(14, 68, 292, 100, 0x0841);
      for (uint8_t ch = 0; ch < 126; ch++) {
        radio.setChannel(ch);
        radio.startListening();
        delayMicroseconds(130);
        radio.stopListening();
        if (radio.testCarrier() || radio.testRPD()) {
          tft.drawFastVLine(18 + ch * 2, 85, 80, 0x07E0);
        }
      }
    } else if (mode == 3) {
      radio.setChannel(40);
      radio.write(pkt, 32);
      delay(50);
    }
  }
}

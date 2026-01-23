/*
  Display.cpp - Class responsible of controlling the display
  
  Copyright (C) 2020 -2021 @G4lile0, @gmag12 and @dev_4m1g0

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include "Display.h"
#include "graphics.h"
#include "../ConfigManager/ConfigManager.h"
#include "../Mqtt/MQTT_credentials.h"
#include "../Logger/Logger.h"
#include "../GnssManager/GnssManager.h"

OLEDDisplay* display;
OLEDDisplayUi* ui = NULL;

void msOverlay(OLEDDisplay *display, OLEDDisplayUiState* state);
void drawFrame1(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawFrame2(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawFrame3(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawFrame4(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawFrame5(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawFrame6(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawFrame7(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawFrame8(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
// void drawFrame9(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);


uint8_t frameCount = 8;
FrameCallback frames[] = { drawFrame1, drawFrame8, drawFrame3, drawFrame5, drawFrame2, drawFrame4, drawFrame6,drawFrame7 };
uint8_t overlaysCount = 1;
OverlayCallback overlays[] = { msOverlay };

unsigned long tick_interval;
int tick_timing = 100;
int graphVal = 1;
int delta = 1;
uint8_t oldOledBright = -1; // to force brightness update on first run

#define DISPLAY_TIMEOUT 300000 // 5 minutes
static unsigned long lastDisplayActivity = 0;

void displayResetTimeout() {
    lastDisplayActivity = millis();
}

void displayInit()
{
  lastDisplayActivity = millis();
  board_t board;
  uint8_t boardIdx = ConfigManager::getInstance().getBoard();
   if (!ConfigManager::getInstance().getBoardConfig(board))
    return;
  
  if (boardIdx == LILYGO_TBEAM_SUPREME || boardIdx == TTGO_TBEAM_SX1262) {
      Log::console(PSTR("Display: Initializing SH1106 for Supreme"));
      display = new SH1106Wire(board.OLED__address, board.OLED__SDA, board.OLED__SCL);
  } else {
      display = new SSD1306Wire(board.OLED__address, board.OLED__SDA, board.OLED__SCL);
  }

  ui = new OLEDDisplayUi(display);
  ui->setTargetFPS(60);
  ui->setActiveSymbol(activeSymbol);
  ui->setInactiveSymbol(inactiveSymbol);
  ui->setIndicatorPosition(BOTTOM);
  ui->setIndicatorDirection(LEFT_RIGHT);
  ui->setFrameAnimation(SLIDE_LEFT);
  ui->setFrames(frames, frameCount);
  ui->setOverlays(overlays, overlaysCount);

  #if CONFIG_IDF_TARGET_ESP32S3                                      // Heltec Lora 32 V3 patch to enable Vext that power OLED
  if (ConfigManager::getInstance().getBoard()== HELTEC_LORA32_V3 ) { 
      pinMode (36, OUTPUT); 
      digitalWrite(36, LOW);
      }
  #endif

  if (board.OLED__RST != UNUSED) {
    if (!((strcmp(ESP.getChipModel(), "ESP32-PICO-D4") == 0) && (board.OLED__RST == 16)))  {
        pinMode(board.OLED__RST, OUTPUT);
        digitalWrite(board.OLED__RST, LOW);
        delay(50);
        digitalWrite(board.OLED__RST, HIGH);
      }
  }

  /* ui init() also initialises the underlying display */
  ui->init();

  if (ConfigManager::getInstance().getFlipOled())
    display->flipScreenVertically();
}

void msOverlay(OLEDDisplay *display, OLEDDisplayUiState* state)
{
  display->setTextAlignment(TEXT_ALIGN_RIGHT);
  display->setFont(ArialMT_Plain_10);

  struct tm* timeinfo;
  time_t currenttime = time (NULL);
  if(currenttime < 0)
  {
    Log::error(PSTR("Failed to obtain time"));
    return;
  }
  timeinfo = localtime (&currenttime);

  // Usar buffer estático para evitar fragmentación del heap
  char timeBuffer[20];
  snprintf(timeBuffer, sizeof(timeBuffer), "%2d:%02d:%02d", 
           timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
  display->drawString(128, 0, timeBuffer);

  if (ConfigManager::getInstance().getDayNightOled())
  {
    if (timeinfo->tm_hour < 6 || timeinfo->tm_hour > 18) display->normalDisplay(); else display->invertDisplay(); // change the OLED according to the time. 
  }
}

// ... (existing code) ...

void drawFrame8(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y)
{
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->drawString(x+2, 16+y, "MQTT:" );
  if (status.mqtt_connected) { display->drawString( x+7,  26+y, "ON"); }  else { display->drawString( x+5,  26+y, "OFF"); }
  display->drawXbm(x + 32, y + 4, WiFi_Logo_width, WiFi_Logo_height, WiFi_Logo_bits);

  // GNSS on Right
  display->setTextAlignment(TEXT_ALIGN_RIGHT);
  char gnssBuf[16];
  if (GnssManager::getInstance().hasFix()) {
      snprintf(gnssBuf, sizeof(gnssBuf), "GPS:%d", GnssManager::getInstance().getSatellites());
  } else {
      snprintf(gnssBuf, sizeof(gnssBuf), "GPS:..");
  }
  display->drawString(x+126, 16+y, gnssBuf);
  if (GnssManager::getInstance().hasFix()) {
      display->drawString(x+126, 26+y, "FIX");
  } else {
      display->drawString(x+126, 26+y, "No Fix");
  }

  // The coordinates define the center of the text
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->drawString(64 + x, 42 + y, "Connected " + (WiFi.localIP().toString()));
}  

void displayShowConnected()
{
  display->clear();
  display->drawXbm(34, 0 , WiFi_Logo_width, WiFi_Logo_height, WiFi_Logo_bits);
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->drawString(64 , 34 , "WiFi:" + String(ConfigManager::getInstance().getWiFiSSID()));
  display->drawString(64 , 44 , "OTP --->    " + String(mqttCredentials.getOTPCode ()) );
  display->drawString(64 ,53 , (WiFi.localIP().toString()));
  display->display();
}

void displayShowInitialCredits()
{
  display->setFont(ArialMT_Plain_16);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->drawString(0,5,"tinyGS");
  display->setFont(ArialMT_Plain_10);
  display->drawString(50,23,"ver. " + String(status.version));

  display->drawString(5,38,"by @gmag12 @4m1g0");
  display->drawString(40,52,"& @g4lile0");
  display->display();
}

void displayShowApMode()
{
  display->clear();
  display->setFont(ArialMT_Plain_10);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->drawString(0, 6,"Connect to AP:");
  display->drawString(0,18,"->"+String(ConfigManager::getInstance().getThingName()));
  display->drawString(5,32,"to configure your Station");
  display->drawString(10,52,"IP:   192.168.4.1");
  display->display();
}

void displayShowStaMode(bool ap)
{
  display->clear();
  display->drawXbm(34, 0 , WiFi_Logo_width, WiFi_Logo_height, WiFi_Logo_bits);
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->drawString(64 , 35 , "Connecting " + String(ConfigManager::getInstance().getWiFiSSID()));
  if (ap)
    display->drawString(64 , 52 , "Config AP available");
  display->display();
}

/**
 * Updates the display brightness based on the configuration.
 * If the brightness has changed, it sets the new brightness or turns off the display.
 * Also updates the UI if screen is on.
 */
void displayUpdate()
{
  // Get the current OLED brightness from configuration
  uint8_t oledBright = ConfigManager::getInstance().getOledBright();

  // Check for timeout
  if (oledBright > 0 && millis() - lastDisplayActivity > DISPLAY_TIMEOUT) {
      displayTurnOff();
      return;
  }

  // Check if brightness has changed
  if (oldOledBright != oledBright) {
    if (oledBright) {
      // Set the new brightness
      display->setBrightness(2*oledBright);
    } else {
      // Turn off the display if brightness is 0
      displayTurnOff();
    }
    // Save the brightness value
    oldOledBright = oledBright;
  }

  if (oledBright) {
    // Update the UI if screen is on
    ui->update();
  }
}

void displayTurnOff()
{
  display->displayOff();
  oldOledBright = 0;
}

void displayNextFrame() {
  if (ui)
    ui->nextFrame();
}

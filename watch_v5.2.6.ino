/*
  notif display and scrolling done

  for pushSprite:
  tx, ty is the position on the TFT of the top left corner of the plotted sprite portion
  sx, sy is the position of the top left corner of a window area within the sprite
  sw, sh are the width and height of the window area within the sprite

  try increasing stack size of tasks in case of SW_CPU Reset
  Setup2_ST7735.h
  #define TFT_MOSI 23
  #define TFT_SCLK 18
  #define TFT_CS   15  // Chip select control pin
  #define TFT_DC    2  // Data Command control pin
  #define TFT_RST   4  // Reset pin (could connect to RST pin)
*/

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <ESP32Time.h>
#include <TFT_eSPI.h>
#include <FreeRTOS.h>

#define BUILTINLED 2
#define displayEn 21                                                                //display enable pin
#define notifCount 10                                                               // number of notifications to be stored
#define notifSize 100                                                               // size of each notification
#define touchThreshold 50
#define bgColor TFT_BLACK                                                           // background color of text
#define swipeDuration 300                                                           // these many milliseconds of difference in touching both the buttons will be taken as a swipe 

#define IWIDTH  128                                                                 // height and width of notification sprite
#define IHEIGHT 256

#define SERVICE_UUID           "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define CHARACTERISTIC_UUID_RX "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
#define CHARACTERISTIC_UUID_TX "6e400003-b5a3-f393-e0a9-e50e24dcca9e"

uint8_t* pData;
char notifData[notifCount][notifSize];
uint8_t notifPtr = notifCount;                                                      // keeps a track of where the last notif data was written
uint8_t batteryLevel[] = {0xAB, 0x00, 0x05, 0xFF, 0x91, 0x80, 0x00, 0x64};          // last second: 00-not charging, 01-charging, 02-charged; last: battery%

bool touched1 = false;                                                              // touch input 1
bool touched2 = false;                                                              // touch input 2

unsigned long touchTime1 = 0;                                                       // time at which touch 1 is touched
unsigned long touchTime2 = 0;                                                       // time at which touch 2 is touched

bool topClick = false;                                                              // touch 1 clicked
bool bottomClick = false;                                                           // touch 2 clicked
bool swipeDown = false;                                                             // touch 1 + touch 2
bool swipeUp = false;                                                               // touch 2 + touch 1

ESP32Time rtc;                                                                      // object for ESP32 time management library
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite notifSprite = TFT_eSprite(&tft);                                                // Create Sprite object "notifSprite" with pointer to "tft" object. the pointer is used by pushSprite() to push it onto the TFT
TaskHandle_t button_input;
TaskHandle_t button_input2;
TaskHandle_t main_task;

static BLECharacteristic* pCharacteristicTX;
static BLECharacteristic* pCharacteristicRX;
static bool deviceConnected = false;

//::::::::::::::::::::::::::::::::::::::::: Functions ::::::::::::::::::::::::::::::::::::::::::::

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
    }
    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
    }
};

void dataFilter(std::string value) {                                // AB 00 xx FF yy           ; xx = length;  yy = command type

  //  Serial.print("Notify callback for characteristic ");
  //  Serial.print(pData[4], HEX);                                  // debug point
  //  //  Serial.print(pCharacteristic->getUUID().toString().c_str());
  int dataLen = value.length();
  //  Serial.print(" of data length ");
  //  Serial.println(dataLen);

  if (pData[0] != 0xAB) {                                           // if first byte is not AB, it is an extension message of long notifs
    //first packet has 12 bytes of data and each subsequent packet has 19 bytes so notifPtr has to be kept same and data has to be written
    Serial.print("packet number: ");
    byte packetNo = pData[0];
    Serial.println(packetNo);                                       // debug point
    for ( byte i = 1; i < dataLen ; i++) {
      notifData[notifPtr][(packetNo * 19) + 12 + i - 1] = (char)pData[i]; //placing packets to their respective position in allocated memory
      Serial.print((char)notifData[notifPtr][(packetNo * 19) + 12 + i - 1]); //debug point
    }
    Serial.println();
  }

  else {

    switch (pData[4]) {                                             // pData[4] = yy

      case 0x93:                                                    // set time, yy = 0x93
        Serial.println("setting time");                             // YYYY MM DD h m s
        rtc.setTime(pData[13], pData[12], pData[11], pData[10], pData[9], ((pData[7] << 8) + pData[8]));        // AB 00 xx FF 93 80 00 YY YY MM DD h m s
        Serial.println(rtc.getTimeDate());                                                                      // debug point
        break;

      case 0x91:                                                    // battery request, yy = 0x91
        batteryLevel[7]--;                                          // debug point, reducing battery level by 1% everytime
        pCharacteristicTX->setValue(batteryLevel, 8);
        pCharacteristicTX->notify();
        Serial.println("Sent Battery level");
        break;

      case 0x72:                                                    // notifs, yy = 0x72

        if (notifPtr == 0) {                                        // overwrite notifs when maxed out
          notifPtr = notifCount;
        }

        switch (pData[6]) {                                         // AB 00 XX FF 72 80 zz 02
          case 0x01:                                                // call, zz = 0x01              special notification to be configured to turn on screen and display PENDING
            Serial.print("Call from: ");
            for ( byte i = 8; i < dataLen ; i++) {
              Serial.print((char)pData[i]);
            }
            Serial.println();
            break;

          default:                                                  // message, general notif, zz = 0x03; whatsapp, zz = 0x0A, all the general notifications are stored in the memory throug this
            notifPtr--;
            for ( byte i = 0; i < (dataLen - 8); i++) {
              notifData[notifPtr][i] = (char)pData[i + 8];
              Serial.print((char)notifData[notifPtr][i]);           // debug point to show on screen
            }
            Serial.print("saved to memory");                        // debug point
            break;
        }
        break;

      case 0x71:                                                    // find watch, yy = 0x71
        Serial.println("**************RINGING SOMEONE IS LOOKING FOR ME**************");
        break;

    }
  }

  //  Serial.print("TX  ");
  //  for (int i = 0; i < dataLen; i++) {
  //    Serial.printf("%02X ", pData[i]);
  //  }
  //  Serial.println();

}

class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string value = pCharacteristic->getValue();
      pData = pCharacteristic->getData();                           // global
      if (pData != NULL) {
        dataFilter(value);                                          // callback sends data to dataFilter function
      }
    }
};

void initBLE() {
  BLEDevice::init("Vardhan's Watch");
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);
  pCharacteristicTX = pService->createCharacteristic(
                        CHARACTERISTIC_UUID_TX,
                        BLECharacteristic::PROPERTY_NOTIFY
                      );
  pCharacteristicRX = pService->createCharacteristic(
                        CHARACTERISTIC_UUID_RX,
                        BLECharacteristic::PROPERTY_WRITE |
                        BLECharacteristic::PROPERTY_WRITE_NR
                      );
  pCharacteristicRX->setCallbacks(new MyCallbacks());
  pCharacteristicTX->addDescriptor(new BLE2902());


  pService->start();
  // BLEAdvertising *pAdvertising = pServer->getAdvertising();      // this still is working for backward compatibility
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);                              // functions that help with iPhone connections issue
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();
  Serial.println("Characteristic defined! Now you can read it in your phone!");
}

void printCentre(String string, bool nextLine = false) {            // print centre aligned text
  tft.setCursor((( tft.width() - tft.textWidth(string) ) / 2), tft.getCursorY());
  if (nextLine) {
    tft.println(string);
  }
  else {
    tft.println(string);
  }
}

void IRAM_ATTR touchCallback() {
  touched1 = true;
  touchTime1 = millis();
}

void IRAM_ATTR touchCallback2() {
  touched2 = true;
  touchTime2 = millis();
}

//::::::::::::::::::::::::::::::::::::::::: Tasks ::::::::::::::::::::::::::::::::::::::::::::

void mainTask(void * parameters) {
  while (1) {

    if (topClick || bottomClick || swipeDown || swipeUp) {

      topClick = false;
      bottomClick = false;
      swipeUp = false;
      swipeDown = false;

      tft.fillScreen(bgColor);
      tft.setCursor(0, 0);
      tft.setTextSize(1);
      tft.setTextFont(7);                                                       // fonts 1 and 7 are noice

      printCentre(rtc.getTime("%I"), true);
      printCentre(rtc.getTime("%M"));
      digitalWrite(displayEn, HIGH);                                            // turning on backlight

      for (byte i = 0; i < 40; i++) {                                           // polling

        if (topClick) {                                                         // top button for notification

          topClick = false;
          notifSprite.fillSprite(bgColor);                                      // the sprite is created in the setup function, it has to be updated and pushed
          notifSprite.setCursor(0, 0);

          if (notifPtr == notifCount) {                                         // initial condition
            notifSprite.print("No notifications");
          }

          else {
            for (byte i = notifPtr; i < notifCount; i++) {                      // iterates from the most recent notif to the last one
              for (byte j = 0; j < notifSize; j++) {
                notifSprite.print((char)notifData[i][j]);
              }
              notifSprite.println("\n");
            }
          }

          notifSprite.pushSprite(0, 0);

          // scroll input
          int nSpriteXPos = 0;

waitToSwipe:
          for (byte i = 0; i < 40; i++) {

            if (swipeUp) {

              swipeUp = false;

              for (byte i = 0; i < 6; i++) {
                nSpriteXPos += i;
                if ((nSpriteXPos + 128) > IHEIGHT) {
                  nSpriteXPos = IHEIGHT - 128;
                }
                notifSprite.pushSprite(0, 0, 0, (nSpriteXPos), 128, 128);
              }

              for (byte i = 6; i > 0; i--) {
                nSpriteXPos += i;
                if ((nSpriteXPos + 128) > IHEIGHT) {
                  nSpriteXPos = IHEIGHT - 128;
                }
                notifSprite.pushSprite(0, 0, 0, (nSpriteXPos), 128, 128);
              }

              goto waitToSwipe;
            }

            else if (swipeDown) {

              swipeDown = false;

              for (byte i = 0; i < 6; i++) {
                nSpriteXPos -= i;
                if (nSpriteXPos < 0) {
                  nSpriteXPos = 0;
                }
                notifSprite.pushSprite(0, 0, 0, (nSpriteXPos), 128, 128);
              }
              
              for (byte i = 6; i > 0; i--) {
                nSpriteXPos -= i;
                if (nSpriteXPos < 0) {
                  nSpriteXPos = 0;
                }
                notifSprite.pushSprite(0, 0, 0, (nSpriteXPos), 128, 128);
              }
              
              goto waitToSwipe;
            }
            
            else if (bottomClick) {
              bottomClick = false;
            }
            
            vTaskDelay( 100 / portTICK_PERIOD_MS );
          }
          break;
        }
        vTaskDelay( 100 / portTICK_PERIOD_MS );
      }
      digitalWrite(displayEn, LOW);
    }
  }
}

void buttonInput( void * parameters) {
  while (1) {
    if (touched1) {
      vTaskDelay( swipeDuration / portTICK_PERIOD_MS );
      if ((touched2) & (touchTime2 - touchTime1 < swipeDuration)) {
        topClick = false;
        bottomClick = false;
        swipeUp = true;
        swipeDown = false;
        Serial.println("swipe up");
        vTaskDelay( 400 / portTICK_PERIOD_MS );                                // cooldown delay to avoid retriggering and top being executed
      }
      else if (!touched2) {
        topClick = true;
        bottomClick = false;
        swipeUp = false;
        swipeDown = false;
        Serial.println("top");
        vTaskDelay( 400 / portTICK_PERIOD_MS );                                // cooldown delay to avoid retriggering
      }
      touched1 = false;
      touched2 = false;
    }
    vTaskDelay( 2 / portTICK_PERIOD_MS );
  }
}

void buttonInput2( void * parameters) {
  while (1) {
    if (touched2) {
      vTaskDelay( swipeDuration / portTICK_PERIOD_MS );
      if ((touched1) & (touchTime1 - touchTime2 < swipeDuration)) {
        topClick = false;
        bottomClick = false;
        swipeUp = false;
        swipeDown = true;
        Serial.println("swipe down");
        vTaskDelay( 400 / portTICK_PERIOD_MS );                                // cooldown delay to avoid retriggering and bottom being executed
      }
      else if (!touched1) {
        topClick = false;
        bottomClick = true;
        swipeUp = false;
        swipeDown = false;
        Serial.println("bottom");
        vTaskDelay( 400 / portTICK_PERIOD_MS );                                // cooldown delay to avoid retriggering
      }
      touched1 = false;
      touched2 = false;
    }
    vTaskDelay( 2 / portTICK_PERIOD_MS );
  }
}

//::::::::::::::::::::::::::::::::::::::::: Setup ::::::::::::::::::::::::::::::::::::::::::::

void setup() {
  Serial.begin(115200);
  Serial.println("Booting");

  tft.init();
  tft.setTextColor(TFT_ORANGE, bgColor);                            // (foreground, background) overwrites so previous text doesn't need to be cleared
  tft.fillScreen(bgColor);
  tft.setTextWrap(true, false);                                     // to prevent overflows, on the Y axis it is turned off or it will overwrite the first lines
  //  tft.writecommand(0x39);                                           // IDLE mode, 8 bit color to minimize power, only basic colors allowed
  //  tft.setFreeFont(FSB9);
  //  tft.setTextDatum(TC_DATUM);                                   // works with tft.drawString()
  pinMode(displayEn, OUTPUT);

  pinMode(BUILTINLED, OUTPUT);
  digitalWrite(BUILTINLED, HIGH);
  //  memset(notifData, 0, sizeof(notifData));                      // initializing notif array with zero

  touchAttachInterrupt(T4, touchCallback, touchThreshold);          // T4, GPIO13
  touchAttachInterrupt(T9, touchCallback2, touchThreshold);         // T9, GPIO32

  initBLE();
  //  delay(10000);
  //  digitalWrite(displayEn, HIGH);                                // turning on backlight
  //  digitalWrite(displayEn, HIGH);                                // turning on backlight
  //  tft.setTextSize(1);
  //  tft.print("A long stream of data that is meant to overflow the available space on the display so much that the entire display will be filled and we would need to scroll lets see if it can do that on its own or we would have to do that ourselves. Oh man I need a content writer now to write more text this is just so much of rubbish, I am tired of it myself.");    //debug point


  notifSprite.createSprite(IWIDTH, IHEIGHT);                                // these sprites will later be updated and pushed to screen
  notifSprite.setTextSize(1);                                               // Font size scaling is x1
  notifSprite.setTextFont(1);                                               // Font 4 selected
  notifSprite.setTextColor(TFT_WHITE);                                      // Black text, no background colour
  notifSprite.setTextWrap(true, true);                                      // Turn off if you enable horizontal scrolling

  xTaskCreate(buttonInput,                                          //func name
              "taking inputs from upper touch button",              //description
              700,                                                  //stack size
              NULL,                                                 //task parameters
              1,                                                    //priority
              &button_input);                                       //task handle

  xTaskCreate(buttonInput2,                                         //func name
              "taking inputs from upper touch button",              //description
              700,                                                  //stack size
              NULL,                                                 //task parameters
              1,                                                    //priority
              &button_input2);                                      //task handle

  xTaskCreate(mainTask,
              "main program to wake screen and create subtasks",
              3000,
              NULL,
              2,
              &main_task);
}

void loop() {
}

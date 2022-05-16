Time and notifications data can be fetched from smartphone to watch. The app can be found here: https://github.com/fbiego/DT78-App-Android 
Credits to fbiego for reverse engineering the DT78 smartwatch project. 

The code here uses RTOS tasks and BLE for communication. The dataFilter function decodes the BLE packets based on the command types explained by fbiego here: https://github.com/fbiego/dt78 

The display used is ST7735 which uses TFT_eSPI library for communication.

This is a standalone library that contains both graphics functions
and the TFT chip driver library. It supports the ESP8266, ESP32 and
STM32 processors with performance optimised code. Other Arduino IDE
compatible boards are also supported but the library then uses
generic functions which will be slower. The library uses 32 bit
variables extensively so this will affect performance on 8 and 16
bit processors.

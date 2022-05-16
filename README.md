Time and notifications data can be fetched from smartphone to watch. The app can be found here: https://github.com/fbiego/DT78-App-Android 
Credits to fbiego for reverse engineering the DT78 smartwatch project. 

The code here uses RTOS tasks and BLE for communication. The dataFilter function decodes the BLE packets based on the command types explained by fbiego here: https://github.com/fbiego/dt78 

The display used is ST7735 which uses TFT_eSPI library for communication.

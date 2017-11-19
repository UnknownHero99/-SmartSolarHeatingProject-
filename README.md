# Smart Solar Heating Project
It's a program for solar collector controller, esp32.

## Parts used:
- Esp32
- 4x ds18b20 temperature sensors
- BME280 pressure sensor
- Rtc module
- 3.5" TFT lcd
- Rgb led diode (for status signalization)

## You can set:
- manual pump control(on/off) or automatic
- min temp difference , if temp difference is lower than this pump wont be enabled
- max temp of boiler , if boiler is higher than this pupm will stop
- min temp of collector , if temp of collector is lowet than this pump wont be enabled
- max temp of collector , if temp of collecter is higher than this and boiler temp is lower than max temp of boiler pump will be enabled
- RTC time
- altitude(for pressure sensor)

## Features:
- Log to SD card
- Smart pump control
- Esp32 web interface to control pump options are Pump on, Pump off and Automatic
- Log to thingspeak, displaying graphs in esp32 web interface
- Statistics (Max Min of temperatures, pressure etc.)
- Current sensor data
- Encoder controlled
- RGB signalization

## RGB signalization:
- Green, pump is operating
- Violet, pump is not operating
- Red, overheated
- Blue, sensor problem, check serial monitor for more details

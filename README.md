HomeServer
==========

Home automation software dealing with following involved hardware components:
- custom main board containing ATMega128A microcontroller (http://www.aliexpress.com/item/ATMEL-ATMega128-M128-AVR-Minimum-Core-Development-system-board-Module-ISP-JTAG/1893754515.html), upgraded with a 16MHz crystal, supplied with 3.3V.
- custom Wiznet W5500 TCT/IP Ethernet module (http://www.ebay.com/itm/321589940136) connected to the main board over SPI, same 3.3V as the main board.
- mini SD-card module connected to the main board over SPI, same 3.3V.
- Viessmann Vitocal 300-A heat pump, having a self-made Optolink interface connected to the home network over a WiFi-UART bridge board type HLK-RM04, 5V local.
- FAST EnergyCam with open wire interface cable supplied with 3.3V, connected to the home network through a WiFi-UART bridge (HLK-RM04), 5V local.

Software description
--------------------
The software is using the Time (https://github.com/PaulStoffregen/Time) and SdFat (https://github.com/greiman/SdFat) libraries for Arduino.
Each odd minute the time is updated from the local gateway Fritzbox acting as NTP server.
Each even minute some parameters (temperature values and status information) are read out from the heat pump and stored to the SD-card. Also, the OCR-ed value of the actual mechanical power meter status received from the EnergyCam nd is stored to the SD-card, too.
The home index page shows those values together with the current date and time information.
A more datailed graphical interface shows the development of these parameters over a day.

Todo:
----
- integrate radio interface module based on nRF905 radio IC (over SPI) to communicate with the central ventilation system Pluggit P300.
- integrate CC1101 module (over SPI) to radio control some wall outlet units.
- implement software module to control my satellite receiver DM800se over TCP.


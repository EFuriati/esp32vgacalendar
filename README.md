# esp32vgacalendar
Calendar with Esp32 and VGA Out 



First Steps:

Download the libraries:

"fabgl.h" // http://www.fabglib.org/ <br>
ESPAsyncWebServer.h <br>
AsyncElegantOTA.h <Br>
ESPAsyncWiFiManager.h <br>
NTPClient.h <br>
IRsend.h <br>
TimeLib.h <br>
LITTLEFS.h <br>
Adafruit_BMP280.h // if using temperature sensor <br>
ESPmDNS.h // to find the esp32 on your network <br>


include one of:
  
  #include "ptbr.h"  // english en.h or portuguese ptbr.h <Br>
  #include "en.h"  <br>
  
  Define your timezone:
  #define GMT_TIME_ZONE -3  // change to your timezone

  

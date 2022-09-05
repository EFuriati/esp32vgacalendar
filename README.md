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

  First boot:
  
  When first initializaded, if the esp32 isn't formatted, or the files are not found, the software will format and write some basic configuration,
  the initial password will be:  admin:admin
  
  Follow the instructions for the vga connection on :
  
  http://www.fabglib.org/
  
  I use the IRsend library to control the tv (on/off mostly), and i use a ldr connected to the tv led to know the tv status (tv led on = tv is off, tv led off = tv on).
  
  To update, i use the AsyncElegantOTA library, because the esp32 is connected to the tv, and away from the computer, is possible to set a password for the upgrade.
  The link for the upgrade is:
  http://esp32ip/update
  
  
  
  
  
  

/*  full.cpp - Comprehensive example showing use of the MkWifiDev library including
               changing of the mode flags, setting the time and logging from other files.

    This demo will try to connect to the specified WiFi network and if successful, allow
    OTA (over-the-air) updates to be applied. Log messages are sent to the local serial
    port and to a remote terminal if connected.
    
    To upload OTA capable firmware for the first time use serial upload like normal,
    thereafter you may use serial or OTA.

    If using platformio, the following will be useful in platformio.ini environment:
      upload_port = YOUR_DEVICE         ; Device name or IP address for OTA
      upload_protocol = espota                
      ;upload_flags = --auth=admin      ; Only necessary if authentication is enabled 
        
      monitor_raw = yes                       ; Enable text coloring in Serial Monitor
      monitor_port = socket://YOUR_DEVICE:23  ; Use socket for remote, serial port for local

    PuTTY is an excellent terminal program - use Port 23 & set 'Connection type' to 'Raw'
      (Set 'Terminal' | 'Local line editing' to 'Force off' to ensure terminal characters
        are sent immidiately instead of waiting for enter to be pressed)

    For a simpler example refer to the 'basic' example, or 'nowifi' for how to use the 
    library for local only (ie no remote/OTA).

    This file is part of MkWifiDev, a library which simplifies cable-free development. It 
    enables colorised logging to local & remote terminals and supports Arduino OTA firmware 
    updates.  Available at https://github.com/zaddi/MkWifiDev

    MkWifiDev is distributed under the MIT License
*/

#include "MkWifiDev.h"
#include "another.h"

//#define DEBUG_SHOW_FILE      // Uncomment this to show the calling filename & line number in output
//#define DEBUG_SHOW_FUNCTION  // Uncomment this to show the calling function name in output

//#define FILE_LOGGING_ENABLED   // Uncomment this (and check SD/SPI settings) to enable logging to file

// The following lines show how to exclude a message type from a build
//#undef DBG_VERBOSE                  // Undefine first to avoid compiler warning
//  #define DBG_VERBOSE(...)    { }   // Replaces the log macro with nothing ie removing the log message


#ifdef FILE_LOGGING_ENABLED
  #include "SPI.h"
  #include "SD.h"

  // Update these definitions for your hardware (if you've defined FILE_LOGGING_ENABLED)
  #define SPI_MISO    GPIO_NUM_16
  #define SPI_MOSI    GPIO_NUM_15
  #define SPI_SCK     GPIO_NUM_11
  #define SPI_SD_CS   GPIO_NUM_42

  File logFile;
#endif

//#define LED_BUILTIN  GPIO_NUM_?   // If not defined for your board, you can specify a pin here to show wifi status
//#define LED_INVERT                // Uncomment this if you need to invert the LED polarity

#ifdef LED_BUILTIN
  #ifdef LED_INVERT
    #define LED_ON    (HIGH)
    #define LED_OFF   (LOW)
  #else
    #define LED_ON    (LOW)
    #define LED_OFF   (HIGH)
  #endif
#endif

#define GMT_OFFSET       (-5*3600)    // Set timezone offset in seconds (eg US Eastern is -5h)
#define DAYLIGHT_OFFSET  (0)          // Set to your dst offset if active (eg 3600 for 1h)

#include "wifi_credentials.h"   // Remove this line and enter your wifi details
                                //  (enclosed in "quotes") in the lines below

const char* ssid     = YOUR_WIFI_SSID_HERE;         // eg = "MyHomeWiFi";
const char* password = YOUR_WIFI_PASSWORD_HERE;     // eg = "MyPa55w0rd";
const char* devname  = DEVICE_NAME;                 // eg = "ESP-WIDGET1";

uint8_t testBuffer[128];

void setup() {

#ifdef LED_BUILTIN
  pinMode(LED_BUILTIN, OUTPUT);         // set the LED pin mode
  digitalWrite(LED_BUILTIN, LED_OFF);   // and turn it off to start
#endif

  //dbgTAG = "MkWifiDev TestApp";       // Set the tag if desired to identify the source of debug messages

  Serial.begin(115200);

  //WifiDev.setSerial(Serial2);         // If we wanted to log to a different serial port

  WifiDev.setDisplayModeFlags(MkWifiDev::SHOW_MILLISECONDS);    // Show milliseconds in message timestamps
  //WifiDev.setDisplayModeFlags(MkWifiDev::WIDE_HEXDUMP);       // Enable wide hex dump display (32 bytes vs 16)

  //WifiDev.clearDisplayModeFlags(MkWifiDev::SHOW_TIMESTAMPS);   // Hide the timestamps with each message
  //WifiDev.clearDisplayModeFlags(MkWifiDev::SHOW_COLOUR);       // Disable message coloring

  DBG_VERBOSE("Starting MkWifiDev Demo");
  DBG_INFO("Anything before calling WifiDev.begin() will only be sent to Serial port");

  WifiDev.begin(ssid, password, devname);   // Start wifi

  WifiDev.configTime(GMT_OFFSET, DAYLIGHT_OFFSET);   // Enable internet time sync

  //ArduinoOTA.setPassword("admin");    // Enable OTA authentication (password required to apply updates)

  // Write some test data to our testBuffer
  for(uint32_t i=0; i<sizeof(testBuffer); i++)
    testBuffer[i] = i;

  another_setup();

}

uint8_t dbgc = 0;
bool color = true;

void sdcard() {
#ifdef FILE_LOGGING_ENABLED
  if(logFile) {
    logFile.close();
    DBG_ALERT("Stopped logging to SD Card");
    return;
  }
  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, SPI_SD_CS); // map  SPI pins SCK, MISO, MOSI, SS

  if(SD.begin(SPI_SD_CS)) {   
    DBG_INFO("Mounted SD Card (Size: %d GB)", SD.cardSize() >> 30);

    logFile = SD.open("/logfile.txt", FILE_WRITE);

    if(logFile) {
      WifiDev.setLogFile(logFile);
      DBG_ALERT("Started logging to 'logfile.txt'");
    }
  }
  else {
    DBG_ERROR("Failed to access SD card");
  }
#endif
}


void loop() {
  if(WifiDev.loop())   
    return;

#ifdef LED_BUILTIN // Show wifi connection status on LED (if pin has been defined)
  digitalWrite(LED_BUILTIN, (WiFi.status() == WL_CONNECTED) ? LED_ON : LED_OFF);  
#endif

  // Output a message every 5 seconds
  static uint32_t tprev = millis();
  if(millis()-tprev > 5000) {
    DBG_DEBUG("Press Ctrl-A at any time to toggle mode. Up for %d seconds", tprev/1000);
    another_func();
    tprev += 5000;
  }

  if(dbgc)
    DBG_VERBOSE("Fast debug output - one per loop() call %d", dbgc--);

  // Respond to debug control (from network if connected, otherwise serial port)
  if(WifiDev.available()) {
    char c = WifiDev.read();
    switch(c) {

      // 1-8 Demonstrate different message types
      case '1' : DBG_PRINT("This is a message with no severity/colouring"); break;
      case '2' : DBG_VERBOSE("This is a verbose message"); break;
      case '3' : DBG_DEBUG("This is a debug message"); break;
      case '4' : DBG_INFO("This is an informational message"); break;
      case '5' : DBG_WARNING("This is a warning"); break;
      case '6' : DBG_ALERT("This is an alert!"); break;
      case '7' : DBG_ERROR("This is an ERROR!"); break;
      case '8' : DBG_CRITICAL("This is a CRITICAL MESSGE !!!"); break;

      // Examples of using DBG_CPRINT() to specify the message color
      case 'a' : DBG_CPRINT(MkWifiDev::BrightBlue, "This is a BrightBlue message"); break;
      case 'e' : DBG_CPRINT(MkWifiDev::Green, "This is a green message"); break;

      // Start logging to SD card (if FILE_LOGGING_ENABLED is set)
      case 'C' : sdcard(); break;

      // Upper/lower case w to set/clear the wide hexdump display mode flag
      case 'W' : DBG_ALERT("Set wide display mode");   WifiDev.setDisplayModeFlags(MkWifiDev::WIDE_HEXDUMP); break;
      case 'w' : DBG_ALERT("Clear wide display mode"); WifiDev.clearDisplayModeFlags(MkWifiDev::WIDE_HEXDUMP); break;

      case 'b' : DBG_ALERT("Output a burst of 20 messages");  dbgc = 20; break;

      case 'c' : DBG_ALERT("Turning color %s", color?"OFF":"ON"); 
                 color=!color; 
                 if(color) 
                    WifiDev.setDisplayModeFlags(MkWifiDev::SHOW_COLOUR); 
                 else 
                    WifiDev.clearDisplayModeFlags(MkWifiDev::SHOW_COLOUR);  
                 break;

      // Demonstrate hexdumps
      case 'h' : DBG_HEXDUMP("Test Buffer (128 bytes):", testBuffer, 128, MkWifiDev::INFO); break;
      case 's' : DBG_HEXDUMP("Test Buffer (First 16 bytes):", testBuffer, 16); break;
      case 'd' : DBG_HEXDUMP("Test Buffer (First 8 bytes):", testBuffer, 8); break;

      // Initiate an internet time sync
      case 'n' : DBG_ALERT("Requesting NTP time from server"); WifiDev.configTime(GMT_OFFSET, DAYLIGHT_OFFSET); break;

      // Demonstrate setting/clearing a dbgTAG for messages from this file
      case 't' : DBG_ALERT("dbgTAG set to '%s'", dbgTAG ? "" : "MyModule"); dbgTAG = dbgTAG ? nullptr : "MyModule"; break;

      // Default message if key has no associated action
      default  : DBG_INFO("Keys 1-8 for messages, h)exdump n)tp time t)dbgTAG b)urst c)olour"); break;
    }
  }
}
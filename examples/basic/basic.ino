/*  basic.ino - Example showing typical use of the MkWifiDev library

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

    Refer to the 'full' example for additional features including changing the display 
    settings, printing hex memory dumps & synchronising the local clock to Internet time.

    This file is part of MkWifiDev, a library which simplifies cable-free development. It 
    enables colorised logging to local & remote terminals and supports Arduino OTA firmware 
    updates.  Available at https://github.com/zaddi/MkWifiDev

    MkWifiDev is distributed under the MIT License
*/

#include "MkWifiDev.h"

#include "wifi_credentials.h"   // Remove this line and enter your wifi details
                                //  (enclosed in "quotes") in the lines below

const char* ssid     = YOUR_WIFI_SSID_HERE;
const char* password = YOUR_WIFI_PASSWORD_HERE;
const char* devname  = DEVICE_NAME;

void setup() {

    Serial.begin(115200);

    DBG_VERBOSE("Starting MkWifiDev 'Basic' Demo");

    DBG_INFO("This demo includes 'over-the-air' firmware update support...");
    DBG_INFO("  ...and remote 'serial' terminal access!");

    DBG_DEBUG("Connecting to '%s' with network device name '%s'", ssid, devname);

    WifiDev.begin(ssid, password, devname);

    // Uncomment the following to enable OTA authentication
    // ArduinoOTA.setPassword("admin");   // Use the same password to upload new firmware

    // Use DBG_CPRINT() if you want to explicitly state the text color
    DBG_CPRINT(MkWifiDev::Yellow, "It really is this easy %d use!", 2);
}

uint32_t tprev = 0; 

void loop() {

  if(WifiDev.loop())     // Returns true if an OTA update is in progress
    return;              //   Exit loop immediately for best OTA experience

  if(millis()-tprev > 5000) {   // Output a message every 5 seconds
    tprev += 5000;
    DBG_DEBUG("Press Ctrl-A to toggle mode. Running for %d sec", tprev/1000);
  }

  // Check for user input (from terminal if connected otherwise serial port)
  if(WifiDev.available()) {
    char c = WifiDev.read();
    DBG_ALERT("User key '%c' (Decimal value = %d)", c, c);
  }
}
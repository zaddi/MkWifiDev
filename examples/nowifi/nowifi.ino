/*
    This example demonstrates use of the MkWifiDev library *without* Wifi. It offers
    local access to the message coloring and debug features, and without the Wifi
    library dependancies supports many Arduino platforms aside from ESP devices.

    If using platformio, the following will be useful in platformio.ini environment:
        monitor_raw = yes         ; Enable text coloring in Serial Monitor

    Alternatively PuTTY is an excellent terminal program with text coloring

    This file is part of MkWifiDev, a library which simplifies cable-free development. It 
    enables colorised logging to local & remote terminals and supports Arduino OTA firmware 
    updates.  Available at https://github.com/zaddi/MkWifiDev

    Refer to the other examples which demonstrate remote development using Wifi.
*/

#define LOCAL_SERIAL_ONLY       // Disables inclusion of Wifi & OTA features

#include "MkWifiDev.h"

void setup() {

    Serial.begin(115200);

    DBG_VERBOSE("Starting MkWifiDev 'NoWifi' Demo");

    DBG_INFO("No WiFi or OTA support included in this build");

    // Use DBG_CPRINT() if you want to explicitly state the text color
    DBG_CPRINT(MkWifiDev::Yellow, "It really is this easy!");

}


void loop() {

  // Although not needed for OTA, call loop() to enable the command interface 
  //    (and monitor heap usage - ESP32 only)
  WifiDev.loop();     

  // Output a debug message once every 5 seconds
  static uint32_t tprev = 0;
  if(millis()-tprev > 5000)
    DBG_DEBUG("Press Ctrl-A at any time to toggle mode. Up for %d seconds", (tprev += 5000)/1000);

  // Check for user input (automagically uses remote terminal connected, otherwise serial port)
  if(WifiDev.available()) {
    char c = WifiDev.read();
    DBG_ALERT("User key '%c' (Decimal value = %d)", c, c);
  }
  
}
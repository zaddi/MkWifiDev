/* MkWifiDev.cpp - This is the library implementation file

   This file is part of MkWifiDev, a library which simplifies cable-free development. It 
   enables colorised logging to local & remote terminals and supports Arduino OTA firmware 
   updates.  Available at https://github.com/zaddi/MkWifiDev

   MkWifiDev is distributed under the MIT License
*/

#include "Arduino.h"
#include "MkWifiDev.h"

#define EVENT_MSG_MAX_LEN   (256)
#define TERMINAL_WIDTH      (74)

const uint8_t colors[] = {  MkWifiDev::White, 
                            MkWifiDev::Cyan, 
                            MkWifiDev::Green, 
                            MkWifiDev::BrightBlue, 
                            MkWifiDev::Yellow, 
                            MkWifiDev::Magenta, 
                            MkWifiDev::Red, 
                            MkWifiDev::BrightRed};   

MkWifiDev &MkWifiDev::getInstance() {
  static MkWifiDev instance;
  return instance;
}

MkWifiDev &WifiDev = WifiDev.getInstance();

int MkWifiDev::peek() {
  return pCommand->peek();
}

// Duplicate output to all connected streams
void MkWifiDev::println(const char *buff) {
  if(termConnected)
    pCommand->println(buff);
  
  pSerial->println(buff);

  if(logFile) {
    logFile->println(buff);
    logFile->flush();
  }
}

// Duplicate output to all connected streams
void MkWifiDev::print(const char *buff) {
  if(termConnected)
    pCommand->print(buff);
  
  pSerial->print(buff);

  if(logFile) {
    logFile->print(buff);
    logFile->flush();
  }
}

#ifndef LOCAL_SERIAL_ONLY

void MkWifiDev::begin(const char *ssid, const char *password, const char *mdns_name) {
  bOtaBusy = false;
  mdns_devname = mdns_name;
  WiFi.hostname(mdns_name);       // Set the host name to the same as the mdns name
  WiFi.begin(ssid, password);
}

void MkWifiDev::setSerial(Stream &serialPort) {
  
  if(pCommand == pSerial)   // Make sure command stream updated (if terminal not active)
    pCommand = &serialPort;

  pSerial = &serialPort;
}

void MkWifiDev::setLogFile(Stream &f) {
  logFile = &f;
}

void MkWifiDev::connect_loop()
{
  if(WiFi.status() != WL_CONNECTED) {
    if(conn_state == connected) {
      DBG_ALERT("Lost WiFi connection. Attempting to reconnect..");
      conn_state = connecting;
      pserver->close();
      WiFi.reconnect();
    }
    uint32_t tnow = millis();
    static uint32_t tprev = tnow;
    if((tnow-tprev > 5000)) {
      DBG_INFO("Still attempting to connect to WiFi...");
      tprev = tnow;
    }
    return;
  } 

  // Already connected, nothing more to do
  if(conn_state == connected)
    return;

  conn_state = connected;

  //start server
#ifdef ESP8266
  pserver = new WiFiServer(23);
  pserver->begin();
#else   // ie esp32
  pserver = new WiFiServer;
  pserver->begin(23);
#endif
  pserver->setNoDelay(true);

  DBG_ALERT("Wifi Ready! Use client (eg 'PuTTY') & connect to %s port 23", WiFi.localIP().toString().c_str());

  if(mdns_devname != NULL) {
    DBG_ALERT("mDNS Enabled - Device may be reached using '%s.local'", mdns_devname);
    ArduinoOTA.setHostname(mdns_devname);
  }

  DBG_ALERT("Press Ctrl-A to enter command mode.");

  ArduinoOTA.onStart([]() {
    WifiDev.bOtaBusy = true;
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    DBG_ALERT("Started updating %s", type.c_str());
  });

  ArduinoOTA.onEnd([]() {
    WifiDev.bOtaBusy = false;
    DBG_ALERT("OTA update complete!");
    DBG_ALERT("About to restart, please reconnect remote terminal");
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    static uint8_t div = 0;
    if(!((div++)&0x7))  // Reduce output to 1/8
      DBG_DEBUG("Progress: %u%%\r", (progress / (total / 100)));
  });

  ArduinoOTA.onError([](ota_error_t error) {
    WifiDev.bOtaBusy = false;
    DBG_ALERT("Error[%u]: ", error);
    if      (error == OTA_AUTH_ERROR)    DBG_ALERT("Auth Failed");
    else if (error == OTA_BEGIN_ERROR)   DBG_ALERT("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) DBG_ALERT("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) DBG_ALERT("Receive Failed");
    else if (error == OTA_END_ERROR)     DBG_ALERT("End Failed");
  });

  ArduinoOTA.begin();
}

void MkWifiDev::configTime(long gmtOffset, int daylightOffset, const char * server) {
  ::configTime(gmtOffset, daylightOffset, server);
  nNtpRetries = 3;
}

void MkWifiDev::getNtpTime() {
  nNtpRetries = 3;
}

bool MkWifiDev::isOtaBusy() {
  return bOtaBusy;
}

#endif

int MkWifiDev::available() {
  if(bCommandMode)
    return 0;

  int ret = pCommand->available();

  return (ret && (peek() == 0x01)) ? 0 : ret;   // Hide Ctrl-A from being available to app (loop will handle it)
}


int MkWifiDev::read() {
  if(bCommandMode)
    return 0;

  return pCommand->read();
}

void MkWifiDev::setDisplayModeFlags(uint8_t flags) {
  dispMode |= flags;
}

void MkWifiDev::clearDisplayModeFlags(uint8_t flags) {
  dispMode &= ~flags;
}

uint8_t MkWifiDev::toggleTypeEnableFlag(uint8_t flagIndex) {
  return enableFlags ^= (1 << flagIndex);
}

bool MkWifiDev::IsMessageMuted(MessageType type) {
  if(type < 16) {
    if(bCommandMode)
      return true;

    if(!(enableFlags & (1 << type)))
      return true;
  }
  return false;
}

void MkWifiDev::Report(const char* dbgTAGptr, MessageType type, const char *format,...) {
  if(IsMessageMuted(type))
    return;

 // Simple lockout implementation
  static bool bReportBusy = false;
  while(bReportBusy)
    delay(10);
  bReportBusy = 1;

  char timestamp[40];

  if((dispMode & SHOW_TIMESTAMPS) && (type != RAW_NO_TS)) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    
    // If time is not set (ie before ~2020), dont use timezone, show time since boot
    bool clockSet = (tv.tv_sec > 50*365*24*3600);    
    tm *t = clockSet ? localtime(&tv.tv_sec) : gmtime(&tv.tv_sec);
    strftime(timestamp, sizeof(timestamp), (dispMode & SHOW_DATE) ? "%Y/%m/%d %H:%M:%S" : "%H:%M:%S", t);

    if(dispMode & SHOW_MILLISECONDS)
      sprintf(timestamp+strlen(timestamp), ".%03d", int(tv.tv_usec/1000)%1000);
    
    strcat(timestamp, " : ");
  }
  
  char buff[EVENT_MSG_MAX_LEN] = "";

  bool bColor = ((dispMode & SHOW_COLOUR) && (type != RAW_NO_TS));
  if(bColor)  // Use color from table unless override flag is set
    sprintf(buff, "\033[%dm", (type & OVERRIDE) ? (type & 0x7F) : colors[type & 7]);

  if(dispMode & SHOW_TIMESTAMPS) 
    strcat(buff, timestamp);

  if(dbgTAGptr) {
    strcat(buff, dbgTAGptr);
    strcat(buff, " : ");
  }

  // Add textual representation of message type
  if(dispMode & SHOW_TYPE) 
    if(!(type & OVERRIDE) && type) {
      //NORMAL, VERBOSE, DEBUG, INFO, WARNING, ALERT, ERROR, CRITICAL 
      const char msg_types[] = " VDIWAEC";
      sprintf(timestamp, "[%c]", msg_types[type & 7]);
      strcat(buff, timestamp);
    }

  int len = strlen(buff);

  va_list args;
  va_start (args,format);
  vsnprintf(buff+len,sizeof(buff)-len,format,args);
  va_end (args);

  // Remove trailing newline if present
  len = strlen(buff);
  if(buff[len-1] == '\n')
    buff[len-1] = '\0';

  if(bColor)
    strcat(buff, "\033[0m");

  buff[sizeof(buff)/sizeof(buff[0])-1]='\0';    // Just to be safe, set last pos in buffer to null

  // Flush the serial port before sending the next message to prevent overflow of the transmit buffer if
  // there is a burst of messages (although this will slow down the program creating the output!)
  pSerial->flush();

  println(buff);

  bReportBusy = 0;

}

void MkWifiDev::HexDump(const char* dbgTAG, const char *message, void* addr, int len, MessageType type)
{
  if(IsMessageMuted(type))
    return;

  char buff[160] = "";

  if(addr == nullptr) {
    sprintf(buff, "%s [Null ptr]", message);
    Report(dbgTAG, type, buff);
    return;
  }

  uint8_t bwidth = (dispMode & WIDE_HEXDUMP) ? 32 : 16;  // Bytes per line to display
  bool ShortDump = (len <= bwidth/2);

  if(ShortDump)  // For less than 16 bytes, append data to message line
    strcpy(buff, message);
  else
    Report(nullptr, type, message);
  
  bool bColor = ((dispMode & SHOW_COLOUR) && (type != RAW_NO_TS) && !ShortDump);
  if(bColor)
    sprintf(buff, "\033[%dm", colors[type & 7]);


  uint8_t *ptr = (uint8_t*)addr;
  while(len > 0) {
    char tmp[16];
    sprintf(tmp, " %08X :", (uint32_t)ptr);
      strcat(buff, tmp);
    for(int i=0; i<bwidth && len; i++, len--) {
      if(!(i&7))  // Group into blocks of 8 bytes
        strcat(buff, " ");
      sprintf(tmp, "%02X ", *ptr++);
      strcat(buff, tmp);
    }
    if(bColor && !len)
      strcat(buff, "\033[0m");

    if(ShortDump)
        Report(nullptr, type, buff);
    else
      println(buff);

    buff[0] = '\0';
  }
}

void MkWifiDev::printFullLine(char *line) {
      memset(line, '-', TERMINAL_WIDTH-2);
  line[0] = ' ';
  line[1] = '+';
  line[TERMINAL_WIDTH-2] = '+';
  line[TERMINAL_WIDTH-1] = '\0';
  println(line);
}

void MkWifiDev::printWithEnd(char *line) {
  for(int i=strlen(line); i<TERMINAL_WIDTH-2; i++)
    line[i] = ' ';
  strcpy(line + TERMINAL_WIDTH - 2, "|");
  println(line);
}

void MkWifiDev::checkMemUsage() {
#ifdef ESP32
  uint32_t tnow = millis();
  static uint32_t tprev = tnow;
  if((tnow-tprev) > 250) {    // Only check every 250 ms (or adjust if needed)
    uint32_t minFreeHeapNow = esp_get_minimum_free_heap_size();
    static uint32_t minFreePrev = minFreeHeapNow;
    if(minFreePrev != minFreeHeapNow) {
      DBG_WARNING("Minimum Free Heap dropped by %d bytes to %d bytes", minFreePrev - minFreeHeapNow, minFreeHeapNow);
      minFreePrev = minFreeHeapNow;
    } 
    tprev = tnow;
  }
#endif
}

bool MkWifiDev::loop() {

#ifndef LOCAL_SERIAL_ONLY
  connect_loop();

  ArduinoOTA.handle();
#endif
  
  checkMemUsage();

#ifndef LOCAL_SERIAL_ONLY

  if(nNtpRetries && WiFi.status() == WL_CONNECTED) {
    uint32_t tnow = millis();
    static uint32_t timeForNextAttempt = 0;

    if(tnow > timeForNextAttempt) {
      struct tm timeinfo;
      if(getLocalTime(&timeinfo), 1500) {  // 1.5 second timeout
        DBG_ALERT("Received NTP Time: %s", asctime(&timeinfo));   
        nNtpRetries = 0;
      } else {
        DBG_ERROR("Failed to obtain NTP time. Will retry %d more times", --nNtpRetries);      
        timeForNextAttempt = tnow + 10000;     // 10 seconds between retries
      }
    }
  }

  static bool bSendWelcome = false;
  static uint32_t tconnect;

  //check if there are any new clients
  if (pserver && pserver->hasClient()){
      bool bAccepted = false;
      if (!serverClient || !serverClient.connected()){
        if(serverClient) serverClient.stop();
        #if defined(ESP32)
          serverClient = pserver->available();
        #elif defined(ESP8266)
          serverClient = pserver->accept();
        #endif
        if (!serverClient) DBG_ALERT("available broken");
        DBG_ALERT("Client connected with IP: %s", serverClient.remoteIP().toString().c_str());
        bAccepted = true;
        bSendWelcome = true;
        tconnect = millis();
        bCommandMode = false;
      }
    if (!bAccepted) {
      #if defined(ESP32)
        pserver->available().stop();
      #elif defined(ESP8266)
        pserver->accept().stop();
      #endif
    }
  }

  if(bSendWelcome && serverClient.connected()) {
    if((millis() - tconnect) > 100) {   // Wait a bit after connection to ensure message goes through
      serverClient.println(" +---------------------------------------------+");
      serverClient.println(" |     Connected to remote device via WiFi     |");
      serverClient.println(" |        Press Ctrl-A for Command Mode        |");
      serverClient.println(" +---------------------------------------------+");
      bSendWelcome = false;
      termConnected = 1;
      pCommand = &serverClient;
    }
  }

  if(termConnected && !serverClient.connected()) {
    DBG_ALERT("Remote terminal disconnected, resuming control by serial port");
    termConnected = 0;
    pCommand = pSerial;
  }
#endif

  char line[TERMINAL_WIDTH+1];

  if(pCommand->available()) {
    if(peek() == 0x01) {  // Check for Ctrl-a
      bCommandMode = !bCommandMode;
      if(!bCommandMode) {
        pCommand->read();   // remove Ctrl-a from buffer
        DBG_ALERT("Returning to normal mode");
      }
    }  
    if(bCommandMode) {
      static uint8_t waitForConfirm = 0;
      char c = tolower(pCommand->read());

      if(waitForConfirm && (c == 'y')) {
        DBG_ALERT("About to restart, please reconnect if using remote terminal");
        delay(200);
        #if defined(ESP32)
          esp_restart();
        #elif defined (ESP8266)
          ESP.restart();
        #endif
      }
      waitForConfirm = 0;

      switch(c) {
        case 'v' : toggleTypeEnableFlag(VERBOSE); break;   
        case 'd' : toggleTypeEnableFlag(DEBUG);   break;   
        case 'i' : toggleTypeEnableFlag(INFO);    break;   
        case 'w' : toggleTypeEnableFlag(WARNING); break;   
        case 't' : dispMode ^= SHOW_TIMESTAMPS; break;
        case 'y' : dispMode ^= SHOW_DATE; break;
        case 'm' : dispMode ^= SHOW_MILLISECONDS;   break;
        case 'c' : dispMode ^= SHOW_COLOUR; break; 
        case 'f' : dispMode ^= SHOW_TYPE; break; 
        case 'r' : println("Are you sure want to restart?");
                   println("  Press 'y' to confirm, any other key to cancel:");
                   waitForConfirm = 1; 
                   return bOtaBusy;  
        default : c = 0x01; break;  // Show full menu if none of the above
      }

      printFullLine(line);

// Only show this portion of output on initial Ctrl-A, or unasigned control character received
if(c==0x01) {
#ifdef _APPNAME_
      strcpy(line, " |  " TOSTRING(_APPNAME_) " : Built " __DATE__ " " __TIME__);
#else
      strcpy(line, " |  MkWifiDev - Build Timestamp " __DATE__ " " __TIME__);
#endif
      printWithEnd(line);
      printFullLine(line);
#if defined(ESP32)
      sprintf(line, " |  %s Rev%d,  %d core(s)    ChipID: %llX",
        ESP.getChipModel(), ESP.getChipRevision(), ESP.getChipCores(), ESP.getEfuseMac());
      printWithEnd(line);
 
      sprintf(line, " |  CPU Frequency: %d MHz  %d MB Flash  %d KB RAM  %d KB PSRAM",
        getCpuFrequencyMhz(), int(ESP.getFlashChipSize() / (1024.0 * 1024)), int(ESP.getHeapSize() / 1024.0), 
        int(ESP.getPsramSize() / 1024.0));
      printWithEnd(line);

#ifndef LOCAL_SERIAL_ONLY
      char lan[32];
      if (WiFi.status() == WL_CONNECTED)
        sprintf(lan, "Connected   IP %s", WiFi.localIP().toString().c_str());
      else
        strcpy(lan,"Not Connected");
      sprintf(line, " |  Wifi %s    Debug Control: %s",
        lan, termConnected ? "Network" : "Serial");
      printWithEnd(line);

      //This seems to be really slow, so leave out for now
      //sprintf(line, " |  Sketch Size: %d,   Free Space: %d", ESP.getSketchSize(), ESP.getFreeSketchSpace());
      //printWithEnd(line);

      sprintf(line, " |  Free Main RAM: %d, Free Heap: Low: %d, Current: %d", 
        esp_get_free_heap_size()-ESP.getFreePsram(), esp_get_minimum_free_heap_size(), esp_get_free_heap_size() );
      printWithEnd(line);

      time_t uptime = esp_timer_get_time()/1000000;
      struct tm *tm_up = gmtime(&uptime);
      sprintf(line, " |  System Uptime: %d days %dh %d m %ds     WiFi RSSI: %d", int(uptime/(24*3600)), 
        tm_up->tm_hour, tm_up->tm_min, tm_up->tm_sec, WiFi.RSSI()); 
      printWithEnd(line);
#else
      //This seems to be really slow, so leave out for now
      //sprintf(line, " |  Sketch Size: %d,   Free Space: %d", ESP.getSketchSize(), ESP.getFreeSketchSpace());
      //printWithEnd(line);

      sprintf(line, " |  Free Main RAM: %d, Free Heap: Low: %d, Current: %d", 
        esp_get_free_heap_size()-ESP.getFreePsram(), esp_get_minimum_free_heap_size(), esp_get_free_heap_size() );
      printWithEnd(line);

      time_t uptime = esp_timer_get_time()/1000000;
      struct tm *tm_up = gmtime(&uptime);
      sprintf(line, " |  System Uptime: %d days %dh %d m %ds", int(uptime/(24*3600)), 
        tm_up->tm_hour, tm_up->tm_min, tm_up->tm_sec); 
      printWithEnd(line);
#endif

      const char* esp_reset_strings[] = { "Unknonw", "Power On", "Ext Pin", "Software Reset", "Exception/Panic", 
        "Internal Watchdog", "Task Watchdog", "Other Watchdog", "Deep Sleep", "Brown Out", "Reset over SDIO" };

      esp_reset_reason_t r = esp_reset_reason();
      sprintf(line, " |  ESP Restart Reason: %d (%s)", r, esp_reset_strings[r]); 
      printWithEnd(line);

      printFullLine(line);
#elif defined(ESP8266)
      sprintf(line, " |  %s   Version %s   ChipID: %08X",
        "ESP8266", ESP.getCoreVersion().c_str(), ESP.getChipId());
      printWithEnd(line);

#ifndef LOCAL_SERIAL_ONLY
      sprintf(line, " |  CPU Frequency: %d MHz  %d MB Flash     MAC Addr: %s",
        ESP.getCpuFreqMHz(), int(ESP.getFlashChipSize() / (1024.0 * 1024)), WiFi.macAddress().c_str() );
      printWithEnd(line);

      char lan[32];
      if (WiFi.status() == WL_CONNECTED)
        sprintf(lan, "Connected  IP: %s", WiFi.localIP().toString().c_str());
      else
        strcpy(lan,"Not Connected");
      sprintf(line, " |  Wifi %s    DeviceName: %s",
        lan, mdns_devname);
      printWithEnd(line);

      sprintf(line, " |  Free Heap: %d       Debug Control: %s", system_get_free_heap_size(), 
          termConnected ? "Network" : "Serial");
      printWithEnd(line);

      time_t uptime = millis()/1000;
      struct tm *tm_up = gmtime(&uptime);
      sprintf(line, " |  System Uptime: %d days %dh %d m %ds      WiFi RSSI: %d", int(uptime/(24*3600)), 
        tm_up->tm_hour, tm_up->tm_min, tm_up->tm_sec, WiFi.RSSI()); 
      printWithEnd(line);
#else      
      sprintf(line, " |  CPU Frequency: %d MHz  %d MB Flash",
        ESP.getCpuFreqMHz(), int(ESP.getFlashChipSize() / (1024.0 * 1024)));
      printWithEnd(line);

      time_t uptime = millis()/1000;
      struct tm *tm_up = gmtime(&uptime);
      sprintf(line, " |  System Uptime: %d days %dh %d m %ds", int(uptime/(24*3600)), 
        tm_up->tm_hour, tm_up->tm_min, tm_up->tm_sec); 
      printWithEnd(line);
#endif

      sprintf(line, " |  ESP Restart Reason: %s", ESP.getResetReason().c_str()); 
      printWithEnd(line);

      printFullLine(line);

#endif
}
      strcpy(line, " |  In Command Mode (Debug Paused) - Press Ctrl-A again to exit");
      printWithEnd(line);
      sprintf(line, " |    v)erbose [%c]      d)ebug [%c]     i)nfo [%c]     w)arning [%c]", 
        enableFlags&2?'#':' ', enableFlags&4?'#':' ', enableFlags&8?'#':' ', enableFlags&16?'#':' ');
      printWithEnd(line);
      strcpy(line, " |  t)imestamps   m)illiseconds   y)y/mm/dd   f)lags   c)olor   r)eset");
      printWithEnd(line);
      printFullLine(line);
      print(" | ");    // Output before example message
      Report(nullptr, MessageType(OVERRIDE | Cyan),"%sExample message with current settings", dispMode & SHOW_TYPE ? "[V]" : "");
      printFullLine(line);
    }
  }

#ifndef LOCAL_SERIAL_ONLY
  // If a TCP debug session is active, respond to any activity on local serial port
  if(termConnected) {
    if(pSerial->available()) {
      DBG_ALERT("Remote terminal is active, ignoring local serial commands");
      while(pSerial->available())
        pSerial->read();
    }
  }
  return bOtaBusy;
#else
  return false;
#endif
}


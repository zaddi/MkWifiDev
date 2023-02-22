/* MkWifiDev.h - This is the library interface specification

   This file is part of MkWifiDev, a library which simplifies cable-free development. It 
   enables colorised logging to local & remote terminals and supports Arduino OTA firmware 
   updates.  Available at https://github.com/zaddi/MkWifiDev

   MkWifiDev is distributed under the MIT License
*/

#ifndef MkWifiDev_h
#define MkWifiDev_h

// Define these as desired (or set them using compiler build flags, eg in platformio.ini)
//#define _APPNAME_  "MyCoolApp" // App name shown in the Command Mode terminal display
//#define DEBUG_SHOW_FILE        // Shows the calling filename & line number in output
//#define DEBUG_SHOW_FUNCTION    // Shows the calling function name in output
//#define LOCAL_SERIAL_ONLY      // No wifi or OTA support will be included

#include <Arduino.h>
#if defined(LOCAL_SERIAL_ONLY)
    #warning "Building MkWifiDev without WiFi support - Remote debugging and OTA updates disabled"
    #include "sys/time.h"
#else
    #include <ArduinoOTA.h> 
#endif

// TOSTRING/STRINGIFY Macros from http://www.decompile.com/cpp/faq/file_and_line_error_string.htm
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

#ifdef DEBUG_SHOW_FILE
    #define _PRT_A1_       __FILE__ ":" TOSTRING(__LINE__) " "
#else
    #define _PRT_A1_
#endif

#ifdef DEBUG_SHOW_FUNCTION
    #define _PRT_B1_     "%s() : "
#elif defined(DEBUG_SHOW_FILE)
    #define _PRT_B1_     ": "
#else
    #define _PRT_B1_
#endif

#define DBG_REPORT(type, msg, ...)	 WifiDev.Report(dbgTAG, type, msg, ##__VA_ARGS__)       // Never shows file/function info

#ifdef DEBUG_SHOW_FUNCTION
    #define DBG_MKPRINT(type, msg, ...)	     WifiDev.Report(dbgTAG, type,   _PRT_A1_ _PRT_B1_ msg, __func__, ##__VA_ARGS__)
    #define DBG_CPRINT(color, msg, ...)	     WifiDev.Report(dbgTAG, MkWifiDev::MessageType(MkWifiDev::OVERRIDE | color),   _PRT_A1_ _PRT_B1_ msg, __func__, ##__VA_ARGS__)
#else
    #define DBG_MKPRINT(type, msg, ...)	     WifiDev.Report(dbgTAG, type,   _PRT_A1_ _PRT_B1_ msg, ##__VA_ARGS__)
    #define DBG_CPRINT(color, msg, ...)	     WifiDev.Report(dbgTAG, MkWifiDev::MessageType(MkWifiDev::OVERRIDE | color),   _PRT_A1_ _PRT_B1_ msg, ##__VA_ARGS__)
#endif

#define DBG_PRINT(msg, ...)	     DBG_MKPRINT(MkWifiDev::NORMAL,   msg, ##__VA_ARGS__)
#define DBG_VERBOSE(msg, ...)    DBG_MKPRINT(MkWifiDev::VERBOSE,  msg, ##__VA_ARGS__)
#define DBG_DEBUG(msg, ...)	     DBG_MKPRINT(MkWifiDev::DEBUG,    msg, ##__VA_ARGS__)
#define DBG_INFO(msg, ...)	     DBG_MKPRINT(MkWifiDev::INFO,     msg, ##__VA_ARGS__)
#define DBG_WARNING(msg, ...)    DBG_MKPRINT(MkWifiDev::WARNING,  msg, ##__VA_ARGS__)
#define DBG_ALERT(msg, ...)	     DBG_MKPRINT(MkWifiDev::ALERT,    msg, ##__VA_ARGS__)
#define DBG_ERROR(msg, ...)	     DBG_MKPRINT(MkWifiDev::ERROR,    msg, ##__VA_ARGS__)
#define DBG_CRITICAL(msg, ...)   DBG_MKPRINT(MkWifiDev::CRITICAL, msg, ##__VA_ARGS__)

#define DBG_HEXDUMP(msg, addr, len, ...)  WifiDev.HexDump(dbgTAG, _PRT_A1_ msg, (void*)addr, len, ##__VA_ARGS__)

// Default global null debug label. Individual modules may local specify a label if desired
static const char* dbgTAG = nullptr;

// Singleton class implementation based on posting at
//   https://forum.arduino.cc/t/how-to-write-an-arduino-library-with-a-singleton-object/666625/2   

class MkWifiDev : public Stream
{
  private:    
    MkWifiDev() = default;

    uint8_t dispMode = SHOW_TIMESTAMPS | SHOW_COLOUR;   // Default show timestamps & color
    bool bCommandMode = false;
    uint8_t nNtpRetries = 0;
    uint8_t enableFlags = 0xFF;
    Stream *logFile = nullptr;
    Stream *pSerial = &Serial;


#ifndef LOCAL_SERIAL_ONLY
    WiFiServer *pserver = NULL;
    WiFiClient serverClient;
    const char* mdns_devname = NULL;
    uint8_t termConnected = 0;
    enum t_conn_state { idle, connecting, connected };
    t_conn_state conn_state;
#endif

  public:
    bool bOtaBusy = false;
    static MkWifiDev &getInstance();
    MkWifiDev(const MkWifiDev &) = delete;      // No copying
    MkWifiDev &operator=(const MkWifiDev &) = delete;

    size_t write(uint8_t a) { return Serial.write(a); };   // Added this to enable stream inheritance to work with singleton instance

    enum Colours { White = 37, Cyan = 36, Green = 32, BrightBlue = 94, Yellow = 33, Magenta = 35, Red = 31, BrightRed = 91};   

    enum MessageType { NORMAL, VERBOSE, DEBUG, INFO, WARNING, ALERT, ERROR, CRITICAL, RAW_NO_TS, OVERRIDE = 128 }; 

    enum DisplayFlags { SHOW_TIMESTAMPS = 1, SHOW_MILLISECONDS = 2, SHOW_DATE = 4, SHOW_COLOUR = 8, SHOW_TYPE = 16, WIDE_HEXDUMP = 0x80 };
    
    // Must be called to enable Command Mode, OTA updates and memory usage monitoring. Returns true if OTA update in progress
    bool loop();    

    // Specify the port to be used for serial communications ('Serial' assumed by default)
    void setSerial(Stream &serialPort);

    // If a file (or other stream) is specified, all serial/terminal output will copied there as well
    void setLogFile(Stream &f);

    // Outputs a message as used by the DBG_xxx macros. dbgTAG is ignored if null, type specifies the level (warning/debug etc)
    void Report(const char* dbgTAG, MessageType type, const char *format,...);

    // Outputs an area of memory with a leading message
    void HexDump(const char* dbgTAG, const char *message, void* addr, int len, MessageType type = VERBOSE);

    // Indicates if any control characters are available to be read (from either Serial port or TCP socket if connected)
    int available();

    // Reads from Serial port (or TCP socket if connected) 
    int read();

    // Reads the next byte from Serial port (or TCP socket) without removing it
    int peek();

    // Set display mode flags
    void setDisplayModeFlags(uint8_t flags);

    // Clear display mode flags
    void clearDisplayModeFlags(uint8_t flags);

#ifndef LOCAL_SERIAL_ONLY
    // Starts WiFi and connects to specified network
    void begin(const char *ssid, const char *password, const char *mdns_name = NULL);

    // Optionally configure local timezone. This will automatically initiate an NTP time request
    void configTime(long gmtOffset = 0, int daylightOffset = 0, const char * server = "pool.ntp.org");

    // Trigger NTP request from server
    void getNtpTime();

    // Check whether an OTA update is currently in progress
    bool isOtaBusy();
#endif

  private:
    int force_read();       // Bypass check if in command mode
    int force_available();  // Bypass check if in command mode
    void checkMemUsage();
    void connect_loop();
    void print(const char *buff);
    void println(const char *buff);
    void printFullLine(char *line);
    void printWithEnd(char *line);
    bool IsMessageMuted(MessageType type);
    uint8_t toggleTypeEnableFlag(uint8_t flagIndex);

}; 

extern MkWifiDev &WifiDev;      // The singleton instance

//MkWifiDev_h
#endif

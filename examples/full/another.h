/* another.h - This is the interface specification

   This file is included in the full example to demonstrate logging from multiple files 
   and the options available to differentiate such messages if desired.

   This file is part of MkWifiDev, a library which simplifies cable-free development. It 
   enables colorised logging to local & remote terminals and supports Arduino OTA firmware 
   updates.  Available at https://github.com/zaddi/MkWifiDev

   MkWifiDev is distributed under the MIT License
*/

#ifndef another_h
#define another_h

#include "MkWifiDev.h"

//#define DEBUG_SHOW_FILE      // Uncomment this to show the calling filename & line number in output
//#define DEBUG_SHOW_FUNCTION  // Uncomment this to show the calling function name in output

void another_setup();
void another_func();

#endif  // another_h
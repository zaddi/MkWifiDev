/* another.cpp - Implementation file

   This file is included in the full example to demonstrate logging from multiple files 
   and the options available to differentiate such messages if desired.

   This file is part of MkWifiDev, a library which simplifies cable-free development. It 
   enables colorised logging to local & remote terminals and supports Arduino OTA firmware 
   updates.  Available at https://github.com/zaddi/MkWifiDev

   MkWifiDev is distributed under the MIT License
*/

#include "another.h"

void another_setup() {
    dbgTAG = "JustAnother";             // Set our tag for this file's logging output
    DBG_VERBOSE("Setup complete");
}

void another_func() {

    static bool toggle = false;

    DBG_INFO("I say %s", toggle ? "hey" : "ho");

    toggle = !toggle;
}


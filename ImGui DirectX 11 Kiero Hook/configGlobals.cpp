#include "IniWriter.h"
#include "IniReader.h"
#include "attributes.h"
#include "configGlobals.h"

CIniWriter iniWriter((char*)CONFIG_PATH);
CIniReader iniReader((char*)CONFIG_PATH);
char* str_settings = (char*)"Settings";

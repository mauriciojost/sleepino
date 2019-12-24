#ifndef MAIN_INC
#define MAIN_INC

#define CLASS_MAIN "MA"

#include <ModuleSleepino.h>
#include <main4ino/Misc.h>

#ifndef PROJ_VERSION
#define PROJ_VERSION "snapshot"
#endif // PROJ_VERSION

//////////////////////////////////////////////////////////////
// Provided by generic Main
//////////////////////////////////////////////////////////////

// Standard arduino setup
void setup();

bool initWifiSimple();


#endif // MAIN_INC

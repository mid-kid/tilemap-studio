#ifndef VERSION_H
#define VERSION_H

#define PROGRAM_NAME "Tilemap Studio"

#define PROGRAM_AUTHOR "Rangi"

#define CURRENT_YEAR "2021"

#define PROGRAM_VERSION 4,0,1
#ifdef _DEBUG
#define PROGRAM_VERSION_STRING "4.0.1 [DEBUG]"
#else
#define PROGRAM_VERSION_STRING "4.0.1"
#endif

#define PROGRAM_EXE_NAME "tilemapstudio"

#ifdef _WIN32
#define PROGRAM_EXE PROGRAM_EXE_NAME ".exe"
#elif defined(__APPLE__)
#define PROGRAM_EXE PROGRAM_NAME ".app"
#else
#define PROGRAM_EXE PROGRAM_EXE_NAME
#endif

#endif

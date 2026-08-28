/**
 * @copyright @showdate "%Y " Anton Chernov. All rights reserved.
 * @file    main.cpp
 * @version 0.1.0
 * @authors Anton Chernov
 * @date    2026-08-28
 * @date    @showdate "%m/%d/%Y"
 */

/******************************* Included files ******************************/
#include <stdio.h>
/********************************* Definition ********************************/

/**
 * @def VERSION_MAJOR
 * @brief Major version number of Cypher (breaking API changes).
 */
#define VERSION_MAJOR     0

/**
 * @def VERSION_MINOR
 * @brief Minor version number of Cypher (backwards-compatible additions).
 */
#define VERSION_MINOR     1

/**
 * @def VERSION_PATCH
 * @brief Patch version number of Cypher (backwards-compatible bug fixes).
 */
#define VERSION_PATCH     0

/**
 * @def VERSION_STRING
 * @brief Cypher version as a printable "MAJOR.MINOR.PATCH" string literal.
 * @details Assembled at compile time from the numeric version macros, so it
 * costs no RAM - suitable even for the most memory-constrained targets.
 */
#define VERSION_STR_(x)   #x
#define VERSION_XSTR_(x)  VERSION_STR_(x)
#define VERSION_STRING \
    VERSION_XSTR_(VERSION_MAJOR) "." \
    VERSION_XSTR_(VERSION_MINOR) "." \
    VERSION_XSTR_(VERSION_PATCH)


/***************************** Private variables *****************************/
/***************************** Private variables *****************************/

#ifdef __GNUC__  // GCC/MinGW only
const char version_info[] __attribute__((section(".version"), used)) =
    "FileDescription: Oscilloscope application\n"
    "FileVersion: 0.1.0.0\n"
    "ProductName: Oscilloscope\n"
    "ProductVersion: 0.1.0.0\n"
    "CompanyName: N/A\n"
    "LegalCopyright: Copyright (C) Anton Chernov, 2026\n"
    "OriginalFilename: run\n";

const char build_info[] __attribute__((section(".buildinfo"), used)) =
    "Build date: " __DATE__ " " __TIME__ "\n"
    "Compiler: GCC " __VERSION__ "\n";

#endif // __GNUC__
/**************************** Function prototypes ****************************/
/********************* Application Programming Interface *********************/

int main (void) {
    printf("Hello world!\n");
    return 0;
}
/*---------------------------------------------------------------------------*/
/***************************** Private functions *****************************/
/*---------------------------------------------------------------------------*/
/*****************************************************************************/

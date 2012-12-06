#include <stdio.h>

// University of Southampton, 2012
// EMECS Group Design Project

// This file enables or disables the module debug outputs via printf
// from different modules

#ifndef DEBUGOUTPUTCONTROL_H
#define DEBUGOUTPUTCONTROL_H

// ************************************************************************
// Use the statement below to disable ALL debug outputs, takes priority
// over sections below
// ************************************************************************

#define nDISABLE_ALL_DEBUG_OUTPUTS

// ************************************************************************
// Use the section below to enable / disable debug outputs for each module
// Example:
// #define ENABLE_DEBUG_OUTPUT_ALARM - to enable debug output from Alarm module
// #define nENABLE_DEBUG_OUTPUT_ALARM - to disable debug output from Alarm md.
// ************************************************************************

#ifndef DISABLE_ALL_DEBUG_OUTPUTS

#define ENABLE_DEBUG_OUTPUT_XBEE

#endif

// ************************************************************************
// Implementations for debug functions
// No need to make changes here, modify statements above instead
// ************************************************************************

#ifdef ENABLE_DEBUG_OUTPUT_XBEE
#define module_debug_xbee(fmt, ...)   printf("XBee: "fmt"\n", ##__VA_ARGS__)
#else
#define module_debug_xbee(fmt, ...)
#endif

#endif // DEBUGOUTPUTCONTROL_H

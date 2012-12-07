#include <stdio.h>

// University of Southampton, 2012
// EMECS Group Design Project

// This file enables or disables the module debug outputs via printf
// from different modules

#ifndef DEBUGOUTPUTCONTROL_H
#define DEBUGOUTPUTCONTROL_H

#include <stdio.h>

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
#define nENABLE_DEBUG_OUTPUT_XBEE
#define ENABLE_DEBUG_OUTPUT_STRG

#endif

#include "debug_output_impl.h"

#endif // DEBUGOUTPUTCONTROL_H

#pragma once

#include <stdarg.h>
#include <stdio.h>
#include "helpers.h"

#define LOGLEVEL_NONE       0
#define LOGLEVEL_ERROR      1
#define LOGLEVEL_WARNING    2
#define LOGLEVEL_INFO       3
#define LOGLEVEL_DEBUG      4
#define LOGLEVEL_ALL        5

void set_log_level(int log_level);
void logm(char *format, ...);
void info(char *format, ...);
void warning(char *format, ...);
void error(char *format, ...);
void debug(char *format, ...);
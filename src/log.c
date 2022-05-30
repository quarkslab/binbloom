#include "log.h"

extern pthread_mutex_t io_lock;

static int g_log_level = LOGLEVEL_NONE;
static char *g_log_levels[] = {
    "",
    "\e[1;31m  ERROR",
    "\e[1;33mWARNING",
    "\e[1;32m   INFO",
    "\e[1;36m  DEBUG",
};


/**
 * @brief   Displays a log trace in console
 * @param   log_level       Logging level
 * @param   format          Format string to use
 * @param   args            Variable list of arguments to use
 * @return  number of characters written on success, <0 otherwise
 **/

int bb_log(int log_level, char *format, va_list args)
{
    int ret = 0;
    if (log_level <= g_log_level)
    {       
        /* Clear progress bar if any. */
        progress_bar_done();

        pthread_mutex_lock(&io_lock);

        /* Prefix with log level. */
        if (log_level > 0)
            printf("%s: ", g_log_levels[log_level]);
        else
            printf("\e[1;0m");
        ret = vprintf(format, args);
        printf("\e[0m");

        pthread_mutex_unlock(&io_lock);
    }
    return ret;
}


/**
 * @brief   Set current log level
 * @param   log_level   New log level
 **/

void set_log_level(int log_level)
{
    if ((log_level >= LOGLEVEL_NONE) && (log_level <= LOGLEVEL_ALL))
        g_log_level = log_level;
}


/**
 * @brief   Log a normal message
 * @param   format  Text format
 * @param   ...     Format string arguments
 **/

void logm(char *format, ...)
{
    va_list args;

    va_start(args, format);
    bb_log(LOGLEVEL_NONE, format, args);
    va_end(args);
}


/**
 * @brief   Log an informational message
 * @param   format  Text format
 * @param   ...     Format string arguments
 **/

void info(char *format, ...)
{
    va_list args;

    va_start(args, format);
    bb_log(LOGLEVEL_INFO, format, args);
    va_end(args);
}


/**
 * @brief   Log a warning message
 * @param   format  Text format
 * @param   ...     Format string arguments
 **/

void warning(char *format, ...)
{
    va_list args;

    va_start(args, format);
    bb_log(LOGLEVEL_WARNING, format, args);
    va_end(args);
}


/**
 * @brief   Log an error message
 * @param   format  Text format
 * @param   ...     Format string arguments
 **/

void error(char *format, ...)
{
    va_list args;

    va_start(args, format);
    bb_log(LOGLEVEL_ERROR, format, args);
    va_end(args);
}


/**
 * @brief   Log a debug message
 * @param   format  Text format
 * @param   ...     Format string arguments
 **/

void debug(char *format, ...)
{
    va_list args;

    va_start(args, format);
    bb_log(LOGLEVEL_DEBUG, format, args);
    va_end(args);
}


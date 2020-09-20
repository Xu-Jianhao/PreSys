/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                                                                             *
 *    Describe :                                                               *
 *          Loginfo System.
 *
 *    Author   :  Xu-Jianhao                                                   *
 *                               Biu Biu Biu !!!                               *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>

#include "predef.h"
#include "tools.h"

int open_logfile(const char *path, const char *filename);

void LogToFile(int, int, const char *, const char *, int, const char *, va_list);

void logwrite(int, const char *, const char *, int, const char *, ...);

#define LOG_WRITE(FMT, ...) \
    logwrite(errno, MODULE, __FILE__, __LINE__, FMT, ##__VA_ARGS__);

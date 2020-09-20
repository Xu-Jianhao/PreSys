/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                                                                             *
 *    Describe :                                                               *
 *          Loginfo System.
 *
 *    Author   :  Xu-Jianhao                                                   *
 *                               Biu Biu Biu !!!                               *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include "loghelper.h"
#include "predef.h"
#include "tools.h"


int open_logfile(const char *path, const char *filename)
{
    int fd;
    char logfile[256];
    char newlogfile[256];
    char timeStr[128];
    
    if(path == NULL || filename == NULL) {
          return PRE_ERR;
    }
    
    /* 拼接日志文件名 */
    strcat(logfile, path);
    strcat(logfile, filename);

    /*在重启动时,切换日志文件*/
    if(access(logfile, F_OK) == 0) {
        if(gettimeStr(timeStr, sizeof(timeStr), "%Y-%m-%d_%H%M") == NULL) {
            return PRE_ERR;
        }
        strcat(newlogfile, path);
        strcat(newlogfile, timeStr);
        strcat(newlogfile, filename);
        if(rename(logfile, newlogfile)) {
            return PRE_ERR;
        }
    } else {
        errno = 0;
    }
    if((fd = open(logfile, O_CREAT|O_WRONLY|O_SYNC, S_IRUSR|S_IWUSR)) == -1) {
        return PRE_ERR;
    }

    return fd;
}

void LogToFile(int errnoflag, int error, const char *module, const char *file, int line,  const char *fmt, va_list ap)
{
    /* 格式:日期__FILE__ : __LINE__ -模块-错误信息*/
    char    buf[MAXLINE];

    if(gettimeStr(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S ") == NULL) {     /* 获取时间 */
        strcat(buf, "null ");
    }
    
    snprintf(buf+strlen(buf), MAXLINE-strlen(buf)-1, "『%s:%d』[%s]", file, line, module);

    
    vsnprintf(buf+strlen(buf), MAXLINE-strlen(buf)-1, fmt, ap);
    if(errnoflag)
        snprintf(buf+strlen(buf), MAXLINE-strlen(buf)-1, ": %s", strerror(error));
    strcat(buf, "\n");

    fputs(buf,stderr);
    fflush(stderr);
}

void logwrite(int error, const char *module, const char *file, int line, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    error == 0 ? LogToFile(0, 0, module, file, line, fmt, ap) : LogToFile(1, error, module, file, line, fmt, ap);
    va_end(ap);
}


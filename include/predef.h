#ifndef  _PRESYS_H
#define  _PRESYS_H



#include <sys/types.h>
#include <sys/stat.h>

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>

#include <signal.h>

#include <semaphore.h>


#define PRE_OK      0
#define PRE_ERR     -1
#define PRE_TRUE    1
#define PRE_FALSE   0

#define PRE_CLOSE       11
#define PRE_SENDERROR   12

#define RET_OK      101
#define RET_ERROR   102

#define PROID       111
#define PATHNAME    "/home/xjh/Space/git/PreSys/presys.pid"
#define SEMNAME     "/presys_sem"
#define SHMSIZE     4096*4
#define SERVERNAME  "presys"
#define LOGPATH     "/home/xjh/Space/git/PreSys/log/"
#define LOGFILE     "presys.log"
#define PIDFILE     "/home/xjh/Space/git/PreSys/presys.pid"
#define WORKDIR     "/home/xjh/Space/git/PreSys/"

#define MAXLINE     256
#define MAXINFO     1024
#define MAXCOUNT    15          /* SHMSIZE / sizeof(r_info) */

#define SHMFULL     11
#define SHMEMPTY    12

#define REQUEST 0               /* 请求 */
#define RESPOND 1               /* 应答 */

#define SELECT  0x00000001
#define INSERT  0x00000002
#define DELETE  0x00000003
#define UPDATE  0x00000004


typedef void (*pro_handler)();

/* pre_list */
struct list_t {
    size_t          len;
    void            *data;
    struct list_t   *next;
};

typedef struct list_t  pre_list;

/* requets info */
struct r_info_t {
    char        type;
    int         flag;               /* 有效标志 */
    int         sockfd;             /* 客户端通信套接字 */
    int         retCode;            /* 返回码 */
    char        content[1024];      /* 请求内容 */
};

typedef struct r_info_t r_info;


struct ctl_Info_t {
    int     shmid;                  /* SHM ID */
    void    *shmaddr;               /* SHM address */
    sem_t   *semid;                 /* SEM ID */
};

typedef struct ctl_Info_t  CtlInfo;

#endif  /* _PRESYS_H */

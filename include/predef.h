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

#define RFULL     11            /* Request queue full. */
#define REMPTY    12            /* Request queue empty. */
#define QFULL     13            /* Respond queue full. */
#define QEMPTY    14            /* Respond queue empty. */

#define SELECT  'S'
#define INSERT  'I'
#define DELETE  'D'
#define UPDATE  'U'

#define SYSERROR    201
#define NORECORD    202
#define NOEXIST     203
#define UNKNOWN     204
#define MYSQLERROR  100

#define SELECTOK    1
#define INSERTOK    2
#define UPDATEOK    3
#define DELETEOK    4



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
    char        type;               /* 请求类型 */
    int         sockfd;             /* 客户端通信套接字 */
    int         retCode;            /* 返回码 */
    char        content[1024];      /* 请求内容 */
};

struct Request_t {
    char    name[30];
    char    addr[64];
    char    time[64];
    char    msg[MAXINFO];
};

struct ctl_Info_t {
    int     shmid;                  /* SHM ID */
    pid_t   pid;
    void    *shmaddr;               /* SHM address */
    sem_t   *semid;                 /* SEM ID */
};


typedef struct r_info_t r_info;

typedef struct Request_t Request;

typedef struct ctl_Info_t  CtlInfo;


#endif  /* _PRESYS_H */

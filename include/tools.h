/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                                                                             *
 *    Describe :                                                               *
 *          工具函数.
 *
 *    Author   :  Xu-Jianhao                                                   *
 *                               Biu Biu Biu !!!                               *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
#ifndef _PRE_TOOLS_H
#define _PRE_TOOLS_H

#include "predef.h"

#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <fcntl.h>
#include <semaphore.h>
#include <time.h>



#define CREAT_SEM   1
#define GET_SEM     0


#define pre_read_lock(fd, offset, whence, len) \
    pre_lock_reg((fd), F_SETLK, F_RDLCK, (offset), (whence), (len))

#define pre_readw_lock(fd, offset, whence, len) \
    pre_lock_reg((fd), F_SETLKW, F_RDLCK, (offset), (whence), (len))

#define pre_write_lock(fd, offset, whence, len) \
    pre_lock_reg((fd), F_SETLK, F_WRLCK, (offset), (whence), (len))

#define pre_writew_lock(fd, offset, whence, len) \
    pre_lock_reg((fd), F_SETLKW, F_WRLCK, (offset), (whence), (len))

#define pre_un_lock(fd, offset, whence, len) \
    pre_lock_reg((fd), F_SETLK, F_UNLCK, (offset), (whence), (len))


#define pre_is_read_lockable(fd, offset, whence, len) \
    (pre_lock_test((fd), F_RDLCK, (offset), (whence), (len)) == PRE_FALSE)

#define pre_is_write_lockable(fd, offset, whence, len) \
    (pre_lock_test((fd), F_WRLCK, (offset), (whence), (len)) == PRE_FALSE)



/* 获取当前时间字符串格式 */
char *gettimeStr(char *, size_t, const char *);

/* Init CtlInfo */
CtlInfo *pre_init_ctl();

void pre_free_ctl(CtlInfo *);

/* 初始化共享内存 */
int pre_init_shm(const char *, int, size_t, int);

int pre_empty_shm(void *);

int pre_rm_shm(int, const void *);

/* 初始化信号量 */
sem_t *pre_init_sem(unsigned int, const char *, int, mode_t, unsigned int);

int pre_rm_sem(sem_t *,  const char *);

/* set fd cloexec */
int set_cloexec(int);

/* LIST */
pre_list *pre_get_node();

pre_list *pre_list_insert(pre_list *, pre_list *);

int pre_list_destory(pre_list *);

void pre_node_free(pre_list *);

/* SHM QUEUE */
int addinfo_ToRshm(void *, r_info *);
int addinfo_ToQshm(void *, r_info *);

int getinfo_FromRshm(void *, r_info *);
int getinfo_FromQshm(void *, r_info *);

int Rshm_IsEmpty(void *);
int Qshm_IsEmpty(void *);

int Rshm_IsFull(void *);
int Qshm_IsFull(void *);

/* SIGNAL OPERATION */
typedef void (*sighandler_t)(int);

int pre_signal_catch(int, sighandler_t);

int pre_signal_ctl(int, int);

void pre_wait_ready();

void pre_tell_ready(pid_t, int);

/* Get PID from pid file. */
pid_t pre_getChildPid(int);

/* FILE LOCK */
int pre_lock_reg(int, int, int, off_t, int, off_t);

pid_t pre_lock_test(int, int, off_t, int, off_t);

#endif  /* _PRE_TOOLS_H */

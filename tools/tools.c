/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                                                                             *
 *    Describe :                                                               *
 *          工具函数.
 *
 *    Author   :  Xu-Jianhao                                                   *
 *                               Biu Biu Biu !!!                               *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include "predef.h"
#include "tools.h"
#include "loghelper.h"

/* 获取当前时间字符串格式 */
char *gettimeStr(char *buf, size_t len, const char *format)
{
    struct tm *tt = NULL;
    time_t t;
    char timestr[128] = "";
    
    t = time(NULL);
    tt = localtime(&t);

    strftime(timestr, sizeof(timestr), format, tt);
    
    if(len <= strlen(timestr)) {
        return NULL;
    }

    strcpy(buf, timestr);
    
    return buf;
}

/* Init CtlInfo */
CtlInfo *pre_init_ctl()
{
    CtlInfo *p;

    if((p = (CtlInfo *)malloc(sizeof(CtlInfo))) == NULL)
        return NULL;
    memset(p, 0, sizeof(CtlInfo));
    return p;
}

void pre_free_ctl(CtlInfo *info)
{
    free(info);
}


/* 初始化共享内存 */
int pre_init_shm(const char *pathname, int k_id, size_t size, int shmflg)
{
    int     shmid;
    key_t   s_key;

    if((s_key = ftok(pathname, k_id)) < 0)
        return PRE_ERR;

    if((shmid = shmget(s_key, size, shmflg)) == -1)
        return PRE_ERR;

    return shmid;
}

int pre_empty_shm(void *shmaddr)
{
    memset(shmaddr, 0, SHMSIZE);

    int *front = (int *)shmaddr;
    int *rear  = (int *)(shmaddr+4);

    *front = 0;
    *rear  = 0;

    return PRE_OK;
}

int pre_rm_shm(int shmid, const void *shmaddr)
{
    if(shmid == PRE_ERR)
        return PRE_OK;

    if(shmaddr != NULL || shmaddr != (void *)-1)
        if(shmdt(shmaddr) < 0)
            return PRE_ERR;

    if(shmctl(shmid, IPC_RMID, NULL) < 0)
        return PRE_ERR;

    return PRE_OK;
}

/* 初始化信号量 */
sem_t *pre_init_sem(unsigned int flag, const char *semName, int oflag, mode_t mode, unsigned int value)
{
    sem_t   *semid;

    if(flag == CREAT_SEM) {
        if((semid = sem_open(semName, oflag, mode, value)) == SEM_FAILED)
            return SEM_FAILED;
    } else {
        if((semid = sem_open(semName, 0)) == SEM_FAILED)
            return SEM_FAILED;
    }

    return semid;
}

int pre_rm_sem(sem_t *semid, const char *name)
{
    if(semid == SEM_FAILED)
        return PRE_OK;

    if(sem_close(semid) < 0)
        return PRE_ERR;

    if(sem_unlink(name) < 0)
        return PRE_ERR;

    return PRE_OK;
}

/* 设置文件描述符执行时关闭标志 */
int set_cloexec(int fd)
{
    int     val;

    if((val = fcntl(fd, F_GETFD, 0)) < 0)
        return PRE_ERR;

    val |= FD_CLOEXEC;
    return(fcntl(fd, F_SETFD, val));
}

/* LIST */
pre_list *pre_get_node()
{
    pre_list *m_node = NULL;

    if((m_node = (pre_list *)malloc(sizeof(pre_list))) == NULL)
        return NULL;

    m_node->len     = 0;
    m_node->data    = NULL;
    m_node->next    = NULL;

    return m_node;
}

pre_list *pre_list_insert(pre_list *list, pre_list *node)
{
    if(node == NULL) {
        return NULL;
    }

    if(list == NULL) {
        return node;
    }

    pre_list    *p;

    p = list;

    while(p->next != NULL)
    {
        p = p->next;
    }

    p->next = node;

    return list;
}

int pre_list_destory(pre_list *list)
{
    if(list == NULL)
        return PRE_OK;
    
    pre_node_free(list);

    return PRE_OK;
}

void pre_node_free(pre_list *node)
{
    if(node->next != NULL)
        pre_node_free(node->next);

    if(node->data != NULL)
        free(node->data);
    
    free(node);

    return;
}

/* SHM */
int pre_addinfo_Toshm(void *shmaddr, r_info *info)
{
    int     *rear;
    void    *content;

    rear    = (int *)(shmaddr+4);
    content = shmaddr+8;

    if(pre_shm_IsFull(shmaddr) == PRE_TRUE) {
        return SHMFULL;
    }

    memcpy(content+(*rear)*sizeof(r_info), info, sizeof(r_info));
    
    *rear = ((*rear)+1) % MAXCOUNT;

    return PRE_OK;
}

int pre_getinfo_Fromshm(void *shmaddr, r_info *info)
{
    int     *front;
    void    *content;

    front   = (int *)shmaddr;
    content = shmaddr+8;

    if(pre_shm_IsEmpty(shmaddr) == PRE_TRUE) {
        return SHMEMPTY;
    }

    memcpy(info, content+(*front)*sizeof(r_info), sizeof(r_info));

    *front = ((*front)+1) % MAXCOUNT;

    return PRE_OK;
}

int pre_shm_IsEmpty(void *shmaddr)
{
    int     *front;
    int     *rear;

    front   = (int *)shmaddr;
    rear    = (int *)(shmaddr+4);

    if((*front) == (*rear))
        return PRE_TRUE;
    else
        return PRE_FALSE;
}

int pre_shm_IsFull(void *shmaddr)
{
    int     *front;
    int     *rear;

    front   = (int *)shmaddr;
    rear    = (int *)(shmaddr+4);

    if(((*rear) + 1) % MAXCOUNT == (*front))
        return PRE_TRUE;
    else
        return PRE_FALSE;
}

/* SIGNAL OPERATION */
int pre_signal_catch(int signo, sighandler_t handler)
{
    struct sigaction act;
    
    act.sa_handler = handler;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    if(signo == SIGALRM) {
#ifdef SA_INTERRUPT
        act.sa_flags |= SA_INTERRUPT;
#endif
    } else {
        act.sa_flags |= SA_RESTART;         /* 设置系统调用重启动 */
    }

    if((sigaction(signo, &act, NULL)) < 0)
        return PRE_ERR;

    return PRE_OK;
}

int pre_signal_block(int signo, sigset_t *oldmask)
{
    sigset_t newmask;

    sigemptyset(&newmask);
    sigaddset(&newmask, signo);

    if(sigprocmask(SIG_BLOCK, &newmask, oldmask) < 0)
        return PRE_ERR;

    return PRE_OK;
}


void pre_wait_ready()
{
    sigset_t zeromask;

    sigemptyset(&zeromask);

    sigsuspend(&zeromask);

}

void pre_tell_ready(pid_t pid, int signo)
{
    kill(pid, signo);
}


/* 获取pid,并转换为pid_t */
pid_t pre_getChildPid(int cid)
{
    int     fd, n;
    pid_t   pid[3];
    char    buf[MAXLINE];

    if((fd = open(PIDFILE, O_RDWR)) < 0) 
        return PRE_ERR;
    
    lseek(fd, 0, SEEK_SET);

    if((n = read(fd, buf, sizeof(buf)) <= 0))
        return PRE_ERR;

    buf[n] = '\0';

    sscanf(buf, "%d\n%d\n%d\n", (int *)&pid[0], (int *)&pid[1], (int *)&pid[2]);

    return pid[cid];
}


/* FILE LOCK */

/* 加锁或解锁一个文件区域的函数 */
int pre_lock_reg(int fd, int cmd, int type, off_t offset, int whence, off_t len)
{
    struct flock lock;

    lock.l_type     = type;         /* F_RDLCK, F_WRLCK, F_UNLCK */
    lock.l_start    = offset;       /* bytes offset, relative to l_whence */
    lock.l_whence   = whence;       /* SEEK_SET, SEEK_CUR, SEEK_END */
    lock.l_len      = len;          /* #bytes (0 means to EOF) */

    return(fcntl(fd, cmd, &lock));
}

/* 测试一把记录锁 */
pid_t pre_lock_test(int fd, int type, off_t offset, int whence, off_t len)
{
    struct flock lock;

    lock.l_type     = type;         /* F_EDLCK or F_WRLCK */
    lock.l_start    = offset;       /* byte offset, relative to l_whence */
    lock.l_whence   = whence;
    lock.l_len      = len;

    if(fcntl(fd, F_GETLK, &lock) < 0)
        return PRE_ERR;

    if(lock.l_type == F_UNLCK)
        return PRE_FALSE;               /* false,region isn't locked by another proc */

    return lock.l_pid;                  /* true, return pid of lock owner */
}


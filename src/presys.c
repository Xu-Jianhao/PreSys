/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                                                                             *
 *    Describe :                                                               *
 *          Prescription Management System.
 *
 *    Author   :  Xu-Jianhao                                                   *
 *                               Biu Biu Biu !!!                               *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include <sys/resource.h>
#include <sys/wait.h>

#include "predef.h"
#include "tools.h"
#include "loghelper.h"

#define MODULE      "PRESYS"

void pre_exit_log(int flag, const char *funcName, int errNum, const char *errMsg);

#define EXITLOG(funcName, errNum) \
    pre_exit_log(PRE_TRUE, (funcName), (errNum), NULL);

#define EXITLOG2(funcName, errMsg) \
    pre_exit_log(PRE_FALSE, (funcName), 0, (errMsg));


void pre_init();

pid_t pre_create_process(pro_handler);

int pre_writepid_tofile(pid_t *pid, int fd);

int pre_startUp_child(int flag, int fd);

void daemonize();

void pre_child_destroy(int arg);
void pre_child_quit(int arg);

void pre_loopWait();


extern void pre_connect_process();

extern void pre_business_process();

int     RSflag;
int     Qflag;
pid_t   pid[3];

int main(int argc, const char *argv[])
{
    int     pfd;

    pre_init();

    /* TODO: start-up arg. */

    /* start-up daemonize. */
    daemonize();

    /* Create pidfile and lock pidfile. */
    if((pfd = open(PIDFILE, O_CREAT|O_WRONLY|O_SYNC, S_IRUSR|S_IWUSR)) < 0) {
        LOG_WRITE("open pidfile failed.");
        exit(-1);
    }
   
    set_cloexec(pfd);

    if(pre_write_lock(pfd, 0, SEEK_SET, 0) < 0) {
        LOG_WRITE("lock pidfile failed.");
        exit(-1);
    }

    /* First Start-Up. */
    pre_startUp_child(1, pfd);

    pre_loopWait(pfd);

    LOG_WRITE("PreSys is Quit!!");

    exit(0);
}

void pre_loopWait(int fd)
{

    while(!Qflag)
    {
        if(sleep(5) > 0) {
            errno = 0;
            if(Qflag) {
                break;
            }
        }
        if(RSflag) {
            if(pre_startUp_child(0, fd) != PRE_OK)
                break;
            RSflag = 0;
        }
    }
    kill(pid[1], SIGQUIT);
    kill(pid[2], SIGQUIT);
}

/* flag 为启动标志,第一次启动flag值为1 */
int pre_startUp_child(int flag, int fd) 
{
    pid[0] = getpid();

    if(!flag) {
        if(kill(pid[1], 0) == -1 && errno == ESRCH) {
            errno = 0;
            pid[1] = pre_create_process(pre_connect_process);   /* Create connect process. */
            LOG_WRITE("Connect  Process Restart Success!");
        }
        if(kill(pid[2], 0) == -1 && errno == ESRCH) {
            errno = 0;
            pid[2] = pre_create_process(pre_business_process);  /* Create connect process. */
            LOG_WRITE("Connect  Process Restart Success!");
        }
    } else {
        pid[1] = pre_create_process(pre_connect_process);       /* Create connect process. */
        LOG_WRITE("Connect  Process Start-Up Success!");

        sleep(1);

        pid[2] = pre_create_process(pre_business_process);      /* Create business process. */
        LOG_WRITE("Business Process Start-Up Success!");
    }
            
    if(pre_writepid_tofile(pid, fd) != PRE_OK) {
        LOG_WRITE("Write pid to pidfile failed!");
        return PRE_ERR;
    }
    return PRE_OK;
}


void pre_init()
{
    int     pfd;

    if(access(PIDFILE, F_OK) == 0) {
        if((pfd = open(PIDFILE, O_WRONLY)) < 0)
            EXITLOG("open PIDFILE", errno);

        set_cloexec(pfd);
        
        if(!pre_is_write_lockable(pfd, 0, SEEK_SET, 0))
            EXITLOG2(MODULE, "process already exists!!");
    } else {
        errno = 0;
    }

    /* Register SIGCHLD catch function. */
    if(pre_signal_catch(SIGCHLD, pre_child_destroy) != PRE_OK) {
        EXITLOG("catch SIGCHLD failed!", errno);
    }

    /* Register SIGQUIT catch function. */
    if(pre_signal_catch(SIGQUIT, pre_child_quit) != PRE_OK) {
        EXITLOG("catch SIGQUIT failed!", errno);
    }

    Qflag  = 0;
    RSflag = 0;
}


pid_t pre_create_process(pro_handler processFunc)
{
    pid_t   pid;

    if((pid = fork()) < 0) {
        LOG_WRITE("fork error");
        exit(-1);
    } else if(pid > 0) {
        return pid;
    }

    /* TODO : 恢复信号集 */
    pre_signal_catch(SIGCHLD, SIG_DFL);
    //pre_signal_catch(SIGQUIT, SIG_DFL);

    if(processFunc != NULL) {
        processFunc();
    }
    exit(0); 
}

int pre_writepid_tofile(pid_t *pid, int fd)
{
    int i;

    lseek(fd, 0, SEEK_SET);

    for(i = 0; i < 3; i++) {
        if(dprintf(fd, "%d\n", (int)pid[i]) <= 0)
            return PRE_ERR;
    }
    return PRE_OK;
}

void daemonize()
{
    int                 fd0, fd1, fd2;
    pid_t               pid;
    struct sigaction    sa;

    /* Clear file creation mask. */
    umask(0);

    /* Become a session leader to lose controlling TTY. */
    
    if((pid = fork()) < 0) {
        EXITLOG("fork", errno);
    } else if(pid != 0) {
        exit(0);
    }

    setsid();
    
    /* Ensure future open's won't allocate controlling TTYs. */
    
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if(sigaction(SIGHUP, &sa, NULL) < 0)
        EXITLOG("sigaction", errno);

    if((pid = fork()) < 0) {
        EXITLOG("fork", errno);
    } else if(pid != 0) {
        exit(0);
    }

    /*
     * Change the current working directory to the presys
     * so we won't prevent file systems from being unmounted.
     */
    if(chdir(WORKDIR) < 0) 
        EXITLOG("chdir", errno);

    /* Close all open file decsriptors. */
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    fd0 = open("/dev/null", O_RDWR);
    fd1 = dup(0);
    if((fd2 = open_logfile(LOGPATH, LOGFILE)) == PRE_ERR)
        exit(-1);
    
    if(fd0 != 0 || fd1 != 1 || fd2 != 2) {
        write(fd2, "START ERROR from open fd1 fd2 fd3", 34);
        exit(-1);
    }
}

/* 子进程回收 */
void pre_child_destroy(int arg)
{
    pid_t   pid;

    for(;;) {
        if((pid = waitpid(-1, NULL, WNOHANG)) <= 0)
            break;

        LOG_WRITE("Child Process [%ld]  exit!", (long)pid);

        if(!Qflag)
            RSflag = 1;
    }
}

void pre_child_quit(int arg)
{
    Qflag = 1;
}

void pre_exit_log(int flag, const char *funcName, int errNum, const char *errMsg)
{
    if(flag == PRE_FALSE)
        printf("START ERROR: %s - %s\n", funcName, errMsg);
    else
        printf("START ERROR: %s - %s\n", funcName, strerror(errNum)); \
    exit(-1);
}



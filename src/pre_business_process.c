/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                                                                             *
 *    Describe :                                                               *
 *          Business Process - Operation mysql.
 *
 *    Author   :  Xu-Jianhao                                                   *
 *                               Biu Biu Biu !!!                               *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include "predef.h"
#include "loghelper.h"
#include "tools.h"

#define MODULE "BUSINESS"

extern int Qflag;

void pre_loopStart(CtlInfo *ctlinfo, MYSQL *mysql);

void pre_sigusr1_func(int arg);

int pre_Get_Request(CtlInfo *ctlinfo, r_info *info);

int pre_Return_Respond(CtlInfo *ctlinfo, r_info *info);

int pre_Request_handle(r_info *info, MYSQL * mysql);

void pre_packing(r_info *info, Request *req);

int pre_unpacking(Request *req, char *content) ;

void pre_business_process()
{
    CtlInfo     *ctlinfo;
    MYSQL       *mysql;

    do {
        
        if((ctlinfo = pre_init_ctl()) == NULL) {
            LOG_WRITE("pre_init_ctl failed");
            break;
        }

        /* Register signal SIGUSR1. */
        if(pre_signal_catch(SIGUSR1, pre_sigusr1_func) != PRE_OK) {
            LOG_WRITE("Register SIGUSR1 catch function failed");
            break;
        }

        /* Block SIGUSR1. */
        if(pre_signal_ctl(SIGUSR1, SIG_BLOCK) != PRE_OK) {
            LOG_WRITE("Block SIGUSR1 failed");
            break;
        }

        /* Get SEM. */
        if((ctlinfo->semid = pre_init_sem(0, SEMNAME, 0, 0, 0)) == SEM_FAILED) {
            LOG_WRITE("pre_init_sem GET failed");
            break;
        }
        
        LOG_WRITE("GET SEM SUCCESS!!  [semid = %p]", ctlinfo->semid);

        /* Get SHM. */
        if((ctlinfo->shmid = pre_init_shm(PATHNAME, PROID, 0, 0)) == PRE_ERR) {
            LOG_WRITE("pre_init_shm GET failed");
            break;
        }
        
        /* Connect SHM. */
        if((ctlinfo->shmaddr = shmat(ctlinfo->shmid, NULL, 0)) == (void *)-1) {
            LOG_WRITE("shmat shmaddr failed");
            break;
        }
        
        LOG_WRITE("CONNECT SHM SUCCESS!!  [shmid = %d]", ctlinfo->shmid);

        /* Get Connect pid. */
        if((ctlinfo->pid = pre_getChildPid(1)) <= 0) {
            LOG_WRITE("Get Connect pid failed");
            break;
        }

        /* Connect mysql. */
        if(pre_mysql_connect(&mysql) == PRE_ERR) {
            LOG_WRITE("Mysql Connect failed %s", mysql_error(mysql));
            break;
        }
        
        LOG_WRITE("CONNECT MYSQL SUCCESS!!");

        Qflag = 0;

        /* LOOP. */
        pre_loopStart(ctlinfo, mysql);
    
    
    } while(0);

    /* ---clear--- */
    if(ctlinfo->semid != NULL && ctlinfo->semid != SEM_FAILED) {
        sem_close(ctlinfo->semid);
        ctlinfo->semid = NULL;
    }

    if(ctlinfo->shmid != PRE_ERR && ctlinfo->shmaddr != NULL && ctlinfo->shmaddr != (void *)-1) {
        shmdt(ctlinfo->shmaddr);
        ctlinfo->shmaddr = NULL;
    }

    pre_mysql_destory(mysql);

    pre_free_ctl(ctlinfo);
}

void pre_loopStart(CtlInfo *ctlinfo, MYSQL *mysql)
{
    int         ret;
    r_info      info;

    while(!Qflag) {
       
        /* Get data from SHM. */
        if(pre_Get_Request(ctlinfo, &info) == REMPTY) {

            LOG_WRITE("Rqueue is EMPTY!!");
        
            pre_wait_ready();
            
            continue;
        }

        
        /* Mysql opertion. */
        LOG_WRITE("Get data is [%s]", info.content);
        if(pre_Request_handle(&info, mysql) == PRE_ERR) {
        
            LOG_WRITE("Request handler failed  unknown error");

            pre_requestError(&info);
        }

        /* insert data to QSHM. */
        if((ret = pre_Return_Respond(ctlinfo, &info)) == QFULL) {
    
            LOG_WRITE("Qqueue is FULL");    /* sleep  and try */
            
            break;
        
        } else if(ret == PRE_ERR) {
            
            LOG_WRITE("pre_Return_Respond failed");
            
            break;
        }

    }
}

int pre_Get_Request(CtlInfo *ctlinfo, r_info *info)
{
    sem_wait(ctlinfo->semid);
    
    if(getinfo_FromRshm(ctlinfo->shmaddr, info) == REMPTY) {
        
        sem_post(ctlinfo->semid);

        return REMPTY;
    }

    sem_post(ctlinfo->semid);

    return PRE_OK;
}

int pre_Return_Respond(CtlInfo *ctlinfo, r_info *info)
{
    sem_wait(ctlinfo->semid);

    if(addinfo_ToQshm(ctlinfo->shmaddr, info) == QFULL) {
    
        sem_post(ctlinfo->semid); 
        
        return QFULL;
    }

    sem_post(ctlinfo->semid);

    if(kill(ctlinfo->pid, 0) == -1 && errno == ESRCH) {
        
        errno = 0;

        if((ctlinfo->pid = pre_getChildPid(1)) <= 0) {
        
            LOG_WRITE("Get pid[1] failed");

            return PRE_ERR;
        }
    }

    pre_tell_ready(ctlinfo->pid, SIGUSR2);

    return PRE_OK;
}

/*
 * type  sockfd  retcode   content
 * */
int pre_Request_handle(r_info *info, MYSQL *mysql)
{
    Request req;

    memset(&req, 0, sizeof(req));

    if(pre_unpacking(&req, info->content) != PRE_OK) {
        LOG_WRITE("Unpacking failed");
        return PRE_ERR;
    }

    switch(info->type) {
        
        case SELECT :

            if((info->retCode = pre_select(&req, mysql)) == SYSERROR) {
                
                LOG_WRITE("SELECT SQL failed  : %s", mysql_error(mysql));
            }
                 
            break;
        
        case INSERT :

            if((info->retCode = pre_insert(&req, mysql)) == SYSERROR) {
                
                LOG_WRITE("INSERT SQL failed %s", mysql_error(mysql));
            }

            break;

        case DELETE :

            if((info->retCode = pre_delete(&req, mysql)) == SYSERROR) {
                
                LOG_WRITE("DELETE SQL failed  : %s", mysql_error(mysql));
            }

            break;

        case UPDATE :

            if((info->retCode = pre_update(&req, mysql)) == SYSERROR) {
                
                LOG_WRITE("UPDATE SQL failed  : %s", mysql_error(mysql));
            }

            break;

        default:

            pre_noexist(&req);

            LOG_WRITE("Request type no exist!!");

            break;
    }

    errno = 0;

    pre_packing(info, &req);

    return PRE_OK;
}

int pre_unpacking(Request *req, char *content) 
{
    int     i;
    char    *pos;

    pos = strtok(content, "|");
    
    for(i = 0; pos != NULL; i++) {
        switch(i) {
        
            case 0 :
                memcpy(req->name, pos, strlen(pos));
                break;
            case 1 :
                memcpy(req->addr, pos, strlen(pos));
                break;
            case 2 :
                memcpy(req->time, pos, strlen(pos));
                break;
            case 3 :
                memcpy(req->msg, pos, strlen(pos));
                break;

            default:
                LOG_WRITE("Data ERROR!!!");
                return PRE_ERR;
        
        }
        pos = strtok(NULL, "|");
    }

    if(i == 0)
        return PRE_ERR;
    return PRE_OK;
}

void pre_packing(r_info *info, Request *req)
{
    memset(info->content, 0, MAXINFO);

    sprintf(info->content, "%s|%s|%s|%s", req->name, req->addr, req->time, req->msg);
}

void pre_sigusr1_func(int arg)
{
}

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

void pre_loopStart(CtlInfo *ctlinfo);

void pre_sigusr1_func(int arg);

int pre_Get_Request(CtlInfo *ctlinfo, r_info *info);

int pre_Return_Respond(CtlInfo *ctlinfo, r_info *info);

int pre_Request_handle(r_info *info);

void pre_business_process()
{
    CtlInfo     *ctlinfo;

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


        Qflag = 0;

        /* LOOP. */
        pre_loopStart(ctlinfo);
    
    
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

    pre_free_ctl(ctlinfo);
}

void pre_loopStart(CtlInfo *ctlinfo)
{
    int         ret;
    r_info      info;

    while(!Qflag) {
       
        LOG_WRITE("Loop  - Wait");
        pre_wait_ready();
        LOG_WRITE("Catch - Done");
        
        /* Get data from SHM. */
        if(pre_Get_Request(ctlinfo, &info) == REMPTY) {

            LOG_WRITE("Rqueue is EMPTY!!");
        
            continue;
        }

        
        /* Mysql opertion. */
        info.content[strlen(info.content)] = '\0';
        LOG_WRITE("Get data is [%s]", info.content);
        pre_Request_handle(&info);

       
        /* inset data to QSHM. */
        if((ret = pre_Return_Respond(ctlinfo, &info)) == QFULL) {
    
            LOG_WRITE("Qqueue is FULL");
            
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
int pre_Request_handle(r_info *info)
{
    info->retCode = 0;

    switch(info->type) {
        
        case SELECT :

            break;
        
        case INSERT :

            break;

        case DELETE :

            break;

        case UPDATE :

            break;

        default:
            /* TODO */
            break;
    }

    return PRE_OK;
}

void pre_sigusr1_func(int arg)
{
}

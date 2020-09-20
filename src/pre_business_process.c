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
            LOG_WRITE("Catch SIGUSR1 failed");
            break;
        }

        /* Block SIGUSR1. */
        if(pre_signal_block(SIGUSR1, NULL) != PRE_OK) {
            LOG_WRITE("Block SIGUSR1 failed");
            break;
        }

        /* Get SEM. */
        if((ctlinfo->semid = pre_init_sem(0, SEMNAME, 0, 0, 0)) == SEM_FAILED) {
            LOG_WRITE("pre_init_sem GET failed");
            break;
        }
        LOG_WRITE("GET SEMID SUCCESS!!  [semid = %p]", ctlinfo->shmid);

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
        LOG_WRITE("CONNECT SHM SUCCESS!!");

        /* Connect mysql. */

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
    while(!Qflag) {
        pre_wait_ready();
        LOG_WRITE("----- Catch SIGUSR1 -----");
        
        /* Lock and Get data from SHM. */

        /* Mysql opertion. */

        /* Lock and inset data to SHM. */

        /* Tell SIGUSR2. */
        
    }
}

void pre_sigusr1_func(int arg)
{
}

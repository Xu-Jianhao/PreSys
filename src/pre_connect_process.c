/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                                                                             *
 *    Describe :                                                               *
 *          Connect Process - Processing client requests.
 *
 *    Author   :  Xu-Jianhao                                                   *
 *                               Biu Biu Biu !!!                               *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include <sys/socket.h>
#include <sys/epoll.h>
#include <netdb.h>


#include "predef.h"
#include "loghelper.h"
#include "tools.h"


#define QLEN        10
#define MODULE      "CONNECT"
#define EPSIZE      1024
#define HOSTLEN     128

extern int Qflag;

CtlInfo     *ctlinfo;

int pre_recv_request(int sockfd, CtlInfo *ctlinfo);

int pre_sendTo(r_info info);

int pre_socket_init(int type, const struct sockaddr *addr, socklen_t alen, int qlen)
{
    int fd, err;
    int reuse = 1;

    if((fd = socket(addr->sa_family, type, 0)) < 0) {
        return PRE_ERR;
    }

    do {
        if(set_cloexec(fd) < 0)
            break;

        /* 设置复用 */
        if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(int)) < 0)
            break;

        if(bind(fd, addr, alen) < 0)
            break;

        if(type == SOCK_STREAM || type == SOCK_SEQPACKET)
            if(listen(fd, qlen) < 0)
                break;

        return fd;

    }while(0);

    err = errno;
    close(fd);
    errno = err;
    return -1;
}

int pre_addfdToepoll(int efd, int sockfd)
{
    struct epoll_event  tep;

    memset(&tep, 0, sizeof(tep));
    tep.events  = EPOLLIN;
    tep.data.fd = sockfd;

    if(epoll_ctl(efd, EPOLL_CTL_ADD, sockfd, &tep) < 0)
        return PRE_ERR;

    return PRE_OK;
}

int pre_deletefdFromepoll(int efd, int sockfd)
{
    if(epoll_ctl(efd, EPOLL_CTL_DEL, sockfd, NULL) < 0)
        return PRE_ERR;
    
    close(sockfd);

    return PRE_OK;
}

int pre_start_listen(int efd, pre_list *lfd_list, CtlInfo *ctlinfo)
{
    int                nready, i, ret;
    int                cfd;
    socklen_t          c_len;
    pre_list           *node;
    struct sockaddr    c_addr;
    struct epoll_event ep[EPSIZE];

    while(!Qflag)
    {
        if((nready = epoll_wait(efd, ep, EPSIZE, -1)) < 0) {
            
            if(errno == EINTR) {
            
                errno = 0;
                
                LOG_WRITE("%s", "epoll_wait block from signal!!!");
                
                if(Qflag)
                    break;                      /* QUIT */
                
                continue;
            } else {
                
                LOG_WRITE("%s", "epoll_wait failed");
                
                return PRE_ERR;
            }
        }

        for(i = 0; i < nready; i++) {
            
            if(!(ep[i].events & EPOLLIN))       /* 非读事件 */
                continue;

            for(node = lfd_list; node != NULL; node = node->next) {
            
                if(ep[i].data.fd == *((int *)node->data)) {

                    c_len = sizeof(c_addr);

                    if((cfd = accept(ep[i].data.fd, &c_addr, &c_len)) < 0) {
                
                        LOG_WRITE("%s", "accept failed");
                        
                        return PRE_ERR;
                    }

                    if(pre_addfdToepoll(efd, cfd) != PRE_OK) {
                        
                        LOG_WRITE("add cfd [%d] to epoll failed", cfd);
                        
                        return PRE_ERR;
                    }
                    
                    LOG_WRITE("Client [%d] connect SUCCESS!!", cfd);
                    
                    break;
                } 
            }

            if((node != NULL) && (ep[i].data.fd == *((int *)node->data)))
                continue;

            /* 客户端业务请求 */
            if((ret = pre_recv_request(ep[i].data.fd, ctlinfo)) == PRE_ERR) {
            
                LOG_WRITE("pre_recv_request failed!!");

                Qflag = 1;
            
            } else if(ret == PRE_CLOSE) {
            
                if(pre_deletefdFromepoll(efd, ep[i].data.fd) != PRE_OK) {
                
                    LOG_WRITE("Close client fd [%d] failed", ep[i].data.fd);
                    
                    return PRE_ERR;
                
                }
                
                LOG_WRITE("Close client fd [%d] SUCESS!", ep[i].data.fd);
            
            }
        }
    }
    return PRE_OK;
}

void pre_sigusr2_func(int arg)
{
    r_info info;

    if(pre_signal_ctl(SIGUSR2, SIG_BLOCK) == PRE_ERR)
        return;

    /* Get Respond from Qshm. */
    sem_wait(ctlinfo->semid);
    
    while(!Qshm_IsEmpty(ctlinfo->shmaddr)) {
        
        getinfo_FromQshm(ctlinfo->shmaddr, &info);
        
        LOG_WRITE("Get info.content = [%s]", info.content);
        
        if(pre_sendTo(info) == PRE_SENDERROR) {
            break;
        }
    }
    
    sem_post(ctlinfo->semid);

    if(pre_signal_ctl(SIGUSR2, SIG_UNBLOCK) == PRE_ERR) {
        LOG_WRITE("SIGUSR2 pre_signal_ctl unblock failed");
        Qflag = 1;
    }
}


int pre_epoll_init(pre_list **list)
{   
    int     ret;
    int     efd, lfd;
    struct addrinfo *ailist = NULL;
    struct addrinfo *aip;
    pre_list        *node;
    struct addrinfo hint;
    char    host[HOSTLEN];

    if((efd = epoll_create(EPSIZE)) < 0) {
        
        LOG_WRITE("%s", "epoll_create failed");
        
        return PRE_ERR;
    }

    if(gethostname(host, sizeof(host)) < 0) {
    
        LOG_WRITE("%s", "gethostname failed");
        
        goto errout;
    }

    memset(&hint, 0, sizeof(hint));
    hint.ai_flags     = AI_PASSIVE;
    hint.ai_socktype  = SOCK_STREAM;
    hint.ai_canonname = NULL;
    hint.ai_addr      = NULL;
    hint.ai_next      = NULL;

    
    /* 获取套接字地址信息 */
    if((ret = getaddrinfo(host, SERVERNAME, &hint, &ailist)) != 0) {    
       
        LOG_WRITE("getaddrinfo failed %s", gai_strerror(ret));
        
        goto errout;

    } else {
        errno = 0;
    }

    for(aip = ailist; aip != NULL; aip = aip->ai_next) {

        if((lfd = pre_socket_init(SOCK_STREAM, aip->ai_addr, aip->ai_addrlen, QLEN)) > 0) {

            /* ADD lfd TO EPOLL */
            if(pre_addfdToepoll(efd, lfd) == PRE_OK) {                   

                if((node = pre_get_node()) == NULL) {
                   
                    LOG_WRITE("pre_get_node failed");
                    goto errout;

                }
                
                node->len = sizeof(int);
                node->data = malloc(node->len);
                memcpy(node->data, &lfd, node->len);

                if((*list = pre_list_insert(*list, node)) == NULL) {
                   
                    LOG_WRITE("pre_list_insert failed");
                    goto errout;
                
                }

                LOG_WRITE("Listen fd [%d] Add to epoll SUCCESS", lfd);

            } else {

                LOG_WRITE("listen fd [%d] add to epoll FAILED", lfd);
                goto errout;

            }
        } else {

            LOG_WRITE("init listen fd  failed", lfd);
            goto errout;

        }
    }
    return efd;


errout:
    close(efd);
    
    if(ailist != NULL)
        freeaddrinfo(ailist);

    for(node = *list; node != NULL; node = node->next)
        close(*((int *)node->data));

    return PRE_ERR;
}

int pre_recv_request(int sockfd, CtlInfo *ctlinfo)
{
    int         len;
    int         count;
    r_info      info;
    char        buf[MAXINFO];

    memset(buf, 0, MAXINFO);
    memset(&info, 0, sizeof(r_info));
    
    info.sockfd  = sockfd;

    len = recv(sockfd, buf, sizeof(buf), 0);        //ECONNRESET

    if(len < 0) {

        if(errno == ECONNRESET) {
            errno = 0;
            return PRE_CLOSE;
        }
    
        LOG_WRITE("recv failed, client sockfd [%d]  errno - [%d]", sockfd, errno);
        
        return PRE_ERR;
    } else if(len == 0) {
        return PRE_CLOSE;
    }

    LOG_WRITE("Recv from Client [%d] data : %s",info.sockfd, buf);

    if(memcmp(buf, SERVERNAME, 6) != 0) {
        
        strcpy(info.content, "Invalid data");
        
        info.retCode = RET_ERROR;
        
        if(pre_sendTo(info) == PRE_SENDERROR) {
            /* TODO */
        }
        
        return PRE_CLOSE;
    }

    info.type   = buf[6];
    memcpy(info.content, (buf+7), (strlen(buf)-7));

    /* Get pid[2] and tell pid[2]. */
    if(kill(ctlinfo->pid, 0) == -1 && errno == ESRCH) {

        errno = 0;

        if((ctlinfo->pid = pre_getChildPid(2)) <= 0) {
        
            LOG_WRITE("Get pid[2] failed");
            
            return PRE_ERR; 
        }
    }

    count = 0;
    do{
        sem_wait(ctlinfo->semid);
        
        if(addinfo_ToRshm(ctlinfo->shmaddr, &info) == RFULL) {
        
            sem_post(ctlinfo->semid);
            
            LOG_WRITE("Rshm is FULL");
            
            pre_tell_ready(ctlinfo->pid, SIGUSR1);
            
            count++;

            sleep(1);
            
            continue;
        }

        sem_post(ctlinfo->semid);
    
        pre_tell_ready(ctlinfo->pid, SIGUSR1);

        return PRE_OK;

    }while(count < 3);

    
    LOG_WRITE("Try 3 times Done!! Rqueue just FULL!!");
    
    return PRE_ERR;
}

int pre_sendTo(r_info info)
{
    int     len, count;
    char    buf[MAXINFO];

    memset(buf, 0, sizeof(buf));

    snprintf(buf, MAXINFO-1, "%d|%s", info.retCode, info.content);

    LOG_WRITE("SEND BUF - [%s]", buf);

    count = 0;
    while(count != strlen(buf)) {
      
        len = send(info.sockfd, buf + count, strlen(buf) - count, 0);
        
        if(len == -1) {
            return PRE_SENDERROR;
        }
        
        count += len;
    }

    LOG_WRITE("send msg to client [%d]-[%s]", info.sockfd, buf);
   
    return PRE_OK;
}


void pre_connect_process()
{
    int         efd;
    pre_list    *lfd_list;

    do{

        if((ctlinfo = pre_init_ctl()) == NULL) {
            LOG_WRITE("pre_init_ctl failed");
            break;
        }

        /* GET SEM. */
        if((ctlinfo->semid = pre_init_sem(0, SEMNAME, 0, 0, 0)) == SEM_FAILED) {
            LOG_WRITE("pre_init_sem GET failed");
            break;
        }
        LOG_WRITE("GET SEM SUCCESS!!   [semid = %p]", ctlinfo->semid);

        /* GET SHM. */
        if((ctlinfo->shmid = pre_init_shm(PATHNAME, PROID, 0, 0)) == PRE_ERR) {
            LOG_WRITE("pre_init_shm GET failed");
            break;
        }

        /* 连接共享内存 */
        if((ctlinfo->shmaddr = shmat(ctlinfo->shmid, NULL, 0)) == (void *)-1) {
            LOG_WRITE("shmat shmaddr failed");
            break;
        }
        LOG_WRITE("CONNECT SHM SUCCESS!!   [shmid = %d]", ctlinfo->shmid);

        /* 清空共享内存 */
        pre_empty_shm(ctlinfo->shmaddr);

        /* 初始化epoll,添加监听套接字 */
        if((efd = pre_epoll_init(&lfd_list)) < 0) {
            LOG_WRITE("EPOLL INIT failed");
            break;
        }

        Qflag = 0;
        
        /* Register SIGUSR2 catch function. */
        if(pre_signal_catch(SIGUSR2, pre_sigusr2_func) != PRE_OK) {
            LOG_WRITE("Register SIGUSR2 catch function failed");
            break;
        }

        /* Get Business pid. */
        if((ctlinfo->pid = pre_getChildPid(2)) <= 0) {
            LOG_WRITE("Get Business pid failed");
            break;
        }

        /* 启动监听 */
        if(pre_start_listen(efd, lfd_list, ctlinfo) == PRE_ERR) {
            LOG_WRITE("EPOLL LISTEN failed");
            break;
        } else {
            LOG_WRITE("EPOLL LISTEN END!!");
            break;
        }

    }while(0);


    /* -----clear----- */
    if(ctlinfo->semid != NULL && ctlinfo->semid != SEM_FAILED) {
        
        sem_close(ctlinfo->semid);
        
        ctlinfo->semid = NULL;
    }

    if(ctlinfo->shmid != PRE_ERR && ctlinfo->shmaddr != NULL && ctlinfo->shmaddr != (void *)-1) {
        
        shmdt(ctlinfo->shmaddr);
        
        ctlinfo->shmaddr = NULL;
    }
    
    pre_list_destory(lfd_list);
    
    pre_free_ctl(ctlinfo);
}


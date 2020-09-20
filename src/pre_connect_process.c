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

    while(1)
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
            LOG_WRITE("Client [%d] Request!!!!!", ep[i].data.fd);
            if((ret = pre_recv_request(ep[i].data.fd, ctlinfo)) == PRE_ERR) {
                continue;
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
    pid_t       pid;
    char        buf[1024];
    r_info      info;

    info.flag    = REQUEST;
    info.sockfd  = sockfd;

    len = recv(sockfd, buf, sizeof(buf), 0);

    if(len < 0) {
        LOG_WRITE("recv failed, client sockfd [%d]", sockfd);
        return PRE_ERR;
    } else if(len == 0) {
        LOG_WRITE("client sockfd [%d] close", sockfd);
        return PRE_CLOSE;
    }

    if(memcmp(buf, SERVERNAME, 6) != 0) {
        strcpy(info.content, "Invalid data");
        info.retCode = RET_ERROR;
        if(pre_sendTo(info) == PRE_SENDERROR) {
            /* TODO */
        }
        return PRE_CLOSE;
    }

    info.type   = buf[6];
    memcpy(info.content, (buf+7), strlen(buf+7));

    sem_wait(ctlinfo->semid);
    if(pre_addinfo_Toshm(ctlinfo->shmaddr, &info) == SHMFULL) {
        sem_post(ctlinfo->semid);
        LOG_WRITE("%s", "SHM is FULL");
        return PRE_OK;
    }
    sem_post(ctlinfo->semid);    

    /* Get pid[2] and tell pid[2]. */
    if((pid = pre_getChildPid(2)) < 0) {
        LOG_WRITE("Get pid[2] failed");
        return PRE_ERR;                 /* TODO */
    }
    pre_tell_ready(pid, SIGUSR1);
    return PRE_OK;
}

int pre_sendTo(r_info info)
{
    int     len, count;
    char    buf[MAXINFO];

    memset(buf, 0, sizeof(buf));

    snprintf(buf, MAXINFO-1, "%s%d", SERVERNAME, info.retCode);

    memcpy(buf+strlen(buf), info.content, strlen(info.content));

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
    CtlInfo     *ctlinfo;


    do{

        if((ctlinfo = pre_init_ctl()) == NULL) {
            LOG_WRITE("pre_init_ctl failed");
            break;
        }

        /* 创建信号量 */
        if((ctlinfo->semid = pre_init_sem(CREAT_SEM, SEMNAME,\
                        O_CREAT|O_EXCL, S_IRUSR|S_IWUSR, 1)) == SEM_FAILED)
        {
            LOG_WRITE("pre_init_sem CREAT failed");
            break;
        }
        LOG_WRITE("CREATE SEMID SUCCESS!!   [semid = %p]", ctlinfo->semid);

        /* 创建共享内存 */
        if((ctlinfo->shmid = pre_init_shm(PATHNAME, PROID, SHMSIZE,\
                        IPC_CREAT|IPC_EXCL|S_IRUSR|S_IWUSR)) == PRE_ERR) 
        {
            LOG_WRITE("pre_init_shm CREAT failed");
            break;
        }
        LOG_WRITE("CREATE SHMID SUCCESS!!   [shmid = %d]", ctlinfo->shmid);

        /* 连接共享内存 */
        if((ctlinfo->shmaddr = shmat(ctlinfo->shmid, NULL, 0)) == (void *)-1) {
            LOG_WRITE("shmat shmaddr failed");
            break;
        }

        /* 清空共享内存 */
        pre_empty_shm(ctlinfo->shmaddr);

        /* 初始化epoll,添加监听套接字 */
        if((efd = pre_epoll_init(&lfd_list)) < 0) {
            LOG_WRITE("EPOLL INIT failed");
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
    if(pre_rm_sem(ctlinfo->semid, SEMNAME) != PRE_OK) {
        LOG_WRITE("%s", "pre_rm_sem failed");
    }
    if(pre_rm_shm(ctlinfo->shmid, ctlinfo->shmaddr) != PRE_OK) {
        LOG_WRITE("%s", "pre_rm_shm failed");
    }
    pre_list_destory(lfd_list);
    pre_free_ctl(ctlinfo);
}


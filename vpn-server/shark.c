#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>       //read() write()
#include <net/if.h>       //struct ifreq, IFNAMSIZ
#include <sys/ioctl.h>    //TUNSETIFF
#include <sys/epoll.h>    //epoll
#include <fcntl.h>        //O_RDWR
#include <linux/if_tun.h> //IFF_TAP
#include <linux/ip.h>     //struct iphdr
#include <arpa/inet.h>    //struct in_addr
#include <netinet/tcp.h>  //struct tcphdr
#include <netinet/udp.h>  //struct udphdr

#define DEBUG

#define MAX_PKG_LEN 65535

typedef struct iphdr* piphdr;
typedef struct tcphdr* ptcphdr;
typedef struct udphdr* pudphdr;
typedef unsigned char u_char;

int HEAD_IP = 20;
int OPEN_MAX = 500;
int LISTENQ = 5;

void ProcessShark(struct sockaddr_in* clent_addr, u_char* user_data, int data_size);
void ProcessBack(u_char* buffer, int nread);

int udp_fd;
int tun_fd;
struct sockaddr_in* clent_addrs[256] = { 0 };
int main(int argc, char* argv[])
{
    if (argc != 2) {
        printf("Usage: ./shark $port\n");
        return 2;
    }
    int listen_port = atoi(argv[1]);
    if (listen_port == 0) {
        printf("listen_port[%d] error!\n", listen_port);
        return 2;
    }

    printf("sharking...\n");

    udp_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_fd < 0) {
        printf("create socket fail!\n");
        return -1;
    }
    struct sockaddr_in ser_addr;
    memset(&ser_addr, 0, sizeof(ser_addr));
    ser_addr.sin_family = AF_INET;
    ser_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    ser_addr.sin_port = htons(listen_port);
    //struct sockaddr* ser_addr = (struct sockaddr*)&ser_addr;
    int ret = bind(udp_fd, (struct sockaddr*)&ser_addr, sizeof(ser_addr));
    if (ret < 0) {
        printf("socket bind[%d] fail!\n", listen_port);
        return -1;
    }

    char *clonedev = "/dev/net/tun";
    if ((tun_fd = open(clonedev, O_RDWR)) < 0) {
        printf("error1\n");
        return -1;
    }
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_flags = IFF_TUN | IFF_NO_PI;
    strncpy(ifr.ifr_name, "tun0", IFNAMSIZ);
    int err;
    if ((err = ioctl(tun_fd, TUNSETIFF, (void *) &ifr)) < 0) {
        close(tun_fd);
        printf("error2 err[%d]\n", err);
        return err;
    }
    printf("tun/tap[%s]\n", ifr.ifr_name);

    int epollfd = epoll_create(OPEN_MAX);
    if (epollfd <= 0) {
        printf("epollfd[%d]\n", epollfd);
        return epollfd;
    }

    struct epoll_event epev;
    epev.events = EPOLLIN;
    epev.data.fd = udp_fd;
    ret = epoll_ctl(epollfd, EPOLL_CTL_ADD, udp_fd, &epev);
    if (ret != 0) {
        printf("ret1[%d]\n", ret);
        return ret;
    }
    listen(udp_fd, LISTENQ);

    struct epoll_event epev2;
    epev2.events = EPOLLIN;
    epev2.data.fd = tun_fd;
    ret = epoll_ctl(epollfd, EPOLL_CTL_ADD, tun_fd, &epev2);
    if (ret != 0) {
        printf("ret2[%d]\n", ret);
        return ret;
    }
    listen(tun_fd, LISTENQ);

    struct epoll_event events_in[OPEN_MAX];
    uint8_t buf[MAX_PKG_LEN] = { 0 };

    socklen_t len = sizeof(struct sockaddr_in);
    while (1) {
        int event_count = epoll_wait(epollfd, events_in, OPEN_MAX, -1);
        //超时返回 0 , 出错返回 -1 , timeout 设置为 -1 表示无限等待.
        if (event_count == -1) {
            printf("epoll_wait error\n");
            exit(1);
        }
        for (int i = 0; i < event_count; i++) {
            if (events_in[i].data.fd == tun_fd && events_in[i].events & EPOLLIN) {
                int n = read(tun_fd, buf, MAX_PKG_LEN);
                if (n <= 0) continue;
                ProcessBack(buf, n);
            } else if (events_in[i].data.fd == udp_fd && events_in[i].events & EPOLLIN) {
                struct sockaddr_in* clent_addr = (struct sockaddr_in*)malloc(len);
                int n = recvfrom(udp_fd, buf, MAX_PKG_LEN, 0, (struct sockaddr*)clent_addr, &len);
                if (n <= 0) continue;
                ProcessShark(clent_addr, buf, n);
            }
        }
    }

    return 0;
}

void ProcessShark(struct sockaddr_in* clent_addr, u_char* user_data, int data_size)
{
    int cip = user_data[15]; //源地址,子网ip最后
    if (!clent_addrs[cip]) free(clent_addrs[cip]);
    clent_addrs[cip] = clent_addr;

#ifdef DEBUG
    piphdr user_data_ip = (piphdr)user_data;
    char ip1[16] = { 0 };
    char ip2[16] = { 0 };
    sprintf(ip1, "%s", inet_ntoa(*(struct in_addr*)(&user_data_ip->saddr)));
    sprintf(ip2, "%s", inet_ntoa(*(struct in_addr*)(&user_data_ip->daddr)));
    unsigned ack = -1;
    u_short port_src, port_dst;
    if (user_data_ip->protocol == IPPROTO_TCP) {
        ptcphdr tcp = (ptcphdr)(user_data + HEAD_IP);
        port_src = ntohs(tcp->source);
        port_dst = ntohs(tcp->dest);
    } else if (user_data_ip->protocol == IPPROTO_UDP) {
        pudphdr udp = (pudphdr)(user_data + HEAD_IP);
        port_src = ntohs(udp->source);
        port_dst = ntohs(udp->dest);
    } else {
        printf("protocol[%d] return\n", user_data_ip->protocol);
        return;
    }
    printf("send %s:%d -> %s:%d data_size:%d ip_p:%d tot_len[%d]\n",
        ip1, port_src, ip2, port_dst, data_size, user_data_ip->protocol,
        ntohs(user_data_ip->tot_len));
#endif

    write(tun_fd, user_data, data_size);
}

void ProcessBack(u_char* buffer, int nread)
{
    piphdr pip = (piphdr)buffer;
    if (nread < 4 || nread != ntohs(pip->tot_len)) {
        printf("read err[%d][%d]\n", nread, ntohs(pip->tot_len));
        return;
    }

#ifdef DEBUG
    char ip1[16] = { 0 };
    char ip2[16] = { 0 };
    sprintf(ip1, "%s", inet_ntoa(*(struct in_addr*)(&pip->saddr)));
    sprintf(ip2, "%s", inet_ntoa(*(struct in_addr*)(&pip->daddr)));
    printf("nread[%d] tot_len[%d] protocol[%d] saddr[%s] daddr[%s]\n",
        nread, ntohs(pip->tot_len), pip->protocol, ip1, ip2);
#endif

    int cip = buffer[19]; //目的地址,子网ip最后
    struct sockaddr* clent_addr = (struct sockaddr*)clent_addrs[cip];
    if (!clent_addr) return;

    //发包
    sendto(udp_fd, buffer, nread, 0, clent_addr, sizeof(struct sockaddr_in));
}

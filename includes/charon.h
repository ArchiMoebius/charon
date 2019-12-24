#ifndef TUNPAT_H
#define TUNPAT_H

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/if_tun.h>
#include <net/if.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#define BUFSIZE 2048

#ifdef DEBUG
#define printd(x) printf x
#else
#define printd(x) \
  do {            \
  } while (0)
#endif

enum SOCKET_USE { SU_SERVER, SU_CLIENT };

extern enum SOCKET_USE socketUse;

extern int crn_tun(int* fd,
                   struct ifreq* ifr,
                   char* dev,
                   char* deviceIP,
                   char* destinationIP,
                   int flags);

extern int crn_socket(int* sock_fd,
                      struct sockaddr_in* sock_addr,
                      char* ip,
                      int port,
                      enum SOCKET_USE socketUse);

// Get sockaddr, IPv4 or IPv6:
extern void crn_run(int udp_fd, int tun_fd);

void* get_in_addr(struct sockaddr* sa);
int set_tun_addr(int temp_sock_fd,
                 struct ifreq* ifr,
                 char* ip,
                 unsigned long flag);

#endif  // TUNPAT_H

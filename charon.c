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

#include "charon.h"

extern int crn_tun(int* fd,
                   struct ifreq* ifr,
                   char* dev,
                   char* deviceIP,
                   char* destinationIP,
                   int flags) {
  int err = 0;
  int temp_sock_fd = 1;

  if ((*fd = open("/dev/net/tun", O_RDWR)) < 0) {
    perror("Opening /dev/net/tun");
    return -1;
  }

  memset(ifr, 0, sizeof(struct ifreq));

  ifr->ifr_flags = flags;

  if (dev) {
    strncpy(ifr->ifr_name, dev, IFNAMSIZ);
  }

  if ((err = ioctl(*fd, TUNSETIFF, (void*)ifr)) < 0) {
    perror("ioctl(TUNSETIFF)");
    close(*fd);
    return -1;
  }

  strcpy(dev, ifr->ifr_name);

  if ((temp_sock_fd = socket(PF_INET, SOCK_DGRAM, 0)) < 0) {
    printf("Cannot open UDP socket: %s\n", strerror(errno));
    close(*fd);
    return -1;
  }

  if (set_tun_addr(temp_sock_fd, ifr, deviceIP, SIOCSIFADDR) < 0) {
    close(*fd);
    close(temp_sock_fd);
    return -1;
  }

  if (set_tun_addr(temp_sock_fd, ifr, destinationIP, SIOCSIFDSTADDR) < 0) {
    close(*fd);
    close(temp_sock_fd);
    return -1;
  }

  if (ioctl(temp_sock_fd, SIOCGIFFLAGS, (void*)ifr) < 0) {
    printf("SIOCGIFFLAGS: %s\n", strerror(errno));
    close(*fd);
    return -1;
  }

  ifr->ifr_flags |= IFF_UP;
  ifr->ifr_flags |= IFF_RUNNING;

  if (ioctl(temp_sock_fd, SIOCSIFFLAGS, (void*)ifr) < 0) {
    printf("SIOCSIFFLAGS: %s\n", strerror(errno));
    close(*fd);
    return -1;
  }

  close(temp_sock_fd);

  return 0;
}

extern int crn_socket(int* sock_fd,
                      struct sockaddr_in* sock_addr,
                      char* ip,
                      int port,
                      enum SOCKET_USE socketUse) {
  int ret = 0;
  int optval = 1;

  if ((*sock_fd = socket(PF_INET, SOCK_DGRAM, 0)) < 0) {
    perror("socket()");
    return -1;
  }

  memset(sock_addr, 0, sizeof(struct sockaddr_in));

  sock_addr->sin_family = AF_INET;
  sock_addr->sin_addr.s_addr = inet_addr(ip);
  sock_addr->sin_port = htons(port);

  switch (socketUse) {
    case SU_SERVER:
      /* avoid EADDRINUSE error on bind() */
      if (setsockopt(*sock_fd, SOL_SOCKET, SO_REUSEADDR, (char*)&optval,
                     sizeof(optval)) < 0) {
        perror("setsockopt()");
        ret = -1;
        break;
      }

      if (bind(*sock_fd, (struct sockaddr*)sock_addr,
               sizeof(struct sockaddr_in)) < 0) {
        perror("bind()");
        ret = -1;
        break;
      }
      break;
    case SU_CLIENT:
      if (connect(*sock_fd, (struct sockaddr*)sock_addr,
                  sizeof(struct sockaddr_in)) < 0) {
        perror("bind()");
        ret = -1;
        break;
      }
      break;
    default:
      perror("socket use invalid");
      ret = -1;
      break;
  }
  return ret;
}

extern void crn_run(int udp_fd, int tun_fd) {
#ifdef DEBUG
  char remoteIP[INET6_ADDRSTRLEN];
#endif

  char buffer[BUFSIZE] = {0};
  struct sockaddr_storage their_addr;
  socklen_t addr_len = sizeof(their_addr);

  int fd = (tun_fd > udp_fd) ? tun_fd : udp_fd;

  while (1) {
    int ret;
    fd_set tunudp_fd_set;

    FD_ZERO(&tunudp_fd_set);
    FD_SET(tun_fd, &tunudp_fd_set);
    FD_SET(udp_fd, &tunudp_fd_set);

    ret = select(fd + 1, &tunudp_fd_set, NULL, NULL, NULL);

    if (ret < 0 && errno == EINTR) {
      continue;
    }

    if (ret < 0) {
      perror("select()");
      exit(1);
    }

    memset(&buffer, 0, BUFSIZE);

    if (FD_ISSET(tun_fd, &tunudp_fd_set)) {
      int nread = read(tun_fd, buffer, BUFSIZE - 1);

      if (nread < 0) {
        if (errno == EINTR)
          continue;
        else
          perror("Failed read");
      }

      buffer[BUFSIZE] = '\0';
      printd(("TAP2NET Read %d bytes from the tap interface\n", nread));
      printd(("buf: %s\n", buffer));

      write(udp_fd, buffer, nread);

      printd(("TAP2NET Written to the network\n"));
    }

    if (FD_ISSET(udp_fd, &tunudp_fd_set)) {
      /* data from the network: read it, and write it to the tun/tap interface.
       * We need to read the length first, and then the packet */

      /* Read length */
      uint16_t nread;

      if ((nread = recvfrom(udp_fd, buffer, BUFSIZE - 1, 0,
                            (struct sockaddr*)&their_addr, &addr_len)) == -1) {
        perror("recvfrom");
        exit(1);
      }

#ifdef DEBUG
      printd(("listener: packet from %s\n",
              inet_ntop(their_addr.ss_family,
                        getInAddr((struct sockaddr*)&their_addr), remoteIP,
                        INET6_ADDRSTRLEN)));
#endif
      if (nread == 0) {
        /* ctrl-c at the other end */
        break;
      }

      printd(("NET2TAP Read %d bytes from the network\n", nread));
      buffer[BUFSIZE] = '\0';
      printd(("\nBUfNet2tap: %s\n", buffer));

      write(tun_fd, buffer, nread);
      printd(("NET2TAP Written to the tap interface\n", nwrite));
    }
  }
}

// Only helper functions below here

#ifdef DEBUG
// Get sockaddr, IPv4 or IPv6:
// thanks https://beej.us/guide/bgnet/html/#datagram
void* get_in_addr(struct sockaddr* sa) {
  if (sa->sa_family == AF_INET) {
    return &(((struct sockaddr_in*)sa)->sin_addr);
  }

  return &(((struct sockaddr_in6*)sa)->sin6_addr);
}
#endif

int set_tun_addr(int temp_sock_fd,
                 struct ifreq* ifr,
                 char* ip,
                 unsigned long flag) {

  struct sockaddr_in tun_addr;

  memset(&tun_addr, 0, sizeof(tun_addr));
  tun_addr.sin_family = AF_INET;
  tun_addr.sin_addr.s_addr = inet_addr(ip);
  memcpy(&ifr->ifr_addr, &tun_addr, sizeof(struct sockaddr));

  if (ioctl(temp_sock_fd, flag, (void*)ifr) < 0) {
    printf("set_tun_addr ioctl fail: %s", strerror(errno));
    return -1;
  }

  return 0;
}

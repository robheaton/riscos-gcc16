/* net/if.h - the interface requests of socketioctl (): struct ifreq and struct ifconf, the IFF_ flags (the layout of the RISC OS Internet module, which is 4.4BSD's; the request numbers are in <sys/ioctl.h>).
   Example: struct ifconf ifc; char buf[1024]; ifc.ifc_len = sizeof buf; ifc.ifc_buf = buf; socketioctl (s, SIOCGIFCONF, &ifc);  the entries are not all the same size: step by _SIZEOF_ADDR_IFREQ. */
#ifndef _NET_IF_H
#define _NET_IF_H
#ifdef __cplusplus
extern "C" {
#endif
#include <sys/types.h>
#include <sys/socket.h>

#define IF_NAMESIZE 16
#define IFNAMSIZ IF_NAMESIZE

#define IFF_UP 0x1                      /* interface is up */
#define IFF_BROADCAST 0x2               /* broadcast address valid */
#define IFF_DEBUG 0x4                   /* turn on debugging */
#define IFF_LOOPBACK 0x8                /* is a loopback net */
#define IFF_POINTOPOINT 0x10            /* interface is point-to-point link */
#define IFF_SMART 0x20                  /* interface manages its own routes */
#define IFF_RUNNING 0x40                /* resources allocated */
#define IFF_NOARP 0x80                  /* no address resolution protocol */
#define IFF_PROMISC 0x100               /* receive all packets */
#define IFF_ALLMULTI 0x200              /* receive all multicast packets */
#define IFF_OACTIVE 0x400               /* transmission in progress */
#define IFF_SIMPLEX 0x800               /* can't hear own transmissions */
#define IFF_LINK0 0x1000                /* per link layer defined bit */
#define IFF_LINK1 0x2000
#define IFF_LINK2 0x4000
#define IFF_ALTPHYS IFF_LINK2           /* use alternate physical connection */
#define IFF_MULTICAST 0x8000            /* supports multicast */
#define IFF_SRCSUPPLIED 0x10000         /* (the RISC OS Internet driver's own flags) */
#define IFF_NEED_IPADDR 0x20000
#define IFF_NEED_MASK 0x40000
#define IFF_NODRIVER 0x80000

struct ifreq
{
  char ifr_name[IFNAMSIZ];              /* if name, e.g. "en0" */
  union
  {
    struct sockaddr ifru_addr;
    struct sockaddr ifru_dstaddr;
    struct sockaddr ifru_broadaddr;
    short ifru_flags[2];
    int ifru_metric;
    int ifru_mtu;
    int ifru_phys;
    int ifru_media;
    caddr_t ifru_data;
  } ifr_ifru;
};
#define ifr_addr ifr_ifru.ifru_addr             /* address */
#define ifr_dstaddr ifr_ifru.ifru_dstaddr       /* other end of a point to point link */
#define ifr_broadaddr ifr_ifru.ifru_broadaddr   /* broadcast address */
#define ifr_flags ifr_ifru.ifru_flags[0]        /* flags (low 16 bits) */
#define ifr_flagshigh ifr_ifru.ifru_flags[1]    /* flags (high 16 bits) */
#define ifr_metric ifr_ifru.ifru_metric
#define ifr_mtu ifr_ifru.ifru_mtu
#define ifr_phys ifr_ifru.ifru_phys
#define ifr_media ifr_ifru.ifru_media
#define ifr_data ifr_ifru.ifru_data

#define _SIZEOF_ADDR_IFREQ(ifr) \
  ((ifr).ifr_addr.sa_len > sizeof (struct sockaddr) ? (sizeof (struct ifreq) - sizeof (struct sockaddr) + (ifr).ifr_addr.sa_len) : sizeof (struct ifreq))

struct ifconf
{
  int ifc_len;                          /* size of the associated buffer */
  union
  {
    caddr_t ifcu_buf;
    struct ifreq *ifcu_req;
  } ifc_ifcu;
};
#define ifc_buf ifc_ifcu.ifcu_buf       /* buffer address */
#define ifc_req ifc_ifcu.ifcu_req       /* array of structures returned */

#ifdef __cplusplus
}
#endif
#endif

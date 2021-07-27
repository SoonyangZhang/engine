#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "log.h"
#include "egn_inet.h"
#if defined (WIN32)
int inet_pton_v4(const char* src, void* dst) {
  const int kIpv4AddressSize = 4;
  int found = 0;
  const char* src_pos = src;
  unsigned char result[kIpv4AddressSize];
  memset(result,0,sizeof(result));
  while (*src_pos != '\0') {
    // strtol won't treat whitespace characters in the begining as an error,
    // so check to ensure this is started with digit before passing to strtol.
    if (!isdigit(*src_pos)) {
      return 0;
    }
    char* end_pos;
    long value = strtol(src_pos, &end_pos, 10);
    if (value < 0 || value > 255 || src_pos == end_pos) {
      return 0;
    }
    ++found;
    if (found > kIpv4AddressSize) {
      return 0;
    }
    result[found - 1] =(value);
    src_pos = end_pos;
    if (*src_pos == '.') {
      // There's more.
      ++src_pos;
    } else if (*src_pos != '\0') {
      // If it's neither '.' nor '\0' then return fail.
      return 0;
    }
  }
  if (found != kIpv4AddressSize) {
    return 0;
  }
  memcpy(dst, result, sizeof(result));
  return 1;
}
const char* inet_ntop_v4(const void* src, char* dst, socklen_t size) {
  if (size < INET_ADDRSTRLEN) {
    return NULL;
  }
  const struct in_addr* as_in_addr =src;
    snprintf(dst, size, "%d.%d.%d.%d",
                      as_in_addr->S_un.S_un_b.s_b1,
                      as_in_addr->S_un.S_un_b.s_b2,
                      as_in_addr->S_un.S_un_b.s_b3,
                      as_in_addr->S_un.S_un_b.s_b4);
  return dst;
}
// Helper function for inet_ntop for IPv6 addresses.
const char* inet_ntop_v6(const void* src, char* dst, socklen_t size) {
  if (size < INET6_ADDRSTRLEN) {
    return NULL;
  }
  const uint16_t* as_shorts =(src);
  int runpos[8];
  int current = 1;
  int max = 0;
  int maxpos = -1;
  int run_array_size =8;
  // Run over the address marking runs of 0s.
  for (int i = 0; i < run_array_size; ++i) {
    if (as_shorts[i] == 0) {
      runpos[i] = current;
      if (current > max) {
        maxpos = i;
        max = current;
      }
      ++current;
    } else {
      runpos[i] = -1;
      current = 1;
    }
  }

  if (max > 0) {
    int tmpmax = maxpos;
    // Run back through, setting -1 for all but the longest run.
    for (int i = run_array_size - 1; i >= 0; i--) {
      if (i > tmpmax) {
        runpos[i] = -1;
      } else if (runpos[i] == -1) {
        // We're less than maxpos, we hit a -1, so the 'good' run is done.
        // Setting tmpmax -1 means all remaining positions get set to -1.
        tmpmax = -1;
      }
    }
  }

  char* cursor = dst;
  // Print IPv4 compatible and IPv4 mapped addresses using the IPv4 helper.
  // These addresses have an initial run of either eight zero-bytes followed
  // by 0xFFFF, or an initial run of ten zero-bytes.
  if (runpos[0] == 1 && (maxpos == 5 ||
                         (maxpos == 4 && as_shorts[5] == 0xFFFF))) {
    *cursor++ = ':';
    *cursor++ = ':';
    if (maxpos == 4) {
      cursor += snprintf(cursor, INET6_ADDRSTRLEN - 2, "ffff:");
    }
    const struct in_addr* as_v4 =(const struct in_addr*)(&(as_shorts[6]));
    inet_ntop_v4(as_v4, cursor,(INET6_ADDRSTRLEN - (cursor - dst)));
  } else {
    for (int i = 0; i < run_array_size; ++i) {
      if (runpos[i] == -1) {
        cursor += snprintf(cursor,
                           INET6_ADDRSTRLEN - (cursor - dst),
                           "%x", ntohs(as_shorts[i]));
        if (i != 7 && runpos[i + 1] != 1) {
          *cursor++ = ':';
        }
      } else if (runpos[i] == 1) {
        // Entered the run; print the colons and skip the run.
        *cursor++ = ':';
        *cursor++ = ':';
        i += (max - 1);
      }
    }
  }
  return dst;
}
int inet_pton_v6(const char* src, void* dst) {
  // sscanf will pick any other invalid chars up, but it parses 0xnnnn as hex.
  // Check for literal x in the input string.
  const char* readcursor = src;
  char c = *readcursor++;
  while (c) {
    if (c == 'x') {
      return 0;
    }
    c = *readcursor++;
  }
  readcursor = src;

  struct in6_addr an_addr;
  memset(&an_addr, 0, sizeof(an_addr));

  uint16_t* addr_cursor = (uint16_t*)(&an_addr.s6_addr[0]);
  uint16_t* addr_end =(uint16_t*)(&an_addr.s6_addr[16]);
  bool seencompressed = false;

  // Addresses that start with "::" (i.e., a run of initial zeros) or
  // "::ffff:" can potentially be IPv4 mapped or compatibility addresses.
  // These have dotted-style IPv4 addresses on the end (e.g. "::192.168.7.1").
  if (*readcursor == ':' && *(readcursor+1) == ':' &&
      *(readcursor + 2) != 0) {
    // Check for periods, which we'll take as a sign of v4 addresses.
    const char* addrstart = readcursor + 2;
    if (strchr(addrstart, (int)".")) {
      const char* colon =strchr(addrstart, (int)"::");
      if (colon) {
        uint16_t a_short;
        int bytesread = 0;
        if (sscanf(addrstart, "%hx%n", &a_short, &bytesread) != 1 ||
            a_short != 0xFFFF || bytesread != 4) {
          // Colons + periods means has to be ::ffff:a.b.c.d. But it wasn't.
          return 0;
        } else {
          an_addr.s6_addr[10] = 0xFF;
          an_addr.s6_addr[11] = 0xFF;
          addrstart = colon + 1;
        }
      }
      struct in_addr v4;
      if (inet_pton_v4(addrstart, &v4.s_addr)) {
        memcpy(&an_addr.s6_addr[12], &v4, sizeof(v4));
        memcpy(dst, &an_addr, sizeof(an_addr));
        return 1;
      } else {
        // Invalid v4 address.
        return 0;
      }
    }
  }

  // For addresses without a trailing IPv4 component ('normal' IPv6 addresses).
  while (*readcursor != 0 && addr_cursor < addr_end) {
    if (*readcursor == ':') {
      if (*(readcursor + 1) == ':') {
        if (seencompressed) {
          // Can only have one compressed run of zeroes ("::") per address.
          return 0;
        }
        // Hit a compressed run. Count colons to figure out how much of the
        // address is skipped.
        readcursor += 2;
        const char* coloncounter = readcursor;
        int coloncount = 0;
        if (*coloncounter == 0) {
          // Special case - trailing ::.
          addr_cursor = addr_end;
        } else {
          while (*coloncounter) {
            if (*coloncounter == ':') {
              ++coloncount;
            }
            ++coloncounter;
          }
          // (coloncount + 1) is the number of shorts left in the address.
          // If this number is greater than the number of available shorts, the
          // address is malformed.
          if (coloncount + 1 > addr_end - addr_cursor) {
            return 0;
          }
          addr_cursor = addr_end - (coloncount + 1);
          seencompressed = true;
        }
      } else {
        ++readcursor;
      }
    } else {
      uint16_t word;
      int bytesread = 0;
      if (sscanf(readcursor, "%4hx%n", &word, &bytesread) != 1) {
        return 0;
      } else {
        *addr_cursor = htons(word);
        ++addr_cursor;
        readcursor += bytesread;
        if (*readcursor != ':' && *readcursor != '\0') {
          return 0;
        }
      }
    }
  }

  if (*readcursor != '\0' || addr_cursor < addr_end) {
    // Catches addresses too short or too long.
    return 0;
  }
  memcpy(dst, &an_addr, sizeof(an_addr));
  return 1;
}
// Implementation of inet_ntop (create a printable representation of an
// ip address). XP doesn't have its own inet_ntop, and
// WSAAddressToString requires both IPv6 to be  installed and for Winsock
// to be initialized.

const char* inet_ntop(int af, const void *src,
                            char* dst, socklen_t size) {
  if (!src || !dst) {
    return NULL;
  }
  switch (af) {
    case AF_INET: {
      return inet_ntop_v4(src, dst, size);
    }
    case AF_INET6: {
      return inet_ntop_v6(src, dst, size);
    }
  }
  return NULL;
}

// As above, but for inet_pton. Implements inet_pton for v4 and v6.
// Note that our inet_ntop will output normal 'dotted' v4 addresses only.
int inet_pton(int af, const char* src, void* dst) {
  if (!src || !dst) {
    return 0;
  }
  if (af == AF_INET) {
    return inet_pton_v4(src, dst);
  } else if (af == AF_INET6) {
    return inet_pton_v6(src, dst);
  }
  return -1;
}
#else
#include <unistd.h>
#include <sys/socket.h>
#include <errno.h>
int bind_tcp(int familiy,const char *ip,uint16_t port,int backlog){
    int fd=-1;
    int yes=1;
    struct sockaddr_storage servaddr;
    if(sockaddr_init(&servaddr,familiy,ip,port)!=0){
        return fd;
    }
    size_t addr_size = sizeof(struct sockaddr_storage);
    fd=socket(AF_INET, SOCK_STREAM, 0);
    if(fd<0){
        return fd;
    }
    if(setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int))!=0){
        close(fd);
        fd=-1;
        return fd;
    }
    if(setsockopt(fd,SOL_SOCKET, SO_REUSEPORT,(const void *)&yes ,sizeof(int))!=0){
        close(fd);
        fd=-1;
        return fd;
    }
    if(bind(fd, (struct sockaddr *)&servaddr, addr_size)<0){
        log_warn("%s",strerror(errno));
        close(fd);
        fd=-1;
        return fd;
    }
    if(-1==listen(fd,backlog)){
        log_warn("%s",strerror(errno));
        close(fd);
        fd=-1;
        return fd;
    }
    return fd;
}
int bind_udp(int familiy,const char *ip,uint16_t port,bool tp){
    int fd=-1;
    struct sockaddr_storage servaddr;
    if(sockaddr_init(&servaddr,familiy,ip,port)!=0){
        return fd;
    }
    socklen_t addr_len= sizeof(struct sockaddr_storage);
    fd=bind_udp_addr((struct sockaddr*)&servaddr,addr_len,tp);
    return fd;
}
int bind_udp_addr(const struct sockaddr* addr, socklen_t addr_len,bool tp){
    int fd=-1;
    int yes=1;
    fd=socket(AF_INET, SOCK_DGRAM, 0);
    if(fd<0){
        return fd;
    }
    if(setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int))!=0){
        close(fd);
        fd=-1;
        return fd;
    }
    if(setsockopt(fd,SOL_SOCKET, SO_REUSEPORT,(const void *)&yes ,sizeof(int))!=0){
        close(fd);
        fd=-1;
        return fd;
    }
    if(tp&&setsockopt(fd, SOL_IP, IP_TRANSPARENT, &yes, sizeof(int))!=0){
        close(fd);
        fd=-1;
        return fd;
    }
    if(tp&&setsockopt(fd, IPPROTO_IP,IP_RECVORIGDSTADDR, &yes, sizeof(int))!=0){
        close(fd);
        fd=-1;
        return fd;
    }
    if(bind(fd,addr,addr_len)<0){
        log_warn("%s",strerror(errno));
        close(fd);
        fd=-1;
        return fd;
    }
    return fd;
}
#endif
int sockaddr_init(struct sockaddr_storage* addr,int familiy,
                  const char*ip,uint16_t port){
    memset(addr,0,sizeof(*addr));
    if(AF_INET==familiy){
        struct sockaddr_in *_addr = (struct sockaddr_in*)addr;
        _addr->sin_family = AF_INET;
        if(inet_pton(AF_INET,ip,&_addr->sin_addr)!=1){
            return -1;
        }
        _addr->sin_port=htons(port);
    }else if(AF_INET6==familiy){
        struct sockaddr_in6 *_addr = (struct sockaddr_in6*)addr;
        _addr->sin6_family = AF_INET6;
        if(inet_pton(AF_INET6,ip,&_addr->sin6_addr)!=1){
            return -1;
        }
        _addr->sin6_port=htons(port);
    }else{
        return -1;
    }
    return 0;
}
int sockaddr_string(const struct sockaddr_storage* addr,uint16_t *port,char *dst,socklen_t dst_size){
    if(AF_INET==addr->ss_family){
        struct sockaddr_in *_addr = (struct sockaddr_in*)addr;
        if(NULL==inet_ntop(AF_INET,&_addr->sin_addr,dst,dst_size)){
            return -1;
        }
        *port=ntohs(_addr->sin_port);
    }else if(AF_INET6==addr->ss_family){
        struct sockaddr_in6 *_addr = (struct sockaddr_in6*)addr;
        if(NULL==inet_ntop(AF_INET6,&_addr->sin6_addr,dst,dst_size)){
            return -1;
        }
        *port=ntohs(_addr->sin6_port);
    }else{
        return -1;
    }
    return 0;
}


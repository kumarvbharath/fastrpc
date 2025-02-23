#ifndef __TEST_IOCTL_H__
#define __TEST_IOCTL_H__

#include <stdint.h>
#include <sys/types.h>

// Override open
int open(const char *pathname, int flags, ...) {
    return 0;
}

// Override close
int close(int fd) {
    return 0;
}

// Override ioctl
int ioctl(int fd, unsigned long request, ...) {
    return 0;
}

#endif // __TEST_IOCTL_H__
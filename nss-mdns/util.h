#ifndef fooutilhfoo
#define fooutilhfoo

#include <sys/time.h>
#include <time.h>
#include <inttypes.h>

typedef uint64_t usec_t;

usec_t timeval_diff(const struct timeval *a, const struct timeval *b);
int timeval_cmp(const struct timeval *a, const struct timeval *b);
usec_t timeval_age(const struct timeval *tv);
void timeval_add(struct timeval *tv, usec_t v);

int set_nonblock(int fd);
int set_cloexec(int fd);

int wait_for_write(int fd, struct timeval *end);
int wait_for_read(int fd, struct timeval *end);

#endif

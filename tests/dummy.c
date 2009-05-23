#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

void run(void *self, size_t size) {
  char s[512];
  int fd = open("/tmp/dummy.bundle.out", O_WRONLY|O_TRUNC|O_CREAT);
  snprintf(s, sizeof(s), "INFECTED(%p, %d)\n", self, size);
  if (fd > 0) {
    write(fd, s, strlen(s));
    close(fd);
  }
}

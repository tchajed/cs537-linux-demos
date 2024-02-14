#include <assert.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

// producer for the pipe: just writes some constant data
int parent(int readfd, int writefd) {
  dup2(writefd, STDOUT_FILENO);
  close(writefd); // this does not seem needed, not sure why? but it is safe
  close(readfd);  // we also close the unused read end, for a different reason
  // these printfs will write to the pipe (because of the dup2 above); then the
  // child will exit which implicitly closes the pipe (stdout)
  printf("hello world\n");
  printf("hi there\n");
  printf("another line \n");
  printf("the fourth line\n");

  // we need to flush and close so the child gets EOF and exits before waiting
  // for it (the flush is because we're using the higher-level printf interface
  // rather than directly calling `write()` on the stdout file descriptor; note
  // that ordinary code would use fclose which does this rather than mixing the
  // high-level and low-level interfaces)
  fflush(stdout);
  close(STDOUT_FILENO);

  // wait for the child in order to get its exit status, to make sure the return
  // code reflects errors there as well
  int stat_loc;
  wait(&stat_loc);
  if (WIFEXITED(stat_loc)) {
    return WEXITSTATUS(stat_loc);
  }

  return 0;
}

int child(int readfd, int writefd) {
  char buf[1024];
  // without this close, the second read will block waiting for more data
  // because we haven't closed the last writefd associated with the pipe
  close(writefd);

  //// this doesn't demonstrate the issue - not sure what head is doing
  //// specially
  // dup2(readfd, STDIN_FILENO);
  // close(readfd);
  // execlp("head", "head", "-1", NULL);

  int n = read(readfd, buf, sizeof(buf));
  assert(n > 7);
  buf[7] = '\0';
  printf("%s\n", buf);

  // this should return 0 to indicate EOF
  n = read(readfd, buf, sizeof(buf));
  if (n != 0) {
    fprintf(stderr, "got another %d bytes\n", n);
    return 1;
  }
  close(readfd);
  return 0;
}

int main() {
  int filedes[2];
  pipe(filedes);

  int readfd = filedes[0];
  int writefd = filedes[1];

  if (fork() == 0) {
    return child(readfd, writefd);
  } else {
    return parent(readfd, writefd);
  }
}

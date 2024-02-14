#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

// producer for the pipe: just writes some constant data
int parent(int readfd, int writefd) {
  dup2(writefd, STDOUT_FILENO);
  close(writefd); // TODO(tej): this does not seem needed, not sure why? but it
                  // is safe
  close(readfd);  // we also close the unused read end (the effect of this isn't
                  // illustrated in this demo)

  // these printfs will write to the pipe (because of the dup2 above)
  printf("hello world\n");
  printf("hi there\n");
  printf("another line \n");
  printf("the fourth line\n");

  // NOTE: the code below here adds some robustness by getting the child's exit
  // status, you can ignore it on first read

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

int child(int readfd, int writefd, bool close_unused) {
  // without this close, the second read will block waiting for more data
  // because we haven't closed the last writefd associated with the pipe
  if (close_unused) {
    close(writefd);
  }

  //// this doesn't demonstrate the issue - not sure what head is doing
  //// differently
  // dup2(readfd, STDIN_FILENO);
  // close(readfd);
  // execlp("head", "head", "-1", NULL);

  // this will read all of the data
  char buf[1024];
  int n = read(readfd, buf, sizeof(buf));
  // print the first 6 bytes
  assert(n >= 6);
  buf[6 + 1] = '\0';
  printf("%s\n", buf);

  // this should return 0 to indicate EOF, but only if we've properly closed all
  // writefds for the pipe
  n = read(readfd, buf, sizeof(buf));
  // this is unexpected
  if (n != 0) {
    fprintf(stderr, "got another %d bytes\n", n);
    return 1;
  }
  return 0;
}

int main(int argc, char **argv) {
  bool close_unused = true;
  if (argc > 1 && strcmp(argv[1], "--no-close-unused") == 0) {
    close_unused = false;
  }

  // open a pipe, fork, and run `child` and `parent` as two separate processes

  int filedes[2];
  pipe(filedes);

  int readfd = filedes[0];
  int writefd = filedes[1];

  if (fork() == 0) {
    return child(readfd, writefd, close_unused);
  } else {
    return parent(readfd, writefd);
  }
}

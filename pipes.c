#include <assert.h>
#include <getopt.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/signal.h>
#include <time.h>
#include <unistd.h>

const char *text_lines[] = {
    "hello world\n",
    "hi there\n",
    "another line \n",
    "the fourth line\n",
};

// producer for the pipe: just writes some constant data
int parent(int readfd, int writefd, bool close_unused) {
  sigaction(SIGPIPE, &(struct sigaction){.sa_handler = SIG_IGN}, NULL);
  if (close_unused) {
    // we also close the unused read end, which causes the process to receive
    // the SIGPIPE signal (ignored by the above), and also for writes to fail
    // (which allows the producer to stop producing data when the downstream is
    // no longer waiting for it)
    close(readfd);
  }

  // we're going to avoid using printf since it introduces some extra buffering
  // which makes things more confusing
  for (int i = 0; i < 4; i++) {
    int ret = write(writefd, text_lines[i], strnlen(text_lines[i], 1024));
    if (ret < 0) {
      printf("done after write %d\n", i);
      return 0;
    }
    // sleep for a bit to allow the consumer to run and close the pipe
    if (i == 0) {
      struct timespec rqtp = {
          .tv_sec = 0,
          .tv_nsec = 10 /*ms*/ * 1000 * 1000L,
      };
      nanosleep(&rqtp, NULL);
    }
  }
  printf("wrote entire input\n");

  return 0;
}

// the child process, consuming from the pipe
//
// if read_all is false, this will only read 10 bytes and then stop
int child(int readfd, int writefd, bool read_all, bool close_unused) {
  // without this close, if we're trying to consume all of the input data the
  // second read will block waiting for more data because we haven't closed the
  // last writefd associated with the pipe
  if (close_unused) {
    close(writefd);
  }

  //// this doesn't demonstrate the issue - not sure what head is doing
  //// differently
  // dup2(readfd, STDIN_FILENO);
  // close(readfd);
  // execlp("head", "head", "-1", NULL);

  char buf[1024];
  int bytes_to_read = read_all ? sizeof(buf) : 10;
  int n = read(readfd, buf, bytes_to_read);
  // print the first 6 bytes
  assert(n >= 6);
  buf[6 + 1] = '\0';
  printf("%s\n", buf);

  if (read_all) {
    // this should eventually return 0 to indicate EOF, but only if we've
    // properly closed all writefds for the pipe; if not, the last read will
    // block forever
    while (true) {
      n = read(readfd, buf, sizeof(buf));
      if (n == 0) {
        break;
      }
      if (n < 0) {
        fprintf(stderr, "read failed: %d\n", n);
        return 1;
      }
    }
  }

  // the readfd is implicitly closed to signal to the producer that we're done
  // (especially important if we're not going to read all of the data, eg,
  // something like `head -1`)
  return 0;
}

int main(int argc, char **argv) {
  int c;
  bool close_unused = true;
  bool read_all = true;

  while (true) {
    int this_option_optind = optind ? optind : 1;
    int option_index = 0;
    static struct option long_options[] = {
        {"close-unused", no_argument, 0, 0},
        {"read-all", no_argument, 0, 0},
        {"no-close-unused", no_argument, 0, 0},
        {"no-read-all", no_argument, 0, 0},
        {"help", no_argument, 0, 0},
        {0, 0, 0, 0},
    };
    c = getopt_long(argc, argv, "", long_options, &option_index);
    if (c == -1) {
      break;
    }
    // written by copilot
    if (strcmp(long_options[option_index].name, "close-unused") == 0) {
      close_unused = true;
    } else if (strcmp(long_options[option_index].name, "no-close-unused") ==
               0) {
      close_unused = false;
    } else if (strcmp(long_options[option_index].name, "read-all") == 0) {
      read_all = true;
    } else if (strcmp(long_options[option_index].name, "no-read-all") == 0) {
      read_all = false;
    } else if (strcmp(long_options[option_index].name, "help") == 0) {
      printf("Usage: %s [--[no-]read-all] [--[no-]close-unused]\n", argv[0]);
      printf("\n");
      printf("--read-all will read the entire input in the consumer\n");
      printf("--no-close-unused will skip the recommend step of closing the "
             "unused ends of the pipe\n");
      printf(" (that is, the read end on the producer and the write end on the "
             "consumer)\n");
      return 0;
    }
  }

  // open a pipe, fork, and run `child` and `parent` as two separate processes

  int filedes[2];
  pipe(filedes);

  int readfd = filedes[0];
  int writefd = filedes[1];

  if (fork() == 0) {
    return child(readfd, writefd, read_all, close_unused);
  } else {
    return parent(readfd, writefd, close_unused);
  }
}

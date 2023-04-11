#include "kernel/types.h"
#include "user.h"

#define MSG1 "ping"
#define MSG2 "pong"
#define BLOCK_SIZE 32

void read_msg(int* pipefd) {
  close(pipefd[1]);
  char buf[BLOCK_SIZE];
  int read_byte = read(pipefd[0], &buf, BLOCK_SIZE);
  printf("%d: got ", getpid());
  while (read_byte > 0) {
    write(1, &buf, read_byte);
    read_byte = read(pipefd[0], &buf, BLOCK_SIZE);
  }
  printf("\n");
  close(pipefd[0]);
}

void write_msg(int* pipefd, char* msg) {
  close(pipefd[0]);
  write(pipefd[1], msg, strlen(msg));
  close(pipefd[1]);
}

int main(void) {
  int pipefd1[2];
  int pipefd2[2];
  if (pipe(pipefd1) == -1 || pipe(pipefd2) == -1) {
    printf("Cannot create pipe\n");
    exit(1);
  }
  int pid = fork();
  if (pid < 0) {
    printf("Cannot fork\n");
    exit(2);
  }
  if (pid != 0) {
    write_msg(pipefd1, MSG1);
    read_msg(pipefd2);
  } else {
    read_msg(pipefd1);
    write_msg(pipefd2, MSG2);
  }
  exit(0);
}

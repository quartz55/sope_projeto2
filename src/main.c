/* SISTEMAS OPERATIVOS
   Arquitectura cliente-servidor
   Programa servidor - srv_03.c
   O cliente envia um código de operação e nome do utilizador ao servidor
   e este escreve no ecrã "<Username> has requested operation <opcode>".
   O servidor termina quando receber 'opcode' igual a zero.
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#define MAX_NAME_LEN 20

int main(void) {
  int fd, fd_dummy;
  char name[MAX_NAME_LEN];
  int opcode;
  if (mkfifo("/tmp/requests", 0660) < 0)
    if (errno == EEXIST)
      printf("FIFO '/tmp/requests' already exists\n");
    else
      printf("Can't create FIFO\n");
  else
    printf("FIFO '/tmp/requests' sucessfully created\n");

  if ((fd = open("/tmp/requests", O_RDONLY)) != -1)
    printf("FIFO '/tmp/requests' openned in READONLY mode\n");

  if ((fd_dummy = open("/tmp/requests", O_WRONLY)) != -1)
    printf("FIFO '/tmp/requests' openned in WRITEONLY mode\n");

  do {
    read(fd, &opcode, sizeof(int));
    if (opcode != 0) {
      read(fd, name, MAX_NAME_LEN);
      printf("%s has requested operation %d\n", name, opcode);
    }
  } while (opcode != 0);

  close(fd);
  close(fd_dummy);

  if (unlink("/tmp/requests") < 0)
    printf("Error when destroying FIFO '/tmp/requests'\n");
  else
    printf("FIFO '/tmp/requests' has been destroyed\n");

  exit(0);
}

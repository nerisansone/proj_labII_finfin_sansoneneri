#define  _GNU_SOURCE
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h> 
#include <unistd.h>
#include <sys/socket.h>


#define QUI __LINE__, __FILE__
#define HOST "127.0.0.1"
#define PORT 52583 
#define Max_sequence_length 2048

void termina(const char *messaggio) {
  if(errno==0)  fprintf(stderr,"== %d == %s\n",getpid(), messaggio);
  else fprintf(stderr,"== %d == %s: %s\n",getpid(), messaggio, strerror(errno));
  exit(1);
}

/* Write "n" bytes to a descriptor */
ssize_t writen(int fd, void *ptr, size_t n) {
  size_t nleft;
  ssize_t nwritten;

  nleft = n;
  while (nleft > 0) {
    if ((nwritten = write(fd, ptr, nleft)) < 0) {
      if (nleft == n)
        return -1; 
      else
        break; 
    } else if (nwritten == 0)
      break;
    nleft -= nwritten;
    ptr += nwritten;
  }
  return (n - nleft); /* return >= 0 */
}

int main(int argc, char *argv[]) {
  if (argc != 2) {
    fprintf(stderr, "Usage: ./client1.out <nomefile>\n");
    exit(1);
  }

  FILE *file = fopen(argv[1], "r"); /* apro file */
  if (file == NULL)
    termina("Client1: error opening file\n");

  /* Gestione apertura 
    socket + eventuali errori */
  char *line = NULL;
  size_t len = 0;
  ssize_t el_read;
  size_t e;
  int tmp;

  while ((el_read = getline(&line, &len, file)) != -1) {
    if (strlen(line) == 1 && line[0] == '\n') {
      // linea vuota
        continue;
    }
    if (strlen(line) > 2048) {
      continue;
    }
    // creo socket
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == -1)
      termina("Client1: error creating socket\n");
    
     /* connessione a indirizzo e porta definiti a inizio file */ 
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(HOST);
    server_addr.sin_port = htons(PORT);
  
    if (connect(s, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
      printf("Client1 was not able to connect to%d\n", htons(PORT));
      fclose(file);
      close(s);
      exit(1);
    }
  
    // mando identificatore
    char *client_type = "client_tipo1";
    if (send(s, client_type, strlen(client_type), 0) < 0)
      termina("Client1: cannot send id\n");
    
    //invio lunghezza riga e caratteri
    tmp = htonl(strlen(line));
    e = writen(s, &tmp, sizeof(tmp));
    if (e != sizeof(int))
      termina("Client1: error writin length\n");

  
    if (send(s, line, el_read, 0) < 0)
      termina("Client1: error sending line\n");

    close(s);
  }

  //Chiudo il file che ho mandato al server riga per riga
  fclose(file);
  free(line);

  printf("Client1 terminato\n");
  return 0;
}

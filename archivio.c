#define _GNU_SOURCE
#include "rwunfair.h"
#include "xerrori.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <search.h>
#include <semaphore.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

#define QUI __LINE__, __FILE__
#define Num_elem 1000000
#define PC_buffer_len 10
#define Max_sequence_length 2048

// struct che segue schema fornito dal professore 
typedef struct {
  int valore;  // Numero di occorrenze della stringa
  ENTRY *next; // Puntatore alla prossima entry
} coppia;

// struct lettori
typedef struct {
  pthread_cond_t *condHR; // condition capolet
  pthread_cond_t *condR; // condition lettori
  pthread_mutex_t
      *mutexR; // mutex lettori associata a condition lettori
  char **bufferR; // buffer condiviso
  int index_inR;   // indice inserimento (solo un produttore: capolet)
  int *num_el_buffR; // numero elementi nel buffer
  int *index_outR;    // indice estrazione (lettori)
  int HRpipe;  // pipe di comunicazione tra capolet e lettori
  FILE *logReaders; // file di log per i lettori
} readers;

// struct scrittori
typedef struct {
  pthread_cond_t *condHW;      // condition caposc
  pthread_cond_t *condW;   // condition scrittori
  pthread_mutex_t *mutexW; // mutex associata a conditionscrittori
  char **bufferW;          // buffer condiviso con scrittori
  int index_inW; // indice next inserimento nel buffer condiviso
  int *num_el_bufferW;
  int *index_outW; // indice lettura dal buffer condiviso
  int HWpipe;         // ricezione dalla pipe
} writers;

// struct per la gestione dei segnali
typedef struct { 
  pthread_t *thHR;
  pthread_t *thHW;
} signals;

ENTRY *headlis_entry = NULL;

void distruggi_entry(ENTRY *e) {
  free(e->key);
  free(e->data);
  free(e);
}

void delete_ht(ENTRY *lis) {
  if (lis != NULL) {
    coppia *c = lis->data;
    delete_ht(c->next); // Chiamata ricorsiva per distruggere tutta la lista
    distruggi_entry(lis);
  }
}

//struct per la gesitone unfair dell'accesso all'hash table definita nel file rwunfair
rwHT struct_rwHT;

//Var globale che tiene conto dei elementi nella hash table
atomic_int HT_tot_str = 0;

// funzione che crea un oggetto di tipo ENTRY fornito dal professore
ENTRY *crea_entry(char *s, int n) {
  ENTRY *e = malloc(sizeof(ENTRY));
  if (e == NULL)
    xtermina("Archivio: errore 1 malloc crea_entry", QUI);
  e->key = strdup(s);
  e->data = malloc(sizeof(coppia));
  if (e->key == NULL || e->data == NULL)
    xtermina("Archivio: errore inizializzazione campi entry", QUI);
  // Inizializzo coppia
  coppia *c = (coppia *)e->data;
  c->valore = n;
  c->next = NULL;
  return e;
}

// Funzione per l'aggiunta di una stringa alla tabella hash
void aggiungi(char *s) {
  // Implementazione della logica per l'aggiunta di una stringa alla tabella hash
  // Utilizzare le funzioni di hsearch.h per l'accesso alla tabella hash
  // creo entry e cerco, se la trovo allora incremento 
  //il valore, altrimenti la aggiungo
  ENTRY *e = crea_entry(s, 1);
  ENTRY *er = hsearch(*e, FIND);
  if (er == NULL) {          // aggiungo entry se la stringa non è presente
    er = hsearch(*e, ENTER); 
    if (er == NULL)
      xtermina("Aggiungi: error", QUI);
    // aggiungo entry in testa alla lista 
    //e incremento il numero di elementi presenti
    coppia *c = (coppia *)e->data;
    c->next = headlis_entry;
    headlis_entry = e;
    HT_tot_str += 1;
  } else {
   
    assert(strcmp(e->key, er->key) == 0);
    coppia *c = (coppia *)er->data;
    c->valore += 1;
    distruggi_entry(e); 
  }
}

// Funzione per il conteggio di una stringa nella tabella hash
int conta(char *s) {
  int res;
    // Implementazione della logica per il conteggio di una stringa nella tabella hash
    // Utilizzare le funzioni di hsearch.h per l'accesso alla tabella hash
  ENTRY *search = crea_entry(s, 1);
  ENTRY *r = hsearch(*search, FIND);
  if (r == NULL) { // se non trovo la stringa nella tabella hash ritorno 0
    printf("%s -> not found in ht\n", s);
    res = 0;
  } else {
    printf("%s -> %d\n", s, *((int *)r->data));
    res = *((int *)r->data);
  }
  distruggi_entry(search);
  return res;
}


// Funzione capolettore, che legge dalla pipe e scrive nel buffer condiviso
// quando la pipe é chiusa scrive 3X1T nel buffer per segnalare la fine
void *headReader(void *arg) {
  readers *readers_struct = (readers *)arg;
  int capolet = readers_struct->HRpipe;
  char **bufferR = readers_struct->bufferR;
  int len_seq_from_pipe; // lunghezza della sequenza di caratteri letta dalla
                         // pipe
  // alloco buffer per lettura da pipe e delimitatori
  char *capoletbuff = malloc(Max_sequence_length * sizeof(char));
  if (capoletbuff == NULL)
    xtermina("Archivio: Errore allocazione capoletbuff\n", QUI);
 
  char *delimR = malloc(9 * sizeof(char));
  if (capoletbuff == NULL)
    xtermina("Archivio: Errore allocazione delimitatori capolet\n", QUI);
  strcpy(delimR, ".,:; \n\r\t");

  char *endchar = "3X1T";

  while (1) {
    //lunghezza dalla pipe
    ssize_t num_reads = read(capolet, &len_seq_from_pipe, sizeof(len_seq_from_pipe));

    if (num_reads == -1) {
      xtermina("Capolet: errore read capolet\n", QUI);
    }
    if (num_reads == 0) { //se è 0 vuol dire che la pipe è chiusa
      int outindex = readers_struct->index_inR;
      for (int i = 0; i < 10; i++) {
        // riempio buffer con 3X1T e chiudo capo
        bufferR[outindex++ % PC_buffer_len] = endchar;
        *(readers_struct->num_el_buffR) += 1;
        pthread_cond_signal(readers_struct->condR);
      }
      // dealloco tutto
      free(delimR);
      free(capoletbuff);
      pthread_exit(NULL);
    }
    // se c'è qualcosa leggo
    ssize_t str_inbyte = read(capolet, capoletbuff, len_seq_from_pipe);
    capoletbuff[str_inbyte] = 0;

    // tokenizzo la stringa letta dalla pipe e la inserisco nel buffer
    char *token = strtok(capoletbuff, delimR);

    while (token != NULL) {
      xpthread_mutex_lock(readers_struct->mutexR, QUI);
      while (*readers_struct->num_el_buffR ==10) { // se buffer pieno aspetto
        pthread_cond_wait(readers_struct->condHR, readers_struct->mutexR);
      }
      // inserisco elemento nel buffer, sveglio i lettori e sblocco mutex
      int index = (readers_struct->index_inR++ % PC_buffer_len);
      bufferR[index] = strdup(token);
      *(readers_struct->num_el_buffR) += 1;
      
      xpthread_mutex_unlock(readers_struct->mutexR, QUI);
      pthread_cond_broadcast(readers_struct->condR);

      token = strtok(NULL, delimR);
    }
  }
}

// Funzione lettori, legge dal buffer
// e esegue conta fino a che non si arriva a 3X1T
void *readerfun(void *arg) {

  char *endchar = "3X1T";
  readers *readers_struct = (readers *)arg;
  rwHT *rwHT = &struct_rwHT;
  char **bufferR = readers_struct->bufferR;
  char *readersout;

  while (1) {
    xpthread_mutex_lock(readers_struct->mutexR, QUI);
    while (*readers_struct->num_el_buffR == 0) { 
      // se buffer vuoto aspetto
      pthread_cond_wait(readers_struct->condR, readers_struct->mutexR);
    }

    // leggo elemento dal buffer
    int index_out_reader = *(readers_struct->index_outR) % PC_buffer_len;
    readersout = bufferR[index_out_reader];

    if (readersout == endchar) { 
      // se ho letto 3X1T, decremento gli elementi nel buffer lettori e termino
      *(readers_struct->num_el_buffR) -= 1;
      // sveglio capolet e sblocco mutex
      pthread_cond_signal(readers_struct->condHR);
      xpthread_mutex_unlock(readers_struct->mutexR, QUI);
      pthread_exit(NULL);
    }

    // se leggo qualcosa eseguo conta con schema reader writer fornito
    *(readers_struct->num_el_buffR) -= 1;
    read_lock(rwHT);
    int occ = conta(readersout);
    // scrivo su file di log
    fprintf(readers_struct->logReaders, "%s %d\n", readersout, occ);
    read_unlock(rwHT);
    *(readers_struct->index_outR) += 1;

    free(readers_struct->bufferR[index_out_reader]);

    pthread_cond_signal(readers_struct->condHR);
    xpthread_mutex_unlock(readers_struct->mutexR, QUI);
  }
}


// funzione caposcrittore che legge dalla pipe e scrive nel buffer
// quando quest'ultimo è pieno sveglia gli scrittori e aspetta
// infine anche lui usa 3X1T per segnalare la fine
void *headWriter(void *arg) {
  writers *writers_struct = (writers *)arg;
  int caposc = writers_struct->HWpipe;
  char **bufferW = writers_struct->bufferW;
  int len_recv_fromcapolet; // lunghezza ricevuta prima di ogni elemento

  //alloco buffer per leggere dalla pipe e per i separatori  
  char *caposcbuff = malloc(Max_sequence_length * sizeof(char));
  if (caposcbuff == NULL)
    xtermina("Archivio: errore allocazione caposcbuff\n", QUI);

  char *delimW = malloc(9 * sizeof(char));
  if (delimW == NULL)
    xtermina("Archivio: errore allocazione delimitatori \n", QUI);
  strcpy(delimW, ".,:; \n\r\t");
  
  char *endchar = "3X1T";

  while (1) {
    // legge la lunghezza dalla pipe
    ssize_t num_reads = read(caposc, &len_recv_fromcapolet, sizeof(len_recv_fromcapolet));

    if (num_reads == -1) {
      xtermina("errore read caposc\n", QUI);
    }
    if (num_reads == 0) { 
      // pipe chiusa allora scrivo 3X1T nel buffer e termino
      int outindex = writers_struct->index_inW;
      for (int i = 0; i < 10; i++) {
        bufferW[outindex++ % PC_buffer_len] = endchar;
        *(writers_struct->num_el_bufferW) += 1;
        pthread_cond_signal(writers_struct->condW);
      }
      // sveglio gli scrittori e termino
      free(delimW);
      free(caposcbuff);
      pthread_exit(NULL);
    }

    // legge dalla pipe e aggiungo 0 finale per strtok
    ssize_t str_inbyte = read(caposc, caposcbuff, len_recv_fromcapolet);
    caposcbuff[str_inbyte] = 0;

    // tokenizzo e inserisco nel buffer
    char *token = strtok(caposcbuff, delimW);

    while (token != NULL) {
      xpthread_mutex_lock(writers_struct->mutexW, QUI);
      while (*writers_struct->num_el_bufferW == 10) { 
        //se buffer pieno aspetto
        pthread_cond_wait(writers_struct->condHW, writers_struct->mutexW);
      }
      // inserisco nel buffer
      int index = (writers_struct->index_inW++ % PC_buffer_len);
      bufferW[index] = strdup(token);
      *(writers_struct->num_el_bufferW) += 1;

      // per evitare che gli scrittori restino in attesa sul buffer
      // vuoto gli sveglio
      xpthread_mutex_unlock(writers_struct->mutexW, QUI);
      pthread_cond_broadcast(writers_struct->condW);

      token = strtok(NULL, delimW);
    }
  }
}

// funzione scrittori che legge dal buffer e eseguono aggiungi
// fino a quando non leggono 3X1T
void *writerfun(void *arg) {

  char *endchar = "3X1T";
  writers *writers_struct = (writers *)arg;
  rwHT *rwHT = &struct_rwHT;
  char **bufferW = writers_struct->bufferW;
  char *writersout;

  while (1) {
    xpthread_mutex_lock(writers_struct->mutexW, QUI);
    while (*writers_struct->num_el_bufferW == 0) { 
      pthread_cond_wait(writers_struct->condW, writers_struct->mutexW);
    }

    int index_out_writer =
        *(writers_struct->index_outW) % PC_buffer_len;
    writersout = bufferW[index_out_writer];

    if (writersout == endchar) { 
      // se leggo 3X1T sveglio capolet e termino
      *(writers_struct->num_el_bufferW) -= 1;
      pthread_cond_signal(writers_struct->condHW);
      xpthread_mutex_unlock(writers_struct->mutexW, QUI);
      pthread_exit(NULL);
    }

    // diminuisco gli elementi nel buffer 
    // e aggiungo elemento alla hash table
    *(writers_struct->num_el_bufferW) -= 1;
    write_lock(rwHT);
    aggiungi(writersout);
    write_unlock(rwHT);
    *(writers_struct->index_outW) += 1;

    // dealloco doppione
    free(writers_struct->bufferW[index_out_writer]);

    pthread_cond_signal(writers_struct->condHW);
    xpthread_mutex_unlock(writers_struct->mutexW, QUI);
  }
}

// funzione per gestire i vari segnali SIGINT, SIGTERM, SIGUSR1
void *signal_handler(void *arg) {
  // inizializzazione maschera gestore di segnali
  signals *signals_struct = (signals *)arg;
  sigset_t sigMask;

  sigemptyset(&sigMask);
  sigaddset(&sigMask, SIGTERM);
  sigaddset(&sigMask, SIGINT);
  sigaddset(&sigMask, SIGUSR1);
  int s;
  while (true) {
    int e = sigwait(&sigMask, &s);
    if (e != 0)
      xtermina("Errore sigwait\n", QUI);
    if (s == SIGINT) {
      fprintf(stderr, "numero di stringhe nella ht -> %d\n", HT_tot_str);
      continue;
    } else if (s == SIGTERM) {

      if (xpthread_join(*(signals_struct->thHR), NULL, QUI) != 0)
        xtermina("error waiting capolet\n", QUI);
      if (xpthread_join(*(signals_struct->thHW), NULL, QUI) != 0)
        xtermina("error waiting caposc\n", QUI);
      fprintf(stdout, "numero di stringhe nella ht -> %d\n", HT_tot_str);

      // dealloco tutta la hash map e gli elementi
      delete_ht(headlis_entry);
      hdestroy();
      pthread_exit(NULL);
    } else if (s == SIGUSR1) {
      // in questo caso devo distruggere la vecchia ht e crearne una nuova
      printf("Reinizializzo ht\n");
      
      delete_ht(headlis_entry);
      hdestroy();
     // ripristino ht
      headlis_entry = NULL;
      HT_tot_str = 0;
      hcreate(Num_elem);
    
      continue;
    }
  }
  return NULL;
}

// il main è responsabile di inizializzare tutto
// (ht, struct...) prende in argomento il numero di lettori e scrittori
// infine dealloca tutto i buffer e chiude mutex
int main(int argc, char *argv[]) {
  if (argc != 3) {
    fprintf(stderr, "Usage: ./archivio <readers> <writers>\n");
    exit(1);
  }

  // creo ht
  int ht = hcreate(Num_elem);
  if (ht == 0) {
    xtermina("Archivio: errore creazione ht\n", QUI);
  }

  // inizializzo struct readers-writers
  pthread_mutex_t mutexHT = PTHREAD_MUTEX_INITIALIZER;
  pthread_cond_t condHT = PTHREAD_COND_INITIALIZER;
  struct_rwHT.condHT = condHT;
  struct_rwHT.mutexHT = mutexHT;
  struct_rwHT.writingHT = false;
  struct_rwHT.readersHT = 0;

  // inizializzo readers and writers ricevuti
  int th_readers = atoi(argv[1]);
  int th_writers = atoi(argv[2]);
  assert(th_writers > 0);
  assert(th_readers > 0);

  // apro file di log
  FILE *readersFilelog = xfopen("lettori.log", "w", QUI);

  //apro pipe
  int capolet = open("capolet", O_RDONLY);
  int caposc = open("caposc", O_RDONLY);

  // inizializzo threads necessari al programma
  pthread_t th_signal_handler;
  pthread_t th_HR;
  pthread_t readersth[th_readers];
  pthread_t th_HW;
  pthread_t writersth[th_writers];

  // inizializzo struct per i segnali
  signals signals_struct;
  signals_struct.thHR = &th_HR;
  signals_struct.thHW = &th_HW;
  // inizializzo maschera per gestore di segnali
  sigset_t mainMask;
  sigemptyset(&mainMask);
  sigaddset(&mainMask, SIGTERM);
  sigaddset(&mainMask, SIGINT);
  sigaddset(&mainMask, SIGUSR1);
  pthread_sigmask(SIG_BLOCK, &mainMask, NULL);

  xpthread_create(&th_signal_handler, NULL, signal_handler, &signals_struct, QUI);
  printf("Archivio: handler dei segnali iniziato\n");

  // gestione capolet - lettori
  char **bufferR = malloc(PC_buffer_len * sizeof(char **));
  if (bufferR == NULL)
    xtermina("Archivio: errore malloc lettori", QUI);
  // inizializzo il numero elementi del buffer e l'indice in uscita
  int num_el_buffR = 0;
  int index_outR = 0;

  // inizializzo la struct per i lettori
  pthread_mutex_t mutexR = PTHREAD_MUTEX_INITIALIZER;
  pthread_cond_t condR = PTHREAD_COND_INITIALIZER;
  pthread_cond_t condHR = PTHREAD_COND_INITIALIZER;
  readers readers_struct;
  readers_struct.condHR = &condHR;
  readers_struct.condR = &condR;
  readers_struct.mutexR = &mutexR;
  readers_struct.bufferR = bufferR;
  readers_struct.index_inR = 0;
  readers_struct.num_el_buffR = &num_el_buffR;
  readers_struct.index_outR = &index_outR;
  readers_struct.HRpipe = capolet;
  readers_struct.logReaders = readersFilelog;

  // creo capolet
  if (xpthread_create(&th_HR, NULL, &headReader, &readers_struct, QUI) != 0)
    xtermina("Archivio: errore creazione capolet\n", QUI);
  printf("Archivio: capolet iniziato\n");

// creo i thread lettori che interagiranno con capolettore
  for (int i = 0; i < th_readers; i++) {
    if (xpthread_create(&readersth[i], NULL, &readerfun, &readers_struct, QUI) != 0)
      xtermina("Archivio: errore creazione del lettore\n", QUI);
  }
  printf("Archivio: avviati i %d thread lettori \n", th_readers);

  // inizializzazione caposcrittore e scrittori
  char **bufferW = malloc(PC_buffer_len * sizeof(char **));
  if (bufferW == NULL)
    xtermina("Archivio: errore malloc scrittori", QUI);
  // inizializzazione del numero degli elementi nel buffer e dell'indice in uscita
  int num_el_bufferW = 0;
  int index_outW = 0;

  // inizializzo la struct per i lettori
  pthread_mutex_t mutexW = PTHREAD_MUTEX_INITIALIZER;
  pthread_cond_t condW = PTHREAD_COND_INITIALIZER;
  pthread_cond_t condHW = PTHREAD_COND_INITIALIZER;
  writers writers_struct;
  writers_struct.condHW = &condHW;
  writers_struct.condW = &condW;
  writers_struct.mutexW = &mutexW;
  writers_struct.bufferW = bufferW;
  writers_struct.index_inW = 0;
  writers_struct.num_el_bufferW = &num_el_bufferW;
  writers_struct.index_outW = &index_outW;
  writers_struct.HWpipe = caposc;

  // crea il thread caposc
  if (xpthread_create(&th_HW, NULL, &headWriter, &writers_struct, QUI) != 0)
    xtermina("Archivio: errore creazione caposc\n", QUI);
  printf("Archivio: caposc avviato\n");

  // creo i thread scrittori che interagiranno con caposcrittore
  for (int i = 0; i < th_writers; i++) {
    if (xpthread_create(&writersth[i], NULL, &writerfun, &writers_struct, QUI) != 0)
      xtermina("Archivio: errore creazione dello scrittore\n", QUI);
  }
  printf("Archivio: avviati i %d thread scrittori \n", th_writers);

  // aspetto che i thread finiscano e faccio join
  for (int i = 0; i < th_readers; i++) {
    if (xpthread_join(readersth[i], NULL, QUI) != 0)
      xtermina("Archivio: error waiting reader threads\n", QUI);
  }

  for (int i = 0; i < th_writers; i++) {
    if (xpthread_join(writersth[i], NULL, QUI) != 0)
      xtermina("Archivio: error waiting writer threads\n", QUI);
  }

  if (xpthread_join(th_signal_handler, NULL, QUI) != 0)
    xtermina("Archivio: error waiting signal handler \n", QUI);

  // dealloco tutto, conditions, mutex e file utilizzati
  pthread_cond_destroy(&condHT);
  xpthread_mutex_destroy(&mutexHT, QUI);
  xpthread_mutex_destroy(&mutexR, QUI);
  xpthread_mutex_destroy(&mutexW, QUI);
  pthread_cond_destroy(&condHR);
  pthread_cond_destroy(&condR);
  pthread_cond_destroy(&condHW);
  pthread_cond_destroy(&condW);
  fclose(readersFilelog);
  xclose(capolet, QUI);
  xclose(caposc, QUI);

  free(bufferR);
  free(bufferW);

  printf("Archivio: Terminazione corretta\n");
  return 0;
}
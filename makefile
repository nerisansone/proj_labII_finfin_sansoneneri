# definizione del compilatore e dei flag di compilazione
# che vengono usate dalle regole implicite
CC=gcc
CFLAGS=-std=c11 -Wall -g -O -pthread
LDLIBS=-lm -lrt -pthread

#  $@ viene sostituito con il nome del target
#  $< viene sostituito con il primo prerequisito
#  $^ viene sostituito con tutti i prerequisiti

# elenco degli eseguibili da creare
EXECS=archivio.out client1

# gli eseguibili sono precondizioni quindi verranno tutti creati
all: $(EXECS) 


# regola per la creazioni degli eseguibili utilizzando xerrori.o
%.out: %.o xerrori.o rwunfair.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)


# regola per la creazione di file oggetto che dipendono da xerrori.h
%.o: %.c xerrori.h rwunfair.h
	$(CC) $(CFLAGS) -c $<

 
# esempio di target che non corrisponde a una compilazione
# ma esegue la cancellazione dei file oggetto e degli eseguibili
clean: 
	rm -f *.o $(EXECS)
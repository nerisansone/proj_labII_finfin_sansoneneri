Il server, una volta avviato, mette in ascolto sulla porta 51583 e lancia un pool di thread per gestire le connessioni dei client. 
Per ogni client, il server riceve le sequenze inviate e le scrive su due pipe separate. 
Inoltre, il server tiene traccia delle connessioni con i client in un file di log.
Il client1.c legge un file di testo passato come argomento da linea di comando e invia le righe non vuote al server. 
Il client2.py prende uno o più file di testo come argomenti e crea un thread per ogni file. 
Ogni thread si connette al server e invia le sequenze lette dal proprio file. 
Una volta terminato l'invio delle sequenze, il client2.py riceve dal server il numero di sequenze inviate.
Archivio.c si occupa dell'interazione tra le sequenze e la tabella hash. 
Viene avviato dal server con il numero di lettori e scrittori come argomenti. 
Archivio.c crea la tabella hash e gestisce l'accesso concorrente utilizzando mutex e condition variables. 
I capi (capolettore e caposcrittore) ricevono le sequenze dalle pipe e le tokenizzano, quindi le inseriscono in un buffer ciclico condiviso con i workers (lettori e scrittori). 
I workers estraggono i token dal buffer, eseguono operazioni sulla tabella hash in modo sicuro e terminano quando ricevono un segnale di terminazione.
Quando il server riceve il segnale di terminazione, chiude la socket, cancella le pipe e invia un segnale SIGTERM al sottoprocesso archivio.c, terminando in modo pulito.
Ho usato varie funzioni fornite dal professore per la gestione degli errori, per la lettura e scrittura nel buffer e per la ricezione dal socket.
Per riassumere il server lancia l'archivio che si occupa della tabella hash e utilizza le pipe per la comunicazione tra il server e il sottoprocesso. 
I client inviano le sequenze al server e ricevono risposte in base alle operazioni eseguite sulla tabella hash.

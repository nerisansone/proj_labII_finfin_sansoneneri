#! /usr/bin/env python3 
import sys, struct, socket, threading, logging, argparse, subprocess, time, signal, os, errno
from concurrent.futures import ThreadPoolExecutor

server = None
archivio = None

def clients_handler(c_socket, addr, HR, HW):   
    # distingue typeA da typeB salvando il tipo
    tipo_client = c_socket.recv(12).decode('utf-8')

    if tipo_client == "client_tipo1":
    
        print(f"Server: Client1 connected on {addr[1]}")
        #numero byte mandati in pipe
        byte_sent = 0
        length=0
        while True:
            #lunghezza della riga ricevuta
            byte_len = recv_all(c_socket,4)
            if byte_len == 0:
                break
            length  = struct.unpack('!i',byte_len[:4])[0]
            if not length:
                break
            #linea ricevuta ancora da decodificare
            byte_line = recv_all(c_socket,length)
            if not byte_line:
                break
            # deocodifcia
            line = byte_line.decode('utf-8')
            byte_len = struct.pack('i',length)

            # invio a capolet
            os.write(HR, byte_len)
            os.write(HR, byte_line)  
            
            byte_sent += len(byte_len)
            byte_sent += len(byte_line)
        
        logging.info("Connection of type A wrote %d bytes", byte_sent)
        #Chiudo la socket appena finisco di leggere tutto
        c_socket.close()
    
    elif tipo_client == "client_tipo2":
        
        print(f"Server: Client2 connected to {addr[1]}")
        #numero byte mandati in pipe e totale di ciò che ricevo
        byte_sent = 0
        tot_recv = 0
        while True:
            
            byte_len = recv_all(c_socket,4)
            if byte_len == b'\x00\x00\x00\x00':  
            #se ricevo 4 byte 0 significa che ho finito 
            # di ricevere e mando il totale
                c_socket.sendall(struct.pack('!i', tot_recv))
                break
            length  = struct.unpack('!i',byte_len[:4])[0]
            if not length:
                break
            #linea ricevuta ancora da decodificare
            byte_line = recv_all(c_socket,length)
            if not byte_line:
                break
            # decodifica
            line = byte_line.decode('utf-8')
            byte_len = struct.pack('i',length)
            # Invia la riga al processo archivio su caposc
            os.write(HW, byte_len)
            os.write(HW, byte_line)
            byte_sent += len(byte_len)
            byte_sent += len(byte_line)
            tot_recv += 1 
        logging.info("Connection B wrote %d bytes", byte_sent)
        
        byte_sent = 0
        
        c_socket.close()
                

def recv_all(conn,n):
  chunks = b''
  bytes_recd = 0
  while bytes_recd < n:
    chunk = conn.recv(min(n - bytes_recd, 2048))
    if len(chunk) == 0:
      return 0
    chunks += chunk
    bytes_recd = bytes_recd + len(chunk)
  return chunks        

# funzione che gestisce il segnale di chiusura
def shutdown_server(sig, frame):
    global server, archivio
    print("Server shutting down") 
     
    server.shutdown(socket.SHUT_RDWR)
    server.close()
    
    # chiudo caposc e capolet
    if os.path.exists("caposc"):
        os.unlink("caposc")
    if os.path.exists("capolet"):
        os.unlink("capolet")
    
    # invio SIGTERM all'archivio
    archivio.send_signal(signal.SIGTERM)

    print("Server shutdown completed")
    exit()
    

# archivio lanciato in modalità normale
def archive(r, w):
    global server, archivio
    # esegue archivio.out passando il numero di lettori e scrittori
    archivio = subprocess.Popen(['./archivio.out', str(r), str(w)])
    print(f"Archivio started with pid {archivio.pid} and {r} readers and {w} writers")

# # archivio lanciato in modalità valgrind
def valgrind_archive(readers, writers):
    global server, archivio
    # Esegue il programma C passando anche valgrind
    archivio = subprocess.Popen(["valgrind","--leak-check=full", "--show-leak-kinds=all",  "--log-file=valgrind-%p.log", "./archivio.out", str(readers), str(writers)])
    print(f"Archivio started with valgrind and pid: {archivio.pid}")

def mainServer(thread_count, readers, writers, valgrind):
    global server, archivio
   
    host = 'localhost'
    port = 51583

    # creo socket
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.bind((host, port))
    server.listen(5) #max 5 connessioni 
    print(f"Server listening on {port}")
    
    
    # creo pool di thread
    executor = ThreadPoolExecutor(thread_count)
    print(f"Server: Pool of {thread_count} threads created")
    
    # creo pipe
    if not os.path.exists("capolet"): 
      os.mkfifo("capolet",0o0666)
      print("Capolet created")
    if not os.path.exists("caposc"):
      os.mkfifo("caposc",0o0666)
      print("Caposc created")
      
    #valgrind
    if valgrind:
      valgrind_archive(readers, writers)
    else:
      archive(readers, writers)


   #se pipe già esistenit allora le apro
    capolet = os.open("capolet", os.O_WRONLY)
    caposc = os.open("caposc", os.O_WRONLY)
    print("Pipes opened")
  
    # log config
    logging.basicConfig(filename='server.log', level=logging.INFO, format='%(asctime)s - %(message)s')

    # gestione SIGINT
    signal.signal(signal.SIGINT, shutdown_server)

    while True:
        
        cli_socket, addr = server.accept()
        
        executor.submit(clients_handler(cli_socket, addr, capolet, caposc), cli_socket)


if __name__ == '__main__':
    # gestione argomenti
    parser = argparse.ArgumentParser(description='Usage ./server.py <thread_count> <reader_count> <writer_count> <valgrind>')
    parser.add_argument("thread_count", type=int, help="Number of threads")
    parser.add_argument("-r", "--readers", type=int, default=3, help="Number or readers")
    parser.add_argument("-w", "--writers", type=int, default=3, help="Number of writers")
    parser.add_argument("-v", "--valgrind", action="store_true", help="Run archivio with valgrind") 

    args = parser.parse_args()

    mainServer(args.thread_count, args.readers, args.writers, args.valgrind)

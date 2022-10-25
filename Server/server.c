#include "../Lib/variable.h"
#include "../Lib/service.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h> 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h> 
#include <signal.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <sys/time.h>
#include <time.h>
#include <math.h>

typedef void Sigfunc(int); 

Sigfunc* signal(int signum, Sigfunc *handler);

void sig_child_handler(int signum);

void sig_alrm_handler(int signum);

int main(int argc, char *argv[]){
  int sockfd, child_sockfd, serv_port, window_size, len, child_len, trial_counter=0;
  struct sockaddr_in addr, child_addr;
  pid_t pid;
  struct sigaction sa;
  struct segment_packet data;
  struct ack_packet ack;
  float loss_rate;
  double timer, synack_timer;
  clock_t timer_sample; 
  bool dyn_timer_enable=false, timer_enable=false, SYNACK_sended=false;
  long conn_req_no;

  //Pulizia
  memset((void *)&data,0,sizeof(data));
  memset((void *)&ack,0,sizeof(ack));
  memset((void *)&addr, 0, sizeof(addr));
  memset((void *)&child_addr, 0, sizeof(addr));

  //Controllo numero di argomenti
  if (argc < 5) { 
    fprintf(stderr, "utilizzo: server <porta server> <dimensione finestra> <probabilita' perdita (float 0.x, -1 for 0)> <timeout (in ms double, -1 for dynamic timer)>\n");
    exit(EXIT_FAILURE);
  }

  //Controllo numero di porta
  if((serv_port=atoi(argv[1]))<1024){
    fprintf(stderr,"inserisci un numero di porta valido\n");
    exit(EXIT_FAILURE);
  }

  //Controllo dimensione finestra
  if((window_size=atoi(argv[2]))==0){
    fprintf(stderr,"inserisci dimensione finestra valida\n");
    exit(EXIT_FAILURE);
  }

  //Controllo probabilita' di perdita
  if((loss_rate=atof(argv[3]))==0){
      fprintf(stderr,"inserisci un loss rate valido\n");
      exit(EXIT_FAILURE);
  }

  if(loss_rate==-1)
    loss_rate=0;

  //Controllo timer
  if((timer=atof(argv[4]))==0){
    fprintf(stderr,"inserisci un timer valido\n");
    exit(EXIT_FAILURE);
  }

  if(timer<0){
    synack_timer=DEFAULT_TIMER;
    dyn_timer_enable=true;
  }
  else
    synack_timer=timer;

  //Seed per la perdita simulata
  srand48(time(NULL));

  //Creazione socket
  if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) { 
    perror("errore in socket padre");
    exit(EXIT_FAILURE);
  }
  
  //Assegnazione tipo di indirizzo
  addr.sin_family = AF_INET;
  //Assegnazione interfacce da cui accettare pacchetti (tutti)
  addr.sin_addr.s_addr = htonl(INADDR_ANY); 
  //Assegnazione porta del server
  addr.sin_port = htons(serv_port);
  len = sizeof(addr);

  //Assegna indirizzo al socket
  if (bind(sockfd, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
    perror("errore in bind");
    exit(EXIT_FAILURE);
  }

  //Installazione gestore sigchild
  if (signal(SIGCHLD, sig_child_handler) == SIG_ERR) { 
    fprintf(stderr, "errore in signal");
    exit(EXIT_FAILURE);
  }

  //Installazione gestore sigalarm
  sa.sa_handler = sig_alrm_handler; 
  sa.sa_flags = 0;
  sigemptyset(&sa.sa_mask);
  if (sigaction(SIGALRM, &sa, NULL) < 0) {
    fprintf(stderr, "errore in sigaction");
    exit(EXIT_FAILURE);
  }

  start:
  while (1) {
    trial_counter=0;
    //Ascolto di richieste di connessione dei client
    if ((recvfrom(sockfd, &data, sizeof(data), 0, (struct sockaddr *)&addr, &len)) < 0) {
      perror("errore in recvfrom attesa client");
      exit(EXIT_FAILURE);
    }

    if(ntohs(data.type)!=SYN)
      continue;
    /*if(simulate_loss(loss_rate)){
      printf("Perdita SYN simulata\n");
      continue;
    }*/

    conn_req_no=ntohl(data.seq_no);

    //Creo il socket del figlio dedicato al client
    if ((child_sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) { 
    perror("errore in socket figlio");
    exit(EXIT_FAILURE);
    }

    //Assegnazione tipo di indirizzo figlio
    child_addr.sin_family = AF_INET;
    //Assegnazione interfacce figlio da cui accettare pacchetti (tutti)
    child_addr.sin_addr.s_addr = htonl(INADDR_ANY); 
     //Assegnazione porta del figlio
    child_addr.sin_port = htons(0);
    child_len=sizeof(child_addr);

    //Assegna l'indirizzo al socket del figlio 
    if (bind(child_sockfd, (struct sockaddr *) &child_addr, sizeof(child_addr)) < 0) {
    perror("errore in bind socket figlio");
    exit(EXIT_FAILURE);
    }

    //Prendo il numero di porta del figlio
    if(getsockname(child_sockfd, (struct sockaddr *) &child_addr, &child_len)<0){
      perror("errore acquisizione numero porta del socket processo figlio");
      exit(EXIT_FAILURE);
    }

    sprintf(data.data,"%d", htons(child_addr.sin_port));

    while(1){ 

      //Se faccio troppi tentativi lascio stare probabilmente il server e' morto
      if(trial_counter>=MAX_TRIALS_NO){
        printf("Il client e' morto oppure il canale e' molto disturbato abort\n");
        close(child_sockfd);
        goto start;
      }

      if(!SYNACK_sended){
        //Invio la nuova porta al client
        if(sendto(sockfd, &data, sizeof(data), 0, (struct sockaddr *)&addr, sizeof(addr))<0){
          perror("errore in sendto porta figlio synack");
          close(child_sockfd);
          goto start;
        }

        SYNACK_sended=true;

        //Start timer
        timer_sample = clock();
        timer_enable = true;
        printf("SYNACK inviato\n");
      }

      //Timeout
      if(((double)(clock()-timer_sample)*1000/CLOCKS_PER_SEC > synack_timer) && (timer_enable)){ 
        timer_sample = clock();

        //Se il timer scade probabilmente e' troppo breve, lo raddoppio
        if(dyn_timer_enable)
          synack_timer=synack_timer*2;

        SYNACK_sended=false;
        trial_counter++;
        printf("Timeout SYNACK\n");
      }

      //Attendo ack syn ack
      if ((recvfrom(sockfd, &ack, sizeof(ack), MSG_DONTWAIT, (struct sockaddr *)&addr, &len)) > 0) {
        //if(!simulate_loss(loss_rate)){
          //Controllo che sia la richiesta corretta
          if((ntohl(ack.seq_no)==conn_req_no)&&(ntohs(ack.type)==SYN)){
            SYNACK_sended=false;
            printf("ACKSYNACK ricevuto\n");
            break;
          }
        //}
        //else
          //printf("PERDITA ACKSYNACK SIMULATA\n");
      }
      
    }

    //Fork del figlio
    if ((pid = fork()) == 0){
      //printf("Sono nel figlio\n");
      while(1){
        //Attesa comando
        alarm(MAX_CHOICE_TIME);
        if ((recvfrom(child_sockfd, &data, sizeof(data), 0, (struct sockaddr *)&child_addr, &child_len)) < 0) {
          perror("errore in recvfrom comando");
          close(child_sockfd);
          exit(EXIT_FAILURE);
        }
       

          switch(ntohs(data.type)){

          case PUT:
            alarm(0);
            ack.type=htons(PUT);
            printf("ascolto");
            //ACK comando
            if(sendto(child_sockfd, &ack, sizeof(ack), 0, (struct sockaddr *)&child_addr, sizeof(child_addr))<0){
              perror("errore sendto ack comando");
              exit(EXIT_FAILURE);
            }
            put_server(child_sockfd, child_addr, loss_rate, data.data);
            break;

          case GET:
            alarm(0);
            ack.type=htons(GET);
            //ACK comando
            if(sendto(child_sockfd, &ack, sizeof(ack), 0, (struct sockaddr *)&child_addr, sizeof(child_addr))<0){
              perror("errore sendto ack comando");
              exit(EXIT_FAILURE);
            }
            get_server(child_sockfd, child_addr, timer, window_size, loss_rate, data.data);
            break;

          case LIST:
            alarm(0);
            ack.type=htons(LIST);

            //ACK comando
            if(sendto(child_sockfd, &ack, sizeof(ack), 0, (struct sockaddr *)&child_addr, sizeof(child_addr))<0){
              perror("errore sendto ack comando");
              exit(EXIT_FAILURE);
            }
            list_server(child_sockfd, child_addr, timer, window_size, loss_rate);
            break;

          default:
            break;
        }

      }
    }
    else
      //Chiudo il socket del figlio nel padre in quanto a lui non serve
      close(child_sockfd);
    
  }
 
   exit(EXIT_SUCCESS);
}


void sig_alrm_handler(int signum){
  printf("SIGALRM\n"); 
}


Sigfunc *signal(int signum, Sigfunc *func){
  struct sigaction  act, oact;

  act.sa_handler = func;
  sigemptyset(&act.sa_mask); 
  act.sa_flags = 0;
  if (signum != SIGALRM) 
     act.sa_flags |= SA_RESTART;  
  if (sigaction(signum, &act, &oact) < 0)
    return(SIG_ERR);
  return(oact.sa_handler);
}

void sig_child_handler(int signum){
  int status;
  pid_t pid;
  while((pid = waitpid(WAIT_ANY, &status, WNOHANG)>0))
    printf("Figlio %d terminato\n",pid);
  return;
}

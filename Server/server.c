#include "../Lib/variable.h"
#include "../Lib/service.h"

typedef void Sigfunc(int); 

Sigfunc* signal(int signum, Sigfunc *handler);

void sig_child_handler(int signum);

void sig_alrm_handler(int signum);

int main(int argc, char *argv[]){
  int sockfd, child_sockfd, serv_port, window_size, len, child_len, trial_counter=0;
  struct sockaddr_in addr, child_addr;
  pid_t pid;
  struct sigaction sa;
  struct data_packet data;
  struct data_packet ack;
  float loss_rate;
  double timer;
  clock_t timer_sample; 
  bool dyn_timer_enable=false, timer_enable=false, SYNACK_sended=false;
  long conn_req_no;

  memset((void *)&data,0,sizeof(data));
  memset((void *)&ack,0,sizeof(ack));
  memset((void *)&addr, 0, sizeof(addr));
  memset((void *)&child_addr, 0, sizeof(addr));

  if (argc < 5) { 
    fprintf(stderr, "./server <porta server> <dimensione finestra> <probabilita' perdita> <timeout (in ms double, 0 per timer dinamico)>\n");
    exit(EXIT_FAILURE);
  }

  if((serv_port=atoi(argv[1]))<1024){
    fprintf(stderr,"inserisci un numero di porta valido\n");
    exit(EXIT_FAILURE);
  }

  if((window_size=atoi(argv[2]))<=0){
    fprintf(stderr,"inserisci dimensione finestra valida\n");
    exit(EXIT_FAILURE);
  }

  loss_rate = atof(argv[3]);
	if((loss_rate < 0) || (loss_rate > 1)) {
      fprintf(stderr,"inserisci un loss rate valido\n");
      exit(EXIT_FAILURE);
  }

  if((timer=atof(argv[4]))<0){
    fprintf(stderr,"inserisci un timer valido\n");
    exit(EXIT_FAILURE);
  }

  if(timer==0){
    timer=DEFAULT_TIMER;
    dyn_timer_enable=true;
  }

  print_head();
  srand48(time(NULL));

  if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) { 
    perror("errore in socket padre");
    exit(EXIT_FAILURE);
  }

  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY); 
  addr.sin_port = htons(serv_port);
  len = sizeof(addr);

  if (bind(sockfd, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
    perror("errore in bind");
    exit(EXIT_FAILURE);
  }

  if (signal(SIGCHLD, sig_child_handler) == SIG_ERR) { 
    fprintf(stderr, "errore in signal");
    exit(EXIT_FAILURE);
  }

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
    if ((recvfrom(sockfd, &data, sizeof(data), 0, (struct sockaddr *)&addr, &len)) < 0) {
      perror("errore in recvfrom attesa client");
      exit(EXIT_FAILURE);
    }

    if(ntohs(data.type)!=SYN)
      continue;
    conn_req_no=ntohl(data.seq_no);

    if ((child_sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) { 
    perror("errore in socket figlio");
    exit(EXIT_FAILURE);
    }

    child_addr.sin_family = AF_INET;
    child_addr.sin_addr.s_addr = htonl(INADDR_ANY); 
    child_addr.sin_port = htons(0);
    child_len=sizeof(child_addr);

    if (bind(child_sockfd, (struct sockaddr *) &child_addr, sizeof(child_addr)) < 0) {
    perror("errore in bind socket figlio");
    exit(EXIT_FAILURE);
    }

    if(getsockname(child_sockfd, (struct sockaddr *) &child_addr, &child_len)<0){
      perror("errore acquisizione numero porta del socket processo figlio");
      exit(EXIT_FAILURE);
    }

    sprintf(data.data,"%d", htons(child_addr.sin_port));

    while(1){ 

      if(trial_counter>=MAX_TRIALS_NO){
        printf("Il client e' morto oppure il canale e' molto disturbato abort\n");
        close(child_sockfd);
        goto start;
      }

      if(!SYNACK_sended){
        if(sendto(sockfd, &data, sizeof(data), 0, (struct sockaddr *)&addr, sizeof(addr))<0){
          perror("errore in sendto porta figlio synack");
          close(child_sockfd);
          goto start;
        }
        SYNACK_sended=true;
        timer_sample = clock();
        timer_enable = true;
        printf("SYNACK inviato\n");
      }

      if(timeout(timer_sample, timer_enable, dyn_timer_enable, &timer, &trial_counter)){
        SYNACK_sended=false;
        printf("Timeout SYNACK\n");
      }

      if ((recvfrom(sockfd, &ack, sizeof(ack), MSG_DONTWAIT, (struct sockaddr *)&addr, &len)) > 0) {
          if((ntohl(ack.seq_no)==conn_req_no)&&(ntohs(ack.type)==SYN)){
            SYNACK_sended=false;
            printf("ACKSYNACK ricevuto\n");
            break;
          }     
      }  
    }

    if ((pid = fork()) == 0){
      while(1){
        alarm(MAX_CHOICE_TIME);
        if ((recvfrom(child_sockfd, &data, sizeof(data), 0, (struct sockaddr *)&child_addr, &child_len)) < 0) {
          perror("errore in recvfrom comando");
          close(child_sockfd);
          exit(EXIT_FAILURE);
        }
        ack.type = data.type;
        if(sendto(child_sockfd, &ack, sizeof(ack), 0, (struct sockaddr *)&child_addr, sizeof(child_addr))<0){
          perror("errore sendto ack comando");
          exit(EXIT_FAILURE);
        }
        switch(ntohs(data.type)){

          case PUT:
            alarm(0);            
            put_server(child_sockfd, child_addr, loss_rate, data.data);
            break;

          case GET:
            alarm(0);          
            get_server(child_sockfd, child_addr, timer, window_size, loss_rate, data.data, dyn_timer_enable);
            break;

          case LIST:
            alarm(0);
            list_server(child_sockfd, child_addr, timer, window_size, loss_rate, dyn_timer_enable);
            break;

          default:
            break;
        }

      }
    }
    else
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
    printf("Client Disconnesso\n");
  return;
}



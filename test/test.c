#include "../Lib/variable.h"
#include "../Lib/service.h"


#define server_port 1026
#define windows 5
#define loss 0
#define timer_inp 0

void sig_alrm_handler(int signum);

int main(int argc, char *argv[]){
  int sockfd, n , serv_port, new_port, window_size, trial_counter=0;
  struct sockaddr_in servaddr, child_addr;
  struct sigaction sa;
  struct data_packet data;
  struct data_packet ack;
  long conn_req_no;
  float loss_rate;
  double timer;
  clock_t timer_sample; 
  bool dyn_timer_enable=false, timer_enable=false, SYN_sended=false;
  int i=0;

  memset((void *)&data,0,sizeof(data));
  memset((void *)&ack,0,sizeof(ack));
  memset((void *)&servaddr, 0, sizeof(servaddr));
  memset((void *)&child_addr, 0, sizeof(child_addr));

  serv_port=server_port;
  window_size = windows;
  loss_rate=loss;
  timer= timer_inp;

  if(timer==0){
    timer=DEFAULT_TIMER;
    dyn_timer_enable=true;
  }
 
  if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) { 
    perror("errore in socket");
    exit(EXIT_FAILURE);
  }

  servaddr.sin_family = AF_INET;
  servaddr.sin_port = htons(serv_port);
  if (inet_pton(AF_INET, argv[1], &servaddr.sin_addr) <= 0) {
    perror("errore in inet_pton");
    exit(EXIT_FAILURE);
  }

  sa.sa_handler = sig_alrm_handler;
  sa.sa_flags = 0;
  sigemptyset(&sa.sa_mask);
  if (sigaction(SIGALRM, &sa, NULL) < 0) {
    fprintf(stderr, "errore in sigaction");
    exit(EXIT_FAILURE);
  }

  if (connect(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0) {
    perror("errore in connect");
    exit(EXIT_FAILURE);
  }

  while(1){

    if(trial_counter>=MAX_TRIALS_NO){
      printf("Il server e' morto oppure il canale e' molto disturbato ritenta piu' tardi\n");
      exit(EXIT_FAILURE);
    }
    
    if(!SYN_sended){
      conn_req_no=lrand48();
      data.seq_no=htonl(conn_req_no);
      data.type=htons(SYN);
      if (send(sockfd, &data, sizeof(data), 0) < 0) {
        perror("errore in send richiesta connession syn");
        exit(EXIT_FAILURE);
      }

      SYN_sended=true;

      timer_sample = clock();
      timer_enable = true;

      printf("SYN inviato\n");
    }

    if(timeout(timer_sample, timer_enable, dyn_timer_enable, &timer, &trial_counter)){
      SYN_sended=false;
    }

    if(recv(sockfd, &data, sizeof(data), MSG_DONTWAIT)>0){
      if((ntohl(data.seq_no)==conn_req_no)&&(ntohs(data.type)==SYN)){
          printf("Ricevuto SYNACK\n");
          timer_enable=false;
          break;
      }
    }
  }
  new_port=ntohs(atoi(data.data));
  ack.type=htons(SYN);
  ack.seq_no=htonl(conn_req_no);
  if (send(sockfd, &ack, sizeof(ack), 0) < 0) {
    perror("errore in send ack_syn_ack");
    exit(EXIT_FAILURE);
  }

  child_addr.sin_family = AF_INET;
  child_addr.sin_port = new_port;
  if (inet_pton(AF_INET, argv[1], &child_addr.sin_addr) <= 0) {
    perror("errore in inet_pton");
    exit(EXIT_FAILURE);
  }

  if (connect(sockfd, (struct sockaddr *) &child_addr, sizeof(child_addr)) < 0) {
    perror("errore in connect");
    exit(EXIT_FAILURE);
  }
  printf("Ti sei connesso con successo\n");

  while(i<10){
    get_client(sockfd, servaddr, timer, loss_rate, dyn_timer_enable);
    i++;
  }

  exit(EXIT_SUCCESS);
}

void sig_alrm_handler(int signum){
  printf("Tempo per la scelta terminato\n");
  exit(EXIT_FAILURE);
}

  

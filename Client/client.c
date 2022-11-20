#include "../Lib/variable.h"
#include "../Lib/service.h"

void sig_alrm_handler(int signum);

int main(int argc, char *argv[]){
  int sockfd, n , serv_port, new_port, window_size, trial_counter=0;
  struct sockaddr_in servaddr, child_addr;
  struct sigaction sa;
  struct data_packet data;
  struct ack_packet ack;
  long conn_req_no;
  float loss_rate;
  double timer;
  clock_t timer_sample; 
  bool dyn_timer_enable=false, timer_enable=false, SYN_sended=false;

  //Pulizia
  memset((void *)&data,0,sizeof(data));
  memset((void *)&ack,0,sizeof(ack));
  memset((void *)&servaddr, 0, sizeof(servaddr));
  memset((void *)&child_addr, 0, sizeof(child_addr));

  //Controllo numero di argomenti
  if (argc < 6) { 
    fprintf(stderr, "utilizzo: client <indirizzo IPv4 server> <porta server> <dimensione finestra> <probabilita' perdita> <timeout (in ms double, 0 for dynamic timer)>\n");
    exit(EXIT_FAILURE);
  }

  //Controllo numero di porta
  if((serv_port=atoi(argv[2]))<1024){
    fprintf(stderr,"inserisci un numero di porta valido\n");
    exit(EXIT_FAILURE);
  }

  //Controllo dimensione finestra
  if((window_size=atoi(argv[3]))==0){
    fprintf(stderr,"inserisci dimensione finestra valida\n");
    exit(EXIT_FAILURE);
  }

  loss_rate = atof(argv[4]);
  //Controllo probabilita' di perdita
	if((loss_rate < 0) || (loss_rate > 1)) {
      fprintf(stderr,"inserisci un loss rate valido\n");
      exit(EXIT_FAILURE);
  }

  //Controllo timer
  if((timer=atof(argv[5]))<0){
    fprintf(stderr,"inserisci un timer valido(non é valido 0)\n");
    exit(EXIT_FAILURE);
  }

  if(timer==0){
    timer=DEFAULT_TIMER;
    dyn_timer_enable=true;
  }
 


  print_head();
  
  //Creazione socket
  if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) { 
    perror("errore in socket");
    exit(EXIT_FAILURE);
  }


  //Assegnazione tipo di indirizzo
  servaddr.sin_family = AF_INET;
  //Assegnazione porta server
  servaddr.sin_port = htons(serv_port);
  //Assegnazione indirizzo IP del server
  if (inet_pton(AF_INET, argv[1], &servaddr.sin_addr) <= 0) {
    perror("errore in inet_pton");
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

  //Per fissare le componenti del server
  if (connect(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0) {
    perror("errore in connect");
    exit(EXIT_FAILURE);
  }

  //Richiesta di connessione al server
  while(1){

    //Se faccio troppi tentativi lascio stare probabilmente il server e' morto
    if(trial_counter>=MAX_TRIALS_NO){
      printf("Il server e' morto oppure il canale e' molto disturbato ritenta piu' tardi\n");
      exit(EXIT_FAILURE);
    }
    

    if(!SYN_sended){
      //Invio SYN con numero di sequenza casuale come identificatore della connessione
      conn_req_no=lrand48();
      data.seq_no=htonl(conn_req_no);
      data.type=htons(SYN);
      if (send(sockfd, &data, sizeof(data), 0) < 0) {
        perror("errore in send richiesta connession syn");
        exit(EXIT_FAILURE);
      }

      SYN_sended=true;

      //Start timer
      timer_sample = clock();
      timer_enable = true;

      printf("SYN inviato\n");
    }

    if(timeout(timer_sample, timer_enable, dyn_timer_enable, &timer, &trial_counter)){
      SYN_sended=false;
    }

    //Attendo SYNACK
    if(recv(sockfd, &data, sizeof(data), MSG_DONTWAIT)>0){
      if((ntohl(data.seq_no)==conn_req_no)&&(ntohs(data.type)==SYN)){
          printf("Ricevuto SYNACK\n");
          timer_enable=false;
          break;
      }
    }
  }

  //Invio ACKSYNACK
  new_port=ntohs(atoi(data.data));
  ack.type=htons(SYN);
  ack.seq_no=htonl(conn_req_no);
  if (send(sockfd, &ack, sizeof(ack), 0) < 0) {
    perror("errore in send ack_syn_ack");
    exit(EXIT_FAILURE);
  }


  //Assegnazione tipo di indirizzo
  child_addr.sin_family = AF_INET;
  //Assegnazione porta del figlio dedicato
  child_addr.sin_port = new_port;
  //Assegnazione indirizzo IP del figlio
  if (inet_pton(AF_INET, argv[1], &child_addr.sin_addr) <= 0) {
    perror("errore in inet_pton");
    exit(EXIT_FAILURE);
  }

  //Per fissare le componenti del server
  if (connect(sockfd, (struct sockaddr *) &child_addr, sizeof(child_addr)) < 0) {
    perror("errore in connect");
    exit(EXIT_FAILURE);
  }
  printf("Ti sei connesso con successo\n");

  while(1){
    alarm(MAX_CHOICE_TIME);
    printf("Cosa posso fare per te? Hai un minuto per scegliere.\n");
    printf("1)PUT\n");
    printf("2)GET\n");
    printf("3)LIST\n");
    printf("Inserisci il numero dell'operazione da eseguire:\n");
    select_opt:
    if(scanf("%d",&n)!=1){
      perror("errore acquisizione operazione da eseguire");
      char c;
      while ((c = getchar()) != '\n' && c != EOF) { }
      goto select_opt;
    }
    switch(n){
      case PUT:
        alarm(0);
        put_client(sockfd, child_addr, timer, window_size, loss_rate, dyn_timer_enable);
        //put(sockfd);
        break;
      case GET:
        alarm(0);
        get_client(sockfd, servaddr, timer, loss_rate, dyn_timer_enable);
        break;
      case LIST:
        alarm(0);
        list_client(sockfd, servaddr, timer, loss_rate, dyn_timer_enable);
        break;
      default:
        printf("Inserisci un numero valido\n");
        goto select_opt;
        break;
    }
  }

  exit(EXIT_SUCCESS);
}

void sig_alrm_handler(int signum){
  printf("Tempo per la scelta terminato\n");
  exit(EXIT_FAILURE);
}

  

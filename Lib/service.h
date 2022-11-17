#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <dirent.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <math.h>


void print_head()
{
  system("clear");
  printf("########################################################################################## \n");
  printf("ooooo     ooo oooooooooo.   ooooooooo.             .oooooo.    oooooooooo.  ooooo      ooo \n");
  printf("`888'     `8' `888'   `Y8b  `888   `Y88.          d8P'  `Y8b   `888'   `Y8b `888b.     `8' \n");
  printf(" 888       8   888      888  888   .d88'         888            888     888  8 `88b.    8  \n");
  printf(" 888       8   888      888  888ooo88P'          888            888oooo888'  8   `88b.  8 \n");
  printf(" 888       8   888      888  888         8888888 888     ooooo  888    `88b  8     `88b.8  \n");
  printf(" `88.    .8'   888     d88'  888                 `88.    .88'   888    .88P  8       `888  \n");
  printf("   `YbodP'    o888bood8P'   o888o                 `Y8bood8P'   o888bood8P'  o8o        `8  \n");
  printf("########################################################################################## \n");
  printf("\n");
  printf("                                                         Creato da Massimo Mazzetti 0253467\n\n");
}


/// simulate_loss 
/// @param loss_rate 
/// @return 
bool simulate_loss(float loss_rate)
{
  double value;     // random value computed
  bool res = false; // result simulate_loss

  // setting srand seed randomly
  struct timespec tms;
  if (clock_gettime(CLOCK_REALTIME, &tms))
  {
    srand(time(NULL)); // if not work use default time(NULL) that return seconds precision (dd/mm/YY - hh:mm:ss)
  }
  else
  {
    int64_t micros = tms.tv_sec * 1000000;
    micros += tms.tv_nsec / 1000;
    if (tms.tv_nsec % 1000 >= 500)
    {
      ++micros;
    }
    srand(micros);
    // printf("Microseconds: %"PRId64"\n",micros);
  }

  // generate random vector for seed48
  int r1 = rand() % 100;
  int r2 = rand() % 100;
  int r3 = rand() % 100;
  unsigned short vec[3] = {r1, r2, r3};
  seed48(vec);
  value = drand48(); // generate a float pseudo random value in [0.0, 1.0)
  // printf("\nvalue: %f\n", value);

  // loss condition checking
  if (value < loss_rate)
    res = true;
  return res;
}

/// timeout 
/// @param timer_sample 
/// @param timer_enable 
/// @param dyn_timer_enable 
/// @param timer 
/// @param trial_counter 
/// @return 
bool timeout(clock_t timer_sample, bool timer_enable, bool dyn_timer_enable, double *timer, int *trial_counter)
{ 
  bool timeout = (( (double) ( (clock() - timer_sample) *1000 )/ CLOCKS_PER_SEC) > (*timer));
  if ((timer_enable) && (timeout))
  {
    timer_sample = clock();
    if (dyn_timer_enable)
    {
      if ( 2 * (*timer) > 10000 )
      {
        *timer = DEFAULT_TIMER; // timer troppo elevato provo a "sbloccare" il canale (forzandolo) e limitare l'attesa
      }
      else
      {
        *timer = 2 * (*timer); // quando entro in ritrasmissione raddoppio il valore del timer per cautela
      }
    }
    *trial_counter = *trial_counter + 1;
    printf("Timeout, new timer =%f\n", *timer);
    return true;
  }
  
  return false;
}

/// upload
/// @param sockfd 
/// @param type 
/// @param addr 
/// @param data 
/// @param dyn_timer_enable 
/// @param timer 
/// @param window_size 
/// @param loss_rate 
void upload(int sockfd, int type, struct sockaddr_in addr, struct segment_packet data, bool dyn_timer_enable, double timer, int window_size, float loss_rate)
{
  DIR *d;
  struct dirent *dir;
  struct segment_packet *packet_buffer;
  struct ack_packet ack;
  clock_t start_sample_RTT;
  double sample_RTT = 0, estimated_RTT = 0, dev_RTT = 0;
  int trial_counter = 0, len = sizeof(addr), fd;
  bool RTT_sample_enable = false, FIN_sended = false, timer_enable = false;
  long base = 0, next_seq_no = 0;
  clock_t timer_sample = clock();
  long file_size = 0, num_of_files = 0;
  off_t head;

  // Alloco il buffer della finestra
  if ((packet_buffer = malloc(window_size * sizeof(struct segment_packet))) == NULL)
  {
    perror("malloc fallita");
    data.length = htons(strlen("Put fallita: errore interno del client"));
    strcpy(data.data, "Put fallita: errore interno del client");
    goto input_termination;
  }


  if (type != LIST)
  {
    // Apro il file
    if ((fd = open(data.data, O_RDONLY)) < 0)
    {
      perror("errore apertura file da inviare");
      data.length = htons(strlen("Get fallita: file non presente"));
      strcpy(data.data, "Get fallita: file non presente");
      goto input_termination;
    }
    // Calcolo dimensione file
    lseek(fd, 0, SEEK_SET);
    file_size = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);
    int num_pack = (ceil((file_size / MTU)));
    printf("Sto inviando %d pacchetti\n", num_pack);
  }
  else
  {
    // Apro la directory contente i file
    if ((d = opendir("./files")) == NULL)
    {
      perror("errore apertura directory dei file");
      data.length = htons(strlen("List fallita: errore interno del server"));
      strcpy(data.data, "List fallita: errore interno del server");
      goto input_termination;
    }
    // Tengo traccia della testa della directory
    head = telldir(d);
    // Conto quanti pacchetti dovro' inviare
    while ((dir = readdir(d)) != NULL)
    {
      if ((strcmp(dir->d_name, ".") == 0) || (strcmp(dir->d_name, "..") == 0))
        continue;
      num_of_files++;
    }
    // printf("numero di file %ld\n",num_of_files);

    // Mi riposiziono
    seekdir(d, head);
  }
  // Pulizia
  memset((void *)packet_buffer, 0, sizeof(packet_buffer));
  memset((void *)&data, 0, sizeof(data));
  memset((void *)&ack, 0, sizeof(ack));

  // Invio dati
  while (((ntohl(ack.seq_no) + 1) * MTU < file_size) || ((ntohl(ack.seq_no) + 1) < num_of_files))
  {

    // Se ci sono troppe ritrasmissioni lascio stare
    if (trial_counter >= MAX_TRIALS_NO)
    {
      printf("il canale e' molto disturbato\n");
      close(sockfd);
      exit(EXIT_FAILURE);
    }

    // Se la finestra non e' piena preparo ed invio il pacchetto
    if (next_seq_no < base + window_size)
    {
      if (type != LIST)
      {
        if ((packet_buffer[next_seq_no % window_size].length = htons(read(fd, packet_buffer[next_seq_no % window_size].data, MTU))) > 0)
        {
          packet_buffer[next_seq_no % window_size].seq_no = htonl(next_seq_no);
          packet_buffer[next_seq_no % window_size].type = htons(NORMAL);
          sendto(sockfd, &packet_buffer[next_seq_no % window_size], sizeof(packet_buffer[next_seq_no % window_size]), 0, (struct sockaddr *)&addr, sizeof(addr));
          // Se e' attivato il timer dinamico campiono per calcolare l'rtt
          if ((dyn_timer_enable) && (!RTT_sample_enable))
          {
            start_sample_RTT = clock();
            RTT_sample_enable = true;
          }
          printf("Inviato pacchetto %d\n", ntohl(packet_buffer[next_seq_no % window_size].seq_no));

          // Se il next sequence number corrisponde con la base lancia il timer
          if (base == next_seq_no)
          {
            timer_sample = clock();
            timer_enable = true;
          }

          next_seq_no++;
        }
      }
      else
      {
        if ((dir = readdir(d)) != NULL)
        {
          if ((strcmp(dir->d_name, ".") == 0) || (strcmp(dir->d_name, "..") == 0))
            continue;
          strcpy(packet_buffer[next_seq_no % window_size].data, dir->d_name);
          packet_buffer[next_seq_no % window_size].length = htons(strlen(dir->d_name));
          packet_buffer[next_seq_no % window_size].seq_no = htonl(next_seq_no);
          packet_buffer[next_seq_no % window_size].type = htons(NORMAL);
          sendto(sockfd, &packet_buffer[next_seq_no % window_size], sizeof(packet_buffer[next_seq_no % window_size]), 0, (struct sockaddr *)&addr, sizeof(addr));

          // Se e' attivato il timer dinamico campiono per calcolare l'rtt
          if ((dyn_timer_enable) && (!RTT_sample_enable))
          {
            start_sample_RTT = clock();
            RTT_sample_enable = true;
          }
          printf("Inviato pacchetto %d\n", ntohl(packet_buffer[next_seq_no % window_size].seq_no));

          // Se il next sequence number corrisponde con la base lancia il timer
          if (base == next_seq_no)
          {
            timer_sample = clock();
            timer_enable = true;
          }

          next_seq_no++;
        }
      }
    }

    if (timeout(timer_sample, timer_enable, dyn_timer_enable, &timer, &trial_counter))
    {
      printf("timer = %f\n", timer);
      for (int i = 0; i < window_size; i++)
      {
        timer_sample = clock();
        sendto(sockfd, &packet_buffer[i], sizeof(packet_buffer[i]), 0, (struct sockaddr *)&addr, sizeof(addr));

        if ((dyn_timer_enable) && (!RTT_sample_enable))
        {
          start_sample_RTT = clock();
          RTT_sample_enable = true;
        }
        printf("Pacchetto %d ritrasmesso\n", ntohl(packet_buffer[i].seq_no));
      }
    }

    // Controllo se ci sono ack
    if (recvfrom(sockfd, &ack, sizeof(struct ack_packet), MSG_DONTWAIT, (struct sockaddr *)&addr, &len) > 0)
    {
      if (!simulate_loss(loss_rate))
      {
        printf("ACK %d ricevuto\n", ntohl(ack.seq_no));
        base = ntohl(ack.seq_no) + 1;

        // Azzero il contatore di tentativi di ritrasmissione in quanto se ricevo ACK il client e' vivo
        trial_counter = 0;
        // Stop del timer associato al pacchetto piu' vecchio della finestra
        if ((dyn_timer_enable) && (RTT_sample_enable))
        { 
            RTT_sample_enable = false;
            sample_RTT = (double)(clock() - start_sample_RTT) * 1000 / CLOCKS_PER_SEC;
            printf("SAMPLE RTT %f\n", sample_RTT);
            estimated_RTT = (double)(0.875 * estimated_RTT) + (0.125 * sample_RTT);
            printf("ESTIMATED RTT %f\n", estimated_RTT);
            dev_RTT = (double)(0.75 * dev_RTT) + (0.25 * fabs(sample_RTT - estimated_RTT));
            printf("DEV RTT %f\n", dev_RTT);
            timer = (double)estimated_RTT + (4 * dev_RTT);        
            printf("ACK %d ricevuto, ricalcolo timer = %f\n", ntohl(ack.seq_no), timer);
        } 
        if (base == next_seq_no)
        {
          timer_enable = false;
          // printf("Ho fermato il timer\n");
        }
      }
      else
        printf("PERDITA ACK SIMULATA\n");
    }
  }
  // Pulizia
  memset((void *)&ack, 0, sizeof(ack));
  memset((void *)&data, 0, sizeof(data));
  trial_counter = 0;
// Termine operazione
input_termination:
  while (1)
  {
    // Se faccio troppi tentativi lascio stare probabilmente il client e' morto
    if (trial_counter >= MAX_TRIALS_NO)
    {
      if (ntohs(data.length) > 0)
        printf("Il canale e' molto disturbato, errore: %s", data.data);
      else
        printf("Il canale e' molto disturbato tuttavia il file e' stato consegnato con successo\n");
      break;
    }

    // Invio il FIN solo se devo farlo per la prima volta o lo rinvio in caso di timeout per non inviarne inutilmente
    if (!FIN_sended)
    {
      data.type = htons(FIN);
      data.seq_no = htonl(next_seq_no);
      sendto(sockfd, &data, sizeof(data), 0, (struct sockaddr *)&addr, sizeof(addr));
      FIN_sended = true;

      // Start timer
      timer_sample = clock();
      timer_enable = true;
      printf("Inviato FIN\n");
    }

    if (timeout(timer_sample, timer_enable, dyn_timer_enable, &timer, &trial_counter))
    {
      FIN_sended = false;
    }

    // Attendo FINACK
    if (recvfrom(sockfd, &ack, sizeof(ack), MSG_DONTWAIT, (struct sockaddr *)&addr, &len) > 0)
    {
      if (!simulate_loss(loss_rate))
      {
        if (ntohs(ack.type) == FIN)
        {
          printf("Ho ricevuto FIN ACK\n");
          break;
        }
      }
      else
        printf("PERDITA FINACK SIMULATA\n");
    }
  }
  if (type != LIST)
  {
    printf("chiudo file\n");
    close(fd);
  }
  else
  {
    printf("chiudo cartella\n");
    closedir(d);
  }
  printf("chiudo socket\n");
  close(sockfd);
}

/// download
/// @param sockfd 
/// @param type 
/// @param data 
/// @param addr 
/// @param loss_rate 
/// @param rm_string 
void download(int sockfd, int type, struct segment_packet data, struct sockaddr_in addr, float loss_rate, char *rm_string)
{
  int fd, trial_counter = 0, len = sizeof(addr), n;
  struct ack_packet ack;
  long expected_seq_no = 0;

  if (type != LIST)
  {
    // Apro file
    if ((fd = open(data.data, O_RDWR | O_CREAT | O_TRUNC, 0666)) < 0)
    {
      perror("errore apertura/creazione file da ricevere");
      exit(EXIT_FAILURE);
    }
  }

  memset((void *)&data, 0, sizeof(data));
  memset((void *)&ack, 0, sizeof(ack));
  ack.seq_no = htonl(-1);
  // Ricevo dati
  while (1)
  {

    // Se ci sono troppi errori di lettura lascio stare
    if (trial_counter >= MAX_TRIALS_NO)
    {
      printf("Il client e' morto oppure il canale e' molto disturbato, errore: %s", data.data);
      close(sockfd);
      exit(EXIT_FAILURE);
    }

    if (recvfrom(sockfd, &data, sizeof(data), 0, (struct sockaddr *)&addr, &len) < 0)
    {
      perror("Pacchetto corrotto errore recv\n");
      trial_counter++;
      continue;
    }

    if (!simulate_loss(loss_rate))
    {
      // Se arriva un pacchetto in ordine lo riscontro e aggiorno il numero di sequenza che mi aspetto
      if (ntohl(data.seq_no) == expected_seq_no)
      {
        if (ntohs(data.type) == FIN)
        {
          // Se e' un FIN di errore printo l'errore, rimuovo il file sporco ed esco
          if (ntohs(data.length) > 0)
          {
            printf("%s\n", data.data);
            system(rm_string);
          }
          else
          {
            printf("Ho ricevuto FIN\n");
          }
          ack.type = htons(FIN);
          ack.seq_no = data.seq_no;
          break;
        }
        else
        {
          if (type == LIST)
          {
            printf("%s\n", data.data);
          }
          else
          {
            data.data[ntohs(data.length)] = 0;
            printf("Ho ricevuto un dato di %d byte del pacchetto %d\n", ntohs(data.length), ntohl(data.seq_no));
            if ((n = write(fd, data.data, ntohs(data.length))) != ntohs(data.length))
            {
              perror("Non ho scritto tutto su file mi riposiziono\n");
              lseek(fd, 0, SEEK_CUR - n);
              continue;
            }
            // printf("Ho scritto %d byte sul file\n",n);
          }
          ack.type = htons(NORMAL);
          ack.seq_no = data.seq_no;
          expected_seq_no++;
        }
      }
      // Invio ack
      sendto(sockfd, &ack, sizeof(ack), 0, (struct sockaddr *)&addr, sizeof(addr));
      printf("ACK %d inviato\n", ntohl(ack.seq_no));
    }
    else
      printf("PERDITA PACCHETTO SIMULATA\n");
  }

  // Invio FINACK
  sendto(sockfd, &ack, sizeof(ack), 0, (struct sockaddr *)&addr, sizeof(addr));
  printf("FIN ACK inviato\n");
  close(fd);
  close(sockfd);
}

/// send_request
/// @param sockfd 
/// @param type 
/// @param data 
/// @param timer 
/// @param dyn_timer_enable 
/// @return 
bool send_request(int sockfd, int type, struct segment_packet data, double timer, bool dyn_timer_enable)
{
  int trial_counter = 0;
  bool command_sended = false, timer_enable = false;
  struct ack_packet ack;
  clock_t timer_sample;

  // Invio richiesta
  while (1)
  {

    // Se faccio troppi tentativi lascio stare probabilmente il server e' morto
    if (trial_counter >= MAX_TRIALS_NO)
    {
      printf("Il server e' morto oppure il canale e' molto disturbato ritenta piu' tardi\n");
      return false;
    }

    if (!command_sended)
    {

      // Invia al server il pacchetto di richiesta
      data.type = htons(type);
      if (send(sockfd, &data, sizeof(data), 0) < 0)
      {
        perror("errore in send file name");
        return false;
      }
      printf("Inviata richiesta\n");
      command_sended = true;
      // Start timer
      timer_sample = clock();
      timer_enable = true;
    }

    if (timeout(timer_sample, timer_enable, dyn_timer_enable, &timer, &trial_counter))
    {
      command_sended = false;
    }

    // Attendo ACK richiesta
    if (recv(sockfd, &ack, sizeof(ack), MSG_DONTWAIT) > 0)
    {
      // if(!simulate_loss(loss_rate)){
      if (ntohs(ack.type) == type)
      {
        printf("Ricevuto ack richesta\n");
        timer_enable = false;
        break;
      }
      //}
      // else
      // printf("PERDITA ACK COMANDO SIMULATA\n");
    }
  }
  return true;
}

/// get_client 
/// @param sockfd 
/// @param servaddr 
/// @param timer 
/// @param loss_rate 
void get_client(int sockfd, struct sockaddr_in servaddr, double timer, float loss_rate)
{
  struct segment_packet data;
  char *rm_string;
  bool dyn_timer_enable = false;
  struct timeval start, end;
  char file_name[256];

  memset((void *)&data, 0, sizeof(data));

  // Attivo timer dinamico
  if (timer < 0)
  {
    dyn_timer_enable = true;
    timer = DEFAULT_TIMER;
  }

// Scelta del file da scaricare dal server
file_choice:
  printf("Inserire il nome del file da scaricare (con estensione):\n");
  if (scanf("%s", file_name) != 1)
  {
    perror("inserire un nome valido");
    char c;
    while ((c = getchar()) != '\n' && c != EOF)
    {
    }
    goto file_choice;
  }

  printf("E' stato scelto %s \n", data.data);

  // Utile solo per la pulizia della directory in caso di errori
  rm_string = malloc(strlen(data.data) + 3);
  sprintf(rm_string, "rm %s", data.data);

  gettimeofday(&start, NULL);

  sprintf(data.data, "./files/%s", file_name);

  if (!send_request(sockfd, GET, data, timer, dyn_timer_enable))
  {
    exit(EXIT_FAILURE);
  }
  
  

  download(sockfd, GET, data, servaddr, loss_rate, rm_string);
  printf("\nGET terminata\n\n");

  gettimeofday(&end, NULL);

  long seconds = (end.tv_sec - start.tv_sec);
  long micros = ((seconds * 1000000) + end.tv_usec) - (start.tv_usec);

  printf("La GET ha impiegato %ld secondi e %ld microsecondi\n", seconds, micros);

  exit(EXIT_SUCCESS);
}

/// list_client
/// @param sockfd 
/// @param servaddr 
/// @param timer 
/// @param loss_rate 
void list_client(int sockfd, struct sockaddr_in servaddr, double timer, float loss_rate)
{
  struct segment_packet data;
  char *rm_string;
  bool dyn_timer_enable = false;
  struct timeval start, end;

  memset((void *)&data, 0, sizeof(data));

  // Attivo timer dinamico
  if (timer < 0)
  {
    dyn_timer_enable = true;
    timer = DEFAULT_TIMER;
  }

  // Utile solo per la pulizia della directory in caso di errori
  rm_string = malloc(strlen(data.data) + 3);
  sprintf(rm_string, "rm %s", data.data);

  gettimeofday(&start, NULL);

  if (!send_request(sockfd, LIST, data, timer, dyn_timer_enable))
  {
    exit(EXIT_FAILURE);
  }
  // Pulizia
  memset((void *)&data, 0, sizeof(data));

  printf("Lista dei file su server:\n\n");

  download(sockfd, LIST, data, servaddr, loss_rate, rm_string);

  printf("\nLIST terminata\n\n");

  gettimeofday(&end, NULL);

  long seconds = (end.tv_sec - start.tv_sec);
  long micros = ((seconds * 1000000) + end.tv_usec) - (start.tv_usec);

  printf("La LIST ha impiegato %ld secondi e %ld microsecondi\n", seconds, micros);

  exit(EXIT_SUCCESS);
}

/// put_client
/// @param sockfd 
/// @param servaddr 
/// @param timer 
/// @param window_size 
/// @param loss_rate 
void put_client(int sockfd, struct sockaddr_in servaddr, double timer, int window_size, float loss_rate)
{
  struct segment_packet data;
  bool dyn_timer_enable = false;
  struct timeval start, end;
  char file_name[256];
  // Attivo timer dinamico
  if (timer < 0)
  {
    dyn_timer_enable = true;
    timer = DEFAULT_TIMER;
  }
  memset((void *)&data, 0, sizeof(data));

// Scelta del file da caricare su server
file_choice:
  printf("Inserire il nome del file da caricare (con estensione):\n");
  if (scanf("%s", file_name) != 1)
  {
    perror("inserire un nome valido");
    char c;
    while ((c = getchar()) != '\n' && c != EOF)
    {
    }
    goto file_choice;
  }

  gettimeofday(&start, NULL);

  sprintf(data.data, "./files/%s", file_name);

  if (!send_request(sockfd, PUT, data, timer, dyn_timer_enable))
  {
    exit(EXIT_FAILURE);
  }

  upload(sockfd, PUT, servaddr, data, dyn_timer_enable, timer, window_size, loss_rate);
  printf("PUT terminata\n");

  gettimeofday(&end, NULL);

  long seconds = (end.tv_sec - start.tv_sec);
  long micros = ((seconds * 1000000) + end.tv_usec) - (start.tv_usec);

  printf("La PUT ha impiegato %ld secondi e %ld microsecondi\n", seconds, micros);

  exit(EXIT_SUCCESS);
}

/// get_server
/// @param sockfd 
/// @param addr 
/// @param timer 
/// @param window_size 
/// @param loss_rate 
/// @param file_name 
void get_server(int sockfd, struct sockaddr_in addr, double timer, int window_size, float loss_rate, char *file_name)
{
  struct segment_packet data;
  bool dyn_timer_enable = false;
  // Attivo timer dinamico
  if (timer < 0)
  {
    dyn_timer_enable = true;
    timer = DEFAULT_TIMER;
  }

  memset((void *)&data, 0, sizeof(data));


  sprintf(data.data, "%s", file_name);
  upload(sockfd, GET, addr, data, dyn_timer_enable, timer, window_size, loss_rate);
  printf("\nGet terminata\n");
  exit(EXIT_SUCCESS);
}

/// put_server
/// @param sockfd 
/// @param addr 
/// @param loss_rate 
/// @param file_name 
void put_server(int sockfd, struct sockaddr_in addr, float loss_rate, char *file_name)
{
  struct segment_packet data;
  char *rm_string;
  // Pulizia
  memset((void *)&data, 0, sizeof(data));

  // Utile solo per la pulizia della directory in caso di errori
  rm_string = malloc(strlen(data.data) + 3);
  sprintf(rm_string, "rm %s", data.data);
  sprintf(data.data, "%s", file_name);
  download(sockfd, PUT, data, addr, loss_rate, rm_string);
  close(sockfd);
  printf("\nPut terminata\n\n");
  exit(EXIT_SUCCESS);
}

/// list_server
/// @param sockfd 
/// @param addr 
/// @param timer 
/// @param window_size 
/// @param loss_rate 
void list_server(int sockfd, struct sockaddr_in addr, double timer, int window_size, float loss_rate)
{

  struct segment_packet data;
  bool dyn_timer_enable = false;
  // Attivo timer dinamico
  if (timer < 0)
  {
    dyn_timer_enable = true;
    timer = DEFAULT_TIMER;
  }

  upload(sockfd, LIST, addr, data, dyn_timer_enable, timer, window_size, loss_rate);
  printf("List terminata\n");
  exit(EXIT_SUCCESS);
}
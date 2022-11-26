void print_head()
{
  if(system("clear") == -1){
    printf("errore system\n");
  };
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
  printf("                                                                 Massimo Mazzetti 0253467\n\n");
}

bool simulate_loss(float loss_rate)
{
  double value;   
  struct timespec tms;
  if (clock_gettime(CLOCK_REALTIME, &tms))
  {
    srand(time(NULL)); 
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
  }


  int r1 = rand() % 100;
  int r2 = rand() % 100;
  int r3 = rand() % 100;
  unsigned short vec[3] = {r1, r2, r3};
  seed48(vec);
  value = drand48(); 

  if (value < loss_rate){
    return true;
  }
  return false;
}

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
        *timer = DEFAULT_TIMER; 
      }
      else
      {
        *timer = 2 * (*timer);
      }
    }
    *trial_counter = *trial_counter + 1;
    printf("Timeout, Nuovo timer =%f\n", *timer);
    return true;
  }
  
  return false;
}

void upload(int sockfd, struct sockaddr_in addr, struct data_packet data, bool dyn_timer_enable, double timer, int window_size, float loss_rate)
{
  DIR *d;
  struct dirent *dir;
  struct data_packet *packet_buffer;
  struct data_packet ack;
  clock_t start_sample_RTT;
  double sample_RTT = 0, estimated_RTT = 0, dev_RTT = 0;
  int trial_counter = 0, len = sizeof(addr), fd;
  bool FIN_sended = false, timer_enable = false;
  long base = 0, next_seq_no = 0;
  clock_t timer_sample = clock();
  long file_size = 0, num_of_files = 0;
  off_t head;
  int type= data.type;
  int pkg_send =0, pkg_loss =0, pkg_resend = 0;

  if ((packet_buffer = malloc(window_size * sizeof(struct data_packet))) == NULL)
  {
    perror("malloc fallita");
    data.length = htons(strlen("Put fallita: errore interno del client"));
    strcpy(data.data, "Put fallita: errore interno del client");
    goto input_termination;
  }


  if (type != LIST)
  {
    if ((fd = open(data.data, O_RDONLY)) < 0)
    {
      perror("errore apertura file da inviare");
      data.length = htons(strlen("Get fallita: file non presente"));
      strcpy(data.data, "Get fallita: file non presente");
      goto input_termination;
    }
    lseek(fd, 0, SEEK_SET);
    file_size = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);
    int num_pack = (ceil((file_size / MTU)));
    printf("Sto inviando %d pacchetti\n", num_pack);
  }
  else
  {
    if ((d = opendir("./files")) == NULL)
    {
      perror("errore apertura directory dei file");
      data.length = htons(strlen("List fallita: errore interno del server"));
      strcpy(data.data, "List fallita: errore interno del server");
      goto input_termination;
    }
    head = telldir(d);
    while ((dir = readdir(d)) != NULL)
    {
      if ((strcmp(dir->d_name, ".") == 0) || (strcmp(dir->d_name, "..") == 0))
        continue;
      num_of_files++;
    }
    seekdir(d, head);
  }
  memset((void *)packet_buffer, 0, sizeof(packet_buffer));
  memset((void *)&data, 0, sizeof(data));
  memset((void *)&ack, 0, sizeof(ack));
  ack.seq_no = htonl(-1);

  while (((ntohl(ack.seq_no) + 1) * MTU < file_size) || ((ntohl(ack.seq_no) + 1) < num_of_files))
  {

    if (trial_counter >= MAX_TRIALS_NO)
    {
      printf("il canale e' molto disturbato\n");
      close(sockfd);
      exit(EXIT_FAILURE);
    }

    if (next_seq_no < base + window_size)
    {
      if (type != LIST)
      {
        if ((packet_buffer[next_seq_no % window_size].length = htons(read(fd, packet_buffer[next_seq_no % window_size].data, MTU))) > 0)
        {
          packet_buffer[next_seq_no % window_size].seq_no = htonl(next_seq_no);
          packet_buffer[next_seq_no % window_size].type = htons(NORMAL);
          sendto(sockfd, &packet_buffer[next_seq_no % window_size], sizeof(packet_buffer[next_seq_no % window_size]), 0, (struct sockaddr *)&addr, sizeof(addr));
          if ((dyn_timer_enable))
          {
            start_sample_RTT = clock();
          }
          printf("++++++++++++++Pacchetto %d inviato++++++++++++++\n", ntohl(packet_buffer[next_seq_no % window_size].seq_no));
          pkg_send++;
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
           if ((dyn_timer_enable))
          {
            start_sample_RTT = clock();
          }
          printf("++++++++++Inviato pacchetto %d++++++++++++++\n", ntohl(packet_buffer[next_seq_no % window_size].seq_no));
          pkg_send++;
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
      for (int i = 0; i < window_size; i++)
      {
        timer_sample = clock();
        sendto(sockfd, &packet_buffer[i], sizeof(packet_buffer[i]), 0, (struct sockaddr *)&addr, sizeof(addr));
        pkg_resend++;
        if ((dyn_timer_enable) )
        {
          start_sample_RTT = clock();
        }
        printf("Pacchetto %d ritrasmesso\n", ntohl(packet_buffer[i].seq_no));
      }
    }

    if (recvfrom(sockfd, &ack, sizeof(struct data_packet), MSG_DONTWAIT, (struct sockaddr *)&addr, &len) > 0)
    {
      if (!simulate_loss(loss_rate))
      {
        base = ntohl(ack.seq_no) + 1;
        trial_counter = 0;
        if ((dyn_timer_enable) )
        { 
            printf("++++++++++++++RICALCOLO TIMER++++++++++++++\n");
            sample_RTT = (double)(clock() - start_sample_RTT) * 1000 / CLOCKS_PER_SEC;
            printf("SAMPLE RTT = %f ", sample_RTT);
            estimated_RTT = (double)(0.875 * estimated_RTT) + (0.125 * sample_RTT);
            printf(" ESTIMATED RTT = %f ", estimated_RTT);
            dev_RTT = (double)(0.75 * dev_RTT) + (0.25 * fabs(sample_RTT - estimated_RTT));
            printf(" DEV RTT = %f ", dev_RTT);
            timer = (double)estimated_RTT + (4 * dev_RTT);        
            printf(" timer = %f \n", timer);
        
        }
        else{
          printf("ACK %d ricevuto\n", ntohl(ack.seq_no));
        } 
        if (base == next_seq_no)
        {
          timer_enable = false;
        }
      }
      else{
        pkg_loss++;
      }
    }
  }
  memset((void *)&ack, 0, sizeof(ack));
  memset((void *)&data, 0, sizeof(data));
  trial_counter = 0;
input_termination:
  while (1)
  {
    if (trial_counter >= MAX_TRIALS_NO)
    {
      if (ntohs(data.length) > 0)
        printf("Il canale e' molto disturbato, errore: %s", data.data);
      else
        printf("Il canale e' molto disturbato tuttavia il file e' stato consegnato con successo\n");
      break;
    }

    if (!FIN_sended)
    {
      data.type = htons(FIN);
      data.seq_no = htonl(next_seq_no);
      sendto(sockfd, &data, sizeof(data), 0, (struct sockaddr *)&addr, sizeof(addr));
      FIN_sended = true;

      timer_sample = clock();
      timer_enable = true;
      printf("Inviato FIN\n");
    }

    if (timeout(timer_sample, timer_enable, dyn_timer_enable, &timer, &trial_counter))
    { 
      pkg_resend++;
      FIN_sended = false;
    }

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
      {
        pkg_loss++;
      }
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
  printf("Ho inviato %d pacchetti\n", pkg_send);
  printf("Ho ritrasmesso %d pacchetti\n", pkg_resend);
  printf("Ho perso %d pacchetti.\n", pkg_loss);
  printf("chiudo socket\n");
  close(sockfd);
}

void download(int sockfd, struct data_packet data, struct sockaddr_in addr, float loss_rate)
{
  int fd, trial_counter = 0, len = sizeof(addr), n;
  struct data_packet ack;
  long expected_seq_no = 0;
  int pkg_loss = 0, pkg_receive=0;
  int type = data.type;
  char *rm_string;

  rm_string = malloc(strlen(data.data) + 3);
  sprintf(rm_string, "rm %s", data.data);

  if (type != LIST)
  {
    if ((fd = open(data.data, O_RDWR | O_CREAT | O_TRUNC, 0666)) < 0)
    {
      perror("errore apertura/creazione file da ricevere");
      exit(EXIT_FAILURE);
    }
  }

  memset((void *)&data, 0, sizeof(data));
  memset((void *)&ack, 0, sizeof(ack));
  ack.seq_no = htonl(-1);
  while (1)
  {
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
      if (ntohl(data.seq_no) == expected_seq_no)
      {
        if (ntohs(data.type) == FIN)
        {
          if (ntohs(data.length) > 0)
          {
            printf("%s\n", data.data);
            if(system(rm_string) == -1){
              printf("errore rimozione file\n");
            };
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
          pkg_receive++;
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
          }
          ack.type = htons(NORMAL);
          ack.seq_no = data.seq_no;
          expected_seq_no++;
        }
      }
      sendto(sockfd, &ack, sizeof(ack), 0, (struct sockaddr *)&addr, sizeof(addr));
    }
    else{
      pkg_loss++;
    }
  }
  sendto(sockfd, &ack, sizeof(ack), 0, (struct sockaddr *)&addr, sizeof(addr));
  printf("FIN ACK inviato\n");
  if (type != LIST)
  {
    close(fd);
  }
  printf("Ho Ricevuto %d pacchetti.\n", pkg_receive);
  printf("Ho Perso %d pacchetti\n", pkg_loss);
  close(sockfd);
}

bool send_request(int sockfd, int type, struct data_packet data, double timer, bool dyn_timer_enable, float loss_rate)
{
  int trial_counter = 0;
  bool command_sended = false, timer_enable = false;
  struct data_packet ack;
  clock_t timer_sample;

  while (1)
  {
    if (trial_counter >= MAX_TRIALS_NO)
    {
      printf("Il server e' morto oppure il canale e' molto disturbato ritenta piu' tardi\n");
      return false;
    }

    if (!command_sended)
    {
      data.type = htons(type);
      if (send(sockfd, &data, sizeof(data), 0) < 0)
      {
        perror("errore in send file name");
        return false;
      }
      printf("Inviata richiesta\n");
      command_sended = true;
      timer_sample = clock();
      timer_enable = true;
    }

    if (timeout(timer_sample, timer_enable, dyn_timer_enable, &timer, &trial_counter))
    {
      command_sended = false;
    }
    if (recv(sockfd, &ack, sizeof(ack), MSG_DONTWAIT) > 0)
    {
      if (ntohs(ack.type) == type)
      {
        printf("Ricevuto ack richesta\n");
        timer_enable = false;
        break;
      }
    }
  }
  return true;
}

void get_client(int sockfd, struct sockaddr_in servaddr, double timer, float loss_rate,  bool dyn_timer_enable)
{
  struct data_packet data;
  char *rm_string;
  struct timeval start, end;
  char file_name[256];

  memset((void *)&data, 0, sizeof(data));
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

  gettimeofday(&start, NULL);

  sprintf(data.data, "./files/%s", file_name);

  if (!send_request(sockfd, GET, data, timer, dyn_timer_enable, loss_rate))
  {
    exit(EXIT_FAILURE);
  }
  
  data.type = GET;
  download(sockfd, data, servaddr, loss_rate);
  printf("\nGET terminata\n\n");

  gettimeofday(&end, NULL);

  long seconds = (end.tv_sec - start.tv_sec);
  long micros = ((seconds * 1000000) + end.tv_usec) - (start.tv_usec);

  printf("La GET ha impiegato %ld secondi e %ld microsecondi\n", seconds, micros);

  exit(EXIT_SUCCESS);
}

void list_client(int sockfd, struct sockaddr_in servaddr, double timer, float loss_rate,  bool dyn_timer_enable)
{
  struct data_packet data;
  char *rm_string;
  struct timeval start, end;

  memset((void *)&data, 0, sizeof(data));
  gettimeofday(&start, NULL);

  if (!send_request(sockfd, LIST, data, timer, dyn_timer_enable, loss_rate))
  {
    exit(EXIT_FAILURE);
  }
  memset((void *)&data, 0, sizeof(data));

  printf("Lista dei file su server:\n\n");

  data.type = LIST;

  download(sockfd, data, servaddr, loss_rate);

  printf("\nLIST terminata\n\n");

  gettimeofday(&end, NULL);

  long seconds = (end.tv_sec - start.tv_sec);
  long micros = ((seconds * 1000000) + end.tv_usec) - (start.tv_usec);

  printf("La LIST ha impiegato %ld secondi e %ld microsecondi\n", seconds, micros);

  exit(EXIT_SUCCESS);
}

void put_client(int sockfd, struct sockaddr_in servaddr, double timer, int window_size, float loss_rate, bool dyn_timer_enable)
{
  struct data_packet data;
  struct timeval start, end;
  char file_name[256];
  memset((void *)&data, 0, sizeof(data));
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

  if (!send_request(sockfd, PUT, data, timer, dyn_timer_enable, loss_rate))
  {
    exit(EXIT_FAILURE);
  }

  data.type= PUT;
  upload(sockfd, servaddr, data, dyn_timer_enable, timer, window_size, loss_rate);
  printf("PUT terminata\n");

  gettimeofday(&end, NULL);

  long seconds = (end.tv_sec - start.tv_sec);
  long micros = ((seconds * 1000000) + end.tv_usec) - (start.tv_usec);

  printf("La PUT ha impiegato %ld secondi e %ld microsecondi\n", seconds, micros);

  exit(EXIT_SUCCESS);
}

void get_server(int sockfd, struct sockaddr_in addr, double timer, int window_size, float loss_rate, char *file_name, bool dyn_timer_enable)
{
  struct data_packet data;

  memset((void *)&data, 0, sizeof(data));


  sprintf(data.data, "%s", file_name);
  data.type = GET;
  upload(sockfd, addr, data, dyn_timer_enable, timer, window_size, loss_rate);
  printf("\nGet terminata\n");
  exit(EXIT_SUCCESS);
}

void put_server(int sockfd, struct sockaddr_in addr, float loss_rate, char *file_name)
{
  struct data_packet data;
  char *rm_string;
  memset((void *)&data, 0, sizeof(data));

  sprintf(data.data, "%s", file_name);
  data.type= PUT;
  download(sockfd, data, addr, loss_rate);
  close(sockfd);
  printf("\nPut terminata\n\n");
  exit(EXIT_SUCCESS);
}

void list_server(int sockfd, struct sockaddr_in addr, double timer, int window_size, float loss_rate, bool dyn_timer_enable )
{
  struct data_packet data;
  data.type = LIST;
  upload(sockfd, addr, data, dyn_timer_enable, timer, window_size, loss_rate);
  printf("List terminata\n");
  exit(EXIT_SUCCESS);
}

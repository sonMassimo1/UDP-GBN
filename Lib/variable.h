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

#define MAX_TRIALS_NO 20
#define DEFAULT_TIMER 10
#define MAX_CHOICE_TIME 60
#define MTU 1024

#define NORMAL 10
#define FIN 11
#define SYN 12

#define PUT 1
#define GET 2
#define LIST 3


struct data_packet {
    int type;
    long seq_no;
    int length;
    char data[MTU];
};


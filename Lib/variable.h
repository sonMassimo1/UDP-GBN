#define MAX_TRIALS_NO 40
#define DEFAULT_TIMER 1000
#define MAX_CHOICE_TIME 120
#define MTU 1024

#define NORMAL 10
#define FIN 11
#define SYN 12

#define PUT 1
#define GET 2
#define LIST 3


struct segment_packet {
    int type;
    long seq_no;
    int length;
    char data[MTU];
};

struct ack_packet {
    int type;
    long seq_no;
};

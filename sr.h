#ifndef SR_H
#define SR_H

#define SR_WINDOW   6
#define MAX_SEQ     256
#define TIMEOUT     16.0

#include <stdbool.h>
#include "emulator.h"

typedef struct {
    struct pkt packet;
    bool sent;
    bool acked;
    double timer_start;
} sr_slot_t;

extern sr_slot_t sr_buffer[MAX_SEQ];
extern int sr_base;
extern int sr_nextseq;

extern bool sr_recvd[MAX_SEQ];
extern struct pkt sr_recv_buffer[MAX_SEQ];
extern int sr_expect;

extern void A_init(void);
extern void B_init(void);
extern void A_input(struct pkt);
extern void B_input(struct pkt);
extern void A_output(struct msg);
extern void A_timerinterrupt(void);

#define BIDIRECTIONAL 0
extern void B_output(struct msg);
extern void B_timerinterrupt(void);

#endif

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include "emulator.h"
#include "sr.h"
double get_sim_time(void);


/* ******************************************************************
   Go Back N protocol.  Adapted from J.F.Kurose
   ALTERNATING BIT AND GO-BACK-N NETWORK EMULATOR: VERSION 1.2  

   Network properties:
   - one way network delay averages five time units (longer if there
   are other messages in the channel for GBN), but can be larger
   - packets can be corrupted (either the header or the data portion)
   or lost, according to user-defined probabilities
   - packets will be delivered in the order in which they were sent
   (although some can be lost).

   Modifications: 
   - removed bidirectional GBN code and other code not used by prac. 
   - fixed C style to adhere to current programming style
   - added GBN implementation
**********************************************************************/

#define RTT  16.0       /* round trip time.  MUST BE SET TO 16.0 when submitting assignment */
#define WINDOWSIZE 6    /* the maximum number of buffered unacked packet */
#define SEQSPACE 256    /* sequence number space for SR */
#define NOTINUSE (-1)   /* used to fill header fields that are not being used */

sr_slot_t sr_buffer[SEQSPACE];
int sr_base = 0;
int sr_nextseq = 0;
bool sr_recvd[SEQSPACE];
struct pkt sr_recv_buffer[SEQSPACE];
int sr_expect = 0;

/* generic procedure to compute the checksum of a packet.  Used by both sender and receiver  
   the simulator will overwrite part of your packet with 'z's.  It will not overwrite your 
   original checksum.  This procedure must generate a different checksum to the original if
   the packet is corrupted.
*/
int ComputeChecksum(struct pkt packet)
{
  int checksum = 0;
  int i;

  checksum = packet.seqnum;
  checksum += packet.acknum;
  for ( i=0; i<20; i++ ) 
    checksum += (int)(packet.payload[i]);

  return checksum;
}

bool IsCorrupted(struct pkt packet)
{
  if (packet.checksum == ComputeChecksum(packet))
    return (false);
  else
    return (true);
}


/********* Sender (A) variables and functions ************/

/* called from layer 5 (application layer), passed the message to be sent to other side */
void A_output(struct msg message)
{
  struct pkt sendpkt;
  int i;

  /* if not blocked waiting on ACK */
  if (((sr_nextseq - sr_base + SEQSPACE) % SEQSPACE) < WINDOWSIZE) {
    if (TRACE > 1)
      printf("----A: New message arrives, send window is not full, send new messge to layer3!\n");

    sendpkt.seqnum = sr_nextseq;
    sendpkt.acknum = NOTINUSE;
    for ( i=0; i<20 ; i++ ) 
      sendpkt.payload[i] = message.data[i];
    sendpkt.checksum = ComputeChecksum(sendpkt); 

    sr_buffer[sr_nextseq].packet = sendpkt;
    sr_buffer[sr_nextseq].sent = true;
    sr_buffer[sr_nextseq].acked = false;
    sr_buffer[sr_nextseq].timer_start = get_sim_time();

    if (TRACE > 0)
      printf("Sending packet %d to layer 3\n", sendpkt.seqnum);
    tolayer3 (A, sendpkt);
    starttimer(A, RTT);

    sr_nextseq = (sr_nextseq + 1) % SEQSPACE;
  }
  else {
    if (TRACE > 0)
      printf("----A: New message arrives, send window is full\n");
    window_full++;
  }
}


/* called from layer 3, when a packet arrives for layer 4 
   In this practical this will always be an ACK as B never sends data.
*/
void A_input(struct pkt packet)
{
  /* if received ACK is not corrupted */ 
  if (!IsCorrupted(packet)) {
    if (TRACE > 0)
      printf("----A: uncorrupted ACK %d is received\n",packet.acknum);
    total_ACKs_received++;

    int ack = packet.acknum;
    if (sr_buffer[ack].sent && !sr_buffer[ack].acked) {
      sr_buffer[ack].acked = true;
      new_ACKs++;
    }

    while (sr_buffer[sr_base].acked) {
      sr_buffer[sr_base].sent = false;
      sr_base = (sr_base + 1) % SEQSPACE;
    }

    stoptimer(A);
    for (int i = 0; i < SEQSPACE; i++) {
      if (sr_buffer[i].sent && !sr_buffer[i].acked) {
        starttimer(A, RTT);
        break;
      }
    }
  }
  else 
    if (TRACE > 0)
      printf ("----A: corrupted ACK is received, do nothing!\n");
}

/* called when A's timer goes off */
void A_timerinterrupt(void)
{
  if (TRACE > 0)
    printf("----A: time out,resend packets!\n");

  double now = get_sim_time();
  for (int i = 0; i < SEQSPACE; i++) {
    if (sr_buffer[i].sent && !sr_buffer[i].acked && 
        (now - sr_buffer[i].timer_start) >= RTT) {
      if (TRACE > 0)
        printf ("---A: resending packet %d\n", sr_buffer[i].packet.seqnum);
      tolayer3(A, sr_buffer[i].packet);
      sr_buffer[i].timer_start = now;
      packets_resent++;
      starttimer(A, RTT);
      break;
    }
  }
}       



/* the following routine will be called once (only) before any other */
/* entity A routines are called. You can use it to do any initialization */
void A_init(void)
{
  /* initialise A's window, buffer and sequence number */
  sr_base = 0;
  sr_nextseq = 0;
  for (int i = 0; i < SEQSPACE; i++) {
    sr_buffer[i].sent = false;
    sr_buffer[i].acked = false;
  }
}



/********* Receiver (B)  variables and procedures ************/


/* called from layer 3, when a packet arrives for layer 4 at B*/
void B_input(struct pkt packet)
{
  struct pkt sendpkt;
  int i;

  if (!IsCorrupted(packet)) {
    int seq = packet.seqnum;

    if (((seq - sr_expect + SEQSPACE) % SEQSPACE) < WINDOWSIZE) {
      if (!sr_recvd[seq]) {
        sr_recv_buffer[seq] = packet;
        sr_recvd[seq] = true;
        packets_received++;
      }

      sendpkt.acknum = seq;
      sendpkt.seqnum = 0;
      for (i = 0; i < 20; i++)
        sendpkt.payload[i] = '0';
      sendpkt.checksum = ComputeChecksum(sendpkt);
      tolayer3(B, sendpkt);

      while (sr_recvd[sr_expect]) {
        tolayer5(B, sr_recv_buffer[sr_expect].payload);
        sr_recvd[sr_expect] = false;
        sr_expect = (sr_expect + 1) % SEQSPACE;
      }
    }
  }
  else {
    if (TRACE > 0) 
      printf("----B: packet corrupted or not expected sequence number, resend ACK!\n");

    if (sr_expect == 0)
      sendpkt.acknum = SEQSPACE - 1;
    else
      sendpkt.acknum = sr_expect - 1;

    sendpkt.seqnum = 0;
    for (i = 0; i < 20; i++)
      sendpkt.payload[i] = '0';
    sendpkt.checksum = ComputeChecksum(sendpkt);
    tolayer3(B, sendpkt);
  }
}

/* the following routine will be called once (only) before any other */
/* entity B routines are called. You can use it to do any initialization */
void B_init(void)
{
  sr_expect = 0;
  for (int i = 0; i < SEQSPACE; i++)
    sr_recvd[i] = false;
}

/******************************************************************************
 * The following functions need be completed only for bi-directional messages *
 *****************************************************************************/

/* Note that with simplex transfer from a-to-B, there is no B_output() */
void B_output(struct msg message)  
{
}

/* called when B's timer goes off */
void B_timerinterrupt(void)
{
}

double get_sim_time(void)
{
  extern double current_sim_time;
  return current_sim_time;
}

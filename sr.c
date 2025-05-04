#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include "emulator.h"
#include "gbn.h"

#define RTT  16.0
#define WINDOWSIZE 6
#define SEQSPACE 7
#define NOTINUSE (-1)

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

static struct pkt buffer[WINDOWSIZE];
static int windowfirst, windowlast;
static int windowcount;
static int A_nextseqnum;

void A_output(struct msg message)
{
  struct pkt sendpkt;
  int i;

  if ( windowcount < WINDOWSIZE) {
    sendpkt.seqnum = A_nextseqnum;
    sendpkt.acknum = NOTINUSE;
    for ( i=0; i<20 ; i++ )
      sendpkt.payload[i] = message.data[i];
    sendpkt.checksum = ComputeChecksum(sendpkt);

    windowlast = (windowlast + 1) % WINDOWSIZE;
    buffer[windowlast] = sendpkt;
    windowcount++;

    tolayer3 (A, sendpkt);

    if (windowcount == 1)
      starttimer(A,RTT);

    A_nextseqnum = (A_nextseqnum + 1) % SEQSPACE;
  } else {
    window_full++;
  }
}

void A_input(struct pkt packet)
{
  int ackcount = 0;
  int i;

  if (!IsCorrupted(packet)) {
    total_ACKs_received++;

    if (windowcount != 0) {
      int seqfirst = buffer[windowfirst].seqnum;
      int seqlast = buffer[windowlast].seqnum;
      if (((seqfirst <= seqlast) && (packet.acknum >= seqfirst && packet.acknum <= seqlast)) ||
          ((seqfirst > seqlast) && (packet.acknum >= seqfirst || packet.acknum <= seqlast))) {

        new_ACKs++;

        if (packet.acknum >= seqfirst)
          ackcount = packet.acknum + 1 - seqfirst;
        else
          ackcount = SEQSPACE - seqfirst + packet.acknum;

        windowfirst = (windowfirst + ackcount) % WINDOWSIZE;

        for (i=0; i<ackcount; i++)
          windowcount--;

        stoptimer(A);
        if (windowcount > 0)
          starttimer(A, RTT);
      }
    }
  }
}

void A_timerinterrupt(void)
{
  int i;

  for(i=0; i<windowcount; i++) {
    tolayer3(A,buffer[(windowfirst+i) % WINDOWSIZE]);
    packets_resent++;
    if (i==0) starttimer(A,RTT);
  }
}

void A_init(void)
{
  A_nextseqnum = 0;
  windowfirst = 0;
  windowlast = -1;
  windowcount = 0;
}

static int expectedseqnum;
static int B_nextseqnum;

void B_input(struct pkt packet)
{
  struct pkt sendpkt;
  int i;

  if  ( (!IsCorrupted(packet))  && (packet.seqnum == expectedseqnum) ) {
    packets_received++;
    sendpkt.acknum = expectedseqnum;
    expectedseqnum = (expectedseqnum + 1) % SEQSPACE;
  } else {
    if (expectedseqnum == 0)
      sendpkt.acknum = SEQSPACE - 1;
    else
      sendpkt.acknum = expectedseqnum - 1;
  }

  sendpkt.seqnum = B_nextseqnum;
  B_nextseqnum = (B_nextseqnum + 1) % 2;

  for ( i=0; i<20 ; i++ )
    sendpkt.payload[i] = '0';

  sendpkt.checksum = ComputeChecksum(sendpkt);
  tolayer3 (B, sendpkt);
}

void B_init(void)
{
  expectedseqnum = 0;
  B_nextseqnum = 1;
}

void B_output(struct msg message)
{
}

void B_timerinterrupt(void)
{
}

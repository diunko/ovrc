#ifndef _OVRC_MQ_H_
#define _OVRC_MQ_H_

#include <time.h>
#include <sched.h>
#include <stdlib.h>
#include <malloc.h>
#include <inttypes.h>
#include <stdio.h>

#define MQ_BUFSCALE (5)
#define MQ_BUFSIZE (1<<MQ_BUFSCALE)
#define __sync_cas __sync_val_compare_and_swap
#define __sync_addf __sync_add_and_fetch

#define MQ_SUCCESS 0
#define MQ_ERROR_FULL 1

#define _MQ_DO_TEST_ 0

typedef struct {
  uint32_t type;
  void *data;
} mq_M;

typedef struct {
  mq_M **buf;
  uint32_t rp;
  uint32_t wp;
} mq_Q;

static __inline__
mq_Q *mq_Q_new(){
  mq_Q *Q=calloc(1,sizeof(mq_Q));
  Q->buf=(mq_M**)memalign(MQ_BUFSIZE,MQ_BUFSIZE*sizeof(void*));
  Q->rp=0;
  return Q;
}
static __inline__
void mq_Q_delete(mq_Q*Q){
  free(Q->buf);
  free(Q);
}

static __inline__
int mq_put(mq_Q*Q,mq_M*m){
  mq_M *cm;
  uint32_t wpl,wpg,wo;
  uint32_t writer_epoch;
  uint32_t wp=Q->wp;
  uint32_t rp=Q->rp;
  mq_M **buf=Q->buf;
  while(1){
    writer_epoch=(wp/MQ_BUFSIZE);
    wo=wp%MQ_BUFSIZE;

    if(rp+MQ_BUFSIZE>wp){ //ok to write to buf[wo]
      cm=__sync_cas(buf+wo,(mq_M*)writer_epoch,m);
      if(cm!=(mq_M*)writer_epoch){
        //somebody has already written here, so update pointers
        //and go for the next cell
        rp=Q->rp;
        wpl=wp+1;
        wpg=Q->wp;
        wp=wpl>wpg?wpl:wpg;
      }else{
        //we've successfully put the message
        //update wp
        __sync_addf(&Q->wp,1);
        break;
      }
    }else{
      //b. our head meets the tail of queue
      return MQ_ERROR_FULL;
    }
  }
  return MQ_SUCCESS;
}

static __inline__
mq_M *mq_get(mq_Q*Q){
  mq_M*m=NULL;
  mq_M **buf=Q->buf;
  mq_M *cm,*ccm;
  uint32_t rp,crp,writer_epoch,reader_epoch,ro;

  while(1){
    rp=Q->rp;
    writer_epoch=rp/MQ_BUFSIZE;
    reader_epoch=(rp+MQ_BUFSIZE)/MQ_BUFSIZE;
    ro=rp%MQ_BUFSIZE;

    __sync_synchronize();
    cm=buf[ro];
    if(cm==(mq_M*)writer_epoch){
      break;
    }else{
      ccm=__sync_cas(buf+ro,cm,(mq_M*)reader_epoch);
      ////////////////////////////////
      if(ccm==cm){
        __sync_addf(&Q->rp,1);
        m=cm;
        break;
      }else{
        //XXX fatal thing, you should scream here
        fprintf(stderr,"somebody screwed my reading place\n");
      }
    }
  }
  return m;
}
static __inline__
int mq_print(mq_Q*Q){
  uint32_t wp=Q->wp,rp=Q->rp;
  printf("queue %p, wp %d, rp %d\n",Q,wp,rp);
  while(wp>rp){
    fprintf(stderr,"msg id %d\n",Q->buf[rp++]->type);
  }
}
static __inline__
int mq_dump(mq_Q*Q){
  uint32_t wp=Q->wp,rp=Q->rp;
  int i=0;
  printf("queue %p, wp %d, rp %d\n",Q,wp,rp);
  for(i=0;i<MQ_BUFSIZE;i++){
    fprintf(stderr,"msg %p\n",Q->buf[i]);
  }
}


#if _MQ_DO_TEST_

typedef struct {
  int tid;
  mq_Q *Q;
  pthread_t thread;
} thread_arg;

#define FACTOR 10
#define NUM_WRITERS (1*FACTOR)
#define MSGS_PER_WRITER (40000000/FACTOR)
#define MSGS_TOT NUM_WRITERS*MSGS_PER_WRITER
#define lfprintf fprintf
#define rfprintf //fprintf

uint32_t rsleeps=0;
uint32_t wsleeps=0;

struct timespec w_1us={.tv_sec=0,.tv_nsec=10000};
struct timespec r_1us={.tv_sec=0,.tv_nsec=1000};

mq_Q *gQ;
mq_M mm[NUM_WRITERS][MSGS_PER_WRITER];

void *writer_proc(void*arg);
void *reader_proc(void*arg);
thread_arg writers[NUM_WRITERS];
thread_arg reader;
FILE *flog;
FILE *rlog;


int main(){
  int i,k=0,r;
  mq_M *m;
  gQ=mq_Q_new();
  reader.tid=0;
  reader.Q=gQ;
  flog=stdout;
  //flog=fopen("log","w");
  rlog=fopen("rlog","w");
  pthread_create(&reader.thread,
                 NULL,
                 reader_proc,
                 (void*)&reader);
  for(i=0;i<NUM_WRITERS;i++){
    writers[i].tid=i;
    writers[i].Q=gQ;
    pthread_create(&(writers[i].thread),
                   NULL,
                   writer_proc,
                   (void*)&(writers[i]));
  }
  void *res;
  for(i=0;i<NUM_WRITERS;i++)
    pthread_join(writers[i].thread,&res);
  pthread_join(reader.thread,&res);
  lfprintf(flog,"rsleeps %d, wsleeps %d.\n",rsleeps,wsleeps);
  lfprintf(flog,"all threads done\n");
  return 0;
}

void *reader_proc(void*arg){
  int k=0;
  int us=0;
  mq_M *m;
  thread_arg *ta=(thread_arg*)arg;
  while(k<MSGS_TOT&&us<1000000*600){
    while(!(m=mq_get(ta->Q))){
      //fprintf(flog,"no messages yet. sleeping for 1us.\n");
      //fprintf(rlog,".\n");
      //sched_yield();
      nanosleep(r_1us,NULL);
      __sync_addf(&rsleeps,1);
      us++;
    }
    rfprintf(flog,"R %x, %d\n",m->type,k);
    k++;
    //fprintf(flog,"R %x, %d\n",m->type,k);
  }
  lfprintf(flog,"reader exiting\n");
  pthread_exit(0);
}

void *writer_proc(void*arg){
  thread_arg *ta=(thread_arg*)arg;
  int k;
  int r;
  int mid;
  for(k=0;k<MSGS_PER_WRITER;k++){
    mid=0x1000000+0x100000*ta->tid+k;
    mm[ta->tid][k].type=mid;
    while(MQ_ERROR_FULL==
          (r=mq_put(ta->Q,mm[ta->tid]+k))){
      //      fprintf(flog,"queue overflow. sleeping for 1us.\n");
      sched_yield();
      nanosleep(w_1us,NULL);
      __sync_addf(&wsleeps,1);
    }
    //    fprintf(flog,"W %x\n",mid);
  }
  lfprintf(flog,"writer %d exiting\n",ta->tid);
  pthread_exit(0);
}
#endif //_MQ_DO_TEST_

#endif //_OVRC_MQ_H_

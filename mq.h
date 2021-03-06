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
#define MQ_ERROR 2

#define _MQ_DO_TEST_ 0

typedef struct {
  uint32_t type;
  void *data;
} mq_M;

typedef struct {
  mq_M **buf;
  uint32_t rp;
  uint32_t wp;
  pthread_mutex_t lock;
  pthread_cond_t cond;
} mq_Q;

static __inline__
mq_Q *mq_Q_new(){
  mq_Q *Q=ovrc_zalloc(sizeof(mq_Q));
  size_t bl=MQ_BUFSIZE*sizeof(void*);
  Q->buf=(mq_M**)memalign(MQ_BUFSIZE,bl);
  memset(Q->buf,0,bl);
  Q->rp=0;
  pthread_mutex_init(&Q->lock,NULL);
  pthread_cond_init(&Q->cond,NULL);
  return Q;
}
static __inline__
void mq_Q_delete(mq_Q*Q){
  free(Q->buf);
  free(Q);
}

static __inline__
mq_M *mq_M_new(uint32_t type,void*data){
  mq_M*m=ovrc_zalloc(sizeof(mq_M));
  m->type=type;
  m->data=data;
  return m;
}

static __inline__
void mq_M_delete(mq_M*m){
  ovrc_free(m);
}

static __inline__
int mq_put(mq_Q*Q,mq_M*m){
  mq_M *cm;
  uint32_t wpl,wpg,wo;
  uint32_t writer_epoch;
  uint32_t wp=Q->wp;
  uint32_t rp=Q->rp;
  mq_M **buf=Q->buf;
  __assert(m);
  ovrc_flog("put message on queue");
  if(MSG_DUMMY<=m->type
     &&m->type<=MSG_DUMMY_LAST)
    ovrc_flogf("%s",MSGS_MAP[m->type]);
  ovrc_flogf("%p",Q);
  ovrc_flogf("%p",m);
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
 end:
  return MQ_ERROR;
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
  if(m){
    ovrc_flog("got message from queue");
    if(MSG_DUMMY<=m->type
       &&m->type<=MSG_DUMMY_LAST)
      ovrc_flogf("%s",MSGS_MAP[m->type]);
    ovrc_flogf("%p",Q);
    ovrc_flogf("%p",m);
  }
  return m;
}

static __inline__
mq_M *
mq_wait(mq_Q*Q){
  mq_M *m;
  m=mq_get(Q);
  if(m==NULL){
    pthread_mutex_lock(&Q->lock);
    while(!(m=mq_get(Q))){
      pthread_mutex_cond_wait(&Q->cond,
                              &Q->lock);
    }
    pthread_mutex_unlock(&Q->lock);
  }
  return m;
}

static __inline__ mq_M *
mq_timed_wait(mq_Q*Q,struct timespec*ti){
  struct timespec ta;
  mq_M *m;
  int res;
  m=mq_get(Q);
  if(m==NULL&&ti){
    res=clock_gettime(CLOCK_REALTIME,&ta);
    if(res==0){
      ta.tv_sec+=((ta.tv_nsec+ti->tv_nsec)/1000000000);
      ta.tv_sec+=ti->tv_sec;
      ta.tv_nsec=(ta.tv_nsec+ti->tv_nsec)%1000000000;
      res=pthread_mutex_lock(&Q->lock);
      if(res==0){
        while(!(m=mq_get(Q))){
          res=pthread_cond_timedwait(&Q->cond,
                                     &Q->lock,
                                     &ta);
          if(res!=0)
            break;
        }
        pthread_mutex_unlock(&Q->lock);
      }
    }else{
      ovrc_flog("pthread_lock failed");
    }
  }
  return m;
}

static __inline__
int mq_put_signal(mq_Q*Q,mq_M*m){
  mq_M *cm;
  int res_put,res;
  res_put=mq_put(Q,m);
  if(res_put==MQ_SUCCESS
     ||res_put==MQ_ERROR_FULL){
    res=pthread_mutex_trylock(&Q->lock);
    if(res==0){
      pthread_cond_signal(&Q->cond);
      pthread_mutex_unlock(&Q->lock);
    }
  }
  return res_put;
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
#define tprintf fprintf

uint32_t rsleeps=0;
uint32_t wsleeps=0;

struct timespec w_1us={.tv_sec=0,.tv_nsec=1000000};
struct timespec r_1us={.tv_sec=0,.tv_nsec=10000};

mq_Q *gQ;
mq_M mm[NUM_WRITERS][MSGS_PER_WRITER];

void *writer_proc(void*arg);
void *reader_proc(void*arg);
void *reader_wait_proc(void*arg);
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
                 reader_wait_proc,
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
  int r;
  struct timespec Xus={.tv_sec=r_1us.tv_sec,.tv_nsec=r_1us.tv_nsec};
  mq_M *m;
  thread_arg *ta=(thread_arg*)arg;
  while(k<MSGS_TOT&&us<1000000*600){
    while(!(m=mq_get(ta->Q))){
      //fprintf(flog,"no messages yet. sleeping for 1us.\n");
      //fprintf(rlog,".\n");
      //sched_yield();
      r=nanosleep(&Xus,&Xus);
      if(r==-1)
        tprintf(flog,                                                   \
                "reader nanosleep returned %d, Xus=={.tv_sec:%ld,.tv_nsec:%ld}\n", \
                r,Xus.tv_sec,Xus.tv_nsec);
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

void *reader_wait_proc(void*arg){
  int k=0;
  int us=0;
  int r;
  struct timespec Xus={.tv_sec=r_1us.tv_sec,.tv_nsec=r_1us.tv_nsec};
  mq_M *m;
  thread_arg *ta=(thread_arg*)arg;
  while(k<MSGS_TOT&&us<1000000*600){
    while(!(m=mq_timed_wait(ta->Q,&r_1us))){
      //fprintf(flog,"no messages yet. sleeping for 1us.\n");
      //fprintf(rlog,".\n");
      //sched_yield();
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
  struct timespec Xus={.tv_sec=w_1us.tv_sec,.tv_nsec=w_1us.tv_nsec};
  for(k=0;k<MSGS_PER_WRITER;k++){
    mid=0x1000000+0x100000*ta->tid+k;
    mm[ta->tid][k].type=mid;
    while(MQ_ERROR_FULL==
          (r=mq_put_signal(ta->Q,mm[ta->tid]+k))){
      //      fprintf(flog,"queue overflow. sleeping for 1us.\n");
      //sched_yield();
      r=nanosleep(&Xus,&Xus);
      if(r==-1)
        tprintf(flog,                                                   \
                "writer nanosleep returned %d, Xus=={.tv_sec:%ld,.tv_nsec:%ld}\n", \
                r,Xus.tv_sec,Xus.tv_nsec);
      __sync_addf(&wsleeps,1);
    }
    //    fprintf(flog,"W %x\n",mid);
  }
  lfprintf(flog,"writer %d exiting\n",ta->tid);
  pthread_exit(0);
}
#endif //_MQ_DO_TEST_

#endif //_OVRC_MQ_H_

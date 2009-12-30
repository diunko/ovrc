#include "ovrc.h"


ovrc_task*
ovrc_task_new(ovrc_task_cb *proc,
              void*data){
  int r;
  ovrc_task *t=ovrc_zalloc(sizeof(ovrc_task));
  __assert(t);
  t->Qin=mq_Q_new();
  __assert(t->Qin);
  r=ovrc_task_init(t,proc,data);
  __assert(r==OVRC_OK);
  return t;
 end:
  if(t){
    if(t->Qin)
      ovrc_free(t->Qin);
    ovrc_free(t);
  }
  return NULL;
}

int
ovrc_task_init(ovrc_task*t,
               ovrc_task_cb *proc,
               void*data){
  t->proc=proc;
  t->data=data;
  t->interval.tv_sec=0;
  t->interval.tv_nsec=10000;
  return OVRC_OK;
}

void
ovrc_task_delete(ovrc_task*t){
  mq_Q_delete(t->Qin);//what to do with messages?
  t->Qin=NULL;
  ovrc_free(t);
}

int ovrc_task_msg(ovrc_task*t,mq_M*m){
  mq_put_signal(t->Qin,m);
}

int
ovrc_task_start(ovrc_task*t){
  int r=pthread_create(&t->thread,
                       NULL,
                       ovrc_task_proc,
                       t);
  if(r==0)
    return OVRC_OK;
  else
    //XXX do some logging
    return OVRC_ERROR;
}

void
ovrc_task_stop(ovrc_task*t){
  int r;
  ovrc_task_msg(t,
                &(mq_M){
                  .type=MSG_TASK_STOP,
                  .data=NULL});
}
int
ovrc_task_join(ovrc_task*t){
  void *res;
  int r;
  r=pthread_join(t->thread,&res);
  if(r==0){
    return OVRC_OK;
  }else{
    switch(r){
    case EINVAL:
    case ESRCH:
    case EDEADLK:
    default:
      return OVRC_ERROR;
    }
  }
}

void *ovrc_task_proc(void *ptr){
  ovrc_task *t=(ovrc_task*)ptr;
  mq_M *m;
  while(1){
    //task t->name starting
    m=mq_timed_wait(t->Qin,&t->interval);
    if(m){
      ovrc_flogf("%p",t);
      ovrc_flog("task received message");
      ovrc_flogf("%p",m);
      ovrc_flogf("%s",MSGS_MAP[m->type]);
      ovrc_flogf("%p",m->data);
      //task t->name received message m->type
      if(MSG_PROCESSED!=
         t->proc(t,m)){
        switch(m->type){
        case MSG_TIMEOUT:
          //task t->name sleeping
          break;
        case MSG_TASK_STOP:
          //task t->name stopping
          //noone should join task
          //if this message isn't processed in task
          //ovrc_mq_M_free(m); don't free static message
          //ovrc_task_delete(t);
          pthread_exit(NULL);
        }
      }
      mq_M_delete(m);
    }
  }
}


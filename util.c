#include "ovrc.h"

char *
ovrc_get_env (const char *name, const char *envp[]) {
  if (envp) {
    int i;
    const int namelen = strlen (name);
    for (i = 0; envp[i]; ++i) {
      if (!strncmp (envp[i], name, namelen)) {
        const char *cp = envp[i] + namelen;
        if (*cp == '=')
          return cp + 1;
      }
    }
  }
  return NULL;
}

char *
ovrc_np (const char *str) {
  if (str)
    return str;
  else
    return "[NULL]";
}

int
ovrc_atoi_null0 (const char *str) {
  if (str)
    return atoi (str);
  else
    return 0;
}


int ovrc_mq_print(mq_Q*Q){
  uint32_t wp=Q->wp,rp=Q->rp;
  printf("queue %p, wp %d, rp %d\n",Q,wp,rp);
  while(wp>rp){
    fprintf(stderr,"msg id %d\n",Q->buf[rp++]->type);
  }
}

int ovrc_mq_dump(mq_Q*Q){
  uint32_t wp=Q->wp,rp=Q->rp;
  int i=0;
  printf("queue %p, wp %d, rp %d\n",Q,wp,rp);
  for(i=0;i<MQ_BUFSIZE;i++){
    fprintf(stderr,"msg %p\n",Q->buf[i]);
  }
}


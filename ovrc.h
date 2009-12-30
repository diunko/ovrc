#ifndef OVRC_H
#define OVRC_H

#include <openvpn/openvpn-plugin.h>
#include <radiusclient-ng.h>

#include <stdint.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sched.h>
#include <malloc.h>
#include <inttypes.h>
#include <errno.h>

#define ovrc_malloc malloc
#define ovrc_zalloc(size) calloc(1,(size))
#define ovrc_free free

#define OVRC_OK 0
#define OVRC_ERROR 1

#define OVRC_CLI_HASH_SIZE 51

enum{
  MSG_DUMMY=0,
  MSG_PROCESSED,
  MSG_CONTINUE,//strange name. definitely needs to be
  //changed. if returned from task_proc_cb, indicates
  //that message should be processed by ovrc_task_proc
  MSG_TASK_STOP,
  MSG_TIMEOUT,
  MSG_CLIENT_AUTH,
  MSG_CLIENT_CONNECT,
  MSG_CLIENT_DISCONNECT,
  MSG_DUMMY_LAST
};

static
char *MSGS_MAP[]={
  "MSG_DUMMY",
  "MSG_PROCESSED",
  "MSG_CONTINUE",
  "MSG_TASK_STOP",
  "MSG_TIMEOUT",
  "MSG_CLIENT_AUTH",
  "MSG_CLIENT_CONNECT",
  "MSG_CLIENT_DISCONNECT",
  "MSG_DUMMY_LAST"
};

/////////////////////////
#define ovrc_flog(str)                          \
  printf("////////////////////\n");             \
  printf("%s:%d @%s: %s\n",                     \
         __FILE__,__LINE__,__FUNCTION__,str)    
#define ovrc_flogf(fmt,arg)                     \
  printf("////////////////////\n");             \
  printf("%s:%d @%s: " #arg"=" fmt "\n",                \
         __FILE__,__LINE__,__FUNCTION__,arg)    
#define __catch(T) if(!(T)) goto finally;
#define __assert(T) if(!(T)) {                  \
  printf("assertion failed at %s:%d\n"          \
         "in function %s: !(%s)",               \
         __FILE__,__LINE__,__FUNCTION__,#T);    \
    goto end;                                   \
  }
////////////////////////

#include "mq.h"

/////////////////////////
//util.h
#define OVRC_SETSV(obj,field,str)               \
  obj->field=ovrc_zalloc(strlen((str)));        \
  strcpy(obj->field,(str))
#define OVRC_SETS(obj,field)                    \
  obj->field=ovrc_zalloc(strlen((field)));      \
  strcpy(obj->field,(field))

char * ovrc_get_env (const char *name, const char *envp[]) ;
char * ovrc_np (const char *str);
int ovrc_atoi_null0 (const char *str);
//util.h
/////////////////////////

//////////////////////////
//hash.h
#define H_KEY_MAX 63
#define H_ERROR -1
#define H_SUCCESS 0
#define H_EXISTS 1
#define H_NOTFOUND 2

#define OVRC_H_TEST 0
#define OVRC_TASK_TEST 1
#define OVRC_SERVER_TEST 0


typedef struct h_entry h_entry;
struct h_entry {
  uint32_t hash;
  char *key;
  void *data;
  h_entry *next;
};

typedef struct  {
  uint32_t size;
  //array of pointers to bucket's first entry
  h_entry **table;
  pthread_mutex_t lock;  
} h_ctx;

typedef void (h_each_cb(h_entry *he));

uint32_t h_hash_str(char *key);
int h_buf_sa_copy(char *buf, char *keys[], int buflen);
char * h_key_new_sa(char *keys[]);
void h_key_delete(char*key);
h_ctx *h_new(uint32_t size);
int h_init(h_ctx *H);
int h_finalize(h_ctx *H);
void h_delete(h_ctx *H);
h_entry *h_entry_new_kp(char *key,void *data);
h_entry *h_entry_new(char *key,void *data);
h_entry *h_entry_new_sa(char *sa[],void *data);
void h_entry_delete(h_entry *he);
h_entry **h_find_nolock(h_ctx *H, char *key, uint32_t hash);
int h_ins(h_ctx *H,h_entry *phe);
h_entry *h_get(h_ctx *H,char *key,uint32_t hash);
h_entry *h_get_sa(h_ctx *H,char *keys[]);
h_entry *h_del(h_ctx *H,char *key);
void h_each_del(h_ctx *H,h_each_cb *f);
void h_each(h_ctx *H,h_each_cb *f);
//hash.h
//////////////////////////

//////////////////////////
//task.h
typedef struct ovrc_task ovrc_task;
typedef int ovrc_task_cb(ovrc_task*,mq_M*);

struct ovrc_task{
  pthread_t thread;
  mq_Q *Qin;
  pthread_mutex_t lock;
  pthread_cond_t cond;
  struct timespec interval;
  ovrc_task_cb *proc;
  void *data;
};

ovrc_task* ovrc_task_new(ovrc_task_cb *proc, void*data);
int ovrc_task_init(ovrc_task*t, ovrc_task_cb *proc, void*data);
void ovrc_task_delete(ovrc_task*t);
int ovrc_task_msg(ovrc_task*t,mq_M*m);
int ovrc_task_start(ovrc_task*t);
void ovrc_task_stop(ovrc_task*t);
int ovrc_task_join(ovrc_task*t);
void *ovrc_task_proc(void *ptr);
//task.h
//////////////////////////

//////////////////////////
//config.h
typedef struct {
  int version;
  int verbosity;
} ovrc_config;
//config.h
//////////////////////////

//////////////////////////
//server.h
typedef struct {
  struct {
    SERVER *acct_server;
    SERVER *auth_server;
  } rc;
  struct {
    char *ccd_path;
    SERVER *mgmt_server;
  } ov;
  struct {
  } mgmt;
  struct {
    int dummy;
  } limits;
  h_ctx *cli_hash;
} ovrc_ctx;//

ovrc_ctx* ovrc_server_new();
void ovrc_server_delete(ovrc_ctx*ctx);
int ovrc_server_init(ovrc_ctx*ctx);
int ovrc_server_finalize(ovrc_ctx*ctx);
int ovrc_server_config_apply(ovrc_ctx*ctx, ovrc_config*cfg);
int ovrc_server_start(ovrc_ctx*ctx);
int ovrc_server_stop(ovrc_ctx*ctx);
int ovrc_server_abort(ovrc_ctx*ctx);
//server.h
/////////////////////////////

//////////////////////////
//client.h
typedef struct {
  char *key;
  uint32_t hash;
  char *common_name;
  char *password;
  char *ip;
  char *port;
  char *auth_control_file;
  char *client_config_file;
  char *client_status_file;
  int interim_imterval;
  
  rc_handle *rc_h;
  ovrc_task *task;
  ovrc_ctx *ctx;
  enum {
    st_cli_start,
    st_cli_auth_done,
    st_cli_connected,
    st_cli_done
  } state;
} ovrc_cli_ctx;

ovrc_cli_ctx* ovrc_client_new(ovrc_ctx*ctx);
void ovrc_client_delete(ovrc_cli_ctx*cli_ctx);
int ovrc_client_init(ovrc_cli_ctx*cli_ctx,ovrc_ctx*ctx);
int ovrc_client_finalize(ovrc_cli_ctx*cli_ctx);
int ovrc_client_task_proc(ovrc_task*t, mq_M*m);
int ovrc_client_rc_auth(ovrc_cli_ctx*);
int ovrc_client_rc_connect(ovrc_cli_ctx*);
int ovrc_client_rc_accounting(ovrc_cli_ctx*);
int ovrc_client_rc_disconnect(ovrc_cli_ctx*);
//client.h
//////////////////////////


#endif //OVRC_H


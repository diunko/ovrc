#include <stdint.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define H_KEY_MAX 63
#define H_ERROR -1
#define H_SUCCESS 0
#define H_EXISTS 1
#define H_NOTFOUND 2
#define OVRC_H_TEST 0
#define catch(T) if(!(T)) goto finally;
#define assert(T) if(!(T)) goto end;

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
h_entry *h_del(h_ctx *H,char *key);
void h_each_del(h_ctx *H,h_each_cb *f);
void h_each(h_ctx *H,h_each_cb *f);


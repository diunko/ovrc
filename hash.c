#include "ovrc.h"

//////////////////////////////////
#define FNV_PRIME 16777619U
#define FNV_BASE  2166136261U
#define FNV_OP(hash,octet) \
  (((uint32_t)(hash)^(uint8_t)(octet))*FNV_PRIME)

uint32_t h_hash_str(char *key){
  uint32_t hash=FNV_BASE;
  char *p=key;
  int i;
  int len=strnlen(key,H_KEY_MAX);
  for(i=0;i<len&&*p;i++,p++){
    hash=FNV_OP(hash,*p);
  }
  return hash;
}

char *
h_key_new_sa(char *keys[]){
  int i,k,j;
  int l=H_KEY_MAX;
  char buf[H_KEY_MAX+1];
  int rem=H_KEY_MAX;
  for(i=0,j=0;j<l&&keys[i];i++){
    for(k=0;j<l&&keys[i][k];){
      buf[j++]=keys[i][k++];
    }
  }
  buf[j]=(char)0;
  char *key=ovrc_malloc(j+1);
  memcpy(key,buf,j+1);
  return key;
}
void
h_key_delete(char*key){
  free(key);
}

h_ctx *
h_new(uint32_t size){
  h_ctx *H=ovrc_zalloc(sizeof(h_ctx));
  H->size=size;
  H->table=ovrc_zalloc(sizeof(h_entry*)*size);
  h_init(H);
  return H;
}
int
h_init(h_ctx *H){
  pthread_mutex_init(&H->lock,NULL);
}
int
h_finalize(h_ctx *H){
  h_each_del(H,h_entry_delete);
}
void
h_delete(h_ctx *H){
  h_finalize(H);
  free(H->table);
  free(H);
}

h_entry *
h_entry_new_kp(char *key,void *data){
  h_entry *he=NULL;
  assert(key);
  int l=strnlen(key,H_KEY_MAX);
  assert(l);
  he=ovrc_zalloc(sizeof(h_entry));
  he->hash=h_hash_str(key);
  he->key=key;
  he->data=data;
 end:
  return he;
}

h_entry *
h_entry_new(char *key,void *data){
  h_entry *he=NULL;
  assert(key);
  int l=strnlen(key,H_KEY_MAX);
  assert(l);
  char *key_l=ovrc_malloc(l+1);
  strncpy(key_l,key,l);
  key_l[l]=(char)0;
  he=h_entry_new_kp(key_l,data);
 end:
  return he;
}

h_entry *
h_entry_new_sa(char *sa[],void *data){
  char *key=h_key_new_sa(sa);
  return h_entry_new_kp(key,data);
}

void
h_entry_delete(h_entry *he){
  free(he->key);
  free(he);
}//

h_entry **
h_find_nolock(h_ctx *H,
              char *key,
              uint32_t hash){
  //returns address (&prev->next)
  //of pointer to entry being searched,
  //or to NULL, if key is not found
  h_entry **pphe=NULL;
  assert(H);
  assert(key);
  hash=hash?hash:h_hash_str(key);
  int l=strnlen(key,H_KEY_MAX);
  for(pphe=H->table+hash%H->size;
      *pphe!=NULL;
      pphe=&((*pphe)->next)){
    assert((*pphe)->key);
    if((*pphe)->hash==hash)
      if(strncmp((*pphe)->key,key,l)==0)
        break;
  }
 end:
  return pphe;
}

int
h_ins(h_ctx *H,h_entry *phe){
  h_entry **pphe;
  int res=H_ERROR;
  assert(H);
  assert(phe);
  assert(phe->next==NULL);
  pthread_mutex_lock(&(H->lock));
    pphe=h_find_nolock(H,phe->key,phe->hash);
    if(
      //something wrong with params
      //supplied to h_find_nolock
      pphe==NULL
    ){
      res=H_ERROR;
    }else if(
      //entry is not found
      *pphe==NULL
    ){
      *pphe=phe;
      res=H_SUCCESS;
    }else if(
      //entry is found.
      //we may even skip this check.
      (*pphe)->hash==phe->hash
      && strncmp((*pphe)->key,phe->key,H_KEY_MAX)==0
    ){
      res=H_EXISTS;
    }
 finally:
  pthread_mutex_unlock(&(H->lock));
 end:
  return res;
}

h_entry *
h_get(h_ctx *H,char *key,uint32_t hash){
  h_entry **pphe=NULL,*phe=NULL;
  assert(H);
  assert(key);
  pthread_mutex_lock(&(H->lock));
    pphe=h_find_nolock(H,key,hash);
    catch(pphe);
    phe=*pphe;
 finally:
  pthread_mutex_unlock(&(H->lock));
 end:
  return phe;
}

h_entry *
h_del(h_ctx *H,char *key){
  h_entry *phe=NULL,**pphe=NULL;
  assert(H);
  assert(key);
  uint32_t hash=h_hash_str(key);
  pthread_mutex_lock(&(H->lock));
    pphe=h_find_nolock(H,key,hash);
    catch(pphe);
    if((*pphe)->hash==hash)
      if(strcmp((*pphe)->key,key)==0){
        phe=*pphe;
        *pphe=(*pphe)->next;
        phe->next=NULL;
      }
 finally:
  pthread_mutex_unlock(&(H->lock));
 end:
  return phe;
}

void h_each_del(h_ctx *H,h_each_cb *f){
  assert(H);
  assert(f);
  pthread_mutex_lock(&(H->lock));
    int i,ti;
    h_entry *phe=NULL,**pphe=NULL,*nphe=NULL;
    for(i=0;i<H->size;i++){
      pphe=H->table+i;
      while(*pphe){
        phe=*pphe;//get current entry
        //and eliminate it from list
        //remember, pphe==&(he->next) for some he
        //i.e. next string is *(&(he->next))=(*(&(he->next))->next; for some he
        *pphe=(*pphe)->next;
        //
        phe->next=NULL;
        (*f)(phe);
      }
    }
 finally:
  pthread_mutex_unlock(&(H->lock));
 end:
  return;
}

void h_each(h_ctx *H,h_each_cb *f){
  assert(H);
  assert(f);
  pthread_mutex_lock(&(H->lock));
    int i,ti;
    h_entry **pphe=NULL;
    for(i=0;i<H->size;i++){
      for(pphe=H->table+i;
          *pphe;
          pphe=&(*pphe)->next){
        (*f)(*pphe);
      }
    }
 finally:
  pthread_mutex_unlock(&(H->lock));
 end:
  return;
}

#if 0

h_entry *
h_find(h_ctx *H,char *key){
  //can't remember, what it's supposed to do?
  h_entry *phe=NULL;
  pthread_mutex_lock(&(H->lock));
    pphe=h_find_nolock(H,he->key,0);
    if(pphe)
    if(*pphe->hash==hash)
      if(strcmp(*pphe->key,key)==0){
        phe=*pphe;
      }
  pthread_mutex_unlock(&(H->lock));
  return phe;
}

#endif

#if OVRC_H_TEST

int test1(){
  char **pkk[]={
    (char *[]){"asdfa",",","sdf",",","asdfa",",","dfadf",0},
    (char *[]){"oija""osidjfoa",",","sidjfoaisd",0},
    (char *[]){"oais",",","djfoa",",","isjdf",0},
    (char *[]){"aois",",","djfoa",",","isjdf",0},
    (char *[]){"oiaj",",","sdfoiasjd",",","fiojdasf",0},
    (char *[]){"aijfp",",","aso9djaj8",",","98adf",0},
    (char *[]){"a09s",",","dfa09",",","fdj0a",",","9fdj",0},
    (char *[]){"aoidfoia",",","jfoaijsdo",",","fiposidj",",","fpaoisdf",0},
    (char *[]){"adskfnasjd",",","nfkjan",",","sdkjfnwe",",","uhrihuhre",0},
    (char *[]){"apsodi",",","jfpaosidjfp",",","oasidjfpqwe",",","rasdnknjadf",0},
    (char *[]){"asdfkjnwje",",","nrkqernqk",",","wjrkqwjre",0},
    (char *[]){"qpoeir",",","upoi34p5i34",",","5u98dsu98uv9",",","8dsfdfadf",0},
    (char *[]){"ja9jfa9dj8fa09",",","dj8fa098df0a9jf09j",",","8afыфващш",",","оывавыаafd",0},
    (char *[]){"lkajdflkans",",","ldfannewi",",","nqpoier",",","qiwumr",0},
    NULL
  };
  char *k;
  int i=0,j=0;
  char **pk;
  for(i=0;pkk[i];i++){
    pk=pkk[i];
    k=h_key_new_sa(pk);
    printf("calculated hash 0x%.8x for %s\n",
           h_hash_str(k),k);
    h_key_delete(k);
  }
  char *ip="1.2.3.4",*port="12345",*common_name="someuser";
  k=h_key_new_sa((char *[]){ip,":",port,",",common_name,0});
  printf("dinamically composed key \"%s\"\n",k);
  h_key_delete(k);
}

int test2(){
  char **pkk[]={
    (char *[]){"asdfa",",","sdf",",","asdfa",",","dfadf",0},
    (char *[]){"oija""osidjfoa",",","sidjfoaisd",0},
    (char *[]){"oais",",","djfoa",",","isjdf",0},
    (char *[]){"aois",",","djfoa",",","isjdf",0},
    (char *[]){"oiaj",",","sdfoiasjd",",","fiojdasf",0},
    (char *[]){"aijfp",",","aso9djaj8",",","98adf",0},
    (char *[]){"a09s",",","dfa09",",","fdj0a",",","9fdj",0},
    (char *[]){"aoidfoia",",","jfoaijsdo",",","fiposidj",",","fpaoisdf",0},
    (char *[]){"adskfnasjd",",","nfkjan",",","sdkjfnwe",",","uhrihuhre",0},
    (char *[]){"apsodi",",","jfpaosidjfp",",","oasidjfpqwe",",","rasdnknjadf",0},
    (char *[]){"asdfkjnwje",",","nrkqernqk",",","wjrkqwjre",0},
    (char *[]){"qpoeir",",","upoi34p5i34",",","5u98dsu98uv9",",","8dsfdfadf",0},
    (char *[]){"ja9jfa9dj8fa09",",","dj8fa098df0a9jf09j",",","8afыфващш",",","оывавыаafd",0},
    (char *[]){"lkajdflkans",",","ldfannewi",",","nqpoier",",","qiwumr",0},
    NULL
  };
  char *k;
  int i=0,j=0;
  char **sa;
  h_ctx *H=h_new(128);
  h_entry *he=NULL;
  for(i=0;pkk[i];i++){
    sa=pkk[i];
    he=h_entry_new_sa(sa,(void*)i);
    printf("created h_entry hash 0x%.8x key %s data %p\n",
           he->hash,he->key,he->data);
    h_ins(H,he);
  }
  printf("double insert");
  void print_entry(h_entry*he){
    printf("found h_entry hash 0x%.8x key %s data %p\n",
           he->hash,he->key,he->data);
  }
  h_each(H,print_entry);
  printf("iterating and deleting elements\n");
  h_each_del(H,print_entry);
  printf("everything deleted, trying to find some entries...");
  h_each(H,print_entry);
  printf("deleting hash");
  h_delete(H);
}
int test3(){
  char **pkk[]={
    (char *[]){"asdfa",",","sdf",",","asdfa",",","dfadf",0},
    (char *[]){"oija""osidjfoa",",","sidjfoaisd",0},
    (char *[]){"oais",",","djfoa",",","isjdf",0},
    (char *[]){"aois",",","djfoa",",","isjdf",0},
    (char *[]){"oiaj",",","sdfoiasjd",",","fiojdasf",0},
    (char *[]){"aijfp",",","aso9djaj8",",","98adf",0},
    (char *[]){"a09s",",","dfa09",",","fdj0a",",","9fdj",0},
    (char *[]){"aoidfoia",",","jfoaijsdo",",","fiposidj",",","fpaoisdf",0},
    (char *[]){"adskfnasjd",",","nfkjan",",","sdkjfnwe",",","uhrihuhre",0},
    (char *[]){"apsodi",",","jfpaosidjfp",",","oasidjfpqwe",",","rasdnknjadf",0},
    (char *[]){"asdfkjnwje",",","nrkqernqk",",","wjrkqwjre",0},
    (char *[]){"qpoeir",",","upoi34p5i34",",","5u98dsu98uv9",",","8dsfdfadf",0},
    (char *[]){"ja9jfa9dj8fa09",",","dj8fa098df0a9jf09j",",","8afыфващш",",","оывавыаafd",0},
    (char *[]){"lkajdflklkans",",","ldfannewi",",","nqpoier",",","qiwumr",0},
    (char *[]){"lkajdflkdans",",","ldfannewi",",","nqpoier",",","qiwumr",0},
    (char *[]){"lkajdflksfans",",","ldfannewi",",","nqpoier",",","qiwumr",0},
    (char *[]){"lkajdflkanowijs",",","ldfannewi",",","nqpoier",",","qiwumr",0},
    (char *[]){"1ssodijlkajdflkans",",","ldfannewi",",","nqpoier",",","qiwumr",0},
    (char *[]){"2ssodijlkajdflkans",",","ldfannewi",",","nqpoier",",","qiwumr",0},
    (char *[]){"3ssodijlkajdflkans",",","ldfannewi",",","nqpoier",",","qiwumr",0},
    (char *[]){"4ssodijlkajdflkans",",","ldfannewi",",","nqpoier",",","qiwumr",0},
    (char *[]){"5ssodijlkajdflkans",",","ldfannewi",",","nqpoier",",","qiwumr",0},
    (char *[]){"6ssodijlkajdflkans",",","ldfannewi",",","nqpoier",",","qiwumr",0},
    (char *[]){"7ssodijlkajdflkans",",","ldfannewi",",","nqpoier",",","qiwumr",0},
    (char *[]){"8ssodijlkajdflkans",",","ldfannewi",",","nqpoier",",","qiwumr",0},
    (char *[]){"9ssodijlkajdflkans",",","ldfannewi",",","nqpoier",",","qiwumr",0},
    (char *[]){"0ssodijlkajdflkans",",","ldfannewi",",","nqpoier",",","qiwumr",0},
    (char *[]){"11ssodijlkajdflkans",",","ldfannewi",",","nqpoier",",","qiwumr",0},
    (char *[]){"22ssodijlkajdflkans",",","ldfannewi",",","nqpoier",",","qiwumr",0},
    (char *[]){"12ssodijlkajdflkans",",","ldfannewi",",","nqpoier",",","qiwumr",0},
    (char *[]){"13ssodijlkajdflkans",",","ldfannewi",",","nqpoier",",","qiwumr",0},
    (char *[]){"14ssodijlkajdflkans",",","ldfannewi",",","nqpoier",",","qiwumr",0},
    (char *[]){"15ssodijlkajdflkans",",","ldfannewi",",","nqpoier",",","qiwumr",0},
    (char *[]){"16ssodijlkajdflkans",",","ldfannewi",",","nqpoier",",","qiwumr",0},
    (char *[]){"17ssodijlkajdflkans",",","ldfannewi",",","nqpoier",",","qiwumr",0},
    (char *[]){"18ssodijlkajdflkans",",","ldfannewi",",","nqpoier",",","qiwumr",0},
    (char *[]){"19ssodijlkajdflkans",",","ldfannewi",",","nqpoier",",","qiwumr",0},
    (char *[]){"20ssodijlkajdflkans",",","ldfannewi",",","nqpoier",",","qiwumr",0},
    (char *[]){"21ssodijlkajdflkans",",","ldfannewi",",","nqpoier",",","qiwumr",0},
    (char *[]){"23ssodijlkajdflkans",",","ldfannewi",",","nqpoier",",","qiwumr",0},
    NULL
  };
  char *k;
  int i=0,j=0;
  char **sa;
  h_ctx *H=h_new(3);
  h_entry *he=NULL;
  int res=0;
  for(i=0;pkk[i];i++){
    sa=pkk[i];
    he=h_entry_new_sa(sa,(void*)i);
    printf("created h_entry hash 0x%.8x key %s data %p\n",
           he->hash,he->key,he->data);
    res=h_ins(H,he);
    if(res!=H_SUCCESS)
      printf("error inserting h_entry({hash:0x%.8x,key:%s,data:%p})\n",
             he->hash,he->key,he->data);
  }
  void print_entry(h_entry*he){
    printf("found h_entry hash 0x%.8x key %s data %p\n",
           he->hash,he->key,he->data);
  }
  h_each(H,print_entry);
  printf("double insert\n");
  for(i=0;pkk[i];i++){
    sa=pkk[i];
    he=h_entry_new_sa(sa,(void*)i);
    printf("created h_entry hash 0x%.8x key %s data %p\n",
           he->hash,he->key,he->data);
    res=h_ins(H,he);
    if(res!=H_SUCCESS){
      printf("error inserting h_entry({hash:0x%.8x,key:%s,data:%p})\n",
             he->hash,he->key,he->data);
      h_entry_delete(he);
    }
  }
  printf("deleting entries\n");
  for(i=0;pkk[i];i++){
    sa=pkk[i];
    he=h_entry_new_sa(sa,(void*)i);
    h_entry *he_d;
    printf("created h_entry hash 0x%.8x key %s data %p\n",
           he->hash,he->key,he->data);
    he_d=h_del(H,he->key);
    if(he_d!=NULL){
      printf("deleted h_entry({hash:0x%.8x,key:%s,data:%p})\n",
             he_d->hash,he_d->key,he_d->data);
      h_entry_delete(he_d);
    }else{
      printf("entry not found: h_entry({hash:0x%.8x,key:%s,data:%p})\n",
             he->hash,he->key,he->data);
    }
    h_entry_delete(he);
  }
  printf("iterating and deleting elements\n");
  h_each_del(H,print_entry);
  printf("everything deleted, trying to find some entries...");
  h_each(H,print_entry);
  printf("deleting hash\n");
  h_delete(H);
}

int main(){
  test1();
  test2();
  test3();
}

#endif

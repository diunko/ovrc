#include "ovrc.h"

#define OVRC_SERVER_TEST 0

ovrc_ctx*
ovrc_server_new(){
  ovrc_ctx*ctx=ovrc_zalloc(sizeof(ovrc_ctx));
  ctx->cli_hash=h_new(OVRC_CLI_HASH_SIZE);
  ovrc_server_init(ctx);
  return ctx;
}
void
ovrc_server_delete(ovrc_ctx*ctx){
  ovrc_server_finalize(ctx);
  h_delete(ctx->cli_hash);
  ovrc_free(ctx);
}

int
ovrc_server_init(ovrc_ctx*ctx){
  //initialize with reasonable start values
  ovrc_flog("");
  return OVRC_OK;
}
int
ovrc_server_finalize(ovrc_ctx*ctx){
}
int ovrc_server_config_apply(ovrc_ctx*ctx,
                             ovrc_config*cfg){
  ovrc_flog("");
  return OVRC_OK;
}

int
ovrc_server_start(ovrc_ctx*ctx){
  ovrc_flog("");
  return OVRC_OK;
}

int
ovrc_server_stop(ovrc_ctx*ctx){
  //XXX stop each client task
  ovrc_flog("");
  return OVRC_OK;
}

int
ovrc_server_abort(ovrc_ctx*ctx){
  ovrc_flog("");
  return ovrc_server_stop(ctx);
}

#if OVRC_SERVER_TEST

int test1(){
  ovrc_ctx*ctx=ovrc_server_new();
  ovrc_server_start(ctx);
  ovrc_server_stop(ctx);
  ovrc_server_delete(ctx);
  return 0;
}

int main(){
  test1();
}

#endif //OVRC_SERVER_TEST

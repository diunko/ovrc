#include "ovrc.h"

/* static */
/* ovrc_cli_ctx ovrc_client_defaults={ */
/*   .interim_interval=10 */
/* }; */

ovrc_cli_ctx*
ovrc_client_new(ovrc_ctx*ctx){
  //XXX do error chacking
  ovrc_cli_ctx *cli=ovrc_zalloc(sizeof(ovrc_cli_ctx));
  cli->task=ovrc_task_new(ovrc_client_task_proc,
                          cli);
  cli->rc_h=rc_new();
  ovrc_client_init(cli,ctx);
  return cli;
}
void
ovrc_client_delete(ovrc_cli_ctx*cli_ctx){
  ovrc_client_finalize(cli_ctx);
  ovrc_task_delete(cli_ctx->task);
  rc_destroy(cli_ctx->rc_h);
  ovrc_free(cli_ctx);
}

int
ovrc_client_init(ovrc_cli_ctx*cli_ctx,ovrc_ctx*ctx){
  //copy defaults
  cli_ctx->interim_imterval=10;
  //ovrc_client_defaults.interim_interval;
  cli_ctx->ctx=ctx;
  cli_ctx->state=st_cli_start;
  //initialize unique keys (?session key)
  return OVRC_OK;
}
int
ovrc_client_finalize(ovrc_cli_ctx*cli_ctx){
  //check and delete status and config files
  return OVRC_OK;
}

int
ovrc_client_task_proc(ovrc_task*t,
                      mq_M*m){
  ovrc_cli_ctx*cli_ctx=(ovrc_cli_ctx*)m->data;
  int res;
  __assert(cli_ctx==t->data);
  switch(m->type){
  case MSG_CLIENT_AUTH:
    res=ovrc_client_rc_auth(cli_ctx);
    break;
  case MSG_CLIENT_CONNECT:
    ovrc_client_rc_connect(cli_ctx);
    cli_ctx->state=st_cli_connected;
    break;
  case MSG_CLIENT_DISCONNECT:
    cli_ctx->state=st_cli_done;
    ovrc_client_rc_disconnect(cli_ctx);
    /* ovrc_client_finalize(cli_ctx); */
    /* ovrc_client_delete(cli_ctx); */
    break;
  case MSG_TIMEOUT:
    //check state
    switch(cli_ctx->state){
    case st_cli_auth_done:
      //if timeout exceeds wait for connect interval
      //  drop client
      break;
    case st_cli_connected:
      //time to send update packet?
      ovrc_client_rc_accounting(cli_ctx);
      break;
    }
    break;
  case MSG_TASK_STOP:
    return MSG_CONTINUE;
    break;
  }
 end:
  return MSG_PROCESSED;
}

////////////////////////////
//radius client routines
int
ovrc_client_rc_auth(ovrc_cli_ctx*cli_ctx){
  int res;
  ovrc_ctx*ctx=cli_ctx->ctx;
  cli_ctx->state=st_cli_auth_done;
  cli_ctx->key=h_key_new_sa((char *[]){
      cli_ctx->common_name,",",
      cli_ctx->ip,":",cli_ctx->port,0});
  ovrc_flogf("rc_auth called for %s",
         cli_ctx->key);
  __assert(cli_ctx->auth_control_file);
  h_entry *he=
    h_entry_new(cli_ctx->key,(void*)cli_ctx);
  res=h_ins(ctx->cli_hash,he);
  FILE *af=fopen(cli_ctx->auth_control_file,
                 "w");
  fprintf(af,"1\n");
  fclose(af);
  return OVRC_OK;
 end:
  return OVRC_ERROR;
}
int 
ovrc_client_rc_connect(ovrc_cli_ctx*cli_ctx){
  ovrc_flogf("rc_connect called for %s\n",
         cli_ctx->key);
  return OVRC_OK;
}
int
ovrc_client_rc_accounting(ovrc_cli_ctx*cli_ctx){
  ovrc_flogf("rc_accounting for %s\n",
         cli_ctx->key);
  return OVRC_OK;
}
int
ovrc_client_rc_disconnect(ovrc_cli_ctx*cli_ctx){
  ovrc_flogf("rc_disconnect for %s\n",
         cli_ctx->key);
  mq_put(cli_ctx->task->Qin,
         mq_M_new(MSG_TASK_STOP,
                  cli_ctx));
  return OVRC_OK;
}
//radius client routines
////////////////////////////

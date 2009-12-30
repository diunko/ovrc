#include "ovrc.h"

ovrc_ctx *
ovrc_server_start_cb(const char *argv[],const char *envp[]){
  /* ovrc_config *cfg=ovrc_config_new(); */

  /* char *verb_string=ovrc_get_env("verb",envp); */
  /* if(verb_string) */
  /*   cfg->verbosity=atoi(verb_string); */

  /* char *config_filename=argv[1]; */
  /* if(config_filename) */
  /*   ovrc_config_filename_set(cfg,config_filename); */
  /* ovrc_config_parse(cfg); */
  printf("ovrc_server_start_cb\n");
  
  ovrc_ctx *ctx=ovrc_server_new();
  //  ovrc_server_config_apply(ctx,cfg);
  //  ovrc_server_start(ctx);
  //  ovrc_config_delete(cfg);
  return ctx;
}

int 
ovrc_client_auth_cb(ovrc_ctx* ctx,
                    ovrc_cli_ctx* cli_ctx,
                    const char *argv[],
                    const char *envp[]){
  const char *common_name=ovrc_get_env("username",envp);
  const char *password=ovrc_get_env("password",envp);
  const char *auth_control_file=ovrc_get_env("auth_control_file",envp);
  const char *untrusted_ip=ovrc_get_env("untrusted_ip",envp);
  const char *untrusted_port=ovrc_get_env("untrusted_port",envp);
  //XXX add lengths and values check
  ovrc_flogf("%p",cli_ctx);
  printf ("ovrc_client_auth_cb DEFER u='%s' p='%s' acf='%s'\n",
          ovrc_np (common_name), ovrc_np (password), ovrc_np (auth_control_file));
  if (!auth_control_file
      ||!common_name
      ||!password
      ||!untrusted_ip
      ||!untrusted_port)
    return OPENVPN_PLUGIN_FUNC_ERROR;  
  
  OVRC_SETS(cli_ctx,common_name);
  OVRC_SETS(cli_ctx,password);
  OVRC_SETSV(cli_ctx,ip,untrusted_ip);
  OVRC_SETSV(cli_ctx,port,untrusted_port);
  OVRC_SETS(cli_ctx,auth_control_file);
  
  mq_put(cli_ctx->task->Qin,
         mq_M_new(MSG_CLIENT_AUTH,
                  cli_ctx));
  ovrc_task_start(cli_ctx->task);
  return OPENVPN_PLUGIN_FUNC_DEFERRED;
}

int
ovrc_client_connect_cb(ovrc_ctx* ctx,
                       ovrc_cli_ctx* cli_ctx,
                       const char *argv[],
                       const char *envp[]){
  h_entry *he=NULL;
  const char *common_name=ovrc_get_env("username",envp);
  const char *trusted_ip=ovrc_get_env("trusted_ip",envp);
  const char *trusted_port=ovrc_get_env("trusted_port",envp);
  char key[H_KEY_MAX+1];
  ovrc_cli_ctx*h_cli=NULL;
  ovrc_flog("");
  //XXX add lengths check
  he=h_get_sa(ctx->cli_hash,(char *[]){common_name,",",trusted_ip,":",trusted_port,0});
  __assert(he);
  h_cli=(ovrc_cli_ctx*)he->data;
  __assert(h_cli);
  __assert(h_cli==cli_ctx);
  mq_put(cli_ctx->task->Qin,
         mq_M_new(MSG_CLIENT_CONNECT,
                  cli_ctx));
  return OPENVPN_PLUGIN_FUNC_SUCCESS;
 end:
  return OPENVPN_PLUGIN_FUNC_ERROR;
}

int
ovrc_client_disconnect_cb(ovrc_ctx* ctx,
                          ovrc_cli_ctx* cli_ctx,
                          const char *argv[],
                          const char *envp[]){
  char key[H_KEY_MAX+1];
  ovrc_cli_ctx*h_cli=NULL;
  h_entry *he=NULL;
  const char *common_name=ovrc_get_env("username",envp);
  const char *trusted_ip=ovrc_get_env("trusted_ip",envp);
  const char *trusted_port=ovrc_get_env("trusted_port",envp);
  const char *bytes_sent=ovrc_get_env("bytes_sent",envp);
  const char *bytes_received=ovrc_get_env("bytes_received",envp);
  ovrc_flogf("%p",cli_ctx);
  //XXX add lengths check
  he=h_get_sa(ctx->cli_hash,(char *[]){
      common_name,",",trusted_ip,":",trusted_port,0});
  __assert(he);
  h_cli=(ovrc_cli_ctx*)he->data;
  __assert(h_cli);
  __assert(h_cli==cli_ctx);
  mq_put(cli_ctx->task->Qin,
         mq_M_new(MSG_CLIENT_DISCONNECT,
                  cli_ctx));
  /* mq_put(cli_ctx->task->Qin, */
  /*        mq_M_new(MSG_TASK_STOP, */
  /*                 cli_ctx)); */
  //finish acct task
  return OPENVPN_PLUGIN_FUNC_SUCCESS;
 end:
  return OPENVPN_PLUGIN_FUNC_ERROR;
}

void
ovrc_server_stop_cb(ovrc_ctx*ctx){
  //finish all tasks
  ovrc_flog("");
  ovrc_server_stop(ctx);
  ovrc_server_delete(ctx);
  //return OPENVPN_PLUGIN_FUNC_SUCCESS;
}

void
ovrc_server_abort_cb(ovrc_ctx*ctx){
  //abort all tasks
  ovrc_flog("");
  ovrc_server_abort(ctx);
  ovrc_server_delete(ctx);
}

ovrc_cli_ctx *
ovrc_client_construct_cb(ovrc_ctx*ctx){
  ovrc_cli_ctx *cli_ctx= ovrc_client_new(ctx);
  ovrc_flogf("%p",cli_ctx);
  ovrc_flogf("%p",cli_ctx->task);
  ovrc_flogf("%p",cli_ctx->task->Qin);
  return cli_ctx;
}

void
ovrc_client_destruct_cb(ovrc_ctx*ctx,ovrc_cli_ctx*cli_ctx){
  ovrc_flog("");
  //(?)check whether cli_ctx is still in hash
  //4ovrc_client_delete(cli_ctx);
}


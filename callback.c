#include "openvpn-plugin.h"
#include "ovrc.h"

ovrc_ctx *
ovrc_server_start_cb(const char *argv[],const char *envp[]){
  ovrc_config *cfg=ovrc_config_new();

  char *verb_string=get_env("verb",envp);
  if(verb_string)
    cfg->verbosity=atoi(verb_string);

  char *config_filename=argv[1];
  if(config_filename)
    ovrc_config_filename_set(cfg,config_filename);
  ovrc_config_parse(cfg);

  ovrc_ctx *ctx=ovrc_ctx_new();
  ovrc_server_config_apply(ctx,cfg);
  ovrc_server_start(ctx);
  ovrc_config_delete(cfg);
  return ctx;
}

int 
ovrc_client_auth_cb(ovrc_ctx* ctx,
                    ovrc_client_ctx* cli_ctx,
                    const char *argv[],
                    const char *envp[]){
  const char *common_name=get_env("username",envp);
  const char *password=get_env("password",envp);
  const char *auth_control_file=get_env("auth_control_file",envp);
  const char *untrusted_ip=get_env("untrusted_ip",envp);
  const char *untrusted_port=get_env("untrusted_port",envp);
  //XXX add lengths check
  printf ("ovrc_client_auth_cb DEFER u='%s' p='%s' acf='%s'\n",
          np (username), np (password), np (auth_control_file));
  if (!auth_control_file
      ||!username
      ||!password
      ||!untrusted_ip
      ||!untrusted_port)
    return OPENVPN_PLUGIN_FUNC_ERROR;  
  ovrc_client_set_common_name(cli_ctx,common_name);
  ovrc_client_set_password(cli_ctx,password);
  ovrc_client_set_untrusted_ip(cli_ctx,untrusted_ip);
  ovrc_client_set_untrusted_port(cli_ctx,untrusted_port);
  ovrc_client_set_auth_control_file(cli_ctx,auth_control_file);
  mq_put(cli_ctx->worker,MSG_CLIENT_AUTH);
  return OPENVPN_PLUGIN_FUNC_DEFERRED;
}

int
ovrc_client_connect_cb(ovrc_ctx* ctx,
                       ovrc_client_ctx* cli_ctx,
                       const char *argv[],
                       const char *envp[]){
  const char *common_name=get_env("username",envp);
  const char *untrusted_ip=get_env("untrusted_ip",envp);
  const char *untrusted_port=get_env("untrusted_port",envp);
  char key[KEY_MAX];
  ovrc_client_ctx*h_cli=NULL;
  //XXX add lengths check
  ovrc_client_key(&key,common_name,untrusted_ip,untrusted_port);
  ovrc_hash_get(ctx->cli_hash,key,&h_cli);
  //assert h_cli!=NULL
  //assert h_cli==cli_ctx
  mq_put(cli->worker,MSG_CLIENT_CONNECT);
  return OPENVPN_PLUGIN_FUNC_SUCCESS;
}

int
ovrc_client_disconnect_cb(ovrc_ctx* ctx,
                          ovrc_client_ctx* cli_ctx,
                          const char *argv[],
                          const char *envp[]){
  const char *username=get_env("username",envp);
  const char *untrusted_ip=get_env("untrusted_ip",envp);
  const char *untrusted_port=get_env("untrusted_port",envp);
  const char *bytes_sent=get_env("bytes_sent",envp);
  const char *bytes_received=get_env("bytes_received",envp);
  char key[KEY_MAX];
  ovrc_client_ctx*h_cli=NULL;
  //XXX add lengths check
  ovrc_client_key(&key,common_name,untrusted_ip,untrusted_port);
  ovrc_hash_get(ctx->cli_hash,key,&h_cli);
  //assert h_cli!=NULL
  //assert h_cli==cli_ctx
  mq_put(cli->worker,MSG_CLIENT_DISCONNECT);
  //finish acct task
  return OPENVPN_PLUGIN_FUNC_SUCCESS;
}

void
ovrc_server_stop_cb(ovrc_ctx*ctx){
  //finish all tasks
  ovrc_server_stop(ctx);
  ovrc_server_delete(ctx);
  return OPENVPN_PLUGIN_FUNC_SUCCESS;
}

void
ovrc_server_abort_cb(ovrc_ctx*ctx){
  //abort all tasks
  ovrc_server_abort(ctx);
  ovrc_server_delete(ctx);
}

cli_ctx *
ovrc_client_construct_cb(){
  return ovrc_client_new();
}

void
ovrc_client_destruct_cb(ovrc_ctx*ctx,ovrc_client_ctx*cli_ctx){
  //(?)check whether cli_ctx is still in hash
  ovrc_client_free(cli_ctx);
}

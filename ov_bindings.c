#include "ovrc.h"

OPENVPN_EXPORT openvpn_plugin_handle_t
openvpn_plugin_open_v1 (unsigned int *type_mask, const char *argv[],
                        const char *envp[]) {
  plugin_ctx *ctx=ovrc_server_start_cb(argv,envp);
  *type_mask =
    OPENVPN_PLUGIN_MASK (OPENVPN_PLUGIN_AUTH_USER_PASS_VERIFY) |
    OPENVPN_PLUGIN_MASK (OPENVPN_PLUGIN_CLIENT_CONNECT_V2) |
    OPENVPN_PLUGIN_MASK (OPENVPN_PLUGIN_CLIENT_DISCONNECT);
  return (openvpn_plugin_handle_t) ctx;
}

OPENVPN_EXPORT int
openvpn_plugin_func_v2 (openvpn_plugin_handle_t handle,
                        const int type, const char *argv[],
                        const char *envp[], void *per_client_context,
                        struct openvpn_plugin_string_list **return_list) {
  ovrc_ctx *ctx = (struct ovrc_ctx *) handle;
  ovrc_cli_ctx *cli_ctx = (ovrc_cli_ctx *) per_client_context;
  switch (type) {
  case OPENVPN_PLUGIN_AUTH_USER_PASS_VERIFY:
    printf ("OPENVPN_PLUGIN_AUTH_USER_PASS_VERIFY\n");
    return ovrc_client_auth_cb(ctx,cli_ctx,argv,envp);
  case OPENVPN_PLUGIN_CLIENT_CONNECT_V2:
    printf ("OPENVPN_PLUGIN_CLIENT_CONNECT_V2\n");
    return ovrc_client_connect_cb(ctx,cli_ctx,argv,envp);
  case OPENVPN_PLUGIN_CLIENT_DISCONNECT:
    printf ("OPENVPN_PLUGIN_CLIENT_DISCONNECT\n");
    return ovrc_client_disconnect_cb(ctx,cli_ctx,argv,envp);
  default:
    return OPENVPN_PLUGIN_FUNC_SUCCESS;
  }
}

OPENVPN_EXPORT void *
openvpn_plugin_client_constructor_v1 (openvpn_plugin_handle_t handle) {
  return (void*)ovrc_client_construct_cb((ovrc_ctx*)handle);
}

OPENVPN_EXPORT void
openvpn_plugin_client_destructor_v1 (openvpn_plugin_handle_t handle,
                                     void *per_client_context) {
  ovrc_client_desctruct_cb((ovrc_ctx*)handle,
                           (ovrc_cli_ctx*)per_client_context);
}

OPENVPN_EXPORT void
openvpn_plugin_close_v1 (openvpn_plugin_handle_t handle) {
  ovrc_server_stop_cb((ovrc_ctx*)handle);
}

OPENVPN_EXPORT void
openvpn_plugin_abort_v1 (openvpn_plugin_handle_t handle) {
  ovrc_server_abort_cb((ovrc_ctx*)handle);
}




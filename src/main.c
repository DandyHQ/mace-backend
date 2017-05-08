#include "mongoose.h"

static const char *s_http_port = "8000";
static struct mg_serve_http_opts s_http_server_opts;

static int is_websocket(const struct mg_connection *nc) {
  return nc->flags & MG_F_IS_WEBSOCKET;
}

static void broadcast(struct mg_connection *nc, const struct mg_str msg) {
  struct mg_connection *c;
  char buf[500];
  char addr[32];
  mg_sock_addr_to_str(&nc->sa, addr, sizeof(addr),
                      MG_SOCK_STRINGIFY_IP | MG_SOCK_STRINGIFY_PORT);

  snprintf(buf, sizeof(buf), "%s %.*s", addr, (int) msg.len, msg.p);
  printf("%s\n", buf); /* Local echo. */
  for (c = mg_next(nc->mgr, NULL); c != NULL; c = mg_next(nc->mgr, c)) {
    if (c == nc) continue; /* Don't send to the sender. */
    mg_send_websocket_frame(c, WEBSOCKET_OP_TEXT, buf, strlen(buf));
  }
}

static void
ev_handler(struct mg_connection *c, int ev, void *ev_data)
{
	switch (ev) {
		case MG_EV_HTTP_REQUEST: {
			mg_serve_http(c, (struct http_message *) ev_data, s_http_server_opts);
			break;
		}
		case MG_EV_WEBSOCKET_HANDSHAKE_DONE: {
			/* New websocket connection. */
			broadcast(c, mg_mk_str("++ joined"));
			break;
		}
		case MG_EV_WEBSOCKET_FRAME: {
			struct websocket_message *wm = (struct websocket_message *) ev_data;
			/* New websocket message. */
			struct mg_str d = {(char *) wm->data, wm->size};
			broadcast(c, d);
			break;
		}
		case MG_EV_CLOSE: {
			/* Disconnect */
			if (is_websocket(c)) {
				broadcast(c, mg_mk_str("-- left"));
			}
			break;
		}
	}
}

int
main(void)
{
	struct mg_mgr mgr;
	struct mg_connection *c;

	mg_mgr_init(&mgr, NULL);
	printf("Started on port %s\n", s_http_port);
	c = mg_bind(&mgr, s_http_port, ev_handler);
	if (c == NULL) {
		printf("Failed to create listener\n");
		return 1;
	}

	mg_set_protocol_http_websocket(c);
	s_http_server_opts.document_root = ".";
	s_http_server_opts.enable_directory_listing = "no";

	for (;;) {
		mg_mgr_poll(&mgr, 1000);
	}
	mg_mgr_free(&mgr);

	return 0;
}

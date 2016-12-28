/*
 * main.cpp
 *
 * Webserver main
 *
 *  Created on: 29.08.2016
 *      Author: JochenAlt
 */

#include "mongoose.h"
#include "CmdDispatcher.h"
#include "Util.h"
#include "logger.h"

INITIALIZE_EASYLOGGINGPP

static const char *s_http_port = "8000";
static const struct mg_str s_get_method = MG_MK_STR("GET");

static struct mg_serve_http_opts s_http_server_opts;

#include <stdlib.h>
#include <ctype.h>

void setupLogger() {
	// setup logger
	el::Configurations defaultConf;
	defaultConf.setToDefault();
	defaultConf.set(el::Level::Error, el::ConfigurationType::Format,
			"%datetime %level [%func] [%loc] %msg");
	defaultConf.set(el::Level::Error, el::ConfigurationType::Filename,
			"logs/walter.log");

	defaultConf.set(el::Level::Info, el::ConfigurationType::Format,
			"%datetime %level %msg");
	defaultConf.set(el::Level::Info, el::ConfigurationType::Filename,
			"logs/webserver.log");

	defaultConf.set(el::Level::Debug, el::ConfigurationType::ToStandardOutput,
			std::string("false"));
	// defaultConf.set(el::Level::Debug, el::ConfigurationType::Enabled,std::string("false"));

	defaultConf.set(el::Level::Debug, el::ConfigurationType::Format,
			std::string("%datetime %level [%func] [%loc] %msg"));
	defaultConf.set(el::Level::Debug, el::ConfigurationType::Filename,
			"logs/webserver.log");

	// logging from uC is on level Trace
	defaultConf.set(el::Level::Trace, el::ConfigurationType::ToStandardOutput,
			std::string("false"));
	defaultConf.set(el::Level::Trace, el::ConfigurationType::Format,
			std::string("%datetime %level [uC] %msg"));
	defaultConf.set(el::Level::Trace, el::ConfigurationType::Filename,
			"logs/webserver.log");

	el::Loggers::reconfigureLogger("default", defaultConf);

	LOG(INFO) << "Walter Website Setup";

}
static void handle_ssi_call(struct mg_connection *nc, const char *param) {
	bool ok;
	string value = CommandDispatcher::getInstance().getVariable(string(param), ok);
    mg_printf_html_escape(nc, value.c_str());
}


static void ev_handler(struct mg_connection *nc, int ev, void *ev_data) {

	struct http_message *hm = (struct http_message *) ev_data;
	switch (ev) {
	case MG_EV_HTTP_REQUEST: {
			string uri(hm->uri.p, hm->uri.len);
			string query(hm->query_string.p, hm->query_string.len);
	        string body(hm->body.p, hm->body.len);

	        bool ok;
			string response;
			bool processed = CommandDispatcher::getInstance().dispatch(uri, query, body, response, ok);
			if (processed) {
				if (ok) {
					mg_printf(nc, "HTTP/1.1 200 OK\r\n"
						"Content-Type: text/plain\r\n"
						"Content-Length: %d\r\n\r\n%s",
							(int) response.length(), response.c_str());
				} else {
					mg_printf(nc, "HTTP/1.1 500 Server Error\r\n"
							"Content-Length: %d\r\n\r\n%s",
							(int) response.length(), response.c_str());
				}
			} else {
				// not api call, serve static content
				mg_serve_http(nc, (http_message*) ev_data, s_http_server_opts);
			}
		break;
	}
	case MG_EV_SSI_CALL:
		handle_ssi_call(nc, (const char*)ev_data);
		break;
	default:
		break;
	}
}

int main(void) {
	struct mg_mgr mgr;
	struct mg_connection *nc;
	cs_stat_t st;

	mg_mgr_init(&mgr, NULL);
	nc = mg_bind(&mgr, s_http_port, ev_handler);
	if (nc == NULL) {
		fprintf(stderr, "Cannot bind to %s\n", s_http_port);
		exit(1);
	}

	// Set up HTTP server parameters
	mg_set_protocol_http_websocket(nc);
	s_http_server_opts.document_root = "web_root"; // Set up web root directory

	if (mg_stat(s_http_server_opts.document_root, &st) != 0) {
		fprintf(stderr, "%s", "Cannot find web_root directory, exiting\n");
		exit(1);
	}

	setupLogger();

	// setup communication to cortex
	bool ok = CommandDispatcher::getInstance().setup();
	if (!ok)
		printf("Initialization failed. No access to Walters cortex.");

	// initialize Execution controller
	TrajectoryExecution::getInstance().setup();

	printf("webserver running on port %s\n", s_http_port);

	while (true) {
		TrajectoryExecution::getInstance().loop();
		mg_mgr_poll(&mgr, 10); // check every 10ms for incoming requests
	}
	mg_mgr_free(&mgr);

	return 0;
}

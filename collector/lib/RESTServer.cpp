#include <cstring>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <vector>

#include <json/json.h>

extern "C" {
    #include <microhttpd.h>
}

#include "ErrorMessage.h"
#include "Handler.h"
#include "RESTServer.h"
#include "Request.h"
#include "Response.h"
#include "Router.h"

namespace collector {

static int 
answer_to_connection(void *cls, struct MHD_Connection *connection, 
                     const char *url, 
                     const char *method, const char *version, 
                     const char *upload_data, 
                     size_t *upload_data_size, void **con_cls)
{
    RESTServer *server = (RESTServer *) cls;
    return server->handleConnection(cls, connection, url, method, version,
        upload_data, upload_data_size, con_cls);
}

static void
request_completed(void *cls, struct MHD_Connection *connection,
                  void **con_cls, enum MHD_RequestTerminationCode toe)
{
    RESTServer *server = (RESTServer *) cls;
    server->finishConnection(cls, connection, con_cls, toe);
}

int
RESTServer::sendResponse(struct MHD_Connection *connection,
                         unsigned int statusCode,
                         const std::string &contentType,
                         const std::string &message)
{
    struct MHD_Response *response = MHD_create_response_from_buffer(
        message.length(), (void *) message.c_str(), MHD_RESPMEM_MUST_COPY);
    MHD_add_response_header(response, MHD_HTTP_HEADER_CONTENT_TYPE, contentType.c_str());
    int ret = MHD_queue_response(connection, statusCode, response);
    MHD_destroy_response(response);
    return ret;
}

int
RESTServer::sendJSON(struct MHD_Connection *connection,
                     unsigned int statusCode, const Json::Value &message)
{
    using std::stringstream;

    Json::StyledStreamWriter writer;
    stringstream out;
    writer.write(out, message);
    const std::string document = out.str();
    return sendResponse(connection, statusCode, "application/json", document);
}

int 
RESTServer::handleConnection(void *cls, struct MHD_Connection *connection, 
                             const char *url, 
                             const char *method, const char *version, 
                             const char *upload_data, 
                             size_t *upload_data_size, void **con_cls)
{
    using namespace std;

    if (NULL == *con_cls) {
        Request *request = new Request;
        *con_cls = request;
        return MHD_YES;
    }

    if (isPost(method)) {
        return handlePost(cls, connection, url, method, version, upload_data, upload_data_size, con_cls);
    } else if (isGet(method)) {
        return handleGet(cls, connection, url, method, version, upload_data, upload_data_size, con_cls);
    } else {
        // Unexpected method
        ErrorMessage response(MHD_HTTP_METHOD_NOT_ALLOWED, "Unknown method", "The specified method isn't exist.");
        return sendJSON(connection, MHD_HTTP_METHOD_NOT_ALLOWED, response.toJSON());
    }
}

bool
RESTServer::isGet(const char *method)
{
    return (0 == strcmp(method, MHD_HTTP_METHOD_GET));
}

bool
RESTServer::isPost(const char *method)
{
    return (0 == strcmp(method, MHD_HTTP_METHOD_POST));
}

int 
RESTServer::handleGet(void *cls, struct MHD_Connection *connection, 
                      const char *url, 
                      const char *method, const char *version, 
                      const char *upload_data, 
                      size_t *upload_data_size, void **con_cls)
{
    using namespace std;

    vector<string> params;
    Handler *handler = router->lookupGetHandler(url, params);

    if (NULL == handler) {
        // Unexpected endpoint
        ErrorMessage response(MHD_HTTP_NOT_FOUND, "Unknown endpoint", "The specified URL doesn't exist.");
        return sendJSON(connection, MHD_HTTP_NOT_FOUND, response.toJSON());
    }

    Request *request = (Request *) *con_cls;
    request->params = params;
    Response response;
    handler->handleRequest(request, response);

    return sendJSON(connection, response.statusCode, response.body);
}

int 
RESTServer::handlePost(void *cls, struct MHD_Connection *connection, 
                             const char *url, 
                             const char *method, const char *version, 
                             const char *upload_data, 
                             size_t *upload_data_size, void **con_cls)
{
    using namespace std;

    Request *request = (Request *) *con_cls;

    if (*upload_data_size != 0) {
        request->body.append(upload_data, *upload_data_size);
        *upload_data_size = 0;
        return MHD_YES;
    }

    Handler *handler = router->lookupPostHandler(url);

    if (NULL == handler) {
        // Unexpected endpoint
        ErrorMessage response(MHD_HTTP_NOT_FOUND, "Unknown endpoint", "The specified URL doesn't exist.");
        return sendJSON(connection, MHD_HTTP_NOT_FOUND, response.toJSON());
    }

    // Missing Content-Length header
    const char *param = MHD_lookup_connection_value(connection, MHD_HEADER_KIND,
        MHD_HTTP_HEADER_CONTENT_LENGTH);
    if (param == NULL) {
        cout << "Missing Content-Length" << endl;
        return sendResponse(connection, MHD_HTTP_BAD_REQUEST, "text/html", "Bad Request");
    }

    // Content-Length and length of the HTTP body don't match
    char *endptr = NULL;
    unsigned long tmp = strtoul(param, &endptr, 10);
    if (*endptr != '\0') {
        cout << "Content-Length isn't a valid number" << endl;
        return sendResponse(connection, MHD_HTTP_BAD_REQUEST, "text/html", "Bad Request");
    }
    if (tmp > this->maxContentLengthBytes) {
        cout << "Content-Length is too big (> " << maxContentLengthBytes << ")" << endl;
        return sendResponse(connection, MHD_HTTP_BAD_REQUEST, "text/html", "Bad Request");
    }

    const size_t contentLength = (size_t) tmp;
    if (request->body.length() != contentLength) {
        cout << "Content-Length doesn't match size of message body" << endl;
        return sendResponse(connection, MHD_HTTP_BAD_REQUEST, "text/html", "Bad Request");
    }

    Response response;
    handler->handleRequest(request, response);

    return sendJSON(connection, response.statusCode, response.body);
}

void
RESTServer::finishConnection(void *cls, struct MHD_Connection *connection,
                  void **con_cls, enum MHD_RequestTerminationCode toe)
{
    Request *request = (Request *) *con_cls;
    if (NULL == request) {
        return;
    }
    *con_cls = NULL;
}

RESTServer::RESTServer(uint16_t portNumber, unsigned long maxContentLengthKB,
        unsigned long maxConns, unsigned long maxConnsPerIP, unsigned long timeout,
        const Router *restRouter)
    : port(portNumber),
      maxContentLengthBytes(maxContentLengthKB * 1024),
      connectionLimit(maxConns),
      connectionLimitPerIP(maxConnsPerIP),
      connectionTimeoutSeconds(timeout),
      router(restRouter)
{
}

RESTServer::~RESTServer() {
}

bool
RESTServer::start() {
    using namespace std;

    cout << "Starting REST Server with the following parameters: connectionLimit="
         << this->connectionLimit
         << ", connectionLimitPerIP="
         << this->connectionLimitPerIP
         << ", connectionTimeoutSeconds="
         << this->connectionTimeoutSeconds
         << endl;

    daemon = MHD_start_daemon(MHD_USE_SELECT_INTERNALLY, port,
        NULL, NULL,
        &answer_to_connection, this,
        MHD_OPTION_NOTIFY_COMPLETED, &request_completed, NULL,
        MHD_OPTION_CONNECTION_LIMIT, this->connectionLimit,
        MHD_OPTION_PER_IP_CONNECTION_LIMIT, this->connectionLimitPerIP,
        MHD_OPTION_CONNECTION_TIMEOUT, this->connectionTimeoutSeconds,
        MHD_OPTION_END);
    if (NULL == daemon) {
        return false;
    }
    cout << "Server running" << endl;
    return true;
}

void
RESTServer::stop() {
    using namespace std;
    MHD_stop_daemon(daemon);
    daemon = NULL;
    cout << "Server stopped" << endl;
}

}   /* namespace collector */


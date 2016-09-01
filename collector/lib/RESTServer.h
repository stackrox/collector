#ifndef _RESTSERVER_H_
#define _RESTSERVER_H_

#include <string>

#include <json/json.h>

#include <Router.h>

extern "C" {
    #include <microhttpd.h>
}

namespace collector {

class RESTServer {
    public:
    RESTServer(
        uint16_t port,
        unsigned long maxContentLengthKB,
        unsigned long connectionLimit,
        unsigned long connectionLimitPerIP,
        unsigned long connectionTimeoutSeconds,
        const Router *router
        );

    ~RESTServer();
    bool start();
    void stop();

    int handleConnection(void *cls, struct MHD_Connection *connection,
                         const char *url, const char *method,
                         const char *version, const char *upload_data,
                         size_t *upload_data_size, void **con_cls);

    void finishConnection(void *cls, struct MHD_Connection *connection,
                          void **con_cls, enum MHD_RequestTerminationCode toe);

    private:
    uint16_t port;
    const unsigned long maxContentLengthBytes;
    const unsigned long connectionLimit;
    const unsigned long connectionLimitPerIP;
    const unsigned long connectionTimeoutSeconds;
    struct MHD_Daemon *daemon;
    const Router *router;

    int sendResponse(struct MHD_Connection *connection,
                     unsigned int statusCode, const std::string &contentType,
                     const std::string &message);

    int sendJSON(struct MHD_Connection *connection,
                     unsigned int statusCode, const Json::Value &message);

    int handleGet(void *cls, struct MHD_Connection *connection,
                  const char *url, const char *method,
                  const char *version, const char *upload_data,
                  size_t *upload_data_size, void **con_cls);

    int handlePost(void *cls, struct MHD_Connection *connection,
                   const char *url, const char *method,
                   const char *version, const char *upload_data,
                   size_t *upload_data_size, void **con_cls);

    bool isGet(const char *method);
    bool isPost(const char *method);
};

}   /* namespace collector */

#endif  /* _RESTSERVER_H_ */


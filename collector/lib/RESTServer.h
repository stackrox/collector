/** collector

A full notice with attributions is provided along with this source code.

This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License version 2 as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with this program; if not, write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

* In addition, as a special exception, the copyright holders give
* permission to link the code of portions of this program with the
* OpenSSL library under certain conditions as described in each
* individual source file, and distribute linked combinations
* including the two.
* You must obey the GNU General Public License in all respects
* for all of the code used other than OpenSSL.  If you modify
* file(s) with this exception, you may extend this exception to your
* version of the file(s), but you are not obligated to do so.  If you
* do not wish to do so, delete this exception statement from your
* version.
*/

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


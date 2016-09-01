#ifndef _ERROR_MESSAGE_H_
#define _ERROR_MESSAGE_H_

#include <json/json.h>

namespace collector {

class ErrorMessage {
    private:
    unsigned int code;
    std::string message;
    std::string description;

    public:
    ErrorMessage(unsigned int code, const std::string message, const std::string description);
    ~ErrorMessage();
    Json::Value toJSON();
};

}   /* namespace collector */

#endif  /* _ERROR_MESSAGE_H_ */


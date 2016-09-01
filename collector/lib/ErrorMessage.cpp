#include "ErrorMessage.h"

#include <json/json.h>

namespace collector {

ErrorMessage::ErrorMessage(unsigned int statusCode, const std::string errorMessage, const std::string detailedDescription)
    : code(statusCode), message(errorMessage), description(detailedDescription)
{
}

ErrorMessage::~ErrorMessage()
{
}

Json::Value
ErrorMessage::toJSON()
{
    Json::Value status(Json::objectValue);
    status["code"] = this->code;
    status["message"] = this->message;
    status["description"] = this->description;
    return status;
}

}   /* namespace collector */


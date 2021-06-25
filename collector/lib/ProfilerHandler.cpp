#include "ProfilerHandler.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include <json/json.h>
#include <sys/stat.h>

#include "Logging.h"
#include "Profiler.h"
#include "Utility.h"

namespace collector {

const std::string ProfilerHandler::kCPUProfileFilename = "/module/cpu_profile";
const std::string ProfilerHandler::kBaseRoute = "/profile";
const std::string ProfilerHandler::kCPURoute = kBaseRoute + "/cpu";
const std::string ProfilerHandler::kHeapRoute = kBaseRoute + "/heap";

bool ProfilerHandler::ServerError(struct mg_connection* conn, const char* err) {
  return mg_send_http_error(conn, 500, err) >= 0;
}

bool ProfilerHandler::ClientError(struct mg_connection* conn, const char* err) {
  return mg_send_http_error(conn, 400, err) >= 0;
}

bool ProfilerHandler::SendCPUProfile(struct mg_connection* conn) {
  WITH_LOCK(mutex_) {
    if (cpu_profile_length_ == 0) {
      return mg_send_http_ok(conn, "text/plain", 0) >= 0;
    }
    mg_send_mime_file(conn, kCPUProfileFilename.c_str(), "application/octet-stream");
  }
  return true;
}

bool ProfilerHandler::SendHeapProfile(struct mg_connection* conn) {
  WITH_LOCK(mutex_) {
    if (heap_profile_length_ == 0) {
      return mg_send_http_ok(conn, "text/plain", 0) >= 0;
    }
    if (mg_send_http_ok(conn, "application/octet-stream", heap_profile_length_) < 0) {
      return false;
    }
    if (mg_write(conn, heap_profile_.get(), heap_profile_length_) != heap_profile_length_) {
      return false;
    }
  }
  return true;
}

bool ProfilerHandler::SendStatus(struct mg_connection* conn) {
  Json::Value resp(Json::objectValue);
  WITH_LOCK(mutex_) {
    resp["supports_cpu"] = Profiler::IsCPUProfilerSupported();
    resp["supports_heap"] = Profiler::IsHeapProfilerSupported();
    resp["cpu"] = Profiler::IsCPUProfilerEnabled() ? "on" : (cpu_profile_length_ > 0 ? "off" : "empty");
    resp["heap"] = Profiler::IsHeapProfilerEnabled() ? "on" : (heap_profile_length_ > 0 ? "off" : "empty");
  }
  std::string json_body = resp.toStyledString();
  if (mg_send_http_ok(conn, "application/json", json_body.length()) < 0) {
    return false;
  }
  return mg_write(conn, json_body.c_str(), json_body.length()) >= 0;
}

bool ProfilerHandler::HandleCPURoute(struct mg_connection* conn, const std::string& post_data) {
  WITH_LOCK(mutex_) {
    if (post_data == "on") {
      if (!Profiler::IsCPUProfilerEnabled()) {
        if (!Profiler::StartCPUProfiler(kCPUProfileFilename)) {
          return ServerError(conn, "failed starting cpu profiler");
        }
        CLOG(INFO) << "started cpu profiler";
      }
    } else if (post_data == "off") {
      if (Profiler::IsCPUProfilerEnabled()) {
        Profiler::StopCPUProfiler();
        struct stat sdata;
        if (stat(kCPUProfileFilename.c_str(), &sdata) == 0) {
          cpu_profile_length_ = sdata.st_size;
        }
        CLOG(INFO) << "stopped cpu profiler, bytes=" << cpu_profile_length_;
      }
    } else if (post_data == "empty") {
      if (cpu_profile_length_ != 0) {
        cpu_profile_length_ = 0;
        if (std::remove(kCPUProfileFilename.c_str()) != 0) {
          CLOG(INFO) << "remove failed: " << StrError();
          return ServerError(conn, "failure while deleting cpu profile");
        }
        CLOG(INFO) << "cleared cpu profile";
      }
    } else {
      return ClientError(conn, "invalid post data");
    }
  }
  return mg_send_http_ok(conn, "text/plain", 0) >= 0;
}

bool ProfilerHandler::HandleHeapRoute(struct mg_connection* conn, const std::string& post_data) {
  WITH_LOCK(mutex_) {
    if (post_data == "on") {
      if (!Profiler::IsHeapProfilerEnabled()) {
        Profiler::StartHeapProfiler();
        CLOG(INFO) << "started heap profiler";
      }
    } else if (post_data == "off") {
      if (Profiler::IsHeapProfilerEnabled()) {
        heap_profile_ = Profiler::AllocAndGetHeapProfile();
        if (heap_profile_ == nullptr) {
          return ServerError(conn, "failed get heap profile");
        }
        heap_profile_length_ = strlen(heap_profile_.get());
        Profiler::StopHeapProfiler();
        CLOG(INFO) << "stopped heap profiler, bytes=" << heap_profile_length_;
      }
    } else if (post_data == "empty") {
      if (heap_profile_length_ != 0) {
        heap_profile_length_ = 0;
        heap_profile_.reset();
        CLOG(INFO) << "cleared heap profile";
      }
    } else {
      return ClientError(conn, "invalid post data");
    }
  }
  return mg_send_http_ok(conn, "text/plain", 0) >= 0;
}

bool ProfilerHandler::handlePost(CivetServer* server, struct mg_connection* conn) {
  if (!Profiler::IsCPUProfilerSupported()) {
    return ServerError(conn, "not supported");
  }
  const mg_request_info* req_info = mg_get_request_info(conn);
  if (req_info == nullptr) {
    return ServerError(conn, "unable to read request");
  }
  std::string uri(req_info->local_uri);
  std::string post_data(server->getPostData(conn));
  if (uri == kCPURoute) {
    return HandleCPURoute(conn, post_data);
  } else if (uri == kHeapRoute) {
    return HandleHeapRoute(conn, post_data);
  }
  return ClientError(conn, "unknown route");
}

bool ProfilerHandler::handleGet(CivetServer* server, struct mg_connection* conn) {
  const mg_request_info* req_info = mg_get_request_info(conn);
  if (req_info == nullptr) {
    return ServerError(conn, "unable to read request");
  }
  std::string uri = req_info->local_uri;
  if (uri == kBaseRoute) {
    return SendStatus(conn);
  } else if (uri == kHeapRoute) {
    return SendHeapProfile((conn));
  } else if (uri == kCPURoute) {
    return SendCPUProfile(conn);
  }
  return ClientError(conn, "unknown route");
}

}  // namespace collector

#include "ProfilerHandler.h"

#include <algorithm>
#include <cstring>
#include <memory>

#include <json/json.h>
#include <sys/stat.h>

#include "Logging.h"
#include "Profiler.h"

namespace collector {

const char* kCPUProfileFilename = "/module/cpu_profile";

bool handleHeapStart(CivetServer* server, struct mg_connection* conn) {
  if (!Profiler::IsHeapProfilerSupported()) {
    return mg_send_http_error(conn, 500, "unsupported") >= 0;
  }
  if (Profiler::IsHeapProfilerEnabled()) {
    return mg_send_http_error(conn, 400, "heap profiler already started ") >= 0;
  }
  Profiler::StartHeapProfiler();
  const mg_request_info* req_info = mg_get_request_info(conn);
  CLOG(INFO) << "started heap profiler - " << req_info->remote_addr;
  return mg_send_http_ok(conn, "text/plain", 0) >= 0;
}

bool handleHeapStop(CivetServer* server, struct mg_connection* conn) {
  if (!Profiler::IsHeapProfilerSupported()) {
    return mg_send_http_error(conn, 500, "unsupported") >= 0;
  }
  if (!Profiler::IsHeapProfilerEnabled()) {
    return mg_send_http_error(conn, 400, "heap profiler not enabled") >= 0;
  }
  const char* heap_profile = Profiler::AllocAndGetHeapProfile();
  if (!heap_profile) {
    return mg_send_http_error(conn, 500, "failed to get heap profile") >= 0;
  }
  int heap_profile_len = strlen(heap_profile);
  if (mg_send_http_ok(conn, "application/octet-stream", heap_profile_len) < 0) {
    return false;
  }
  mg_write(conn, heap_profile, heap_profile_len);
  free((void*)heap_profile);
  Profiler::StopHeapProfiler();
  CLOG(INFO) << "stopped heap profiler - sent " << heap_profile_len << " bytes";
  return true;
}

bool handleCPUStart(CivetServer* server, struct mg_connection* conn) {
  if (!Profiler::IsCPUProfilerSupported()) {
    return mg_send_http_error(conn, 500, "unsupported") >= 0;
  }
  if (Profiler::IsCPUProfilerEnabled()) {
    return mg_send_http_error(conn, 400, "cpu profiler already enabled") >= 0;
  }
  if (!Profiler::StartCPUProfiler(kCPUProfileFilename)) {
    return mg_send_http_error(conn, 500, "failed to start cpu profiler") >= 0;
  }
  const mg_request_info* req_info = mg_get_request_info(conn);
  CLOG(INFO) << "started cpu profiler - " << req_info->remote_addr;
  return mg_send_http_ok(conn, "text/plain", 0) >= 0;
}

bool handleCPUStop(CivetServer* server, struct mg_connection* conn) {
  if (!Profiler::IsCPUProfilerSupported()) {
    return mg_send_http_error(conn, 500, "unsupported") >= 0;
  }
  if (!Profiler::IsCPUProfilerEnabled()) {
    return mg_send_http_error(conn, 400, "cpu profiler not enabled") >= 0;
  }
  Profiler::StopCPUProfiler();
  struct stat sdata;
  int cpu_profile_len = -1;
  if (stat(kCPUProfileFilename, &sdata) == 0) {
    cpu_profile_len = sdata.st_size;
  }
  mg_send_file(conn, kCPUProfileFilename);
  CLOG(INFO) << "stopped cpu profiler - sent " << cpu_profile_len << " bytes";
  return true;
}

bool handleStatus(CivetServer* server, struct mg_connection* conn) {
  Json::Value resp(Json::objectValue);
  resp["cpuProfilerSupported"] = Profiler::IsCPUProfilerSupported();
  resp["cpuProfilerEnabled"] = Profiler::IsCPUProfilerEnabled();
  resp["heapProfilerSupported"] = Profiler::IsHeapProfilerSupported();
  resp["heapProfilerEnabled"] = Profiler::IsHeapProfilerEnabled();
  std::string json_body = resp.toStyledString();
  if (mg_send_http_ok(conn, "application/json", json_body.length()) < 0) {
    return false;
  }
  return mg_write(conn, json_body.c_str(), json_body.length()) >= 0;
}

bool ProfilerHandler::handleGet(CivetServer* server, struct mg_connection* conn) {
  std::string cpu_value, heap_value;
  server->getParam(conn, "cpu", cpu_value);
  server->getParam(conn, "heap", heap_value);

  if (cpu_value.empty() && heap_value.empty()) {
    return handleStatus(server, conn);
  }
  if (heap_value == "start" && cpu_value.empty()) {
    return handleHeapStart(server, conn);
  }
  if (heap_value == "stop" && cpu_value.empty()) {
    return handleHeapStop(server, conn);
  }
  if (cpu_value == "start" && heap_value.empty()) {
    return handleCPUStart(server, conn);
  }
  if (cpu_value == "stop" && heap_value.empty()) {
    return handleCPUStop(server, conn);
  }
  return mg_send_http_error(conn, 400, "bad request") >= 0;
}

}  // namespace collector

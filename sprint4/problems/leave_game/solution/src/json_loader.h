#pragma once
#include <iostream>
#include <fstream>
#include <filesystem>

#include "model.h"

namespace json = boost::json;
using namespace model;
namespace json_loader {

Game LoadGame(const std::filesystem::path& json_path);

std::string GetLogServerStart(const std::string timestamp,
                            const std::string srv_address,
                            const int port);
std::string GetLogServerStop(const std::string timestamp,
                            const int return_code,
                            const std::string exception_what);
std::string GetLogRequest(const std::string timestamp,
                            const std::string client_address,
                            const std::string uri,
                            const std::string http_method);
std::string GetLogResponse(const std::string timestamp,
                            const std::string client_address,
                            const int response_time_msec,
                            const int response_code,
                            const std::string content_type);
std::string GetLogError(const std::string timestamp,
                            const int error_code,
                            const std::string error_text,
                            const std::string where);
}  // namespace json_loader

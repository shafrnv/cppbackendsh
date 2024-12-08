#pragma once
#include <iostream>
#include <fstream>
#include <filesystem>

#include "model.h"

namespace json = boost::json;
using namespace model;
namespace json_loader {

Game LoadGame(const std::filesystem::path& json_path);

std::string GetLogServerStart(std::string_view timestamp,
                            std::string_view srv_address,
                            int port);
std::string GetLogServerStop(std::string_view timestamp,
                            int return_code,
                            std::string_view exception_what);
std::string GetLogRequest(std::string_view timestamp,
                            std::string_view client_address,
                            std::string_view uri,
                            std::string_view http_method);
std::string GetLogResponse(std::string_view timestamp,
                                std::string_view client_address,
                            int response_time_msec,
                            int response_code,
                            std::string_view content_type);
std::string GetLogError(std::string_view timestamp,
                            int error_code,
                            std::string_view error_text,
                            std::string_view where);
}  // namespace json_loader

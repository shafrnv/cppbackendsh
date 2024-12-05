#pragma once
/*
 * Чтобы залогировать получение запроса и формирование ответа, используем паттерн "Декоратор".
 * Заменим обработчик запросов RequestHandler на объект с таким же интерфейсом,
 * который будет вызывать методы исходного обработчика.
 * При этом он добавляет дополнительную функциональность в виде логирования.
 */
#define BOOST_BEAST_USE_STD_STRING_VIEW

#include "json_loader.h"
#include "request_handler.h"

#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>

#include <string>
#include <string_view>
#include <variant>

namespace logging_handler {
using namespace http_handler;
namespace http = boost::beast::http;
namespace net = boost::asio;
namespace logging = boost::log;

const std::string GetTimeStampString();

void LogStartServer(const net::ip::tcp::endpoint &endpoint);
void LogStopServer(const int return_code, const std::string exception_what);
void LogNetworkError(const int error_code,
                     std::string error_text,
                     std::string where);

void LogFormatter(logging::record_view const &rec, logging::formatting_ostream &strm);

template <class MainRequestHandler>
class LoggingRequestHandler {
    static void LogRequest(const StringRequest &request,
                           const std::string client_address) {
        BOOST_LOG_TRIVIAL(info) << json_loader::GetLogRequest(GetTimeStampString(),
                                                              client_address,
                                                              std::string(request.target()),
                                                              std::string(request.method_string()));
    }
    static void LogResponse(const std::string client_address, const int time_msec,
                            const int response_code, const std::string_view content_type) {
        BOOST_LOG_TRIVIAL(info) << json_loader::GetLogResponse(GetTimeStampString(),
                                                               client_address,
                                                               time_msec,
                                                               response_code,
                                                               std::string(content_type));
    }

public:
    explicit LoggingRequestHandler(MainRequestHandler &handler) : decorated_(handler) {}
    LoggingRequestHandler(const MainRequestHandler &) = delete;
    LoggingRequestHandler &operator=(const MainRequestHandler &) = delete;

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>> &&req, Send &&send,
                    const net::ip::tcp::endpoint &client_endpoint) {
        using namespace boost::posix_time;

        std::string client_address = client_endpoint.address().to_string();
        LogRequest(req, client_address);

        ptime start_time = microsec_clock::universal_time();
        std::variant<StringResponse, FileResponse> answer = decorated_(std::move(req));
        time_duration duration = microsec_clock::universal_time() - start_time;

        if (std::holds_alternative<StringResponse>(answer)) {
            StringResponse response(std::move(std::get<StringResponse>(answer)));
            LogResponse(client_address, static_cast<int>(duration.total_milliseconds()),
                        response.result_int(), response[http::field::content_type]);
            send(std::move(response));
        } else if (std::holds_alternative<FileResponse>(answer)) {
            http::response<http::file_body> response(std::move(std::get<FileResponse>(answer)));
            LogResponse(client_address, static_cast<int>(duration.total_milliseconds()),
                        response.result_int(), response[http::field::content_type]);
            send(std::move(response));
        }
    }

private:
    MainRequestHandler &decorated_;
};

} // namespace logging_handler
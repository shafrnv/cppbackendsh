#include "http_server.h"

#include <boost/asio/dispatch.hpp>
#include <iostream>

namespace http_server {
    void SessionBase::Run() {
    // Вызываем метод Read, используя executor объекта stream_.
    // Таким образом вся работа со stream_ будет выполняться, используя его executor
    net::dispatch(stream_.get_executor(),
                  beast::bind_front_handler(&SessionBase::Read, GetSharedThis()));
    }
    void ReportError(beast::error_code ec, std::string_view what) {
        std::cerr << what << ": "sv << ec.message() << std::endl;
    }
}  // namespace http_server

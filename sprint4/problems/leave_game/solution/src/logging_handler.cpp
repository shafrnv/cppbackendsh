#include "logging_handler.h"

namespace logging_handler {
    using namespace std::literals;

    /* Функция принимает параметр типа record_view, содержащий полную информацию о сообщении,
     * и параметр типа formatting_ostream — поток, в который нужно вывести итоговый текст. */
    void LogFormatter(logging::record_view const &rec, logging::formatting_ostream &strm) {
        strm << rec[logging::expressions::smessage];
    }

    const std::string GetTimeStampString() {
        using namespace boost::posix_time;

        const size_t time_str_length = 26; // длина строки времени в ISO
        std::string timestamp = to_iso_extended_string(microsec_clock::universal_time());
        // дробная часть секунд не выводится, если равна нулю, исправим это
        if (timestamp.length() < time_str_length) {
            timestamp += ".000000"; // для microsec_clock
        }
        return timestamp;
    }

    void LogStartServer(const net::ip::tcp::endpoint& endpoint) {
        BOOST_LOG_TRIVIAL(info) << json_loader::GetLogServerStart(GetTimeStampString(),
                                                    endpoint.address().to_string(),
                                                    static_cast<int>(endpoint.port()));
    }

    void LogStopServer(const int return_code, const std::string exception_what) {
        BOOST_LOG_TRIVIAL(info) << json_loader::GetLogServerStop(GetTimeStampString(),
                                                    return_code,
                                                    exception_what);
    }

    void LogNetworkError(const int error_code,
                         const std::string error_text,
                         const std::string where) {
        BOOST_LOG_TRIVIAL(info) << json_loader::GetLogError(GetTimeStampString(),
                                                    error_code,
                                                    error_text,
                                                    where);
    }
} // namespace logging_handler
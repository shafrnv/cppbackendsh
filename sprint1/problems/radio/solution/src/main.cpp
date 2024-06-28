#include "audio.h"
#include <boost/asio.hpp>
#include <iostream>
#include <string>
namespace net = boost::asio;
using net::ip::udp; // рация на основе udp

using namespace std::literals;

void StartServer(uint16_t port){
    Player player(ma_format_u8, 1);
    static const size_t max_buffer_size = 1024;
    try {
        boost::asio::io_context io_context;

        udp::socket socket(io_context, udp::endpoint(udp::v4(), port));

        // Запускаем сервер в цикле, чтобы можно было работать со многими клиентами
        for (;;) {
            // Создаём буфер достаточного размера, чтобы вместить датаграмму.
            Recorder::RecordingResult recv_buf;
            udp::endpoint remote_endpoint;

            // Получаем не только данные, но и endpoint клиента
            auto size = socket.receive_from(boost::asio::buffer(recv_buf.data), remote_endpoint);
            player.PlayBuffer(recv_buf.data.data(), size/player.GetFrameSize(),1.5s);
        }
    } catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
    }
}
void StartClient(uint16_t port){
    Recorder recorder(ma_format_u8, 1);
    try {
        net::io_context io_context;

        
        udp::socket socket(io_context, udp::v4());
        boost::system::error_code ec;

        std::string ip_to_connect;
        std::cin >> ip_to_connect;
        auto endpoint = udp::endpoint(net::ip::make_address(ip_to_connect, ec), port);
        //записал звук
        auto rec_result = recorder.Record(65000, 1.5s);
        //отправил на сервёер
        socket.send_to(net::buffer(rec_result.data,rec_result.frames * recorder.GetFrameSize()), endpoint);

    } catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
    }
}
int main(int argc, const char** argv) { 

    if (argc != 3) {
        std::cout << "Usage: "sv << argv[0] << " <client|server> <port>" << std::endl;
        return 1;
    }
    
    std::string mode = argv[1];
    int port = std::stoi(argv[2]);

    if (mode == "client") {
        StartClient(port);
    } else if (mode == "server") {
        StartServer(port);
    } else {
        std::cerr << "Invalid mode. Use 'client' or 'server'." << std::endl;
        return 1;
    }
    return 0;
}

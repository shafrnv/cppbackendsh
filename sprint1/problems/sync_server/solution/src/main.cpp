#ifdef WIN32
#include <sdkddkver.h>
#endif
// boost.beast будет использовать std::string_view вместо boost::string_view
#define BOOST_BEAST_USE_STD_STRING_VIEW

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/write.hpp> 
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <iostream>
#include <thread>
#include <optional>

namespace net = boost::asio;
using tcp = net::ip::tcp;
using namespace std::literals;
namespace beast = boost::beast;
namespace http = beast::http;
// Запрос, тело которого представлено в виде строки
using StringRequest = http::request<http::string_body>;
// Ответ, тело которого представлено в виде строки
using StringResponse = http::response<http::string_body>;

struct ContentType {
    ContentType() = delete;
    constexpr static std::string_view TEXt_HTML = "text/html"sv;
    // При необходимости внутрь ContentType можно добавить и другие типы контента
};

StringResponse MakeStringResponse(http::status status, std::string_view body, unsigned http_version, 
bool keep_alive, std::string_view content_type =ContentType::TEXt_HTML){
    StringResponse response(status, http_version);
    response.set(http::field::content_type, content_type);
    response.body() = body;
    response.content_length(body.size());
    response.keep_alive(keep_alive);
    return response;
}

StringResponse HandleRequest(StringRequest&& req){

    const auto text_response = [&req](http::status status, std::string_view text){
        return MakeStringResponse(status, text, req.version(), req.keep_alive());
    };
    if (req.method() != http::verb::get && req.method() != http::verb::head) {
        return text_response(http::status::method_not_allowed, "Invalid method"sv);;
    } else{
        return text_response(http::status::ok, "Hello, "s.append(req.target().substr(1)));
    }
}

/*Если клиент отправил очередной запрос, она вернёт StringRequest, а если клиент завершил соединение, 
вернёт std::nullopt. О прочих ошибках функция будет сигнализировать исключениями.
 Роль потока в вашем случае будет играть сокет, а роль сообщения — StringRequest. Для хранения считываемых данных функция требует буфер. 
 В качестве буфера передайте ссылку на объект класса boost::beast::flat_buffer. 
 Этот буфер способен динамически изменять свой размер и состоит из двух смежных областей памяти: 
 область для чтения и область для записи данных.*/
std::optional<StringRequest> ReadRequest(tcp::socket& socket, beast::flat_buffer& buffer){
    beast::error_code ec;
    StringRequest req;
    // Считываем из socket запрос req, используя buffer для хранения данных.
    // В ec функция запишет код ошибки.
    http::read(socket, buffer, req, ec);

    if (ec == http::error::end_of_stream) {
        return std::nullopt;
    }
    if (ec) {
        throw std::runtime_error("Failed to read request: "s.append(ec.message()));
    }
    return req;
}

//Чтобы увидеть содержимое запроса, полученного от клиента, создайте функцию DumpRequest
void DumpRequest(const StringRequest& req) {
    std::cout << req.method_string() << ' ' << req.target() << std::endl;
    // Выводим заголовки запроса
    for (const auto& header :req) {
        std::cout << " "sv << header.name_string() << ": "sv<< header.value() << std::endl;
    }
}
template <typename RequestHandler>
void HandleConnection(tcp::socket& socket, RequestHandler&& handle_request){
    /*Внутри HandleConnection примените функцию ReadRequest, чтобы в цикле считывать и обрабатывать запросы клиента. 
    Перед тем, как выйти из функции вызовите метод tcp::socket::shutdown, 
    чтобы запретить последующую отправку данных в этот сокет — HTTP-сессия завершена.*/
    try
    {
        // Буфер для чтения данных в рамках текущей сессии.
        beast::flat_buffer buffer;

        // Продолжаем обработку запросов, пока клиент их отправляет
        while (auto request = ReadRequest(socket, buffer)){
            // Обрабатываем запрос и формируем ответ сервера
            /*В цикле обработки запроса выведите информацию о запросе функцией DumpRequest и отправьте ответ сервера. 
            Цикл обработки запроса продолжается до тех пор, пока ReadRequest не вернёт std::nullopt, 
            либо метод need_eof объекта http::response не вернёт true.*/
            DumpRequest(*request);
            StringResponse response = handle_request(*std::move(request));
            http::write(socket, response);
            if (response.need_eof()) {
                break;
            }
        }
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
    }
    beast::error_code ec;
    // Запрещаем дальнейшую отправку данных через сокет
    socket.shutdown(tcp::socket::shutdown_send, ec);
}
int main() {
    net::io_context ioc; // Для работы с подсистемой ввода-вывода программе понадобится контекст ввода-вывода.
    // Выведите строчку "Server has started...", когда сервер будет готов принимать подключения

    /*Чтобы приложение могло принимать подключения клиентов, создайте экземпляр класса tcp::acceptor. 
    В качестве первого параметра передайте ссылку на контекст ввода-вывода, 
    а вторым параметром — объект tcp::endpoint, хранящий адрес и порт, с которого сервер будет принимать подключения*/

    //В качестве адреса укажите 0.0.0.0 — так сервер будет прослушивать порт на всех сетевых интерфейсах этого компьютера, то есть на всех связанных с ним IP адресах.
    const auto address = net::ip::make_address("0.0.0.0"); 
    //Номер порта укажите равным 8080.
    constexpr unsigned short port = 8080;

    tcp::acceptor acceptor(ioc, {address,port});
    std::cout << "Server has started..."sv << std::endl;
    while (true) {
        //Для двустороннего обмена данными с удалённым компьютером применяется сокет. В Boost.Asio он представлен классом tcp::socket
        tcp::socket socket(ioc);
        //Вызовите метод acceptor::accept, чтобы дождаться подключения клиента к сокету, переданному в качестве параметра метода:
        acceptor.accept(socket);
        std::thread t(
            [](tcp::socket socket){
                HandleConnection(socket, HandleRequest);
            },
            std::move(socket) // Сокет нельзя скопировать, но можно переместить
        );
        // После вызова detach поток продолжит выполняться независимо от объекта t
        t.detach();
    }
}

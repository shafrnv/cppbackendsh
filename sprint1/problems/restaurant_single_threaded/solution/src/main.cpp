#ifdef WIN32
#include <sdkddkver.h>
#endif

#include <boost/asio.hpp>
#include <chrono>
#include <iostream>
#include <mutex>
#include <sstream>
#include <syncstream>
#include <unordered_map>
#include <memory> 

namespace net = boost::asio;
namespace sys = boost::system;
namespace ph = std::placeholders;
using namespace std::chrono;
using namespace std::literals;
using Timer = net::steady_timer;

class Hamburger {
public:
    [[nodiscard]] bool IsCutletRoasted() const {
        return cutlet_roasted_;
    }
    void SetCutletRoasted() {
        if (IsCutletRoasted()) {  // Котлету можно жарить только один раз
            throw std::logic_error("Cutlet has been roasted already"s);
        }
        cutlet_roasted_ = true;
    }

    [[nodiscard]] bool HasOnion() const {
        return has_onion_;
    }
    // Добавляем лук
    void AddOnion() {
        if (IsPacked()) {  // Если гамбургер упакован, класть лук в него нельзя
            throw std::logic_error("Hamburger has been packed already"s);
        }
        AssureCutletRoasted();  // Лук разрешается класть лишь после прожаривания котлеты
        has_onion_ = true;
    }

    [[nodiscard]] bool IsPacked() const {
        return is_packed_;
    }
    void Pack() {
        AssureCutletRoasted();  // Нельзя упаковывать гамбургер, если котлета не прожарена
        is_packed_ = true;
    }

private:
    // Убеждаемся, что котлета прожарена
    void AssureCutletRoasted() const {
        if (!cutlet_roasted_) {
            throw std::logic_error("Bread has not been roasted yet"s);
        }
    }

    bool cutlet_roasted_ = false;  // Обжарена ли котлета?
    bool has_onion_ = false;       // Есть ли лук?
    bool is_packed_ = false;       // Упакован ли гамбургер?
};

std::ostream& operator<<(std::ostream& os, const Hamburger& h) {
    return os << "Hamburger: "sv << (h.IsCutletRoasted() ? "roasted cutlet"sv : " raw cutlet"sv)
              << (h.HasOnion() ? ", onion"sv : ""sv)
              << (h.IsPacked() ? ", packed"sv : ", not packed"sv);
}

class Logger {
public:
    explicit Logger(std::string id)
        : id_(std::move(id)) {
    }

    void LogMessage(std::string_view message) const {
        /*Чтобы одновременный вывод в консоль из разных потоков не превращался в нечитаемое месиво, 
        воспользуемся классом std::osyncstream из C++20, который обеспечит синхронизацию вывода*/
        std::osyncstream os{std::cout};
        os << id_ << "> ["sv << duration<double>(steady_clock::now() - start_time_).count()
           << "s] "sv << message << std::endl;
    }

private:
    std::string id_;
    steady_clock::time_point start_time_{steady_clock::now()};
};

// Функция, которая будет вызвана по окончании обработки заказа
using OrderHandler = std::function<void(sys::error_code ec, int id, Hamburger* hamburger)>;

/*Класс Order унаследуйте от шаблонного класса std::enable_shared_from_this. 
В качестве параметра шаблона передайте класс Order. 
Шаблон std::enable_shared_from_this позволяет внутри методов класса-наследника безопасно получать shared_ptr, ссылающийся на текущий объект. 
Экземпляры класса Order будут использовать это, чтобы продлить время жизни, пока выполняют асинхронные операции.*/
class Order :public std::enable_shared_from_this<Order> {
public:
     Order(net::io_context& io, int id, bool with_onion, OrderHandler handler)
        : io_{io}
        , id_{id}
        , with_onion_(with_onion)
        , handler_(std::move(handler)) {
    }

    void Execute() {
        /*Чтобы заказ не разрушился раньше времени, в методе Order::Execute нужно запустить асинхронные операции, продлив на время их 
        выполнения время жизни объекту Order. Приготовление гамбургера начинается с обжаривания котлеты (метод RoastCutlet) и 
        маринования лука (метод MarinadeOnion):*/
        logger_.LogMessage("Order started"sv);
        RoastCutlet();
        if (with_onion_) {
            MarinadeOnion();
        }
    }

private:
    bool delivered_ = false;
    Hamburger hamburger_;
    bool onion_marinaded_ = false;
    net::io_context& io_;
    int id_;
    bool with_onion_;
    OrderHandler handler_;
    Logger logger_{std::to_string(id_)};
    //Чтобы обжарить котлету, добавьте внутрь класса Order поле roast_timer_ типа boost::asio::timer, настроенное на 1 секунду
    Timer roast_timer_{io_, 1s};
    Timer marinade_timer_{io_, 2s};
    void RoastCutlet() {
        logger_.LogMessage("RoastCutlet started"sv);\
        /*В методе RoastCutlet вызовите у таймера метод async_wait. 
        Чтобы продлить время жизни текущего объекта Order до окончания операции ожидания, 
        захватите в списке захвата лямбда-функции указатель shared_ptr, возвращённый методом shared_from_this(). 
        Этот указатель скопируется внутрь лямбда-функции, которая передана в метод async_wait. 
        Умный указатель увеличит счётчик ссылок на текущий экземпляр класса. 
        Order не удалится как минимум до вызова лямбда-функции.*/
        roast_timer_.async_wait([self = shared_from_this()](sys::error_code ec) {
            self->OnRoasted(ec);
        });

    }
    void OnRoasted(sys::error_code ec) {
        if (ec) {   
            logger_.LogMessage("RoastError"s + ec.what());
        }else{
            logger_.LogMessage("Cutled roasted"sv);
            hamburger_.SetCutletRoasted();
        }
        CheckReadiness(ec);
    }
    void MarinadeOnion() {
        logger_.LogMessage("MarinadeOnion started"sv);
        marinade_timer_.async_wait([self = shared_from_this()](sys::error_code ec) {
            self->OnOnionMarinaded(ec);
        });
    }

    void OnOnionMarinaded(sys::error_code ec) {
        if (ec) {
            logger_.LogMessage("Marinade onion error: "s + ec.what());
        } else {
            logger_.LogMessage("Onion has been marinaded."sv);
            onion_marinaded_ = true;
        }
        CheckReadiness(ec);
    }
    /*По окончании обеих асинхронных операций нужно проверить готовность заказа. Для этого вызовите метод CheckReadiness.*/
    void CheckReadiness(sys::error_code ec) {
        if (delivered_){
            return;
        }
        if (ec) {
            return Deliver(ec);
        }
        if (CanAddOnion()) {
            logger_.LogMessage("Add onion"sv);
            hamburger_.AddOnion();
        }
        if (IsReadyToPack()){
            Pack();
        }
    }
    void Deliver(sys::error_code ec) {
        // Защита заказа от повторной доставки
        delivered_ = true;
        // Доставляем гамбургер в случае успеха либо nullptr, если возникла ошибка
        handler_(ec, id_, ec ? nullptr : &hamburger_);
    }
    [[nodiscard]] bool CanAddOnion() const {
        // Лук можно добавить, если котлета обжарена, лук замаринован, но пока не добавлен
        return hamburger_.IsCutletRoasted() && onion_marinaded_ && !hamburger_.HasOnion();
    }
    [[nodiscard]] bool IsReadyToPack() const {
        // Если котлета обжарена и лук добавлен, как просили, гамбургер можно упаковывать
        return hamburger_.IsCutletRoasted() && (!with_onion_ || hamburger_.HasOnion());
    }
    void Pack() {
        logger_.LogMessage("Packing"sv);

        // Просто потребляем ресурсы процессора в течение 0,5 с.
        auto start = steady_clock::now();
        while (steady_clock::now() - start < 500ms) {
        }

        hamburger_.Pack();
        logger_.LogMessage("Packed"sv);

        Deliver({});
    }
};
class Restaurant {
public:
    explicit Restaurant(net::io_context& io)
        : io_(io) {
    }

    int MakeHamburger(bool with_onion, OrderHandler handler) {
        const int order_id = ++next_order_id_;
        /* Функция std::make_shared создаёт заказ в динамической памяти и возвращает умный указатель std::shared_ptr<Order>, 
        управляющий временем жизни заказа. Так как указатель не сохраняется ни в какую переменную, 
        сразу после вызова метода Execute деструктор shared_ptr уменьшит счётчик ссылок на заказ. 
        Если счётчик ссылок обнулится, заказ будет разрушен. */
        std::make_shared<Order>(io_, order_id, with_onion, std::move(handler))->Execute();
        return order_id;
    }

private:
    net::io_context& io_;
    int next_order_id_ = 0;
};



int main() {
    net::io_context io;

    Restaurant restaurant{io};

    Logger logger{"main"s};

    struct OrderResult {
        sys::error_code ec;
        Hamburger hamburger;
    };

    std::unordered_map<int, OrderResult> orders;
    auto handle_result = [&orders](sys::error_code ec, int id, Hamburger* h) {
        orders.emplace(id, OrderResult{ec, ec ? Hamburger{} : *h});
    };

    const int id1 = restaurant.MakeHamburger(false, handle_result);
    const int id2 = restaurant.MakeHamburger(true, handle_result);

    // До вызова io.run() никакие заказы не выполняются
    assert(orders.empty());
    io.run();

    // После вызова io.run() все заказы быть выполнены
    assert(orders.size() == 2u);
    {
        // Проверяем заказ без лука
        const auto& o = orders.at(id1);
        assert(!o.ec);
        assert(o.hamburger.IsCutletRoasted());
        assert(o.hamburger.IsPacked());
        assert(!o.hamburger.HasOnion());
    }
    {
        // Проверяем заказ с луком
        const auto& o = orders.at(id2);
        assert(!o.ec);
        assert(o.hamburger.IsCutletRoasted());
        assert(o.hamburger.IsPacked());
        assert(o.hamburger.HasOnion());
    }
}

#pragma once
#ifdef _WIN32
#include <sdkddkver.h>
#endif

#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/strand.hpp>
#include <memory>

#include "hotdog.h"
#include "result.h"

namespace net = boost::asio;
using namespace std::chrono;
using namespace std::literals;
using Timer = net::steady_timer;

// Функция-обработчик операции приготовления хот-дога
using HotDogHandler = std::function<void(Result<HotDog> hot_dog)>;
class Order : public std::enable_shared_from_this<Order>{
public:
    Order(net::io_context& io , int id, Store& store, GasCooker& gas_cooker, HotDogHandler handler)
    : io_{io}, id_{id}, store_{store}, gas_cooker_{gas_cooker}, handler_{std::move(handler)}
    {}

    void Execute() {
        /*Чтобы заказ не разрушился раньше времени, в методе Order::Execute нужно запустить асинхронные операции, продлив на время их 
        выполнения время жизни объекту Order.:*/
        sausage_ = store_.GetSausage();
        bread_ = store_.GetBread();
        FrySausage();
        BakeBread();
    }
private:

    void FrySausage() {
        sausage_->StartFry(gas_cooker_, [self = shared_from_this()]() {
            boost::asio::post(self->strand_, [self]() {
                self->frying_timer_.expires_after(std::chrono::milliseconds(1500));
                self->frying_timer_.async_wait([self](const boost::system::error_code& ec) {
                    self->OnFried();
                });
            });
        });
    }

    void OnFried() {
        sausage_->StopFry();
        CheckReadiness();
    }

    void BakeBread() {
        bread_->StartBaking(gas_cooker_, [self = shared_from_this()]() {
            boost::asio::post(self->strand_, [self]() {
                self->baking_timer_.expires_after(std::chrono::milliseconds(1000));
                self->baking_timer_.async_wait([self](const boost::system::error_code& ec) {
                    self->OnBaked();
                });
            });
        });
    }

    void OnBaked() {
        bread_->StopBaking();
        CheckReadiness();
    }

    void CheckReadiness() {
        if (!sausage_->IsCooked() || !bread_->IsCooked()) {
            return;
        }
        handler_(HotDog{id_, sausage_, bread_});
    }
    net::io_context& io_;
    net::strand<net::io_context::executor_type> strand_{net::make_strand(io_)};
    int id_;
    Store& store_;
    GasCooker& gas_cooker_;
    HotDogHandler handler_;
    std::shared_ptr<Sausage> sausage_;
    std::shared_ptr<Bread> bread_;
    Timer frying_timer_{io_};
    Timer baking_timer_{io_};
};
// Класс "Кафетерий". Готовит хот-доги
class Cafeteria {
public:
    explicit Cafeteria(net::io_context& io)
        : io_{io} {
    }

    // Асинхронно готовит хот-дог и вызывает handler, как только хот-дог будет готов.
    // Этот метод может быть вызван из произвольного потока
    void OrderHotDog(HotDogHandler handler) {
        // TODO: Реализуйте метод самостоятельно
        // При необходимости реализуйте дополнительные классы
        const int order_id = ++next_order_id_;
        std::make_shared<Order>(io_, order_id, store_, *gas_cooker_, std::move(handler))->Execute();
    }

private:
    net::io_context& io_;
    // Используется для создания ингредиентов хот-дога
    Store store_;
    // Газовая плита. По условию задачи в кафетерии есть только одна газовая плита на 8 горелок
    // Используйте её для приготовления ингредиентов хот-дога.
    // Плита создаётся с помощью make_shared, так как GasCooker унаследован от
    // enable_shared_from_this.
    std::shared_ptr<GasCooker> gas_cooker_ = std::make_shared<GasCooker>(io_);
    int next_order_id_ = 0;
};

#include "sdk.h"
#include "json_loader.h"
#include "logging_handler.h"
#include "http_server.h"

#include <boost/program_options.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/io_context.hpp>

#include <iostream>
#include <thread>
#include <filesystem>
#include <cassert>
#include <optional>

using namespace std::literals;

namespace net = boost::asio;
namespace sys = boost::system;
namespace fs = std::filesystem;
namespace {

using InputArchive = boost::archive::text_iarchive;
using OutputArchive = boost::archive::text_oarchive;


// Запускает функцию fn на n потоках, включая текущий
template <typename Fn>
void RunWorkers(unsigned n, const Fn& fn) {
    n = std::max(1u, n);
    std::vector<std::jthread> workers;
    workers.reserve(n - 1);
    // Запускаем n-1 рабочих потоков, выполняющих функцию fn
    while (--n) {
        workers.emplace_back(fn);
    }
    fn();
}

class Ticker : public std::enable_shared_from_this<Ticker> {
public:
    using Strand = net::strand<net::io_context::executor_type>;
    using Handler = std::function<void(std::chrono::milliseconds delta)>;

    // Функция handler будет вызываться внутри strand с интервалом period
    Ticker(Strand strand, std::chrono::milliseconds period, Handler handler)
        : strand_{strand}
        , period_{period}
        , handler_{std::move(handler)} {
    }

    void Start() {
        net::dispatch(strand_, [this, self = shared_from_this()] {
            last_tick_ = Clock::now();
            self->ScheduleTick();
        });
    }

private:
    void ScheduleTick() {
        assert(strand_.running_in_this_thread());
        timer_.expires_after(period_);
        timer_.async_wait([self = shared_from_this()](sys::error_code ec) {
            self->OnTick(ec);
        });
    }

    void OnTick(sys::error_code ec) {
        using namespace std::chrono;
        assert(strand_.running_in_this_thread());

        if (!ec) {
            auto this_tick = Clock::now();
            auto delta = duration_cast<milliseconds>(this_tick - last_tick_);
            last_tick_ = this_tick;
            try {
                handler_(delta);
            } catch (...) {
            }
            ScheduleTick();
        }
    }

    using Clock = std::chrono::steady_clock;

    Strand strand_;
    std::chrono::milliseconds period_;
    net::steady_timer timer_{strand_};
    Handler handler_;
    std::chrono::steady_clock::time_point last_tick_;
}; 

}  // namespace
struct Args {
    std::chrono::milliseconds tick_period;
    std::string config_file;
    std::string www_root;
    fs::path serialization_file;
    std::chrono::milliseconds serialize_period;
    bool randomize_spawn_points = false;
    bool have_tick_period = false;
    bool have_save_state_period = false;
    bool have_serizalize = false;
    bool have_serialization_file = false;
}; 
[[nodiscard]] std::optional<Args> ParseCommandLine(int argc, const char* const argv[]) {
    namespace po = boost::program_options;

    po::options_description desc{"Allowed options"s};

     desc.add_options()
        ("help,h", "Show help")
        ("tick-period,t", po::value<std::string>(), "Set tick period (milliseconds)")
        ("config-file,c", po::value<std::string>(), "Set config file path")
        ("www-root,w", po::value<std::string>(), "Set static files root")
        ("randomize-spawn-points", "Spawn dogs at random positions")
        ("state-file,s", po::value<std::string>(), "Set state file path")
        ("save-state-period", po::value<std::string>(), "Set save state period (milliseconds)");
    po::variables_map vm;
    try {
        po::store(po::parse_command_line(argc, argv, desc), vm);
        po::notify(vm);
        if (vm.count("help")) {
            std::cout << desc << "\n";
            return std::nullopt;
        }
        Args args;
        if (vm.count("tick-period")) {
            std::string tick_period_str = vm["tick-period"].as<std::string>();
            args.have_tick_period = true;
            try {
                args.tick_period = std::chrono::milliseconds(std::stoll(tick_period_str));
            } catch (const std::exception& ex) {
                std::cerr << "Invalid tick period provided: " << tick_period_str << "\n";
                return std::nullopt;
            }
        }
        if (vm.count("config-file")) {
            args.config_file = vm["config-file"].as<std::string>();
        }
        if (vm.count("www-root")) {
            args.www_root = vm["www-root"].as<std::string>();
        }
        if (vm.count("randomize-spawn-points")) {
            args.randomize_spawn_points = true;
        }
        if (vm.count("state-file")) {
            if (vm["state-file"].as<std::string>() != ""){
                args.serialization_file = vm["state-file"].as<std::string>();
                std::ifstream file(args.serialization_file);
                if (file.is_open()) {
                    std::cout << "State file found" << std::endl;
                    args.have_serialization_file = true;
                } else{
                    std::cout << "State file not found" << std::endl;
                }
                args.have_serizalize = true;
            }
            
            if (vm.count("save-state-period")) {
                std::string save_state_period_str = vm["save-state-period"].as<std::string>();
                args.have_save_state_period = true;
                try {
                    args.serialize_period = std::chrono::milliseconds(std::stoll(save_state_period_str));
                } catch (const std::exception& ex) {
                    std::cerr << "Invalid tick period provided: " << save_state_period_str << "\n";
                    return std::nullopt;
                }
            }
        }
        return args;
    } catch (const po::error &ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        std::cout << desc << "\n";
        return std::nullopt;
    }
} 

int main(int argc, const char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: game_server <options>\n";
        return EXIT_FAILURE;
    }

    auto options = ParseCommandLine(argc, argv);
    if (!options) {
        return EXIT_FAILURE;
    }
    const char* db_url = std::getenv("GAME_DB_URL");
    if (!db_url) {
        std::cerr << "Переменная окружения GAME_DB_URL не установлена" << std::endl;
        return EXIT_FAILURE;
    }
    std::cout << db_url << std::endl;
    try {
        pqxx::connection conn(db_url);
        pqxx::work txn(conn);
        pqxx::result r = txn.exec(
            "SELECT EXISTS ("
            "   SELECT FROM information_schema.tables "
            "   WHERE table_schema = 'public' "
            "   AND table_name = 'retired_players'"
            ");"
        );

        bool table_exists = r[0][0].as<bool>();

        if (!table_exists) {

            txn.exec(
                "CREATE TABLE retired_players ("
                "   id UUID PRIMARY KEY,"
                "   name VARCHAR(100),"
                "   score INT,"
                "   play_time_ms DOUBLE PRECISION"
                ");"
            );

            txn.exec(
                "CREATE INDEX retired_players_sort_idx ON retired_players("
                "   score DESC,"
                "   play_time_ms ASC,"
                "   name ASC"
                ");"
            );

            std::cout << "Таблица 'retired_players' создана." << std::endl;
        } else {
            std::cout << "Таблица 'retired_players' уже существует." << std::endl;
        }
        txn.commit();
    } catch (const std::exception& ex) {
        std::cerr << "Инициализация базы данных не удалась: " << ex.what() << std::endl;
        std::exit(1);
    }

    try {
        
        // 1. Загружаем карту из файла и строим модель игры
        model::Game game = json_loader::LoadGame(options->config_file);
        std::string static_files_root = options->www_root;

        // 2. Инициализируем io_context
        const unsigned num_threads = std::thread::hardware_concurrency();
        net::io_context ioc(num_threads);

        // 3. Добавляем асинхронный обработчик сигналов SIGINT и SIGTERM
        net::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&ioc](const sys::error_code& ec, [[maybe_unused]] int signal_number) {
            if (!ec) {
                ioc.stop();
            }
        });
        boost::asio::strand<boost::asio::io_context::executor_type> strand(ioc.get_executor());
        loot_gen::LootGenerator loot_gen(game.GetLootPeriod(), game.GetLootProbability());

        
        // 4. Создаём обработчик HTTP-запросов и связываем его с моделью игры
        http_handler::RequestHandler handler{game,loot_gen,static_files_root,options->randomize_spawn_points, 
                                            options->serialize_period, options->serialization_file,
                                            options->have_save_state_period, strand};
        logging_handler::LoggingRequestHandler logger_handler{handler};
        if (options->have_serialization_file) {
            std::ifstream serialization_file(options->serialization_file, std::ios_base::binary);
            InputArchive input_archive{serialization_file};
            handler.LoadStateInformation(input_archive);
        }

        std::chrono::milliseconds delta_ms = options->tick_period;
        if (options->have_tick_period){ 
            std::cout << "Using tick period:  "<< delta_ms.count() << std::endl;
            auto ticker = std::make_shared<Ticker>(strand, delta_ms,
                [&handler](std::chrono::milliseconds delta) { handler.Tick(static_cast<int>(delta.count()));}
            );
            ticker->Start();
        }
        // 5. Запустить обработчик HTTP-запросов, делегируя их обработчику запросов
        const auto address = net::ip::make_address("0.0.0.0");
        constexpr net::ip::port_type port = 8080;
        http_server::ServeHttp(ioc, {address, port}, [&logger_handler](auto&& req, auto&& send,
                               const auto& client_endpoint) {
            logger_handler(std::forward<decltype(req)>(req), std::forward<decltype(send)>(send), client_endpoint);
        });
        
        // Эта надпись сообщает тестам о том, что сервер запущен и готов обрабатывать запросы
        std::cout << "Server has started..."sv << std::endl;

        // 6. Запускаем обработку асинхронных операций
        RunWorkers(std::max(1u, num_threads), [&ioc] {
            ioc.run();
        });
        logging_handler::LogStopServer(EXIT_SUCCESS, "");
        if (options->have_serizalize){
            handler.SaveStateInformation();
        }
    } catch (const std::exception& ex) {
        logging_handler::LogStopServer(EXIT_FAILURE, ex.what());
        return EXIT_FAILURE;
    }
}

#include <boost/program_options.hpp>
#include <fstream>
#include <iostream>
#include <optional>
#include <vector>

using namespace std::literals;
struct Args {
    std::vector<std::string> source;
    std::string destination;
};

[[nodiscard]] std::optional<Args> ParseCommandLine(int argc, const char* const argv[]) {
    namespace po = boost::program_options;

    po::options_description desc{"All options"s};

    // Выводим описание параметров программы
    std::cout << desc;

    return std::nullopt;
}

int main(int argc, char* argv[]) {
    try {
        if (auto args = ParseCommandLine(argc, argv)) {
            /* тут будем обрабатывать ситуацию, когда нужно скопировать файлы */
        }
        /* Если копировать файлы не нужно, то попадём сюда */
        return EXIT_SUCCESS;
    } catch (const std::exception& e) {
        std::cout << e.what() << std::endl;
        return EXIT_FAILURE;
    }
} 
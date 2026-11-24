#include "config.hpp"
#include "router.hpp"
#include "server.hpp"

#include <iostream>

int main(int argc, char* argv[]) {
    auto config_res = farloom::parse_config(argc, argv);
    if (config_res.is_err()) {
        const auto& err = config_res.error();
        if (err.kind == farloom::ConfigErrorKind::HelpRequested) {
            std::cout << farloom::USAGE;
            return 0;
        }
        std::cerr << "error: " << err.message << "\n";
        std::cerr << "run with --help to see the available options\n";
        return 2;
    }

    auto server_res = farloom::Server::bind(config_res.value(), farloom::Router::with_builtins());
    if (server_res.is_err()) {
        std::cerr << "error: " << server_res.error() << "\n";
        return 1;
    }

    auto run_res = server_res.value().run();
    if (run_res.is_err()) {
        std::cerr << "error: server stopped: " << run_res.error() << "\n";
        return 1;
    }

    return 0;
}

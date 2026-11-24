// main.cpp - command line:  heritage analyze [csv] | heritage serve [csv] [port]
#include "pipeline.hpp"
#include "server.hpp"

int main(int argc, char** argv) {
    std::string cmd = argc > 1 ? argv[1] : "serve";
    std::string csv = argc > 2 ? argv[2] : "data/ndli_sample.csv";
    try {
        if (cmd == "analyze") { hc::analyze(csv, "outputs"); return 0; }
        if (cmd == "serve") { int port = argc > 3 ? std::atoi(argv[3]) : 5000; return hc::serve(csv, "outputs", "data", port); }
    } catch (const std::exception& e) { std::cerr << "error: " << e.what() << std::endl; return 1; }
    std::cerr << "usage: heritage analyze [csv]\n       heritage serve [csv] [port]\n";
    return 2;
}

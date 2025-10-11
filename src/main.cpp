#include "Renderer.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstring>
#include <sstream>
#include <cstdio>
#include <cstdlib>

int main(int argc, char** argv) {
    std::string file;
    int wrap = 0;
    bool no_color = false;
    bool no_urls = false;
    bool paging = false;
    bool show_help = false;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "-w" || a == "--wrap") {
            if (i+1 < argc) { wrap = std::atoi(argv[++i]); }
        } else if (a == "-h" || a == "--help") {
            show_help = true;
        } else if (a == "-p") {
            paging = true;
        } else if (a == "--no-color") {
            no_color = true;
        } else if (a == "--no-urls") {
            no_urls = true;
        } else if (a == "-" ) {
            file = "-";
        } else if (a.size() && a[0] == '-') {
            // unknown, ignore
        } else {
            file = a;
        }
    }

    if (show_help) {
        std::cout << "mdmni - minimal markdown pager\n\n";
        std::cout << "Usage: mdmni [OPTIONS] [file]\n";
        std::cout << "If file is omitted or '-' is given, read from stdin.\n\n";
        std::cout << "Options:\n";
        std::cout << "  -h, --help        Show this help message and exit\n";
        std::cout << "  -w, --wrap N      Wrap output to width N (0 = no wrap)\n";
        std::cout << "  -p                Pipe output through pager (PAGER env or 'less -R')\n";
        std::cout << "      --no-color     Disable ANSI colors\n";
        std::cout << "      --no-urls      Do not show URLs after links/images\n";
        return 0;
    }

    // If invoked via a symlink named 'mdless', always enable paging.
    if (!paging && argc > 0) {
        std::string prog = argv[0];
        size_t pos = prog.find_last_of("/\\");
        if (pos != std::string::npos) prog = prog.substr(pos + 1);
        if (prog == "mdless") paging = true;
    }

    std::vector<std::string> lines;
    if (file.empty() || file == "-") {
        std::string line;
        while (std::getline(std::cin, line)) {
            lines.push_back(line);
        }
    } else {
        std::ifstream in(file);
        if (!in) {
        std::cerr << "mdpp: file not found: " << file << std::endl;
            return 2;
        }
        std::string line;
        while (std::getline(in, line)) lines.push_back(line);
    }

    Renderer r(!no_color, !no_urls, wrap, false);

    if (paging) {
        // Render to a buffer then feed it to the pager (PAGER env or less -R).
        std::ostringstream buf;
        r.render(lines, buf);
        std::string out = buf.str();

        const char* pager_env = std::getenv("PAGER");
        std::string pager = pager_env && pager_env[0] ? pager_env : "less -R";
        FILE* p = popen(pager.c_str(), "w");
        if (p) {
            fwrite(out.data(), 1, out.size(), p);
            pclose(p);
        } else {
            // fallback to stdout
            std::cout << out;
        }
    } else {
        r.render(lines, std::cout);
    }
    return 0;
}

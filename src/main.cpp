#include "Renderer.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstring>
#include <sstream>
#include <cstdio>
#include <cstdlib>

#define BASENAME_OF(path) \
    ((path) ? \
        (std::strrchr((path), '/') ? std::strrchr((path), '/') + 1 : \
         (std::strrchr((path), '\\') ? std::strrchr((path), '\\') + 1 : (path))) \
     : "")

void usageExit(const std::string& progName, int code=0) {
    std::cout << progName << " - minimal markdown pager\n\n";
    std::cout << "Usage: " << progName << " [OPTIONS] [file]\n";
    std::cout << "If file is omitted or '-' is given, read from stdin.\n\n";
    std::cout << "Options:\n";
    std::cout << "  -h, --help        Show this help message and exit\n";
    std::cout << "  -w, --wrap N      Wrap output to width N (0 = no wrap)\n";
    std::cout << "  -p                Pipe output through pager (PAGER env or 'less -R')\n";
    std::cout << "      --no-color    Disable ANSI colors\n";
    std::cout << "      --no-urls     Do not show URLs after links/images\n";
    std::exit(code);
}

int main(int argc, char** argv) {
    std::string progName = BASENAME_OF(argv[0]);
    std::string file;
    int wrap = 0;
    bool paging = false;
    bool noColor = false;
    bool noUrls = false;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "-w" || a == "--wrap") {
            if (argc > i + 1) {
                wrap = std::atoi(argv[++i]);
            }
            else {
                usageExit(progName, -1);
            }
        } else if (a == "-h" || a == "--help") {
            usageExit(progName);
        } else if (a == "-p") {
            paging = true;
        } else if (a == "--no-color") {
            noColor = true;
        } else if (a == "--no-urls") {
            noUrls = true;
        } else if (a == "-" ) {
            file = "-";
        } else {
            file = a;
        }
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
            std::cerr << "mdmni: file not found: " << file << std::endl;
            return 2;
        }
        std::string line;
        while (std::getline(in, line)) lines.push_back(line);
    }

    mdmni::Renderer r(!noColor, !noUrls, wrap);

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

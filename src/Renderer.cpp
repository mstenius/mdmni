#include "Renderer.h"
#include <regex>
#include <iostream>
#include <sstream>

static const std::string CSI = "\033[";
static const std::string RESET = CSI + "0m";

static std::string style(const std::string& code) {
    return CSI + code + "m";
}

Renderer::Renderer(bool use_color_, bool show_urls_, int wrap_, bool inline_images_)
    : use_color(use_color_), show_urls(show_urls_), wrap(wrap_), inline_images(inline_images_) {}

void Renderer::render(const std::vector<std::string>& lines, std::ostream& out) {
    for (const auto& l : lines) {
        process_line(l, out);
    }
    flush_paragraph(out);
}

void Renderer::process_line(const std::string& line, std::ostream& out) {
    if (in_code_fence) {
        if (is_fence_close(line)) {
            in_code_fence = false;
            out << "" << std::endl;
        } else {
            if (use_color) out << style("90") << "│ " << RESET << style("90") << line << RESET << std::endl;
            else out << "    " << line << std::endl;
        }
        return;
    }

    if (is_fence_open(line)) {
        in_code_fence = true;
        out << std::endl;
        return;
    }

    if (is_hr(line)) {
        flush_paragraph(out);
        int width = wrap > 0 ? wrap : 80;
        int w = std::max(10, std::min(1000, width - 2));
        for (int i = 0; i < w; ++i) out << "─";
        out << std::endl;
        return;
    }

    std::smatch m;
    std::regex hregex(R"(^(#{1,6})\s+(.*)$)");
    if (std::regex_match(line, m, hregex)) {
        flush_paragraph(out);
        int level = (int)m[1].length();
        std::string text = apply_inline((std::string)m[2]);
        out << style_heading(level, text) << std::endl << std::endl;
        return;
    }

    // Blockquote
    if (!line.empty() && std::regex_search(line, std::regex(R"(^\s*>)")) ) {
        flush_paragraph(out);
        std::string content = line;
        // strip leading >
        while (!content.empty() && (content.front() == '>' || isspace((unsigned char)content.front()))) content.erase(0,1);
        std::string body = apply_inline(content);
        if (use_color) out << style("90") << "│ " << RESET << style("3") << body << RESET << std::endl;
        else out << "| " << body << std::endl;
        return;
    }

    // Lists
    std::regex lreg(R"(^\s*([-+*]|\d+\.)\s+(.*)$)");
    if (std::regex_match(line, m, lreg)) {
        flush_paragraph(out);
        std::string bullet = (std::string)m[1];
        std::string content = apply_inline((std::string)m[2]);
        std::string sym = (bullet == "-" || bullet == "+" || bullet == "*") ? "•" : bullet;
        if (use_color) out << style("94") << sym << RESET << " " << content << std::endl;
        else out << sym << " " << content << std::endl;
        return;
    }

    if (line.find("|") != std::string::npos && std::regex_match(line, std::regex(R"(^\s*\|.*\|\s*$)"))) {
        // Very simple table handling: just print as-is for now
        flush_paragraph(out);
        out << line << std::endl;
        return;
    }

    if (line.find_first_not_of(" \t\r\n") == std::string::npos) {
        flush_paragraph(out);
        out << std::endl;
        return;
    }

    paragraph.push_back(line);
}

void Renderer::flush_paragraph(std::ostream& out) {
    if (paragraph.empty()) return;
    std::string text;
    for (size_t i = 0; i < paragraph.size(); ++i) {
        if (i) text += " ";
        text += paragraph[i];
    }
    paragraph.clear();
    text = apply_inline(text);
    out << text << std::endl;
}

bool Renderer::is_fence_open(const std::string& line) {
    // Match a sequence of 3 or more backticks or tildes, optionally followed by a language id.
    // Capture the fence chars so we can remember which character and how many were used.
    std::smatch m;
    std::regex open_re(R"(^\s*([`~])\1{2,}[\t ]*[^`~]*\s*$)");
    if (std::regex_match(line, m, open_re)) {
        // m[0] is full match; extract the leading fence run from the start of the trimmed line
        std::string s = line;
        // trim leading whitespace
        size_t pos = s.find_first_not_of(" \t");
        if (pos != std::string::npos) s = s.substr(pos);
        else s.clear();
        // fence run is sequence of same char at start
        if (!s.empty()) {
            char c = s[0];
            int count = 0;
            for (char ch : s) {
                if (ch == c) count++;
                else break;
            }
            code_fence_char = c;
            code_fence_len = count;
            return true;
        }
    }
    return false;
}

bool Renderer::is_fence_close(const std::string& line) {
    if (code_fence_char == '\0' || code_fence_len <= 0) return false;
    // Trim leading whitespace, then count same-char run at start; valid close if run char matches and length >= opener length,
    // and the rest of the line is only whitespace.
    size_t i = 0;
    // skip leading whitespace
    while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;
    int run = 0;
    while (i < line.size() && line[i] == code_fence_char) { ++run; ++i; }
    // rest must be only whitespace
    while (i < line.size()) { if (line[i] != ' ' && line[i] != '\t') return false; ++i; }
    return run >= code_fence_len && run >= 3;
}

bool Renderer::is_hr(const std::string& line) {
    std::string s;
    for (char c : line) if (!isspace((unsigned char)c)) s.push_back(c);
    if (s.size() >= 3) {
        bool all_dash = true;
        for (char c : s) if (c != '-' && c != '*' && c != '_') all_dash = false;
        return all_dash;
    }
    return false;
}

std::string Renderer::style_heading(int level, const std::string& text) {
    if (!use_color) return std::string(level, '#') + " " + text;
    if (level == 1) return style("96;1;4") + text + RESET;
    if (level == 2) return style("96;1") + text + RESET;
    if (level == 3) return style("93;1") + text + RESET;
    if (level == 4) return style("95") + text + RESET;
    return style("36") + text + RESET;
}

std::string Renderer::apply_inline(const std::string& text) {
    std::string s = text;

    // images: ![alt](url) -> fallback text
    s = std::regex_replace(s,
        std::regex(R"(!\[([^\]]*)\]\(([^\)]+)\))"),
        (use_color?"\033[35;1m[Image: $1]\033[0m \033[90m<$2>\033[0m":"[Image: $1] <$2>")
    );

    // links: [text](url)
    s = std::regex_replace(s,
        std::regex(R"(\[([^\]]+)\]\(([^\)]+)\))"),
        (use_color?"\033[34;4m$1\033[0m \033[90m<$2>\033[0m":"$1 <$2>")
    );

    // bold **text** or __text__
    s = std::regex_replace(s,
        std::regex(R"(\*\*(.+?)\*\*)"),
        use_color? (style("1") + "$1" + RESET) : "$1"
    );
    s = std::regex_replace(s,
        std::regex(R"(__([^_]+)__)"),
        use_color? (style("1") + "$1" + RESET) : "$1"
    );

    // italic *text* or _text_ (avoid lookaround; match single-star or single-underscore runs)
    s = std::regex_replace(s,
        std::regex(R"(\*([^*]+)\*)"),
        use_color? (style("3") + "$1" + RESET) : "$1"
    );
    s = std::regex_replace(s,
        std::regex(R"(_([^_]+)_)"), 
        use_color? (style("3") + "$1" + RESET) : "$1"
    );

    return s;
}

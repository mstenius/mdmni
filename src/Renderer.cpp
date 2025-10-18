#include "Renderer.h"
#include <regex>
#include <iostream>
#include <sstream>

//
// ANSI escape codes
//

// CSI = Control Sequence Introducer
static const std::string CSI = "\033[";

// Bold and underline codes
static const std::string BOLD = "1";
static const std::string DIM = "2";
static const std::string ITALIC = "3";
static const std::string UNDERLINE = "4";

// Color codes
static const std::string RED = "31";
static const std::string GREEN = "32";
static const std::string YELLOW = "33";
static const std::string BLUE = "34";
static const std::string MAGENTA = "35";
static const std::string CYAN = "36";
static const std::string WHITE = "37";
static const std::string BRIGHT_BLACK = "90";
static const std::string BRIGHT_RED = "91";
static const std::string BRIGHT_GREEN = "92";
static const std::string BRIGHT_YELLOW = "93";
static const std::string BRIGHT_BLUE = "94";
static const std::string BRIGHT_MAGENTA = "95";
static const std::string BRIGHT_CYAN = "96";
static const std::string BRIGHT_WHITE = "97";

// Background colours
static const std::string BG_RED = "41";
static const std::string BG_GREEN = "42";
static const std::string BG_YELLOW = "43";
static const std::string BG_BLUE = "44";
static const std::string BG_MAGENTA = "45";
static const std::string BG_CYAN = "46";
static const std::string BG_WHITE = "47";
static const std::string BG_BRIGHT_BLACK = "100";
static const std::string BG_BRIGHT_RED = "101";
static const std::string BG_BRIGHT_GREEN = "102";
static const std::string BG_BRIGHT_YELLOW = "103";
static const std::string BG_BRIGHT_BLUE = "104";
static const std::string BG_BRIGHT_MAGENTA = "105";
static const std::string BG_BRIGHT_CYAN = "106";
static const std::string BG_BRIGHT_WHITE = "107";

static std::string textStyle() {
    return CSI + "0m";
}

template<typename... Args>
static std::string textStyle(Args&&... args) {
    std::vector<std::string> parts;
    parts.reserve(sizeof...(Args));
    // Fold expression to emplace each argument (works with std::string or string literals)
    (parts.emplace_back(std::forward<Args>(args)), ...);

    std::string joined;
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i) joined += ";";
        joined += parts[i];
    }
    return CSI + joined + "m";
}

static std::string textStyleReset() {
    return textStyle();
}

namespace mdmni {

Renderer::Renderer(bool useColor_, bool showUrls_, int wrap_)
    : useColor(useColor_), showUrls(showUrls_), wrap(wrap_) {}

void Renderer::render(const std::vector<std::string>& lines, std::ostream& out) {
    for (const auto& l : lines) {
        processLine(l, out);
    }
    flushParagraph(out);
}

void Renderer::processLine(const std::string& line, std::ostream& out) {

    // Inside code fences
    if (insideCodeFences) {
        if (isCodeFenceEnd(line)) {
            insideCodeFences = false;
            out << "" << std::endl;
        } else {
            if (useColor) out << textStyle(GREEN) << "  " << line << textStyleReset() << std::endl;
            else out << "    " << line << std::endl;
        }
        return;
    }

    // Code fence open
    if (isCodeFenceStart(line)) {
        insideCodeFences = true;
        out << std::endl;
        return;
    }

    // Horizontal rule
    if (isHr(line)) {
        flushParagraph(out);
        int width = wrap > 0 ? wrap : 80;
        int w = std::max(10, std::min(1000, width - 2));
        for (int i = 0; i < w; ++i) out << "─";
        out << std::endl;
        return;
    }

    // Headings
    std::smatch m;
    std::regex hregex(R"(^(#{1,6})\s+(.*)$)");
    if (std::regex_match(line, m, hregex)) {
        flushParagraph(out);
        int level = (int)m[1].length();
        std::string text = applyInline((std::string)m[2]);
        out << std::endl << styleHeading(level, text) << std::endl;
        return;
    }

    // Blockquote
    if (!line.empty() && std::regex_search(line, std::regex(R"(^\s*>)")) ) {
        flushParagraph(out);
        std::string content = line;
        // strip leading >
        while (!content.empty() && (content.front() == '>' || isspace((unsigned char)content.front()))) content.erase(0,1);
        std::string body = applyInline(content);
        if (useColor) out << textStyle(BRIGHT_BLACK) << "│ " << textStyleReset() << textStyle(ITALIC) << body << textStyleReset() << std::endl;
        else out << "| " << body << std::endl;
        return;
    }

    // Lists
    std::regex lreg(R"(^\s*([-+*]|\d+\.)\s+(.*)$)");
    if (std::regex_match(line, m, lreg)) {
        flushParagraph(out);
        std::string bullet = (std::string)m[1];
        std::string content = applyInline((std::string)m[2]);
        std::string sym = (bullet == "-" || bullet == "+" || bullet == "*") ? "•" : bullet;
        if (useColor) out << textStyle(BRIGHT_BLUE) << sym << textStyleReset() << " " << content << std::endl;
        else out << sym << " " << content << std::endl;
        return;
    }

    // Tables - Just print as-is for now
    if (line.find("|") != std::string::npos && std::regex_match(line, std::regex(R"(^\s*\|.*\|\s*$)"))) {
        flushParagraph(out);
        out << line << std::endl;
        return;
    }

    if (line.find_first_not_of(" \t\r\n") == std::string::npos) {
        flushParagraph(out);
        out << std::endl;
        return;
    }

    paragraph.push_back(line);
}

void Renderer::flushParagraph(std::ostream& out) {
    if (paragraph.empty()) return;
    std::string text;
    for (size_t i = 0; i < paragraph.size(); ++i) {
        if (i) text += " ";
        text += paragraph[i];
    }
    paragraph.clear();
    text = applyInline(text);
    out << text << std::endl;
}

// Check if line is a code block start (fence)
bool Renderer::isCodeFenceStart(const std::string& line) {
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
            codeFenceChar = c;
            codeFenceLen = count;
            return true;
        }
    }
    return false;
}

// Check if line is a code block end (matching fence)
bool Renderer::isCodeFenceEnd(const std::string& line) {
    if (codeFenceChar == '\0' || codeFenceLen <= 0) return false;
    // Trim leading whitespace, then count same-char run at start; valid close if run char matches and length >= opener length,
    // and the rest of the line is only whitespace.
    size_t i = 0;
    // skip leading whitespace
    while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;
    int run = 0;
    while (i < line.size() && line[i] == codeFenceChar) { ++run; ++i; }
    // rest must be only whitespace
    while (i < line.size()) { if (line[i] != ' ' && line[i] != '\t') return false; ++i; }
    return run >= codeFenceLen && run >= 3;
}

// Check if line is a horizontal rule (---, ***, ___, etc)
bool Renderer::isHr(const std::string& line) {
    std::string s;
    for (char c : line) if (!isspace((unsigned char)c)) s.push_back(c);
    if (s.size() >= 3) {
        bool all_dash = true;
        for (char c : s) if (c != '-' && c != '*' && c != '_') all_dash = false;
        return all_dash;
    }
    return false;
}

// Style a heading line based on its level
std::string Renderer::styleHeading(int level, const std::string& text) {
    if (!useColor) return std::string(level, '#') + " " + text;
    if (level == 1) return textStyle(BRIGHT_CYAN, BOLD, UNDERLINE) + text + textStyleReset();
    if (level == 2) return textStyle(BRIGHT_CYAN, BOLD) + text + textStyleReset();
    if (level == 3) return textStyle(BRIGHT_YELLOW, BOLD) + text + textStyleReset();
    if (level == 4) return textStyle(BRIGHT_MAGENTA) + text + textStyleReset();
    return textStyle("36") + text + textStyleReset();
}

// Apply inline styles and transformations to a text line
std::string Renderer::applyInline(const std::string& text) {
    std::string s = text;

    // images: ![alt](url) -> fallback text
    s = std::regex_replace(s,
        std::regex(R"(!\[([^\]]*)\]\(([^\)]+)\))"),
        (useColor
            ? (textStyle(MAGENTA, BOLD) + std::string("[Image: $1]") + textStyleReset()
               + " " + textStyle(BRIGHT_BLACK) + std::string("<$2>") + textStyleReset())
            : std::string("[Image: $1] <$2>"))
    );

    // links: [text](url)
    s = std::regex_replace(s,
        std::regex(R"(\[([^\]]+)\]\(([^\)]+)\))"),
        (useColor
            ? (textStyle(BLUE, UNDERLINE) + std::string("$1") + textStyleReset()
               + " " + textStyle(BRIGHT_BLACK) + std::string("<$2>") + textStyleReset())
            : std::string("$1 <$2>"))
    );

    // bold **text** or __text__
    s = std::regex_replace(s,
        std::regex(R"(\*\*(.+?)\*\*)"),
        useColor? (textStyle(BOLD) + "$1" + textStyleReset()) : "$1"
    );
    s = std::regex_replace(s,
        std::regex(R"(__([^_]+)__)"),
        useColor? (textStyle(BOLD) + "$1" + textStyleReset()) : "$1"
    );

    // italic *text* or _text_ (avoid lookaround; match single-star or single-underscore runs)
    s = std::regex_replace(s,
        std::regex(R"(\*([^*]+)\*)"),
        useColor? (textStyle(ITALIC) + "$1" + textStyleReset()) : "$1"
    );
    s = std::regex_replace(s,
        std::regex(R"(_([^_]+)_)"), 
        useColor? (textStyle(ITALIC) + "$1" + textStyleReset()) : "$1"
    );

    // inline code `code`
    s = std::regex_replace(s,
        std::regex(R"(`([^`]+)`)"),
        useColor? (textStyle(BRIGHT_WHITE, BG_BRIGHT_BLACK) + "$1" + textStyleReset()) : "$1"
    );

    return s;
}

} // namespace mdmni

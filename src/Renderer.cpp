#include "Renderer.h"
#include <algorithm>
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
    static const std::regex hregex(R"(^(#{1,6})\s+(.*)$)");
    std::smatch m;
    if (std::regex_match(line, m, hregex)) {
        flushParagraph(out);
        int level = (int)m[1].length();
        std::string text = applyInline((std::string)m[2]);
        out << std::endl << styleHeading(level, text) << std::endl;
        return;
    }

    // Blockquote
    static const std::regex bq_regex(R"(^\s*>)");
    if (!line.empty() && std::regex_search(line, bq_regex)) {
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
    static const std::regex lreg(R"(^\s*([-+*]|\d+\.)\s+(.*)$)");
    if (std::regex_match(line, m, lreg)) {
        flushParagraph(out);
        std::string bullet = (std::string)m[1];
        std::string content = applyInline((std::string)m[2]);
        std::string sym = (bullet == "-" || bullet == "+" || bullet == "*") ? "•" : bullet;
        if (useColor) out << textStyle(BRIGHT_BLUE) << sym << textStyleReset() << " " << content << std::endl;
        else out << sym << " " << content << std::endl;
        return;
    }

    // Tables - buffer rows for aligned rendering
    static const std::regex table_regex(R"(^\s*\|.*\|\s*$)");
    if (line.find("|") != std::string::npos && std::regex_match(line, table_regex)) {
        if (tableBuffer.empty()) flushParagraph(out);
        tableBuffer.push_back(line);
        return;
    }

    // Non-table line — flush any buffered table first
    flushTable(out);

    if (line.find_first_not_of(" \t\r\n") == std::string::npos) {
        flushParagraph(out);
        out << std::endl;
        return;
    }

    paragraph.push_back(line);
}

// Flush the current paragraph buffer, applying inline styles and printing as a single line
void Renderer::flushParagraph(std::ostream& out) {
    flushTable(out);
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

// Count UTF-8 code points (each counts as one terminal column for BMP chars)
static size_t utf8Columns(const std::string& s) {
    size_t n = 0;
    for (unsigned char c : s)
        if ((c & 0xC0) != 0x80) ++n;  // skip continuation bytes
    return n;
}

// Return the visible terminal column width of a string, ignoring ANSI escape sequences
static size_t visualWidth(const std::string& s) {
    static const std::regex ansi_re("\033\\[[0-9;]*m");
    return utf8Columns(std::regex_replace(s, ansi_re, ""));
}

// Split a markdown table row into trimmed cell strings, respecting \| escapes
static std::vector<std::string> splitTableRow(const std::string& line) {
    std::vector<std::string> cells;
    size_t start = line.find('|');
    size_t end   = line.rfind('|');
    if (start == std::string::npos || start == end) return cells;

    std::string current;
    for (size_t i = start + 1; i < end; ++i) {
        if (line[i] == '\\' && i + 1 < end && line[i + 1] == '|') {
            current += '|';
            ++i;
        } else if (line[i] == '|') {
            size_t s = current.find_first_not_of(" \t");
            size_t e = current.find_last_not_of(" \t");
            cells.push_back(s == std::string::npos ? "" : current.substr(s, e - s + 1));
            current.clear();
        } else {
            current += line[i];
        }
    }
    size_t s = current.find_first_not_of(" \t");
    size_t e = current.find_last_not_of(" \t");
    cells.push_back(s == std::string::npos ? "" : current.substr(s, e - s + 1));
    return cells;
}

// Return true if the line is a table separator row (|---|:---:|...|)
static bool isTableSeparatorRow(const std::string& line) {
    for (char c : line)
        if (c != '-' && c != ':' && c != '|' && c != ' ' && c != '\t') return false;
    return line.find('-') != std::string::npos;
}

// Flush buffered table rows with aligned column widths
void Renderer::flushTable(std::ostream& out) {
    if (tableBuffer.empty()) return;

    std::vector<std::vector<std::string>> rawRows;
    std::vector<bool> sepFlags;
    for (const auto& line : tableBuffer) {
        sepFlags.push_back(isTableSeparatorRow(line));
        rawRows.push_back(splitTableRow(line));
    }

    size_t numCols = 0;
    for (const auto& row : rawRows) numCols = std::max(numCols, row.size());
    if (numCols == 0) { tableBuffer.clear(); return; }

    // Apply inline styles to data rows and measure column widths
    std::vector<std::vector<std::string>> styledRows(rawRows.size(), std::vector<std::string>(numCols, ""));
    std::vector<size_t> colWidths(numCols, 1);
    for (size_t i = 0; i < rawRows.size(); ++i) {
        if (sepFlags[i]) continue;
        for (size_t j = 0; j < rawRows[i].size(); ++j) {
            styledRows[i][j] = applyInline(rawRows[i][j]);
            colWidths[j] = std::max(colWidths[j], visualWidth(styledRows[i][j]));
        }
    }

    bool hasSep = std::any_of(sepFlags.begin(), sepFlags.end(), [](bool b){ return b; });

    auto printBorder = [&](const char* left, const char* mid, const char* right, const char* fill) {
        out << left;
        for (size_t j = 0; j < numCols; ++j) {
            for (size_t k = 0; k < colWidths[j] + 2; ++k) out << fill;
            out << (j + 1 < numCols ? mid : right);
        }
        out << "\n";
    };

    printBorder("┌", "┬", "┐", "─");

    bool pastSep = false;
    for (size_t i = 0; i < rawRows.size(); ++i) {
        if (sepFlags[i]) {
            printBorder("├", "┼", "┤", "─");
            pastSep = true;
            continue;
        }
        bool isHeader = hasSep && !pastSep;
        out << "│";
        for (size_t j = 0; j < numCols; ++j) {
            const std::string& cell = styledRows[i][j];
            size_t pad = colWidths[j] - visualWidth(cell);
            out << " ";
            if (isHeader && useColor) out << textStyle(BOLD);
            out << cell;
            if (isHeader && useColor) out << textStyleReset();
            out << std::string(pad, ' ') << " │";
        }
        out << "\n";
    }

    printBorder("└", "┴", "┘", "─");
    tableBuffer.clear();
}

// Check if line is a code block start (fence)
bool Renderer::isCodeFenceStart(const std::string& line) {
    // Match a sequence of 3 or more backticks or tildes, optionally followed by a language id.
    // Capture the fence chars so we can remember which character and how many were used.
    static const std::regex open_re(R"(^\s*([`~])\1{2,}[\t ]*[^`~]*\s*$)");
    std::smatch m;
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

    static const std::regex img_re(R"(!\[([^\]]*)\]\(([^\)]+)\))");
    static const std::regex link_re(R"(\[([^\]]+)\]\(([^\)]+)\))");
    static const std::regex bold_star_re(R"(\*\*(.+?)\*\*)");
    static const std::regex bold_under_re(R"(__([^_]+)__)");
    static const std::regex italic_star_re(R"(\*([^*]+)\*)");
    static const std::regex italic_under_re(R"(_([^_]+)_)");

    // images: ![alt](url) -> fallback text
    s = std::regex_replace(s, img_re,
        showUrls
            ? (useColor
                ? (textStyle(MAGENTA, BOLD) + std::string("[Image: $1]") + textStyleReset()
                   + " " + textStyle(BRIGHT_BLACK) + std::string("<$2>") + textStyleReset())
                : std::string("[Image: $1] <$2>"))
            : (useColor
                ? (textStyle(MAGENTA, BOLD) + std::string("[Image: $1]") + textStyleReset())
                : std::string("[Image: $1]"))
    );

    // links: [text](url)
    s = std::regex_replace(s, link_re,
        showUrls
            ? (useColor
                ? (textStyle(BLUE, UNDERLINE) + std::string("$1") + textStyleReset()
                   + " " + textStyle(BRIGHT_BLACK) + std::string("<$2>") + textStyleReset())
                : std::string("$1 <$2>"))
            : (useColor
                ? (textStyle(BLUE, UNDERLINE) + std::string("$1") + textStyleReset())
                : std::string("$1"))
    );

    // bold **text** or __text__
    s = std::regex_replace(s, bold_star_re,
        useColor? (textStyle(BOLD) + "$1" + textStyleReset()) : "$1"
    );
    s = std::regex_replace(s, bold_under_re,
        useColor? (textStyle(BOLD) + "$1" + textStyleReset()) : "$1"
    );

    // italic *text* or _text_ (avoid lookaround; match single-star or single-underscore runs)
    s = std::regex_replace(s, italic_star_re,
        useColor? (textStyle(ITALIC) + "$1" + textStyleReset()) : "$1"
    );
    s = std::regex_replace(s, italic_under_re,
        useColor? (textStyle(ITALIC) + "$1" + textStyleReset()) : "$1"
    );

    // inline code `code`
    static const std::regex code_re(R"(`([^`]+)`)");
    s = std::regex_replace(s, code_re,
        useColor? (textStyle(BRIGHT_WHITE, BG_BRIGHT_BLACK) + "$1" + textStyleReset()) : "$1"
    );

    return s;
}

} // namespace mdmni

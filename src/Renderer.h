// Minimal mdmni (mdcat-like) renderer header
#pragma once

#include <string>
#include <vector>
#include <ostream>

class Renderer {
public:
    Renderer(bool useColor = true, bool showUrls = true, int wrap = 0);
    void render(const std::vector<std::string>& lines, std::ostream& out);
private:
    bool useColor;
    bool showUrls;
    int wrap;

    bool inCodeBlock = false;
    char codeFenceChar = '\0';
    int codeFenceLen = 0;
    std::vector<std::string> paragraph;

    void processLine(const std::string& line, std::ostream& out);
    void flushParagraph(std::ostream& out);
    bool isCodeBlockStart(const std::string& line);
    bool isCodeBlockEnd(const std::string& line);
    bool isHr(const std::string& line);
    std::string applyInline(const std::string& text);
    std::string styleHeading(int level, const std::string& text);
};

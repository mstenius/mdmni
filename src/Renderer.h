// Minimal mdmni (mdcat-like) renderer header
#pragma once

#include <string>
#include <vector>
#include <ostream>

class Renderer {
public:
    Renderer(bool use_color = true, bool show_urls = true, int wrap = 0, bool inline_images = false);
    void render(const std::vector<std::string>& lines, std::ostream& out);
private:
    bool use_color;
    bool show_urls;
    int wrap;
    bool inline_images;

    bool in_code_fence = false;
    char code_fence_char = '\0';
    int code_fence_len = 0;
    std::vector<std::string> paragraph;

    void process_line(const std::string& line, std::ostream& out);
    void flush_paragraph(std::ostream& out);
    bool is_fence_open(const std::string& line);
    bool is_fence_close(const std::string& line);
    bool is_hr(const std::string& line);
    std::string apply_inline(const std::string& text);
    std::string style_heading(int level, const std::string& text);
};

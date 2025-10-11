# mdmni - minimal MD file viewer for the terminal


This repository contains `mdmni`, a small MD file viewer implemmented in C++.

## Build

Build (from project root):

```bash
mkdir -p build && cd build
cmake ..
cmake --build .
```

## Usage

```bash
# Read a file
./mdmni path/to/file.md

# Or from stdin
cat file.md | ./mdmni -

# Basic options:
./mdmni --no-color --no-urls -w 80 file.md

# Usage:
./mdmni --help
mdmni - minimal markdown pager

Usage: mdmni [OPTIONS] [file]
If file is omitted or '-' is given, read from stdin.

Options:
  -h, --help        Show this help message and exit
  -w, --wrap N      Wrap output to width N (0 = no wrap)
      --no-color     Disable ANSI colors
      --no-urls      Do not show URLs after links/images
```

Notes:
- This is a minimal implementation. It implements headings, paragraphs, lists, code fences, simple inline formatting (bold/italic/links), and image fallback text. It does not implement terminal inline images.

## License

This project is released under the MIT License. See `LICENSE` for the full text.

SPDX-License-Identifier: MIT

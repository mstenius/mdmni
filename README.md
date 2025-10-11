# mdmni - minimal MD file viewer for the terminal

This repository contains `mdmni`, a small MD file viewer implemented in C++.

I created it because I wanted a simple viewer that could be easily built on many systems without the need of all the dependencies, bells and whistles of Python, Ruby, different package managers etc. A reasonably modern C++ compiler and CMake should be enough.

Not a complete implementation, it focuses only on the basic primitives.

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
  -p                Pipe output through pager (PAGER env or 'less -R')
      --no-color     Disable ANSI colors
      --no-urls      Do not show URLs after links/images
```

Notes:
- This is a minimal implementation. It implements headings, paragraphs, lists, code fences, simple inline formatting (bold/italic/links), and image fallback text. It does not implement terminal inline images.

## License

This project is released under the MIT License. See `LICENSE` for the full text.

SPDX-License-Identifier: MIT

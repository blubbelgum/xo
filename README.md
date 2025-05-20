xo

> **Note:** This project is primarily for educational purposes as part of the author's journey learning C programming. The implementation serves as a learning exercise in creating cross-platform applications using C.

xo is a lightweight Static Site Generator (SSG) originally built with Bun.js and now also implemented in C. It allows you to create and deploy static websites efficiently with built-in support for Markdown content, layout templating, and live reload during development.

## Implementations

This repository contains two implementations of the xo static site generator:

1. **JavaScript/TypeScript version** - The original implementation using Bun.js runtime
2. **C version** - A cross-platform C implementation with support for Windows and POSIX systems

The C implementation (`xo-c/`) demonstrates how to build portable software that works across different operating systems while maintaining the same functionality as the original JavaScript version.

Features

    Markdown Support: Automatically processes .md files with frontmatter and converts them into HTML.
    Custom Layouts: Easily define layouts for your pages.
    Partial Templates: Modularize content with partials and embed them dynamically.
    Tailwind CSS Integration: Automatically detects and uses Tailwind CSS if installed.
    Live Reload: Built-in WebSocket support for live reload during development.
    Static Asset Handling: Supports CSS, JS, and image assets (JPG, PNG, GIF, SVG).
    Fast Build Process: Powered by Bun.js for fast and efficient site generation.

Installation

To get started, clone the repository and install dependencies:

git clone https://github.com/blubblegum/xo.git
cd xo
bun install

Usage
Development Server

Start the development server with live reload:

bun dev

This will start the server on http://localhost:3000, watch for file changes, and automatically rebuild your site.
Build for Production

To generate the static site for production:

```sh
bun build
```
This will output the static files into the dist directory.

## C Implementation

The C implementation provides the same functionality as the Bun.js version but with native performance and no runtime dependencies.

### Building the C Implementation

To build the C version:

```sh
cd xo-c
mkdir build && cd build
cmake ..
cmake --build .
```

### Using the C Implementation

The C implementation supports the same commands as the Bun.js version:

```sh
# Initialize a new project with sample content
./xo-c init

# Build the site
./xo-c build

# Start development server with live reload
./xo-c dev
```

### Cross-Platform Support

The C implementation works on both Windows and UNIX-like systems (Linux, macOS) with the same codebase, demonstrating:

- Cross-platform threading abstractions
- Filesystem operations that work across operating systems
- Network and HTTP server implementation with platform-specific optimizations

## Partials
You can use partials in your Markdown files. Partial templates are stored in the _partials/ folder and can be included using the following syntax:

```mustache
{{> partialName }}
```

Ensure that the partial files are named appropriately (e.g., newsletter.md).
File Structure

```sh
├── bun.build.ts           # Bun.js build configuration
├── dev.ts                 # Development server with live reload
├── content                # Content folder for your site
│   ├── blog
│   ├── index.md
│   └── _partials
├── layouts                # Layout templates (HTML files)
├── public                 # Static assets (images, styles, etc.)
├── README.md              # Project documentation
├── package.json
└── tsconfig.json
```

## Plugins
### Markdown Processor
The Markdown processor plugin parses .md files with frontmatter, processes partials, and renders Mustache templates to generate HTML content.
### Assets Plugin
The assets plugin handles static files such as images, CSS, and JS. It copies these files to the dist directory.

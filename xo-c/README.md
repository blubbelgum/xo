# XO-C: Static Site Generator in C

> **Note: This is an educational project created for learning C programming.**

This implementation of the XO static site generator is primarily a learning exercise to explore:

- Cross-platform C programming techniques
- Systems programming concepts (threads, filesystem, networking)
- Proper abstractions for ensuring code works on both Windows and POSIX-compliant systems
- C build systems using CMake

## Educational Value

This project demonstrates several important programming concepts:

1. **Platform abstraction** - Creating code that works across different operating systems by abstracting platform-specific functionality
2. **Thread safety** - Using thread synchronization primitives to ensure safe concurrent operations
3. **Memory management** - Manual memory allocation and proper resource cleanup
4. **Network programming** - Implementing a basic HTTP server in C
5. **File system operations** - Working with files and directories in a cross-platform way

## Building

This project uses CMake as its build system. To build:

```sh
mkdir build
cd build
cmake ..
cmake --build .
```

## Architecture

The application is organized into several core components:

- **Markdown parser** - Parses markdown content with frontmatter
- **Template engine** - Simple template rendering with variable substitution
- **HTTP server** - Basic web server to serve content and handle live reload
- **File watcher** - Monitors file changes to trigger rebuilds
- **Build system** - Processes content files into HTML output

## Cross-Platform Compatibility

The codebase contains abstractions to work on both Windows and POSIX systems:

- Thread handling (Windows threads vs pthreads)
- File system operations (handling different path separators)
- Network sockets (Winsock vs Berkeley sockets)
- File watching (ReadDirectoryChangesW on Windows vs inotify on Linux)

## Usage

```sh
# Initialize a new site
./xo-c init

# Build the site
./xo-c build  

# Start a development server with live reload
./xo-c dev
```

## License

This code is provided for educational purposes. Feel free to use it for learning and non-commercial projects. 
# Custom C++ UNIX Shell

A lightweight, POSIX-compliant UNIX shell implemented from scratch in C++. This project demonstrates core operating system concepts, including process management, inter-process communication (IPC) via pipes, file descriptor manipulation, and raw terminal I/O handling.

##  Features

* **Process Execution & Pipelining:** Executes external binaries using the `PATH` environment variable. Supports arbitrary-length command pipelining (`|`) using `fork()`, `pipe()`, and `execvp()`.
* **Input Redirection:** Handles standard output and standard error redirection (`>`, `>>`, `1>`, `2>`, `1>>`, `2>>`) allowing output to be truncated or appended to files.
* **Custom Terminal & Autocomplete:** Utilizes `termios` to enable "raw mode," bypassing standard terminal line buffering. This allows for:
    * **Dynamic Tab Completion:** Auto-completes built-in commands and executables in the `PATH`, including longest-common-prefix resolution.
    * **History Navigation:** Up/down arrow key support to cycle through the command history.
* **Robust Parsing:** Custom string tokenization that accurately processes single quotes, double quotes, backslash escaping, and arbitrary whitespace.
* **Built-in Commands:** Native implementations for `cd`, `pwd`, `echo`, `type`, `history`, and `exit`.
* **Persistent History:** Automatically loads and saves command history to the environment's `HISTFILE`.

##  Technical Stack
* **Language:** C++17
* **Standard Libraries:** `<iostream>`, `<vector>`, `<filesystem>`, `<fstream>`
* **System Calls / POSIX APIs:** * Process Control: `fork()`, `execvp()`, `wait()`
    * IPC & I/O: `pipe()`, `dup()`, `dup2()`, `read()`, `open()`, `close()`
    * Terminal Drivers: `<termios.h>`

##  Usage

Compile the shell using `g++` or `clang++`:
```bash
g++ -std=c++17 main.cpp -o myshell
./myshell
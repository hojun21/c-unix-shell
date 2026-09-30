# Unix Shell with Chat Server

A POSIX shell written from scratch in C. It covers the core of a real shell (process creation, N-stage pipelines, background job control, signal handling, variables) and adds a network layer. From inside the shell you can start a TCP chat server with `select()`-based I/O multiplexing, then connect to it as a client or send it one-off messages.

It is built with `-Wall -Wextra -Werror` and AddressSanitizer, LeakSanitizer and UndefinedBehaviorSanitizer turned on.

### Example session

When the shell starts, it shows the `shell$` prompt, which means it is running and waiting for a command. Everything after `shell$` is typed by the user, and the lines without it are the shell's output.

```text
shell$ name=World                  # set a variable
shell$ echo Hello $name            # variable expansion
Hello World
shell$ cat shell.h | wc            # pipeline of two builtins
word count 15
character count 153
newline count 8
shell$ sleep 5 &                   # run in the background: [job number] PID
[1] 49
shell$ ps                          # list background jobs
sleep 49
shell$ start-server 5000           # start the chat server on port 5000
shell$ send 5000 localhost hi-there
client1: hi-there                  # message broadcast by the server
[1]+  Done sleep 5                 # background job finished
```

---

## Features

### Shell core
| Feature | Details |
|---|---|
| **External commands** | `fork()` + `execv()`, resolved from `/bin` and then `/usr/bin`; the parent waits with `waitpid()` and checks the exit status |
| **Pipelines** | Any number of stages (`a \| b \| c \| ...`); builtins and external programs can be mixed in one pipeline; syntax errors are caught (leading, trailing or doubled `\|`) |
| **Background jobs** | `cmd &` prints `[job] pid`; finished jobs are reaped without blocking and reported as `[n]+  Done cmd` before the next prompt |
| **Job control builtins** | `ps` lists the running background jobs; `kill <pid> [signal]` accepts signals 1 to 64, and the signal may itself be a variable (`kill 123 $sig`) |
| **Variables** | `key=value` assignment; `$var` expansion anywhere in a token, including concatenation (`$a$b`, `pre$a`); values can refer to other variables (`b=$a`) |
| **Signals** | Ctrl-C does not kill the shell; SIGTERM and SIGHUP shut down any running server before the shell exits |

### Builtins written from scratch
| Builtin | Details |
|---|---|
| `echo` | Expands variables |
| `ls [path] [--rec] [--d N] [--f substr]` | Recursive traversal with an optional depth limit and a substring filter |
| `cd path` | Accepts `...` (two levels up) and `....` (three levels up), plus variables inside path segments |
| `cat [file]` / `wc [file]` | Read from a file, or from stdin when used in a pipeline; `wc` counts words, characters and newlines |

### Networking
| Builtin | Details |
|---|---|
| `start-server <port>` | Forks a background chat server on a port from 1024 to 65535 |
| `close-server` | Stops the server cleanly with SIGTERM |
| `start-client <port> <host>` | Interactive chat client; Ctrl-C leaves the chat and returns you to the shell |
| `send <port> <host> <msg...>` | Connects, sends one message, and disconnects |
| `\connected` (typed in the client) | The server replies with the number of connected clients |

---

## Architecture

See **[DESIGN.md](DESIGN.md)** for the key design decisions (pipelines, TCP message framing, the `select()` chat server).

## Example commands

See **[COMMAND.md](COMMAND.md)** for example commands and their output, grouped by feature (variables, pipelines, `ls`, background jobs, chat).

---

## Running it yourself

The shell uses POSIX APIs (`fork`, `execv`, `select`, BSD sockets) that **do not exist on native Windows**. On Windows you need either Docker or WSL.

### Option A: Docker (easiest on Windows; tested)

From the project root in PowerShell:

```powershell
docker run --rm -it --name shell -v "${PWD}:/proj" -w /proj/src gcc:13 bash -c "make clean; make && ./shell"
```

- `-it` is required because the shell is interactive.
- Type `exit` to quit (Ctrl-C deliberately does **not** kill the shell).

**To try the chat between two terminals**, keep the container above running, type `start-server 5000` in it, and then open a second PowerShell window:

```powershell
docker exec -it -w /proj/src shell ./shell
```
```text
shell$ start-client 5000 localhost
hello everyone
client1: hello everyone
```

Press Ctrl-C to leave the chat and return to the `shell$` prompt.

### Option B: WSL / Linux / macOS

```bash
sudo apt install build-essential   # first time only (WSL/Ubuntu)
cd src
make clean; make
./shell
```

> macOS: Apple clang does not support `-fsanitize=leak` or `bounds-strict`. Remove them from `CFLAGS` in the `Makefile`.

### Test suite

```bash
cd tests
python3 main_test_runner.py src
cat ../src/FEEDBACK*.txt           # PASS/FAIL for each test, plus a summary
```

---

## Known limitations

These are deliberate scope cuts from the course specification:

- Input lines are limited to 128 characters, and tokens are split on whitespace only (no quoting or escaping).
- External commands are looked up only in `/bin` and `/usr/bin`, not through `$PATH`.
- Each prompt reads input with a single `read()`, so pasting several lines at once will not run them one per line.

---

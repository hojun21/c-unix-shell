# Unix Shell with Chat Server

A POSIX shell written from scratch in C. It covers the core of a real shell (process creation, N-stage pipelines, background job control, signal handling, variables) and adds a network layer. From inside the shell you can start a TCP chat server with `select()`-based I/O multiplexing, then connect to it as a client or send it one-off messages.

It is built with `-Wall -Wextra -Werror` and AddressSanitizer, LeakSanitizer and UndefinedBehaviorSanitizer turned on.

```text
shell$ name=World
shell$ echo Hello $name
Hello World
shell$ cat shell.h | wc
word count 15
character count 150
newline count 8
shell$ sleep 5 &
[1] 49
shell$ ps
sleep 49
shell$ start-server 5000
shell$ send 5000 localhost hi-there
client1: hi-there
[1]+  Done sleep 5
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

## Architecture and design decisions

```
src/
├── shell.c          REPL loop, signal setup, `&` detection, variable expansion pass
├── io_helpers.c    Raw read()/write() I/O and tokenizer
├── commands.c      Dispatcher: builtins, variable assignment, execv, pipelines, job table
├── builtins.c      Builtins, looked up through a function-pointer table
├── variables.c     Variable store (linked list) and $-expansion engine
├── server.c        Forked select() chat server
├── socket.c        Socket setup, CRLF message framing, reliable writes
└── chat_helpers.c  Client list management (linked list), usernames, broadcast writes
```

**1. Function-pointer builtin dispatch.**
Builtins are stored as two parallel arrays, `BUILTINS[]` (names) and `BUILTINS_FN[]` (`ssize_t (*)(char **)` pointers), and the function array ends with a `NULL` sentinel. `check_builtin()` returns either a callable pointer or `NULL`, so the caller has no `if/else` chain. Adding a builtin takes one function plus one entry in each array.

**2. Builtins run inside pipelines.**
Every pipeline stage is a forked child, and a child that is a builtin calls the builtin function directly instead of calling `execv`. This is why `echo hi | cat | wc` works, even though `echo`, `cat` and `wc` are all this shell's own C functions and not the system binaries.

**3. Pipeline file-descriptor handling.**
All N-1 pipes are created before any `fork()`. Each child `dup2()`s only its own read end and write end, then closes every pipe fd. The parent also closes all of them before calling `waitpid()` on each child. If any fd stayed open, readers would never see EOF and the pipeline would hang. The pipeline counts as successful only when every stage exits with status 0.

**4. Non-blocking job reaping.**
Background jobs go in a fixed-size job table where each slot is marked empty, running, done or killed. Before each prompt the shell calls `waitpid(-1, &status, WNOHANG)` in a loop, so zombies are collected without blocking and a finished job is reported exactly once. When every job has finished, the job numbers reset to 1, as they do in bash.

**5. Signal handling that behaves correctly.**
- Handlers are installed with `sigaction()` rather than `signal()`, so the behaviour is portable and well defined.
- The client swaps in its own SIGINT handler, which only sets a `volatile sig_atomic_t` flag (the only async-signal-safe way to communicate out of a handler). It restores the shell's original handler when you leave the chat.
- A `select()` call interrupted by `EINTR` is handled, not treated as an error, so Ctrl-C leaves the chat cleanly.
- The server ignores `SIGPIPE`, so a client that disconnects mid-write cannot crash it. `EPIPE` and `ECONNRESET` are detected, and the dead client is removed.

**6. Single-process, event-driven chat server.**
The server is one forked process running a `select()` loop over the listening socket and all client sockets. It uses no threads and does not fork per client. Clients are kept in a linked list, get automatic usernames (`client1`, `client2`, ...), and every message is broadcast to all of them. `SO_REUSEADDR` lets the port be reused as soon as the server restarts.

**7. Stream framing over TCP.**
TCP is a byte stream, not a message stream, so the protocol ends each message with `\r\n`. Each client has its own buffer. `read_from_socket()` appends whatever bytes arrive, and `get_message()` removes each complete message and `memmove()`s any leftover partial message to the front of the buffer. A message split across several reads, or several messages arriving in one read, are both handled correctly. `write_to_socket()` loops until every byte has been written, because a single `write()` call may send only part of the buffer.

**8. The client multiplexes the keyboard and the network.**
The chat client runs `select()` on both stdin and the socket, so incoming messages appear while you are typing. If an input line is longer than the protocol's message limit, the client sends it in several messages instead of dropping or truncating it.

**9. Memory safety is enforced.**
Every build links ASan, LSan and UBSan (`-fsanitize=address,leak,object-size,bounds-strict,undefined`) and treats all warnings as errors. String operations are bounded (`strncat`, `snprintf`, `strnlen`), and heap memory (variables, expanded tokens, client structs) is freed when the shell exits.

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
- There is no I/O redirection (`>`, `<`) and no `fg`/`bg`.
- Each prompt reads input with a single `read()`, so pasting several lines at once will not run them one per line.

---

*Built for CSC209 (Software Tools and Systems Programming), University of Toronto.*

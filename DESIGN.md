# Architecture and Design Decisions

For an overview and how to run the shell, see the [README](README.md).

## Source layout
```
src/
├── shell.c         REPL loop, signal setup, `&` detection, variable expansion pass
├── io_helpers.c    Raw read()/write() I/O and tokenizer
├── commands.c      Dispatcher: builtins, variable assignment, execv, pipelines, job table
├── builtins.c      Builtins, looked up through a function-pointer table
├── variables.c     Variable store (linked list) and $-expansion engine
├── server.c        Forked select() chat server
├── socket.c        Socket setup, CRLF message framing, reliable writes
└── chat_helpers.c  Client list management (linked list), usernames, broadcast writes
```

# Architecture Design Decisions

## 1. Pipelines

**Design**
- All N-1 pipes are created before any `fork()`, and every stage runs in its own child process.
- Each child `dup2()`s only its own read end and write end, then closes every pipe fd. The parent also closes all of them, then calls `waitpid()` on each child.
- A stage that is a builtin calls the builtin function directly inside its child instead of calling `execv`.

**Why this design**
- Creating every pipe up front lets each child find its neighbours by index (`pipes[i-1]` and `pipes[i]`), so one loop handles any number of stages.
- Running builtins in a child means they behave like any other program in a pipeline. This is why `echo hi | cat | wc` works, even though all three are this shell's own C functions.

**Problem encountered**
- A pipe only reports EOF when *every* write end is closed. If any process (a child, or the shell itself) keeps a stray write end open, the next stage waits forever and the pipeline hangs.
- Fix: every process closes every pipe fd it doesn't use.
- Inputs such as `| ls`, `ls |` and `ls | | wc` are rejected with a syntax error before anything is forked.

## 2. Message framing over TCP

**Design**
- Every message ends with `\r\n`, and each client has its own buffer.
- `read_from_socket()` appends whatever bytes arrive. `get_message()` takes each complete message out and `memmove()`s any leftover partial message to the front of the buffer.
- `write_to_socket()` loops until every byte has been written.

**Why this design**
- The protocol is plain text, so an end-of-message marker is the simplest framing: there's no length header to encode, and messages stay readable when debugging.
- A buffer per client keeps one client's half-received message separate from everyone else's.

**Problem encountered**
- TCP is a byte stream, not a message stream. One `read()` can return half a message, or two and a half messages, so treating each `read()` as one message would cut messages apart or merge them together.
- Fix: the per-client buffering above handles both cases. `write()` may also send only part of the buffer, which is why `write_to_socket()` loops.
- A typed line longer than the protocol limit is sent as several messages instead of being dropped.

## 3. A single-process, event-driven chat server

**Design**
- `start-server` forks one background process that runs a `select()` loop over the listening socket and every client socket, with no threads and no fork per client.
- Clients are kept in a linked list and get automatic usernames (`client1`, `client2`, ...), and every message is broadcast to all of them.
- The chat client also uses `select()`, on stdin and on the socket, so incoming messages appear while you are typing.

**Why this design**
- Forking the server keeps the shell prompt usable while the server runs.
- With a single `select()` loop, all server state (the client list and the buffers) lives in one process and one thread, so no locks are needed, and a broadcast is just a walk down the list.
- Creating a process or thread per client would cost more and would need shared state to broadcast.

**Problem encountered**
- When the server writes to a client that has already disconnected, the kernel sends SIGPIPE, and by default that kills the whole server.
- Fix: the server ignores SIGPIPE, checks each write for `EPIPE` or `ECONNRESET`, and treats a `read()` that returns 0 as a disconnect.
- The dead client's socket is closed, removed from the `select()` set, and unlinked from the list, even in the middle of a broadcast, so every other client stays connected.

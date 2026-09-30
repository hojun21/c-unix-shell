# Example Commands

Example commands to try, grouped by feature. To build and start the shell, see the [README](README.md#running-it-yourself).

Once you see the `shell$` prompt, type these one line at a time. Everything after `shell$` is typed by the user, and the lines without it are the shell's output.

## Echo and variables
```text
shell$ echo Hello World
Hello World
shell$ name=World
shell$ echo Hello $name
Hello World
shell$ greeting=Hi$name
shell$ echo $greeting
HiWorld
```

## Pipelines
`echo`, `cat` and `wc` are this shell's own builtins.
```text
shell$ cat shell.c | wc
shell$ echo one two three | cat | cat | wc
word count 3
character count 14
newline count 1
```

## Recursive `ls`
Depth 2, showing only names that contain `.c`.
```text
shell$ ls --rec .. --d 2 --f .c
```

## Background jobs
Press Enter after a few seconds to see `[n]+  Done ...`. The PIDs you see will be different.
```text
shell$ sleep 5 &
[1] 42
shell$ sleep 8 &
[2] 43
shell$ ps
sleep 42
sleep 43
```

## Chat server
```text
shell$ start-server 5000
shell$ send 5000 localhost hello-from-send
client1: hello-from-send
shell$ close-server
Server 5000 is closed successfully.
```

## Interactive chat (two terminals)
Terminal 1 (Docker):
```text
shell$ start-server 5000
```
Terminal 2, opened with `docker exec -it -w /proj/src shell ./shell`:
```text
shell$ start-client 5000 localhost
hello everyone
client1: hello everyone
```
Press Ctrl-C to leave the chat and return to the `shell$` prompt.

## Exit
Ctrl-C does not close the shell. Use `exit`, which also stops the chat server if one is running.
```text
shell$ exit
```

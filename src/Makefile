CFLAGS = -g -Wall -Wextra -Werror -fsanitize=address,leak,object-size,bounds-strict,undefined -fsanitize-address-use-after-scope

all: shell

shell: shell.o builtins.o commands.o variables.o io_helpers.o server.o socket.o chat_helpers.o
	gcc ${CFLAGS} -o $@ $^ 

%.o: %.c builtins.h commands.h variables.h io_helpers.h socket.h server.h chat_helpers.h shell.h
	gcc ${CFLAGS} -c $< 

clean:
	rm *.o shell

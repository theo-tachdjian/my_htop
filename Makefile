all:
	gcc -o my_htop *.c -Wall -Wextra -Werror -lncurses

clean:
	rm -f my_htop

run: all
	./my_htop

rdebug:
	gdb ./my_htop

rvalgrind:
	valgrind -s --leak-check=full ./my_htop
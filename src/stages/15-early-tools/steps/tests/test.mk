# Rung tools1: make builds a program with a pattern rule and runs it
# (15-early-tools README). The program prints "make ok".
CC = tcc
LDFLAGS = -static

all: hello
	./hello

hello: hello.o
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) -c -o $@ $<

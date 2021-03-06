CC = gcc
CFLAGS = -Wall -Werror -Wextra -O2 -g

all: oss user

oss: main.c oss.h
	$(CC) $(CFLAGS) main.c -o oss

user: user.c oss.h
	$(CC) $(CFLAGS) user.c -o user

clean:
	rm -rf oss user *.txt
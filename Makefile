CC=gcc
CFLAGS=-Os -s
LDFLAGS=-lcurl

all:
	$(CC) main.c $(CFLAGS) $(LDFLAGS) -o t
	strip t

clean:
	rm -f t
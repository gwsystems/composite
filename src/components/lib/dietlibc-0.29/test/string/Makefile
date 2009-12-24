#DIET=diet
CC=gcc
CFLAGS=-Wall

all: stringtest

TESTPROGRAMS=memccpy memchr memcmp memcpy memrchr strcasecmp strcmp strlen strncat strncpy strrchr strstr

stringtest: $(TESTPROGRAMS)

%: %.c
	$(DIET) $(CC) $(CFLAGS) $^ -o $@

clean:
	rm -f $(TESTPROGRAMS)


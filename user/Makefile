CC = gcc
LD = ld

CFLAGS = -Iinclude -ffreestanding -fno-pie -mcmodel=large -mno-red-zone -mno-sse -mno-mmx -mno-sse2 -Wall -Wextra -std=gnu99 -g
LDFLAGS = -nostdlib -T linker.ld

SRCS_S = lib/start.s
SRCS_C = lib/reaper.c lib/string.c paradigm/main.c

OBJS = $(SRCS_S:.s=.o) $(SRCS_C:.c=.o)

OUT = init.elf

all: $(OUT)

$(OUT): $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.s
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(OUT)

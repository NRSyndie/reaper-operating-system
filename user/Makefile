CC = gcc
LD = ld

CFLAGS = -Iinclude -I../shared/include -Ilib -ffreestanding -fno-pie -fno-stack-protector -mcmodel=large -mno-red-zone -mno-sse -mno-mmx -mno-sse2 -Wall -Wextra -std=gnu99 -g
LDFLAGS = -nostdlib -T linker.ld

# Base objects shared by every userspace ELF (startup + shared libs + blake3 + ABI verify).
SRCS_S = lib/start.s
SRCS_C_BASE = lib/reaper.c lib/string.c shared/verify_bootinfo.c \
              lib/blake3/blake3.c lib/blake3/blake3_portable.c lib/blake3/blake3_dispatch.c
OBJS_BASE = $(SRCS_S:.s=.o) $(SRCS_C_BASE:.c=.o)

# Per-daemon objects.
GENESIS_OBJ   = genesis/main.o
PARADIGM_OBJ  = paradigm/main.o

GENESIS_ELF   = genesis.elf
PARADIGM_ELF  = paradigm.elf

all: $(GENESIS_ELF) $(PARADIGM_ELF)

$(GENESIS_ELF): $(OBJS_BASE) $(GENESIS_OBJ)
	$(LD) $(LDFLAGS) -o $@ $(OBJS_BASE) $(GENESIS_OBJ)

$(PARADIGM_ELF): $(OBJS_BASE) $(PARADIGM_OBJ)
	$(LD) $(LDFLAGS) -o $@ $(OBJS_BASE) $(PARADIGM_OBJ)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.s
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS_BASE) $(GENESIS_OBJ) $(PARADIGM_OBJ) $(GENESIS_ELF) $(PARADIGM_ELF)

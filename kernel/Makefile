# Makefile for Reaper-OS kernel

# --- Configuration ---
TARGET = x86_64-elf
PREFIX = /usr/local/cross

# Tools
CC = gcc
AS = as
LD = ld
OBJCOPY = objcopy

# --- Build Flags ---
# -MMD -MP: Essential for automatic dependency generation
CFLAGS = -I. -Iinclude -I../shared/include -g -ffreestanding -fno-stack-protector -fno-pie -mno-red-zone -mno-sse -mno-mmx -mno-sse2 -mcmodel=kernel -Wall -Wextra -std=gnu11 -MMD -MP
ASFLAGS = -g
LDFLAGS = -nostdlib -T linker.ld

# --- Source Files ---
SRCS_C = main.c console.c serial.c pmm.c vmm.c mode.c limine_reqs.c cpu.c pcid.c slab.c capability.c idt.c gdt.c klog.c syscall.c process.c thread.c scheduler.c genesis.c elf.c lattice.c kmalloc.c ocular.c entry.c
SRCS_ASM = interrupts.s

OBJS = $(SRCS_ASM:.s=.o) $(SRCS_C:.c=.o)
DEPS = $(SRCS_C:.c=.d) # Auto-generated dependency files

# --- Output Files ---
KERNEL_ELF = kernel.elf
ISO_FILE = reaper-os.iso
USER_ELF = ../user/init.elf

# --- Logging Configuration ---
LOG_DIR = docs/logs
TEST_NUMBER := 001 # Increment this for each test run
DATE_FORMAT := $(shell date +'%d%m%y')
LOG_FILENAME := $(LOG_DIR)/A-$(strip $(TEST_NUMBER))-$(strip $(DATE_FORMAT)).log

# --- Limine Bootloader Configuration ---
LIMINE_VERSION := 10.6.2
LIMINE_DIR := $(abspath $(CURDIR)/../limine)
LIMINE_INSTALL := $(LIMINE_DIR)/bin/limine

# Limine files needed for ISO
LIMINE_SYS := $(LIMINE_DIR)/bin/limine-bios.sys
LIMINE_BIOS_CD := $(LIMINE_DIR)/bin/limine-bios-cd.bin
LIMINE_UEFI_CD := $(LIMINE_DIR)/bin/limine-uefi-cd.bin

# --- Quiet Build Configuration ---
# Use 'make V=1' for verbose output
ifeq ($(V),1)
	Q =
	VECHO = @true
else
	Q = @
	VECHO = @echo
endif

# --- Main Targets ---
.PHONY: all run iso clean limine_build verify_matrix
all: $(KERNEL_ELF)

LOG_DIR_TARGET := $(LOG_DIR)
$(LOG_DIR_TARGET):
	$(Q)mkdir -p $(LOG_DIR_TARGET)

run: iso $(LOG_DIR_TARGET)
	$(VECHO) "  LOG     (outputting to serial.log)"
	$(Q)qemu-system-x86_64 -cpu Skylake-Client,+pcid,+invpcid,-x2apic,-tsc-deadline,-hle,-rtm,-xsavec -m 512 -cdrom $(ISO_FILE) -no-shutdown -serial file:serial.log

verify_matrix: iso
	$(VECHO) "  VERIFY  Law2+Fate runtime matrix"
	$(Q)../tools/run_law2_fate_matrix.sh --runs 3 --timeout 35 --iso $(ISO_FILE) --out-dir .

# Build Limine from source using local directory
limine_build:
	$(VECHO) "  BUILD   Limine"
	$(Q)cd $(LIMINE_DIR) && ./bootstrap && ./configure --enable-bios --enable-bios-cd --enable-uefi-x86-64 --enable-uefi-cd && $(MAKE)

# New ISO target for Limine
iso: $(ISO_FILE)

$(ISO_FILE): $(KERNEL_ELF) $(USER_ELF)
	$(VECHO) "  BUILD   User Space"
	$(Q)$(MAKE) -C ../user
	$(VECHO) "  ISO     $(ISO_FILE) (with Limine)"
	$(Q)mkdir -p isofiles/boot
	$(Q)mkdir -p isofiles/EFI/BOOT
	$(Q)cp $(KERNEL_ELF) isofiles/boot/kernel.elf
	$(Q)cp $(USER_ELF) isofiles/boot/init.elf
	$(Q)cp $(LIMINE_SYS) isofiles/
	$(Q)cp $(LIMINE_BIOS_CD) isofiles/
	$(Q)cp $(LIMINE_UEFI_CD) isofiles/
	$(Q)echo 'timeout: 0' > isofiles/limine.conf
	$(Q)echo 'verbose: yes' >> isofiles/limine.conf
	$(Q)echo 'serial: yes' >> isofiles/limine.conf
	$(Q)echo '' >> isofiles/limine.conf
	$(Q)echo '/Reaper-OS' >> isofiles/limine.conf
	$(Q)echo '    protocol: limine' >> isofiles/limine.conf
	$(Q)echo '    kaslr: no' >> isofiles/limine.conf
	$(Q)echo '    path: boot():/boot/kernel.elf' >> isofiles/limine.conf
	$(Q)echo '    module_path: boot():/boot/init.elf' >> isofiles/limine.conf
	$(Q)mkdir -p isofiles/limine
	$(Q)cp isofiles/limine.conf isofiles/boot/limine.conf
	$(Q)cp isofiles/limine.conf isofiles/limine/limine.conf
	$(Q)xorriso -as mkisofs -r -J -b limine-bios-cd.bin -no-emul-boot -boot-load-size 4 -boot-info-table --efi-boot limine-uefi-cd.bin -efi-boot-part --efi-boot-image --protective-msdos-label isofiles -o $(ISO_FILE)
	$(Q)$(LIMINE_INSTALL) bios-install $(ISO_FILE)

clean:
	$(VECHO) "  CLEAN"
	$(Q)rm -f $(OBJS) $(DEPS) $(KERNEL_ELF) $(ISO_FILE) $(LIMINE_TARBALL)
	$(Q)rm -rf isofiles
	$(Q)$(MAKE) -C ../user clean

# --- Compilation & Linking ---
$(KERNEL_ELF): $(OBJS) linker.ld
	$(VECHO) "  LD      $@"
	$(Q)$(LD) $(LDFLAGS) -o $@ $(OBJS)

%.o: %.c
	$(VECHO) "  CC      $<"
	$(Q)$(CC) $(CFLAGS) -c $< -o $@

%.o: %.s
	$(VECHO) "  AS      $<"
	$(Q)$(AS) $(ASFLAGS) $< -o $@

# Include the auto-generated dependency files
-include $(DEPS)

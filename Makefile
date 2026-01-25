# SpecTalkZX Makefile (modular build)
# Default build: SDCC (sdcc_iy) + divMMC/divTiesus UART
#
# Targets:
#   make        -> default (sdcc_iy, divMMC/divTiesus)
#   make ay     -> sdcc_iy, AY bit-bang UART
#   make clean  -> remove build artifacts

ZCC      = zcc
PLATFORM = +zx

# Memory model (keep heaps at 0, small stack, no printf/scanf)
PRAGMAS = -pragma-define:CLIB_MALLOC_HEAP_SIZE=0 \
          -pragma-define:CLIB_STDIO_HEAP_SIZE=0 \
          -pragma-define:CRT_STACK_SIZE=256 \
          -pragma-define:CLIB_OPT_PRINTF=0 \
          -pragma-define:CLIB_OPT_SCANF=0 \
          -pragma-define:CRT_ENABLE_STDIO=0

# Common flags for both targets
CFLAGS_COMMON = -vn -SO3 -startup=0 -compiler=sdcc -clib=sdcc_iy -zorg=24000 \
               --opt-code-size --max-allocs-per-node200000


# Linker flags (same for both targets)
LDFLAGS = -Wl,--gc-section -Wall

# Source files
C_SOURCES = src/spectalk.c src/irc_handlers.c src/user_cmds.c
ASM_COMMON = asm/spectalk_asm.asm
HEADERS = include/spectalk.h include/font64_data.h include/themes.h

# Default target: divMMC/divTiesus hardware UART
all: SpecTalkZX.tap

SpecTalkZX.tap: $(C_SOURCES) $(ASM_COMMON) asm/divmmc_uart.asm $(HEADERS)
	$(ZCC) $(PLATFORM) $(CFLAGS_COMMON) $(PRAGMAS) $(LDFLAGS) \
		$(C_SOURCES) asm/divmmc_uart.asm $(ASM_COMMON) \
		-m -o SpecTalkZX -create-app

# AY bit-bang UART target
ay: SpecTalkZX_AY.tap

SpecTalkZX_AY.tap: $(C_SOURCES) $(ASM_COMMON) ay_uart.asm $(HEADERS)
	$(ZCC) $(PLATFORM) $(CFLAGS_COMMON) $(PRAGMAS) $(LDFLAGS) \
		$(C_SOURCES) ay_uart.asm $(ASM_COMMON) \
		-m -o SpecTalkZX_AY -create-app

clean:
	rm -f *.tap *.bin *_*.bin *.o *.lis *.map *.err *.sym

.PHONY: all ay clean

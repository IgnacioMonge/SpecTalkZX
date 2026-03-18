\
# ============================================================
# SpecTalkZX Makefile (w64devkit-friendly, ASCII-safe)
# Default pipeline: CHECK -> CLEAN -> BUILD -> INFO
# - Spinner (works with /bin/sh)
# - Preflight dependency checks
# - `make` runs clean automatically
# ============================================================

.DEFAULT_GOAL := all

# ------------------------------------------------------------
# Toolchain / target
# ------------------------------------------------------------
CC      = zcc
TARGET  = +zx

# ------------------------------------------------------------
# Project
# ------------------------------------------------------------
OUTPUT  = SpecTalkZX
TAP     = $(BUILD_DIR)/$(OUTPUT).tap
MAP     = $(OUTPUT).map
BUILD_DIR = build
LOG     = $(BUILD_DIR)/build.log

# ------------------------------------------------------------
# Sources
# ------------------------------------------------------------
C_SOURCES    = src/main_build.c

# ------------------------------------------------------------
# Build options
# ------------------------------------------------------------
AY_UART     ?= 0

ifeq ($(AY_UART),1)
ASM_SOURCES  = asm/ay_uart.asm asm/spectalk_asm.asm
else
ASM_SOURCES  = asm/divmmc_uart.asm asm/spectalk_asm.asm
endif
ZORG        = 24000
STACK_SIZE  = 512

EXTRA_CFLAGS ?=
BUILD_PROFILE ?= NORMAL
CFLAGS = -vn -SO3 -startup=31 -compiler=sdcc -clib=sdcc_iy \
         -zorg=$(ZORG) --opt-code-size --fomit-frame-pointer \
         -DST_UART_AY=$(AY_UART)  -Cc--Werror \
         -custom-copt-rules=src/spectalk_copt.rul \
         -pragma-define:CLIB_MALLOC_HEAP_SIZE=0 \
         -pragma-define:CLIB_STDIO_HEAP_SIZE=0 \
         -pragma-define:CRT_STACK_SIZE=$(STACK_SIZE) \
         -pragma-define:CRT_ENABLE_STDIO=0 \
         -Wl,--gc-sections -Wall
# ------------------------------------------------------------
# Helpers
# ------------------------------------------------------------
UART_DESC = $(if $(filter 1,$(AY_UART)),AY bit-bang (9600 baud),divMMC/divTiesus (115200 baud))
SIZE_TAP  = wc -c < "$(TAP)"
BUILD_CMD = $(CC) $(TARGET) $(CFLAGS) $(EXTRA_CFLAGS) $(C_SOURCES) $(ASM_SOURCES) -m -o $(OUTPUT) -create-app

# ------------------------------------------------------------
# ANSI colors (disable with NO_COLOR=1)
# IMPORTANT: keep escapes as *literal* backslashes to avoid embedding ESC/CR chars in Makefile.
# ------------------------------------------------------------
ifeq ($(NO_COLOR),1)
C_RESET :=
C_BOLD  :=
C_DIM   :=
C_RED   :=
C_GRN   :=
C_YEL   :=
C_BLU   :=
C_CYNB  :=
else
C_RESET := \\033[0m
C_BOLD  := \\033[1m
C_DIM   := \\033[2m
C_RED   := \\033[31m
C_GRN   := \\033[32m
C_YEL   := \\033[33m
C_BLU   := \\033[34m
C_CYNB  := \\033[96m
endif

# ------------------------------------------------------------
# Pretty printing
# ------------------------------------------------------------
define HR
	@printf "$(C_DIM)============================================================$(C_RESET)\n"
endef

define STEP
	@printf "$(C_BOLD)$(C_BLU)[%s]$(C_RESET) %s\n" "$(1)" "$(2)"
endef

define OK
	@printf "$(C_GRN)[OK]$(C_RESET) %s\n" "$(1)"
endef

define WARN
	@printf "$(C_YEL)[WARN]$(C_RESET) %s\n" "$(1)"
endef

define ERR
	@printf "$(C_RED)[ERR]$(C_RESET) %s\n" "$(1)"
endef


# ------------------------------------------------------------
# Phony targets
# ------------------------------------------------------------
.PHONY: all check clean bpe build trim info help ay divmmc AY DIVMMC release RELEASE

# ------------------------------------------------------------
# Default pipeline
# ------------------------------------------------------------
all: check clean bpe build trim info
nobpe: check clean build trim info

# Convenience targets
ay:
	@$(MAKE) AY_UART=1 all
divmmc:
	@$(MAKE) AY_UART=0 all
AY:
	@$(MAKE) ay
DIVMMC:
	@$(MAKE) divmmc

help:
	$(call HR)
	@printf "Targets:\n"
	@printf "  make            - CHECK -> CLEAN -> BUILD -> INFO (divmmc by default)\n"
	@printf "  make ay         - Build for AY (AY_UART=1)\n"
	@printf "  make divmmc     - Build for divMMC (AY_UART=0)\n"
	@printf "  make check      - Preflight dependency checks\n"
	@printf "  make clean      - Remove build artifacts\n"
	@printf "  make build      - Build $(TAP)\n"
	@printf "  make info       - Print build info (requires $(TAP))\n"
	@printf "\nOptions:\n"
	@printf "  NO_COLOR=1      - Disable ANSI colors\n"
	$(call HR)

# ------------------------------------------------------------
# CHECK phase
# ------------------------------------------------------------
check:
	$(call HR)
	@printf "$(C_BOLD)$(C_CYNB)SpecTalkZX - Build Pipeline$(C_RESET)\n"
	$(call HR)
	$(call STEP,0/4,Checking toolchain and sources)
	@mkdir -p $(BUILD_DIR)
	@sh -c '\
		fail=0; \
		for t in zcc wc sh; do \
			command -v "$$t" >/dev/null 2>&1 || { echo "[ERR] Missing tool: $$t"; fail=1; }; \
		done; \
		for f in $(C_SOURCES) $(ASM_SOURCES); do \
			[ -f "$$f" ] || { echo "[ERR] Missing file: $$f"; fail=1; }; \
		done; \
		[ "$$fail" = "0" ] || exit 2; \
	'
	$(call OK,Dependencies OK)
	$(call HR)

# ------------------------------------------------------------
# CLEAN phase
# ------------------------------------------------------------
clean:
	$(call STEP,1/4,Cleaning)
	@echo "Cleaning build artifacts..."
	@rm -f "$(OUTPUT)" "$(OUTPUT).tap" "$(TAP)" "$(MAP)" "$(LOG)" "$(BUILD_DIR)/SPECTALK.DAT" *.o *.bin *.sym 2>/dev/null || true
	@rm -rf "$(BUILD_DIR)/bpe_src" "$(BUILD_DIR)/bpe_final" "$(BUILD_DIR)/bpe_dict.bin" 2>/dev/null || true
	$(call OK,Clean complete.)
	$(call HR)

# ------------------------------------------------------------
# BPE phase - compress screen-only strings
# Reads src/*.c (originals), generates build/bpe_final/*.c (compressed)
# Also generates SPECTALK.DAT with BPE dict inserted
# ------------------------------------------------------------
bpe:
	$(call STEP,2/4,BPE compression)
	@mkdir -p $(BUILD_DIR)/bpe_originals
	@cp src/spectalk.c src/irc_handlers.c src/user_cmds.c $(BUILD_DIR)/bpe_originals/
	@cp include/spectalk.h $(BUILD_DIR)/bpe_originals/
	@cp src/SPECTALK.DAT $(BUILD_DIR)/bpe_originals/
	@python tools/bpe_build.py
	$(call OK,BPE complete.)
	$(call HR)

# ------------------------------------------------------------
# BUILD phase (only place where zcc is invoked)
# ------------------------------------------------------------
build: $(TAP)

$(TAP): $(C_SOURCES) $(ASM_SOURCES)
	$(call STEP,3/4,Build)
	@echo "Compiling SpecTalkZX..."
	@echo "UART mode: $(UART_DESC)"
	@if [ "$(BUILD_PROFILE)" = "RELEASE" ]; then printf "$(C_BOLD)$(C_YEL)Build profile: RELEASE$(C_RESET)\n"; fi
	@echo "Log: $(LOG)"
	@$(BUILD_CMD) 2>&1 | tee "$(LOG)" || (printf "$(C_RED)[FAILED]$(C_RESET) Compilation errors (see $(LOG)):\n" && tail -20 "$(LOG)" && exit 1)
	@mv $(OUTPUT).tap $(TAP)
	@cp $(BUILD_DIR)/SPECTALK.DAT $(BUILD_DIR)/ 2>/dev/null || true
	@if [ -d "$(BUILD_DIR)/bpe_originals" ]; then \
		cp $(BUILD_DIR)/bpe_originals/spectalk.c src/; \
		cp $(BUILD_DIR)/bpe_originals/irc_handlers.c src/; \
		cp $(BUILD_DIR)/bpe_originals/user_cmds.c src/; \
		cp $(BUILD_DIR)/bpe_originals/spectalk.h include/; \
		cp $(BUILD_DIR)/bpe_originals/SPECTALK.DAT src/; \
	fi
	$(call OK,Build complete.)
	$(call HR)

# ------------------------------------------------------------
# TRIM phase - strip BSS zeros from TAP (saves ~4KB)
# BSS is zeroed at startup by code_crt_init in spectalk_asm.asm
# ------------------------------------------------------------
trim: $(TAP) $(MAP)
	@sh -c ' \
	  bss=$$(grep "__data_compiler_tail" $(MAP) | grep -o "\$$[0-9A-Fa-f]*" | head -1 | tr -d "\$$"); \
	  if [ -z "$$bss" ]; then \
	    printf "$(C_YEL)[WARN]$(C_RESET) BSS trim skipped (symbol not found in map)\n"; \
	    exit 0; \
	  fi; \
	  trim=$$((0x$$bss - $(ZORG))); \
	  bin="$(OUTPUT)_CODE.bin"; \
	  if [ ! -f "$$bin" ]; then bin="$(OUTPUT)"; fi; \
	  if [ ! -f "$$bin" ]; then \
	    printf "$(C_YEL)[WARN]$(C_RESET) BSS trim skipped (binary not found)\n"; \
	    exit 0; \
	  fi; \
	  full=$$(wc -c < "$$bin"); \
	  saved=$$((full - trim)); \
	  head -c $$trim "$$bin" > $(BUILD_DIR)/trimmed.bin; \
	  z88dk-appmake +zx -b $(BUILD_DIR)/trimmed.bin --org $(ZORG) --blockname SpecTalkZX --usraddr $(ZORG) -o $(TAP) 2>/dev/null; \
	  rm -f $(BUILD_DIR)/trimmed.bin; \
	  printf "$(C_GRN)[OK]$(C_RESET) BSS trimmed: %d -> %d bytes (-%d bytes of zeros)\n" "$$full" "$$trim" "$$saved"; \
	'

# ------------------------------------------------------------
# INFO phase (colored, no redundant "(SpecTalkZX.tap)")
# ------------------------------------------------------------
info: $(TAP)
	$(call STEP,3/3,Info)
	@printf "$(C_BOLD)Output:$(C_RESET) $(C_YEL)%s$(C_RESET)\n" "$(TAP)"
	@printf "$(C_BOLD)Memory map:$(C_RESET) $(C_YEL)%s$(C_RESET)\n" "$(MAP)"
	@printf "$(C_BOLD)Build log:$(C_RESET) $(C_YEL)%s$(C_RESET)\n" "$(LOG)"
	@printf "$(C_BOLD)UART mode:$(C_RESET) $(C_YEL)%s$(C_RESET)\n" "$(UART_DESC)"
	@if [ "$(BUILD_PROFILE)" = "RELEASE" ]; then printf "$(C_BOLD)Build profile:$(C_RESET) $(C_YEL)RELEASE$(C_RESET)\n"; fi
	@printf "$(C_BOLD)Code origin:$(C_RESET) $(C_YEL)%s$(C_RESET)\n" "$(ZORG)"
	@printf "$(C_BOLD)Stack size:$(C_RESET) $(C_YEL)%s bytes$(C_RESET)\n" "$(STACK_SIZE)"
	@printf "$(C_BOLD)Binary TAP size:$(C_RESET) $(C_YEL)%s bytes$(C_RESET)\n" "$$($(SIZE_TAP))"
	$(call HR)

release:
	@$(MAKE) BUILD_PROFILE=RELEASE EXTRA_CFLAGS="--max-allocs-per-node200000" all
release-ay:
	@$(MAKE) BUILD_PROFILE=RELEASE EXTRA_CFLAGS="--max-allocs-per-node200000" AY_UART=1 all
RELEASE:
	@$(MAKE) release

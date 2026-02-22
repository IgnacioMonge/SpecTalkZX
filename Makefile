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
TAP     = $(OUTPUT).tap
MAP     = $(OUTPUT).map
BUILD_DIR = build
LOG     = $(BUILD_DIR)/build.log

# ------------------------------------------------------------
# Sources
# ------------------------------------------------------------
C_SOURCES    = src/main_build.c
ASM_SOURCES  = asm/divmmc_uart.asm asm/spectalk_asm.asm

# ------------------------------------------------------------
# Build options
# ------------------------------------------------------------
AY_UART     ?= 0
ZORG        = 24000
STACK_SIZE  = 512

EXTRA_CFLAGS ?= 
BUILD_PROFILE ?= NORMAL
CFLAGS = -vn -SO3 -startup=0 -compiler=sdcc -clib=sdcc_iy \
         -zorg=$(ZORG) --opt-code-size --fomit-frame-pointer \
         -DST_UART_AY=$(AY_UART)  -Cc--Werror \
         -pragma-define:CLIB_MALLOC_HEAP_SIZE=0 \
         -pragma-define:CLIB_STDIO_HEAP_SIZE=0 \
         -pragma-define:CRT_STACK_SIZE=$(STACK_SIZE) \
         -pragma-define:CRT_ENABLE_STDIO=0 \
         -Wl,--gc-section -Wall 
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
# Spinner (POSIX sh)
# - zcc output redirected to build.log so spinner is visible
# - hides cursor while spinning
# ------------------------------------------------------------
\
\
define RUN_SPINNER
	@sh -c 'i=0; printf "\033[?25l"; spin() { while :; do case $$((i % 4)) in 0) c="|" ;; 1) c="/" ;; 2) c="-" ;; 3) c="\\" ;; esac; i=$$((i+1)); printf "\033[?25l\r\033[K$(C_YEL)%s$(C_RESET) %s" "$(2)" "$$c"; sleep 0.12; done }; spin & spid=$$!; trap "kill $$spid 2>/dev/null; wait $$spid 2>/dev/null; printf \"\r\033[K\033[?25h\"; exit 130" INT TERM; ( $(1) ); rc=$$?; kill $$spid 2>/dev/null; wait $$spid 2>/dev/null; printf "\r\033[K\033[?25h"; if [ "$$rc" = "0" ]; then printf "$(C_GRN)%s$(C_RESET)\n" "$(3)"; else printf "$(C_RED)%s$(C_RESET)\n" "$(4)"; fi; exit "$$rc"'
endef



# ------------------------------------------------------------
# Phony targets
# ------------------------------------------------------------
.PHONY: all check clean build info help ay divmmc AY DIVMMC release RELEASE

# ------------------------------------------------------------
# Default pipeline
# ------------------------------------------------------------
all: check clean build info

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
	$(call STEP,0/3,Checking toolchain and sources)
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
	$(call STEP,1/3,Cleaning)
	@echo "Cleaning build artifacts..."
	@rm -f "$(OUTPUT)" "$(TAP)" "$(MAP)" "$(LOG)" *.o *.bin *.sym 2>/dev/null || true
	$(call OK,Clean complete.)
	$(call HR)

# ------------------------------------------------------------
# BUILD phase (only place where zcc is invoked)
# ------------------------------------------------------------
build: $(TAP)

$(TAP): $(C_SOURCES) $(ASM_SOURCES)
	$(call STEP,2/3,Build)
	@echo "Compiling SpecTalkZX..."
	@echo "UART mode: $(UART_DESC)"
	@if [ "$(BUILD_PROFILE)" = "RELEASE" ]; then printf "$(C_BOLD)$(C_YEL)Build profile: RELEASE$(C_RESET)\n"; fi
	@echo "Log: $(LOG)"
	@$(BUILD_CMD) 2>&1 | tee "$(LOG)" || (printf "$(C_RED)[FAILED]$(C_RESET) Compilation errors (see $(LOG)):\n" && tail -20 "$(LOG)" && exit 1)
	$(call OK,Build complete.)
	$(call HR)

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
	@$(MAKE) BUILD_PROFILE=RELEASE EXTRA_CFLAGS=--max-allocs-per-node200000 all
RELEASE:
	@$(MAKE) release

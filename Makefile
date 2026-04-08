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
PYTHON  ?= python

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
ASM_SOURCES  = asm/divmmc_uart.asm asm/spectalk_asm.asm asm/overlay_loader.asm
ZORG        = 24000
STACK_SIZE  = 512

EXTRA_CFLAGS ?=
BUILD_PROFILE ?= NORMAL
CFLAGS = -vn -SO3 -startup=31 -compiler=sdcc -clib=sdcc_iy \
         -zorg=$(ZORG) --opt-code-size --fomit-frame-pointer \
         -Cc--Werror \
         -custom-copt-rules=src/spectalk_copt.rul \
         -pragma-define:CLIB_MALLOC_HEAP_SIZE=0 \
         -pragma-define:CLIB_STDIO_HEAP_SIZE=0 \
         -pragma-define:CRT_STACK_SIZE=$(STACK_SIZE) \
         -pragma-define:CRT_ENABLE_STDIO=0 \
         -Wl,--gc-sections -Wall
# ------------------------------------------------------------
# Helpers
# ------------------------------------------------------------
UART_DESC = divMMC/divTiesus (115200 baud)
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
.PHONY: all check clean bpe build trim overlay info help release RELEASE nobpe

# ------------------------------------------------------------
# Default pipeline
# ------------------------------------------------------------
all: check clean bpe build trim overlay info
nobpe: check clean build trim overlay info

help:
	$(call HR)
	@printf "Targets:\n"
	@printf "  make            - CHECK -> CLEAN -> BUILD -> INFO\n"
	@printf "  make release    - Release build (max optimization)\n"
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
	@if [ -d "$(BUILD_DIR)/bpe_originals" ]; then \
		echo "  Restoring src/ from bpe_originals (safety)..."; \
		cp $(BUILD_DIR)/bpe_originals/spectalk.c src/ 2>/dev/null || true; \
		cp $(BUILD_DIR)/bpe_originals/irc_handlers.c src/ 2>/dev/null || true; \
		cp $(BUILD_DIR)/bpe_originals/user_cmds.c src/ 2>/dev/null || true; \
		cp $(BUILD_DIR)/bpe_originals/spectalk.h include/ 2>/dev/null || true; \
		cp $(BUILD_DIR)/bpe_originals/overlay_api.h overlay/ 2>/dev/null || true; \
		cp $(BUILD_DIR)/bpe_originals/SPECTALK.DAT src/ 2>/dev/null || true; \
	fi
	@rm -f "$(OUTPUT)" "$(OUTPUT).tap" "$(TAP)" "$(MAP)" "$(LOG)" "$(BUILD_DIR)/SPECTALK.DAT" *.o *.bin *.sym 2>/dev/null || true
	@rm -rf "$(BUILD_DIR)/bpe_src" "$(BUILD_DIR)/bpe_final" "$(BUILD_DIR)/bpe_dict.bin" "$(BUILD_DIR)/bpe_originals" 2>/dev/null || true
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
	@cp overlay/overlay_api.h $(BUILD_DIR)/bpe_originals/
	@$(PYTHON) tools/bpe_build.py
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
	@build_rc=0; $(BUILD_CMD) 2>&1 | tee "$(LOG)" || build_rc=$$?; \
	if [ -d "$(BUILD_DIR)/bpe_originals" ]; then \
		cp $(BUILD_DIR)/bpe_originals/spectalk.c src/; \
		cp $(BUILD_DIR)/bpe_originals/irc_handlers.c src/; \
		cp $(BUILD_DIR)/bpe_originals/user_cmds.c src/; \
		cp $(BUILD_DIR)/bpe_originals/spectalk.h include/; \
		cp $(BUILD_DIR)/bpe_originals/overlay_api.h overlay/; \
		cp $(BUILD_DIR)/bpe_originals/SPECTALK.DAT src/; \
		dat_sz=$$(wc -c < src/SPECTALK.DAT); \
		if [ "$$dat_sz" -ne 1517 ]; then \
			printf "$(C_RED)[BPE CORRUPT]$(C_RESET) src/SPECTALK.DAT is $$dat_sz bytes after restore (expected 1517)\n"; \
			printf "  bpe_originals was corrupted. Restore manually from Development/\n"; \
			exit 1; \
		fi; \
		rm -rf $(BUILD_DIR)/bpe_originals; \
	fi; \
	if [ "$$build_rc" -ne 0 ]; then \
		printf "$(C_RED)[FAILED]$(C_RESET) Compilation errors (see $(LOG)):\n"; \
		tail -20 "$(LOG)"; \
		exit 1; \
	fi
	@mv $(OUTPUT).tap $(TAP)
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
	  trim=$$($(PYTHON) -c "print(0x$$bss - $(ZORG))"); \
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
	  bss_end=$$(grep "__BSS_END_tail" $(MAP) | grep -o "\$$[0-9A-Fa-f]*" | head -1 | tr -d "\$$"); \
	  if [ -n "$$bss_end" ]; then \
	    bss_dec=$$($(PYTHON) -c "print(0x$$bss_end)"); \
	    if [ "$$bss_dec" -ge 62720 ]; then \
	      printf "$(C_RED)[FATAL]$(C_RESET) BSS_END_tail (0x$$bss_end) >= ring_buffer (0xF500). BSS overflow!\n"; \
	      exit 1; \
	    fi; \
	    free=$$((62720 - bss_dec)); \
	    printf "$(C_GRN)[OK]$(C_RESET) BSS guard: 0x$$bss_end < 0xF500 ($$free bytes free)\n"; \
	  fi; \
	'

# ------------------------------------------------------------
# OVERLAY phase - compile help_overlay.c against resident symbols
# Pipeline: .map -> overlay_defs.asm -> compile C -> link OVL
# ------------------------------------------------------------
OVL_SRC   = overlay/spectalk_ovl.c
OVL_ENTRY = overlay/overlay_entry.asm
OVL_API   = overlay/overlay_api.h
OVL_OUT   = $(BUILD_DIR)/SPCTLK1.OVL
OVL_DEFS  = $(BUILD_DIR)/overlay_defs.asm

overlay: $(MAP)
	$(call STEP,OVL,Building overlay)
	@SLOT=$$(grep '_ring_buffer ' $(MAP) | sed -n 's/.*= \$$\([0-9A-Fa-f]*\).*/\1/p' | head -1); \
	if [ -z "$$SLOT" ]; then \
		printf "$(C_RED)[ERR]$(C_RESET) _overlay_slot not found in $(MAP)\n"; \
		exit 1; \
	fi; \
	echo "  overlay_slot = 0x$$SLOT"; \
	$(PYTHON) tools/gen_overlay_defs.py $(MAP) > $(OVL_DEFS); \
	echo "  overlay_defs.asm generated"; \
	zcc +z80 -clib=sdcc_iy --no-crt --opt-code-size \
		-Ioverlay -c $(OVL_SRC) -o $(BUILD_DIR)/spectalk_ovl.o 2>&1 || exit 1; \
	echo "  spectalk_ovl.c compiled"; \
	z80asm -I$(BUILD_DIR) $(OVL_ENTRY) 2>&1 || exit 1; \
	z80asm $(OVL_DEFS) 2>&1 || exit 1; \
	z80asm -b -r0x$$SLOT -o=$(OVL_OUT) \
		overlay/overlay_entry.o \
		$(BUILD_DIR)/spectalk_ovl.o \
		$(BUILD_DIR)/overlay_defs.o 2>&1 || exit 1; \
	ovl_size=$$(wc -c < $(OVL_OUT)); \
	if [ "$$ovl_size" -gt 2048 ]; then \
		printf "$(C_RED)[ERR]$(C_RESET) Overlay too large: $$ovl_size bytes (max 2048)\n"; \
		exit 1; \
	fi; \
	printf "$(C_GRN)[OK]$(C_RESET) SPCTLK1.OVL: $$ovl_size bytes (max 2048)\n"; \
	echo "  Building SPCTLK2.OVL..."; \
	zcc +z80 -clib=sdcc_iy --no-crt --opt-code-size \
		-Ioverlay -c overlay/spectalk_ovl2.c -o $(BUILD_DIR)/spectalk_ovl2.o 2>&1 || exit 1; \
	z80asm -I$(BUILD_DIR) overlay/overlay_entry2.asm 2>&1 || exit 1; \
	z80asm -I$(BUILD_DIR) overlay/globe_render.asm 2>&1 || exit 1; \
	z80asm -b -r0x$$SLOT -o=$(BUILD_DIR)/SPCTLK2.OVL \
		overlay/overlay_entry2.o \
		$(BUILD_DIR)/spectalk_ovl2.o \
		overlay/globe_render.o \
		$(BUILD_DIR)/overlay_defs.o 2>&1 || exit 1; \
	ovl2_size=$$(wc -c < $(BUILD_DIR)/SPCTLK2.OVL); \
	if [ "$$ovl2_size" -gt 2048 ]; then \
		printf "$(C_RED)[ERR]$(C_RESET) SPCTLK2.OVL too large: $$ovl2_size bytes (max 2048)\n"; \
		exit 1; \
	fi; \
	printf "$(C_GRN)[OK]$(C_RESET) SPCTLK2.OVL: $$ovl2_size bytes (max 2048)\n"; \
	echo "  Building SPCTLK3.OVL..."; \
	$(PYTHON) tools/gen_whatsnew.py 2>&1 || exit 1; \
	zcc +z80 -clib=sdcc_iy --no-crt --opt-code-size \
		-Ioverlay -c overlay/spectalk_ovl3.c -o $(BUILD_DIR)/spectalk_ovl3.o 2>&1 || exit 1; \
	z80asm -I$(BUILD_DIR) overlay/overlay_entry3.asm 2>&1 || exit 1; \
	z80asm -b -r0x$$SLOT -o=$(BUILD_DIR)/SPCTLK3.OVL \
		overlay/overlay_entry3.o \
		$(BUILD_DIR)/spectalk_ovl3.o \
		$(BUILD_DIR)/overlay_defs.o 2>&1 || exit 1; \
	ovl3_size=$$(wc -c < $(BUILD_DIR)/SPCTLK3.OVL); \
	if [ "$$ovl3_size" -gt 2048 ]; then \
		printf "$(C_RED)[ERR]$(C_RESET) SPCTLK3.OVL too large: $$ovl3_size bytes (max 2048)\n"; \
		exit 1; \
	fi; \
	printf "$(C_GRN)[OK]$(C_RESET) SPCTLK3.OVL: $$ovl3_size bytes (max 2048)\n"; \
	echo "  Building SPCTLK4.OVL..."; \
	zcc +z80 -clib=sdcc_iy --no-crt --opt-code-size \
		-Ioverlay -c overlay/spectalk_ovl4.c -o $(BUILD_DIR)/spectalk_ovl4.o 2>&1 || exit 1; \
	z80asm -I$(BUILD_DIR) overlay/overlay_entry4.asm 2>&1 || exit 1; \
	z80asm -b -r0x$$SLOT -o=$(BUILD_DIR)/SPCTLK4.OVL \
		overlay/overlay_entry4.o \
		$(BUILD_DIR)/spectalk_ovl4.o \
		$(BUILD_DIR)/overlay_defs.o 2>&1 || exit 1; \
	ovl4_size=$$(wc -c < $(BUILD_DIR)/SPCTLK4.OVL); \
	if [ "$$ovl4_size" -gt 2048 ]; then \
		printf "$(C_RED)[ERR]$(C_RESET) SPCTLK4.OVL too large: $$ovl4_size bytes (max 2048)\n"; \
		exit 1; \
	fi; \
	printf "$(C_GRN)[OK]$(C_RESET) SPCTLK4.OVL: $$ovl4_size bytes (max 2048)\n"; \
	echo "  Packing SPECTALK.OVL..."; \
	dd if=$(BUILD_DIR)/SPCTLK1.OVL of=$(BUILD_DIR)/SPECTALK.OVL bs=2048 conv=sync 2>/dev/null; \
	dd if=$(BUILD_DIR)/SPCTLK2.OVL of=$(BUILD_DIR)/SPECTALK.OVL bs=2048 conv=sync seek=1 2>/dev/null; \
	dd if=$(BUILD_DIR)/SPCTLK3.OVL of=$(BUILD_DIR)/SPECTALK.OVL bs=2048 conv=sync seek=2 2>/dev/null; \
	dd if=$(BUILD_DIR)/SPCTLK4.OVL of=$(BUILD_DIR)/SPECTALK.OVL bs=2048 conv=sync seek=3 2>/dev/null; \
	printf "$(C_GRN)[OK]$(C_RESET) SPECTALK.OVL: $$(wc -c < $(BUILD_DIR)/SPECTALK.OVL) bytes (4 x 2048)\n"; \
	rm -f $(BUILD_DIR)/SPCTLK[1-4].OVL; \
	echo "  Cleaning build intermediates..."; \
	rm -f $(BUILD_DIR)/*.o $(BUILD_DIR)/*.asm $(BUILD_DIR)/*.bin \
		$(BUILD_DIR)/SPECTALK $(BUILD_DIR)/SP2.OVL \
		$(BUILD_DIR)/build.log \
		$(BUILD_DIR)/bpe_dict.bin \
		overlay/*.o $(OUTPUT)_CODE.bin 2>/dev/null; \
	rm -rf $(BUILD_DIR)/bpe_src $(BUILD_DIR)/bpe_final 2>/dev/null; \
	printf "$(C_GRN)[OK]$(C_RESET) Build artifacts cleaned\n"
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
	@$(MAKE) BUILD_PROFILE=RELEASE EXTRA_CFLAGS="--max-allocs-per-node200000" all
RELEASE:
	@$(MAKE) release

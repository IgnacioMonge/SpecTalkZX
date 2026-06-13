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
PYTHON  ?= python3

# ------------------------------------------------------------
# Project
# ------------------------------------------------------------
OUTPUT  = SpecTalkZX
TAP     = $(BUILD_DIR)/$(OUTPUT).tap
MAP     = $(OUTPUT).map
BUILD_DIR = build
LOG     = $(BUILD_DIR)/build.log
BPE_STAMP = $(BUILD_DIR)/.bpe.stamp

# ------------------------------------------------------------
# Sources
# ------------------------------------------------------------
C_SOURCES    = src/main_build.c

# ------------------------------------------------------------
# Build options
# ------------------------------------------------------------
ASM_SOURCES  = asm/divmmc_uart.asm asm/spectalk_asm.asm asm/overlay_loader.asm
ASM_MODULE_SOURCES = asm/spectalk_asm/00_preamble.asm \
                     asm/spectalk_asm/10_core_helpers.asm \
                     asm/spectalk_asm/20_rx_ring_uart.asm \
                     asm/spectalk_asm/30_rendering.asm \
                     asm/spectalk_asm/40_text_numeric_screen.asm \
                     asm/spectalk_asm/50_main_output.asm \
                     asm/spectalk_asm/60_protocol_storage.asm \
                     asm/spectalk_asm/70_input_lookup.asm \
                     asm/spectalk_asm/80_ui_runtime.asm
ASM_DEP_SOURCES = $(ASM_SOURCES) $(ASM_MODULE_SOURCES)
BPE_INPUTS = src/spectalk.c src/irc_handlers.c src/user_cmds.c include/spectalk.h \
             src/SPECTALK.DAT src/SPECTALK_HELP.txt overlay/overlay_api.h \
             overlay/overlay_entry2.asm overlay/earth_about_render.asm \
             tools/bpe_build.py tools/bpe_compress.py \
             release/about_earth/earth_frame0.compact.bin \
             release/about_earth/earth_frame_deltas.bin \
             release/about_earth/earth_attr0.compact4.bin \
             release/about_earth/earth_attr_deltas.compact4.bin \
             release/about_earth/earth_logo.bin
ZORG        = 24000
STACK_SIZE  = 512
BSS_RING_GUARD ?= 96
BSS_RING_WARN  ?= 128
HIGH_FIXED_START ?= 64768
HIGH_FIXED_END   ?= 64847

ifeq ($(SKIP_BPE),1)
TAP_PREP :=
else
TAP_PREP = $(BPE_STAMP)
endif

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
.PHONY: all check clean bpe build restore_bpe trim overlay overlay_build info help release RELEASE nobpe copydat

# ------------------------------------------------------------
# Default pipeline
# ------------------------------------------------------------
all:
	@status=0; \
	$(MAKE) --no-print-directory check || status=$$?; \
	if [ "$$status" -eq 0 ]; then $(MAKE) --no-print-directory clean || status=$$?; fi; \
	if [ "$$status" -eq 0 ]; then $(MAKE) --no-print-directory $(BPE_STAMP) || status=$$?; fi; \
	if [ "$$status" -eq 0 ]; then $(MAKE) --no-print-directory $(TAP) || status=$$?; fi; \
	if [ "$$status" -eq 0 ]; then $(MAKE) --no-print-directory trim overlay info || status=$$?; fi; \
	$(MAKE) --no-print-directory restore_bpe || exit $$?; \
	exit $$status

nobpe:
	@$(MAKE) --no-print-directory check
	@$(MAKE) --no-print-directory clean
	@$(MAKE) --no-print-directory SKIP_BPE=1 $(TAP)
	@$(MAKE) --no-print-directory SKIP_BPE=1 trim overlay info copydat

copydat:
	@cp src/SPECTALK.DAT $(BUILD_DIR)/SPECTALK.DAT 2>/dev/null || true

help:
	$(call HR)
	@printf "Targets:\n"
	@printf "  make            - CHECK -> CLEAN -> BUILD -> INFO\n"
	@printf "  make release    - Release build (max optimization)\n"
	@printf "  make check      - Preflight dependency checks\n"
	@printf "  make clean      - Remove build artifacts\n"
	@printf "  make build      - Run BPE prep + build $(TAP) + restore sources\n"
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
		for t in zcc z80asm z88dk-appmake wc sh grep sed head tail tee dd; do \
			command -v "$$t" >/dev/null 2>&1 || { echo "[ERR] Missing tool: $$t"; fail=1; }; \
		done; \
		$(PYTHON) -c "import sys; raise SystemExit(sys.version_info < (3, 8))" >/dev/null 2>&1 || { echo "[ERR] Missing usable Python 3: $(PYTHON)"; fail=1; }; \
		for f in $(C_SOURCES) $(ASM_DEP_SOURCES) $(BPE_INPUTS) tools/gen_whatsnew.py release/logo.png release/changes.txt release/version.txt; do \
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
		cp $(BUILD_DIR)/bpe_originals/overlay_entry2.asm overlay/ 2>/dev/null || true; \
		cp $(BUILD_DIR)/bpe_originals/earth_about_render.asm overlay/ 2>/dev/null || true; \
		cp $(BUILD_DIR)/bpe_originals/SPECTALK.DAT src/ 2>/dev/null || true; \
	fi
	@rm -f "$(OUTPUT)" "$(OUTPUT).tap" "$(TAP)" "$(MAP)" "$(LOG)" "$(BUILD_DIR)/SPECTALK.DAT" "$(BUILD_DIR)"/*.OVL *.o *.bin *.sym 2>/dev/null || true
	@rm -rf "$(BUILD_DIR)/bpe_src" "$(BUILD_DIR)/bpe_final" "$(BUILD_DIR)/bpe_dict.bin" "$(BUILD_DIR)/bpe_originals" "$(BPE_STAMP)" 2>/dev/null || true
	$(call OK,Clean complete.)
	$(call HR)

# ------------------------------------------------------------
# BPE phase - compress screen-only strings
# Reads src/*.c (originals), generates build/bpe_final/*.c (compressed)
# Also generates SPECTALK.DAT with BPE dict inserted
# ------------------------------------------------------------
bpe: $(BPE_STAMP)

$(BPE_STAMP): $(BPE_INPUTS)
	$(call STEP,2/4,BPE compression)
	@mkdir -p $(BUILD_DIR)/bpe_originals
	@cp src/spectalk.c src/irc_handlers.c src/user_cmds.c $(BUILD_DIR)/bpe_originals/
	@cp include/spectalk.h $(BUILD_DIR)/bpe_originals/
	@cp src/SPECTALK.DAT $(BUILD_DIR)/bpe_originals/
	@cp overlay/overlay_api.h $(BUILD_DIR)/bpe_originals/
	@cp overlay/overlay_entry2.asm $(BUILD_DIR)/bpe_originals/
	@cp overlay/earth_about_render.asm $(BUILD_DIR)/bpe_originals/
	@$(PYTHON) tools/bpe_build.py
	@printf "ok\n" > "$(BPE_STAMP)"
	$(call OK,BPE complete.)
	$(call HR)

# ------------------------------------------------------------
# BUILD phase (only place where zcc is invoked)
# ------------------------------------------------------------
build:
	@status=0; \
	$(MAKE) --no-print-directory $(BPE_STAMP) || status=$$?; \
	if [ "$$status" -eq 0 ]; then $(MAKE) --no-print-directory $(TAP) || status=$$?; fi; \
	$(MAKE) --no-print-directory restore_bpe || exit $$?; \
	exit $$status

restore_bpe:
	@if [ -d "$(BUILD_DIR)/bpe_originals" ]; then \
		cp $(BUILD_DIR)/bpe_originals/spectalk.c src/; \
		cp $(BUILD_DIR)/bpe_originals/irc_handlers.c src/; \
		cp $(BUILD_DIR)/bpe_originals/user_cmds.c src/; \
		cp $(BUILD_DIR)/bpe_originals/spectalk.h include/; \
		cp $(BUILD_DIR)/bpe_originals/overlay_api.h overlay/; \
		cp $(BUILD_DIR)/bpe_originals/overlay_entry2.asm overlay/; \
		cp $(BUILD_DIR)/bpe_originals/earth_about_render.asm overlay/; \
		cp $(BUILD_DIR)/bpe_originals/SPECTALK.DAT src/; \
		dat_sz=$$(wc -c < src/SPECTALK.DAT); \
		if [ "$$dat_sz" -ne 1517 ]; then \
			printf "$(C_RED)[BPE CORRUPT]$(C_RESET) src/SPECTALK.DAT is $$dat_sz bytes after restore (expected 1517)\n"; \
			printf "  bpe_originals was corrupted. Restore manually from Development/\n"; \
			exit 1; \
		fi; \
		rm -rf $(BUILD_DIR)/bpe_originals; \
	fi; \
	rm -f "$(BPE_STAMP)"

$(TAP): $(C_SOURCES) $(ASM_DEP_SOURCES) $(TAP_PREP)
	$(call STEP,3/4,Build)
	@echo "Compiling SpecTalkZX..."
	@echo "UART mode: $(UART_DESC)"
	@if [ "$(BUILD_PROFILE)" != "NORMAL" ]; then printf "$(C_BOLD)$(C_YEL)Build profile: $(BUILD_PROFILE)$(C_RESET)\n"; fi
	@echo "Log: $(LOG)"
	@build_rc=0; $(BUILD_CMD) 2>&1 | tee "$(LOG)" || build_rc=$$?; \
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
	    if [ "$$free" -lt "$(BSS_RING_GUARD)" ]; then \
	      printf "$(C_RED)[FATAL]$(C_RESET) BSS/ring guard too small: $$free bytes free (min $(BSS_RING_GUARD))\n"; \
	      exit 1; \
	    fi; \
	    if [ "$$free" -lt "$(BSS_RING_WARN)" ]; then \
	      printf "$(C_YEL)[WARN]$(C_RESET) BSS guard tight: 0x$$bss_end < 0xF500 ($$free bytes free; warn < $(BSS_RING_WARN))\n"; \
	    else \
	      printf "$(C_GRN)[OK]$(C_RESET) BSS guard: 0x$$bss_end < 0xF500 ($$free bytes free)\n"; \
	    fi; \
	  fi; \
	  ring_hex=$$(grep "_ring_buffer " $(MAP) | grep -o "\$$[0-9A-Fa-f]*" | head -1 | tr -d "\$$"); \
	  stack_hex=$$(grep "TAR__register_sp" $(MAP) | grep -o "\$$[0-9A-Fa-f]*" | head -1 | tr -d "\$$"); \
	  stack_size_hex=$$(grep "CRT_STACK_SIZE" $(MAP) | grep -o "\$$[0-9A-Fa-f]*" | head -1 | tr -d "\$$"); \
	  if [ -n "$$ring_hex" ] && [ -n "$$stack_hex" ] && [ -n "$$stack_size_hex" ]; then \
	    ring_dec=$$($(PYTHON) -c "print(0x$$ring_hex)"); \
	    stack_dec=$$($(PYTHON) -c "print(0x$$stack_hex)"); \
	    stack_size_dec=$$($(PYTHON) -c "print(0x$$stack_size_hex)"); \
	    ring_end=$$((ring_dec + 2048)); \
	    stack_low=$$((stack_dec - stack_size_dec)); \
	    if [ "$$ring_end" -gt "$(HIGH_FIXED_START)" ]; then \
	      printf "$(C_RED)[FATAL]$(C_RESET) high fixed RAM overlaps ring: ring end 0x%04X > 0x%04X\n" "$$ring_end" "$(HIGH_FIXED_START)"; \
	      exit 1; \
	    fi; \
	    if [ "$(HIGH_FIXED_END)" -ge "$$stack_low" ]; then \
	      printf "$(C_RED)[FATAL]$(C_RESET) high fixed RAM overlaps stack: fixed end 0x%04X >= stack low 0x%04X\n" "$(HIGH_FIXED_END)" "$$stack_low"; \
	      exit 1; \
	    fi; \
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

overlay:
	@status=0; \
	if [ "$(SKIP_BPE)" != "1" ]; then $(MAKE) --no-print-directory $(BPE_STAMP) || status=$$?; fi; \
	if [ "$$status" -eq 0 ]; then $(MAKE) --no-print-directory overlay_build || status=$$?; fi; \
	if [ "$(SKIP_BPE)" != "1" ]; then $(MAKE) --no-print-directory restore_bpe || exit $$?; fi; \
	exit $$status

overlay_build: $(TAP)
	$(call STEP,OVL,Building overlay)
	@rm -f $(BUILD_DIR)/SPECTALK.OVL $(BUILD_DIR)/SPECTALK.FIXED.OVL $(BUILD_DIR)/SPCTLK[1-8].OVL
	@SLOT=$$(grep '_ring_buffer ' $(MAP) | sed -n 's/.*= \$$\([0-9A-Fa-f]*\).*/\1/p' | head -1); \
	if [ -z "$$SLOT" ]; then \
		printf "$(C_RED)[ERR]$(C_RESET) _overlay_slot not found in $(MAP)\n"; \
		exit 1; \
	fi; \
	echo "  overlay_slot = 0x$$SLOT"; \
	$(PYTHON) tools/gen_overlay_defs.py $(MAP) > $(OVL_DEFS) || exit 1; \
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
	z80asm -I$(BUILD_DIR) overlay/overlay_entry2.asm 2>&1 || exit 1; \
	z80asm -I$(BUILD_DIR) -Irelease/about_earth overlay/earth_about_render.asm 2>&1 || exit 1; \
	z80asm -b -r0x$$SLOT -o=$(BUILD_DIR)/SPCTLK2.OVL \
		overlay/overlay_entry2.o \
		overlay/earth_about_render.o \
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
	zcc +z80 -clib=sdcc_iy --no-crt --opt-code-size \
		-Ioverlay -c overlay/bookmark_store_ovl.c -o $(BUILD_DIR)/bookmark_store_ovl.o 2>&1 || exit 1; \
	z80asm -I$(BUILD_DIR) overlay/overlay_entry3.asm 2>&1 || exit 1; \
	z80asm -b -r0x$$SLOT -o=$(BUILD_DIR)/SPCTLK3.OVL \
		overlay/overlay_entry3.o \
		$(BUILD_DIR)/spectalk_ovl3.o \
		$(BUILD_DIR)/bookmark_store_ovl.o \
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
	echo "  Building SPCTLK5.OVL..."; \
	zcc +z80 -clib=sdcc_iy --no-crt --opt-code-size \
		-Ioverlay -c overlay/spectalk_ovl5.c -o $(BUILD_DIR)/spectalk_ovl5.o 2>&1 || exit 1; \
	z80asm -I$(BUILD_DIR) overlay/overlay_entry5.asm 2>&1 || exit 1; \
	z80asm -I$(BUILD_DIR) overlay/rtc_seed_ovl.asm 2>&1 || exit 1; \
	z80asm -b -r0x$$SLOT -o=$(BUILD_DIR)/SPCTLK5.OVL \
		overlay/overlay_entry5.o \
		overlay/rtc_seed_ovl.o \
		$(BUILD_DIR)/spectalk_ovl5.o \
		$(BUILD_DIR)/overlay_defs.o 2>&1 || exit 1; \
	ovl5_size=$$(wc -c < $(BUILD_DIR)/SPCTLK5.OVL); \
	if [ "$$ovl5_size" -gt 2048 ]; then \
		printf "$(C_RED)[ERR]$(C_RESET) SPCTLK5.OVL too large: $$ovl5_size bytes (max 2048)\n"; \
		exit 1; \
	fi; \
	printf "$(C_GRN)[OK]$(C_RESET) SPCTLK5.OVL: $$ovl5_size bytes (max 2048)\n"; \
	echo "  Building SPCTLK6.OVL..."; \
	zcc +z80 -clib=sdcc_iy --no-crt --opt-code-size \
		-Ioverlay -c overlay/switcher_ovl.c -o $(BUILD_DIR)/switcher_ovl.o 2>&1 || exit 1; \
	z80asm -I$(BUILD_DIR) overlay/overlay_entry6.asm 2>&1 || exit 1; \
	z80asm -b -r0x$$SLOT -o=$(BUILD_DIR)/SPCTLK6.OVL \
		overlay/overlay_entry6.o \
		$(BUILD_DIR)/switcher_ovl.o \
		$(BUILD_DIR)/overlay_defs.o 2>&1 || exit 1; \
	ovl6_size=$$(wc -c < $(BUILD_DIR)/SPCTLK6.OVL); \
	if [ "$$ovl6_size" -gt 2048 ]; then \
		printf "$(C_RED)[ERR]$(C_RESET) SPCTLK6.OVL too large: $$ovl6_size bytes (max 2048)\n"; \
		exit 1; \
	fi; \
	printf "$(C_GRN)[OK]$(C_RESET) SPCTLK6.OVL: $$ovl6_size bytes (max 2048)\n"; \
	echo "  Building SPCTLK7.OVL..."; \
	zcc +z80 -clib=sdcc_iy --no-crt --opt-code-size \
		-Ioverlay -c overlay/local_cmds_ovl.c -o $(BUILD_DIR)/local_cmds_ovl.o 2>&1 || exit 1; \
	z80asm -I$(BUILD_DIR) overlay/overlay_entry7.asm 2>&1 || exit 1; \
	z80asm -b -r0x$$SLOT -o=$(BUILD_DIR)/SPCTLK7.OVL \
		overlay/overlay_entry7.o \
		$(BUILD_DIR)/local_cmds_ovl.o \
		$(BUILD_DIR)/overlay_defs.o 2>&1 || exit 1; \
	ovl7_size=$$(wc -c < $(BUILD_DIR)/SPCTLK7.OVL); \
	if [ "$$ovl7_size" -gt 2048 ]; then \
		printf "$(C_RED)[ERR]$(C_RESET) SPCTLK7.OVL too large: $$ovl7_size bytes (max 2048)\n"; \
		exit 1; \
	fi; \
	printf "$(C_GRN)[OK]$(C_RESET) SPCTLK7.OVL: $$ovl7_size bytes (max 2048)\n"; \
	echo "  Building SPCTLK8.OVL..."; \
	zcc +z80 -clib=sdcc_iy --no-crt --opt-code-size \
		-Ioverlay -c overlay/bookmarks_ovl.c -o $(BUILD_DIR)/bookmarks_ovl.o 2>&1 || exit 1; \
	z80asm -I$(BUILD_DIR) overlay/overlay_entry8.asm 2>&1 || exit 1; \
	z80asm -b -r0x$$SLOT -o=$(BUILD_DIR)/SPCTLK8.OVL \
		overlay/overlay_entry8.o \
		$(BUILD_DIR)/bookmarks_ovl.o \
		$(BUILD_DIR)/overlay_defs.o 2>&1 || exit 1; \
	ovl8_size=$$(wc -c < $(BUILD_DIR)/SPCTLK8.OVL); \
	if [ "$$ovl8_size" -gt 2048 ]; then \
		printf "$(C_RED)[ERR]$(C_RESET) SPCTLK8.OVL too large: $$ovl8_size bytes (max 2048)\n"; \
		exit 1; \
	fi; \
	printf "$(C_GRN)[OK]$(C_RESET) SPCTLK8.OVL: $$ovl8_size bytes (max 2048)\n"; \
	echo "  Packing SPECTALK.OVL atlas..."; \
	dd if=$(BUILD_DIR)/SPCTLK1.OVL of=$(BUILD_DIR)/SPECTALK.FIXED.OVL bs=2048 conv=sync 2>/dev/null; \
	dd if=$(BUILD_DIR)/SPCTLK2.OVL of=$(BUILD_DIR)/SPECTALK.FIXED.OVL bs=2048 conv=sync seek=1 2>/dev/null; \
	dd if=$(BUILD_DIR)/SPCTLK3.OVL of=$(BUILD_DIR)/SPECTALK.FIXED.OVL bs=2048 conv=sync seek=2 2>/dev/null; \
	dd if=$(BUILD_DIR)/SPCTLK4.OVL of=$(BUILD_DIR)/SPECTALK.FIXED.OVL bs=2048 conv=sync seek=3 2>/dev/null; \
	dd if=$(BUILD_DIR)/SPCTLK5.OVL of=$(BUILD_DIR)/SPECTALK.FIXED.OVL bs=2048 conv=sync seek=4 2>/dev/null; \
	dd if=$(BUILD_DIR)/SPCTLK6.OVL of=$(BUILD_DIR)/SPECTALK.FIXED.OVL bs=2048 conv=sync seek=5 2>/dev/null; \
	dd if=$(BUILD_DIR)/SPCTLK7.OVL of=$(BUILD_DIR)/SPECTALK.FIXED.OVL bs=2048 conv=sync seek=6 2>/dev/null; \
	dd if=$(BUILD_DIR)/SPCTLK8.OVL of=$(BUILD_DIR)/SPECTALK.FIXED.OVL bs=2048 conv=sync seek=7 2>/dev/null; \
	$(PYTHON) tools/overlay_atlas_probe.py \
		--packed $(BUILD_DIR)/SPECTALK.FIXED.OVL \
		--out $(BUILD_DIR)/SPECTALK.OVL \
		--sizes "$$ovl_size,$$ovl2_size,$$ovl3_size,$$ovl4_size,$$ovl5_size,$$ovl6_size,$$ovl7_size,$$ovl8_size" || exit 1; \
	printf "$(C_GRN)[OK]$(C_RESET) SPECTALK.OVL: $$(wc -c < $(BUILD_DIR)/SPECTALK.OVL) bytes (STOA atlas)\n"; \
	rm -f $(BUILD_DIR)/SPCTLK[1-8].OVL $(BUILD_DIR)/SPECTALK.FIXED.OVL; \
	echo "  Cleaning build intermediates..."; \
	rm -f $(BUILD_DIR)/*.o $(BUILD_DIR)/*.asm $(BUILD_DIR)/*.bin \
		$(BUILD_DIR)/SPECTALK $(BUILD_DIR)/SP2.OVL \
		$(BUILD_DIR)/SPECTALK.FIXED.OVL \
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
	@if [ "$(BUILD_PROFILE)" != "NORMAL" ]; then printf "$(C_BOLD)Build profile:$(C_RESET) $(C_YEL)$(BUILD_PROFILE)$(C_RESET)\n"; fi
	@printf "$(C_BOLD)Code origin:$(C_RESET) $(C_YEL)%s$(C_RESET)\n" "$(ZORG)"
	@printf "$(C_BOLD)Stack size:$(C_RESET) $(C_YEL)%s bytes$(C_RESET)\n" "$(STACK_SIZE)"
	@printf "$(C_BOLD)Binary TAP size:$(C_RESET) $(C_YEL)%s bytes$(C_RESET)\n" "$$($(SIZE_TAP))"
	$(call HR)

release:
	@$(MAKE) BUILD_PROFILE=RELEASE EXTRA_CFLAGS="--max-allocs-per-node200000" all
RELEASE:
	@$(MAKE) release

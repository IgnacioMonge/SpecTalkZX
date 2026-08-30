#!/bin/sh
# Overlay build, extracted verbatim from the Makefile recipe.
#
# A recipe reaches the shell as one command line, and Windows truncates that
# at 8191 characters silently. This body is ~10 KB once the Spectranext flags
# are expanded into each of the nine compilations. The cut landed mid-argument
# in the eighth overlay; zcc then saw no input file, printed its usage and
# exited 0, so the build reported success with no atlas, and a stale overlay
# reached hardware. A script is read from a file and has no such limit.
#
# Kept as one continued shell statement, exactly as the recipe had it.
# Inputs arrive as environment variables; see the caller in the Makefile.
# OVL_XFS_WRITE_OBJ and OVL_CLOCK_OBJ carry the two platform conditionals
# that make used to resolve inline.

SLOT=$(grep '_ring_buffer ' ${MAP} | sed -n 's/.*= \$\([0-9A-Fa-f]*\).*/\1/p' | head -1); \
if [ -z "$SLOT" ]; then \
	printf "${C_RED}[ERR]${C_RESET} _overlay_slot not found in ${MAP}\n"; \
	exit 1; \
fi; \
echo "  overlay_slot = 0x$SLOT"; \
${PYTHON} tools/gen_overlay_defs.py ${MAP} > ${OVL_DEFS} || exit 1; \
echo "  overlay_defs.asm generated"; \
zcc +z80 -clib=sdcc_iy --no-crt --opt-code-size ${ZCC_EVIDENCE_FLAGS} \
	-Ioverlay -c ${OVL_SRC} -o ${BUILD_DIR}/spectalk_ovl.o 2>&1 || exit 1; \
echo "  spectalk_ovl.c compiled"; \
z80asm ${Z80ASM_EVIDENCE_FLAGS} -I${BUILD_DIR} ${OVL_ENTRY} 2>&1 || exit 1; \
z80asm ${Z80ASM_EVIDENCE_FLAGS} ${OVL_DEFS} 2>&1 || exit 1; \
z80asm ${Z80ASM_EVIDENCE_FLAGS} -b -r0x$SLOT -o=${OVL_OUT} \
	overlay/overlay_entry.o \
	${BUILD_DIR}/spectalk_ovl.o \
	${BUILD_DIR}/overlay_defs.o 2>&1 || exit 1; \
ovl_size=$(wc -c < ${OVL_OUT}); \
if [ "$ovl_size" -gt 2048 ]; then \
	printf "${C_RED}[ERR]${C_RESET} Overlay too large: $ovl_size bytes (max 2048)\n"; \
	exit 1; \
fi; \
printf "${C_GRN}[OK]${C_RESET} SPCTLK1.OVL: $ovl_size bytes (max 2048)\n"; \
echo "  Building SPCTLK2.OVL..."; \
z80asm ${Z80ASM_EVIDENCE_FLAGS} -I${BUILD_DIR} overlay/overlay_entry2.asm 2>&1 || exit 1; \
z80asm ${Z80ASM_EVIDENCE_FLAGS} -I${BUILD_DIR} -Irelease/about_earth overlay/earth_about_render.asm 2>&1 || exit 1; \
z80asm ${Z80ASM_EVIDENCE_FLAGS} -b -r0x$SLOT -o=${BUILD_DIR}/SPCTLK2.OVL \
	overlay/overlay_entry2.o \
	overlay/earth_about_render.o \
	${BUILD_DIR}/overlay_defs.o 2>&1 || exit 1; \
ovl2_size=$(wc -c < ${BUILD_DIR}/SPCTLK2.OVL); \
if [ "$ovl2_size" -gt 2048 ]; then \
	printf "${C_RED}[ERR]${C_RESET} SPCTLK2.OVL too large: $ovl2_size bytes (max 2048)\n"; \
	exit 1; \
fi; \
printf "${C_GRN}[OK]${C_RESET} SPCTLK2.OVL: $ovl2_size bytes (max 2048)\n"; \
echo "  Building SPCTLK3.OVL..."; \
${PYTHON} tools/gen_whatsnew.py 2>&1 || exit 1; \
zcc +z80 -clib=sdcc_iy --no-crt --opt-code-size ${ZCC_EVIDENCE_FLAGS} \
	-Ioverlay -c overlay/spectalk_ovl3.c -o ${BUILD_DIR}/spectalk_ovl3.o 2>&1 || exit 1; \
zcc +z80 -clib=sdcc_iy --no-crt --opt-code-size ${ZCC_EVIDENCE_FLAGS} \
	-Ioverlay -c overlay/bookmark_store_ovl.c -o ${BUILD_DIR}/bookmark_store_ovl.o 2>&1 || exit 1; \
z80asm ${Z80ASM_EVIDENCE_FLAGS} -I${BUILD_DIR} overlay/overlay_entry3.asm 2>&1 || exit 1; \
if [ "${PLATFORM}" = "spectranext" ]; then \
	z80asm ${Z80ASM_EVIDENCE_FLAGS} -I${BUILD_DIR} overlay/xfs_write_ovl.asm 2>&1 || exit 1; \
fi; \
z80asm ${Z80ASM_EVIDENCE_FLAGS} -b -r0x$SLOT -o=${BUILD_DIR}/SPCTLK3.OVL \
	overlay/overlay_entry3.o \
	${BUILD_DIR}/spectalk_ovl3.o \
	${BUILD_DIR}/bookmark_store_ovl.o \
	${OVL_XFS_WRITE_OBJ} \
	${BUILD_DIR}/overlay_defs.o 2>&1 || exit 1; \
ovl3_size=$(wc -c < ${BUILD_DIR}/SPCTLK3.OVL); \
if [ "$ovl3_size" -gt 2048 ]; then \
	printf "${C_RED}[ERR]${C_RESET} SPCTLK3.OVL too large: $ovl3_size bytes (max 2048)\n"; \
	exit 1; \
fi; \
printf "${C_GRN}[OK]${C_RESET} SPCTLK3.OVL: $ovl3_size bytes (max 2048)\n"; \
echo "  Building SPCTLK4.OVL..."; \
zcc +z80 -clib=sdcc_iy --no-crt --opt-code-size ${ZCC_EVIDENCE_FLAGS} \
	-Ioverlay -c overlay/spectalk_ovl4.c -o ${BUILD_DIR}/spectalk_ovl4.o 2>&1 || exit 1; \
z80asm ${Z80ASM_EVIDENCE_FLAGS} -I${BUILD_DIR} overlay/overlay_entry4.asm 2>&1 || exit 1; \
z80asm ${Z80ASM_EVIDENCE_FLAGS} -b -r0x$SLOT -o=${BUILD_DIR}/SPCTLK4.OVL \
	overlay/overlay_entry4.o \
	${BUILD_DIR}/spectalk_ovl4.o \
	${OVL_XFS_WRITE_OBJ} \
	${BUILD_DIR}/overlay_defs.o 2>&1 || exit 1; \
ovl4_size=$(wc -c < ${BUILD_DIR}/SPCTLK4.OVL); \
if [ "$ovl4_size" -gt 2048 ]; then \
	printf "${C_RED}[ERR]${C_RESET} SPCTLK4.OVL too large: $ovl4_size bytes (max 2048)\n"; \
	exit 1; \
fi; \
printf "${C_GRN}[OK]${C_RESET} SPCTLK4.OVL: $ovl4_size bytes (max 2048)\n"; \
echo "  Building SPCTLK5.OVL..."; \
zcc +z80 -clib=sdcc_iy --no-crt --opt-code-size ${ZCC_EVIDENCE_FLAGS} \
	-Ioverlay -c overlay/spectalk_ovl5.c -o ${BUILD_DIR}/spectalk_ovl5.o 2>&1 || exit 1; \
z80asm ${Z80ASM_EVIDENCE_FLAGS} -I${BUILD_DIR} overlay/overlay_entry5.asm 2>&1 || exit 1; \
z80asm ${Z80ASM_EVIDENCE_FLAGS} -I${BUILD_DIR} overlay/rtc_seed_ovl.asm 2>&1 || exit 1; \
z80asm ${Z80ASM_EVIDENCE_FLAGS} -b -r0x$SLOT -o=${BUILD_DIR}/SPCTLK5.OVL \
	overlay/overlay_entry5.o \
	overlay/rtc_seed_ovl.o \
	${BUILD_DIR}/spectalk_ovl5.o \
	${BUILD_DIR}/overlay_defs.o 2>&1 || exit 1; \
ovl5_size=$(wc -c < ${BUILD_DIR}/SPCTLK5.OVL); \
if [ "$ovl5_size" -gt 2048 ]; then \
	printf "${C_RED}[ERR]${C_RESET} SPCTLK5.OVL too large: $ovl5_size bytes (max 2048)\n"; \
	exit 1; \
fi; \
printf "${C_GRN}[OK]${C_RESET} SPCTLK5.OVL: $ovl5_size bytes (max 2048)\n"; \
echo "  Building SPCTLK6.OVL..."; \
zcc +z80 -clib=sdcc_iy --no-crt --opt-code-size ${ZCC_EVIDENCE_FLAGS} \
	-Ioverlay -c overlay/switcher_ovl.c -o ${BUILD_DIR}/switcher_ovl.o 2>&1 || exit 1; \
if [ "${PLATFORM}" = "spectranext" ]; then \
	zcc +z80 -clib=sdcc_iy --no-crt --opt-code-size ${ZCC_EVIDENCE_FLAGS} \
		-Ioverlay -c overlay/spectranext_clock_ovl.c -o ${BUILD_DIR}/spectranext_clock_ovl.o 2>&1 || exit 1; \
fi; \
z80asm ${Z80ASM_EVIDENCE_FLAGS} -I${BUILD_DIR} overlay/overlay_entry6.asm 2>&1 || exit 1; \
z80asm ${Z80ASM_EVIDENCE_FLAGS} -b -r0x$SLOT -o=${BUILD_DIR}/SPCTLK6.OVL \
	overlay/overlay_entry6.o \
	${BUILD_DIR}/switcher_ovl.o \
	${OVL_CLOCK_OBJ} \
	${BUILD_DIR}/overlay_defs.o 2>&1 || exit 1; \
ovl6_size=$(wc -c < ${BUILD_DIR}/SPCTLK6.OVL); \
if [ "$ovl6_size" -gt 2048 ]; then \
	printf "${C_RED}[ERR]${C_RESET} SPCTLK6.OVL too large: $ovl6_size bytes (max 2048)\n"; \
	exit 1; \
fi; \
printf "${C_GRN}[OK]${C_RESET} SPCTLK6.OVL: $ovl6_size bytes (max 2048)\n"; \
echo "  Building SPCTLK7.OVL..."; \
zcc +z80 -clib=sdcc_iy --no-crt --opt-code-size ${ZCC_EVIDENCE_FLAGS} \
	-Ioverlay -c overlay/local_cmds_ovl.c -o ${BUILD_DIR}/local_cmds_ovl.o 2>&1 || exit 1; \
z80asm ${Z80ASM_EVIDENCE_FLAGS} -I${BUILD_DIR} overlay/overlay_entry7.asm 2>&1 || exit 1; \
z80asm ${Z80ASM_EVIDENCE_FLAGS} -b -r0x$SLOT -o=${BUILD_DIR}/SPCTLK7.OVL \
	overlay/overlay_entry7.o \
	${BUILD_DIR}/local_cmds_ovl.o \
	${BUILD_DIR}/overlay_defs.o 2>&1 || exit 1; \
ovl7_size=$(wc -c < ${BUILD_DIR}/SPCTLK7.OVL); \
if [ "$ovl7_size" -gt 2048 ]; then \
	printf "${C_RED}[ERR]${C_RESET} SPCTLK7.OVL too large: $ovl7_size bytes (max 2048)\n"; \
	exit 1; \
fi; \
printf "${C_GRN}[OK]${C_RESET} SPCTLK7.OVL: $ovl7_size bytes (max 2048)\n"; \
echo "  Building SPCTLK8.OVL..."; \
zcc +z80 -clib=sdcc_iy --no-crt --opt-code-size ${ZCC_EVIDENCE_FLAGS} \
	-Ioverlay -c overlay/bookmarks_ovl.c -o ${BUILD_DIR}/bookmarks_ovl.o 2>&1 || exit 1; \
z80asm ${Z80ASM_EVIDENCE_FLAGS} -I${BUILD_DIR} overlay/overlay_entry8.asm 2>&1 || exit 1; \
z80asm ${Z80ASM_EVIDENCE_FLAGS} -b -r0x$SLOT -o=${BUILD_DIR}/SPCTLK8.OVL \
	overlay/overlay_entry8.o \
	${BUILD_DIR}/bookmarks_ovl.o \
	${BUILD_DIR}/overlay_defs.o 2>&1 || exit 1; \
ovl8_size=$(wc -c < ${BUILD_DIR}/SPCTLK8.OVL); \
if [ "$ovl8_size" -gt 2048 ]; then \
	printf "${C_RED}[ERR]${C_RESET} SPCTLK8.OVL too large: $ovl8_size bytes (max 2048)\n"; \
	exit 1; \
fi; \
printf "${C_GRN}[OK]${C_RESET} SPCTLK8.OVL: $ovl8_size bytes (max 2048)\n"; \
echo "  Packing SPECTALK.OVL atlas..."; \
dd if=${BUILD_DIR}/SPCTLK1.OVL of=${BUILD_DIR}/SPECTALK.FIXED.OVL bs=2048 conv=sync 2>/dev/null; \
dd if=${BUILD_DIR}/SPCTLK2.OVL of=${BUILD_DIR}/SPECTALK.FIXED.OVL bs=2048 conv=sync seek=1 2>/dev/null; \
dd if=${BUILD_DIR}/SPCTLK3.OVL of=${BUILD_DIR}/SPECTALK.FIXED.OVL bs=2048 conv=sync seek=2 2>/dev/null; \
dd if=${BUILD_DIR}/SPCTLK4.OVL of=${BUILD_DIR}/SPECTALK.FIXED.OVL bs=2048 conv=sync seek=3 2>/dev/null; \
dd if=${BUILD_DIR}/SPCTLK5.OVL of=${BUILD_DIR}/SPECTALK.FIXED.OVL bs=2048 conv=sync seek=4 2>/dev/null; \
dd if=${BUILD_DIR}/SPCTLK6.OVL of=${BUILD_DIR}/SPECTALK.FIXED.OVL bs=2048 conv=sync seek=5 2>/dev/null; \
dd if=${BUILD_DIR}/SPCTLK7.OVL of=${BUILD_DIR}/SPECTALK.FIXED.OVL bs=2048 conv=sync seek=6 2>/dev/null; \
dd if=${BUILD_DIR}/SPCTLK8.OVL of=${BUILD_DIR}/SPECTALK.FIXED.OVL bs=2048 conv=sync seek=7 2>/dev/null; \
${PYTHON} tools/overlay_atlas_probe.py \
	--packed ${BUILD_DIR}/SPECTALK.FIXED.OVL \
	--out ${BUILD_DIR}/SPECTALK.OVL \
	--sizes "$ovl_size,$ovl2_size,$ovl3_size,$ovl4_size,$ovl5_size,$ovl6_size,$ovl7_size,$ovl8_size" || exit 1; \
printf "${C_GRN}[OK]${C_RESET} SPECTALK.OVL: $(wc -c < ${BUILD_DIR}/SPECTALK.OVL) bytes (STOA atlas)\n"; \
rm -f ${BUILD_DIR}/SPCTLK[1-8].OVL ${BUILD_DIR}/SPECTALK.FIXED.OVL; \
echo "  Cleaning build intermediates..."; \
rm -f ${BUILD_DIR}/*.o \
	${BUILD_DIR}/SPECTALK ${BUILD_DIR}/SP2.OVL \
	${BUILD_DIR}/SPECTALK.FIXED.OVL \
	overlay/*.o ${OUTPUT}_CODE.bin 2>/dev/null; \
if [ "${EVIDENCE}" != "1" ]; then \
	rm -f ${BUILD_DIR}/*.asm ${BUILD_DIR}/*.bin 2>/dev/null; \
	rm -rf ${BUILD_DIR}/bpe_src ${BUILD_DIR}/bpe_final 2>/dev/null; \
fi; \
printf "${C_GRN}[OK]${C_RESET} Build artifacts cleaned\n"

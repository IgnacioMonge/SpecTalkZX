; Persistent XFS write transaction, linked only into the two writer overlays.

SECTION code_user

PUBLIC _esx_replace_write
EXTERN _esx_freplace
EXTERN _esx_fwrite
EXTERN _esx_fclose
EXTERN _esx_funlink
EXTERN _esx_commit
EXTERN _esx_handle
EXTERN _esx_count
EXTERN _esx_result

; uint8_t esx_replace_write(const char *path) __z88dk_fastcall
_esx_replace_write:
    push ix
    push hl
    pop ix
    call _esx_freplace
    ld a, (_esx_handle)
    or a
    jr z, xfs_write_false

    ld a, (_esx_result)       ; esx_freplace publishes exactly 0=new, 1=existing
    xor 1
    ld (xfs_write_created), a

    call _esx_fwrite
    ld hl, (_esx_result)
    ld de, (_esx_count)
    or a
    sbc hl, de
    push af
    call _esx_fclose
    ld e, l
    pop af
    jr nz, xfs_write_cleanup
    ld a, e
    or a
    jr nz, xfs_write_cleanup

    ; XFS is overlayed: overwriting a committed file leaves the new content in
    ; RAMFS, so it survives a reset and a power cut restores the old version.
    ; Commit on every path, not only on create. On the overwrite path the
    ; write already succeeded and the file is published either way, so a
    ; failed commit is not worth discarding the save for; on create it is,
    ; because an uncommitted new file is simply lost.
    push ix
    pop hl
    call _esx_commit
    ld a, (xfs_write_created)
    or a
    jr z, xfs_write_true
    ld a, (_esx_result)
    or a
    jr z, xfs_write_cleanup
xfs_write_true:
    ld l, 1
    pop ix
    ret

xfs_write_cleanup:
    ld a, (xfs_write_created)
    or a
    jr z, xfs_write_false
    push ix
    pop hl
    call _esx_funlink
xfs_write_false:
    ld l, 0
    pop ix
    ret

xfs_write_created:
    defb 0

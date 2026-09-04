# Overlay Map

SpecTalkZX has three overlay vocabularies. Keep them separate:

- UI mode: value stored in `overlay_mode`, used by the main loop and render suppression.
- Atlas id: first argument to `overlay_exec(ovl_id, entry_id)`, zero-based entry in the STOA atlas.
- Slot file: build-time `SPCTLK1.OVL` through `SPCTLK8.OVL`; file number is atlas id + 1.

On Classic, `overlay_exec()` loads the atlas payload into `_ring_buffer`. On Spectranext, a 256-byte atlas bootstrap follows the 64-byte header and stages the payload through the 512-byte ring slice at `_ring_buffer+512` into one reusable 4K Page B SRAM page. On native Spectrum Next, the NEX expands the atlas into eight dedicated MMU pages, maps the selected page directly at `$2000`, and stores its exact payload length in the page's final two bytes. `overlay_call()` calls the current image through the target-specific trampoline.

## UI Modes

| UI mode macro | Value | Screen owner | Initial atlas call |
|---|---:|---|---|
| `OVERLAY_NONE` | 0 | normal main UI | none |
| `OVERLAY_HELP` | 1 | help pager | `overlay_exec(0, 0)` -> `SPCTLK1` entry 0 |
| `OVERLAY_ABOUT` | 2 | ABOUT/Earth | `overlay_exec(1, 0)` -> `SPCTLK2` entry 0 |
| `OVERLAY_CONFIG` | 3 | config screen | `overlay_exec(4, 0)` -> `SPCTLK5` entry 0 |
| `OVERLAY_STATUS` | 4 | status screen | `overlay_exec(3, 0)` -> `SPCTLK4` entry 0 |
| `OVERLAY_WHATSNEW` | 5 | changelog | `overlay_exec(2, 0)` -> `SPCTLK3` entry 0 |
| `OVERLAY_BOOKMARKS` | 6 | bookmark selector | `overlay_exec(7, 0)` -> `SPCTLK8` entry 0 |

## Atlas Entries

| Atlas id | Slot file | Entries | Primary callers |
|---:|---|---|---|
| 0 | `SPCTLK1.OVL` | 0 help render; 1 banner render; 2 `/channels`; 3 `!theme` message | `help_render_page()`, `draw_banner()`, `cmd_windows_wrapper()`, `cmd_theme()` |
| 1 | `SPCTLK2.OVL` | 0 ABOUT render; 1 ABOUT close; 2 globe tick | `sys_about()`, ABOUT exit paths, ABOUT N-key path, timed ABOUT animation |
| 2 | `SPCTLK3.OVL` | 0 whats-new render; Classic also has 1 bookmark apply, 2 bookmark save | `sys_whatsnew()`; Classic bookmark load/save |
| 3 | `SPCTLK4.OVL` | 0 status render; 1 config save; Spectranext also has 2 bookmark apply, 3 bookmark save and one linked XFS writer | `sys_status()`, `cmd_save()`; Spectranext bookmark load/save |
| 4 | `SPCTLK5.OVL` | 0 config render; 1 cold RTC seed; 2 `!tz rtc`; 3 numeric `!tz`; native Next also has 4 shared bus/ESP reset pulse | `sys_config()`, startup RTC seed, config save refresh, `cmd_tz()`, native `esp_init()` recovery |
| 5 | `SPCTLK6.OVL` | 0 target-dependent clock acquisition (Classic/native Next raw UDP NTP; Spectranext ROM UDP/SNTP); 1 channel switcher render | `sntp_udp_fallback()` (Classic/native Next), `clock_sync_fallback()` (Spectranext), `switcher_render()` |
| 6 | `SPCTLK7.OVL` | 0 ignore; 1 pass; 2 local setting; 3 autoaway; 4 friend | `cmd_ignore()`, `cmd_pass()`, setting wrappers, `cmd_autoaway()`, `cmd_friend()` |
| 7 | `SPCTLK8.OVL` | 0 bookmark render; 1 rows refresh; 2 list; 3 delete; 4 cursor | `cmd_bookmarks()`, bookmark selector key handlers |

## Common Traps

- `OVERLAY_STATUS == 4`, but status is atlas id `3`, slot `SPCTLK4`.
- `OVERLAY_CONFIG == 3`, but config render is atlas id `4`, slot `SPCTLK5`.
- RTC cold seed and `!tz` live in `SPCTLK5`; they are not status entries.
- Native `SPCTLK5` entry 4 performs one reset pulse only; resident `esp_init()` owns the two-attempt limit and the 251-frame `ready` wait.
- Classic keeps bookmark storage in `SPCTLK3` to respect its 2K slots. Spectranext moves it to `SPCTLK4`, whose 4K slot lets bookmark and config saves share one private XFS writer.
- `SPCTLK6` entry 0 is target-dependent: Classic and native Next use raw UDP NTP, while Spectranext aliases the entry to ROM UDP/SNTP clock acquisition. Do not infer the implementation from the slot number.

## Build Contract

- Each `SPCTLK*.OVL` must be <= 2048 bytes on Classic, <= 4096 bytes on Spectranext or <= 8190 bytes on native Spectrum Next; native reserves `$3FFE..$3FFF` for the exact payload length used by entry validation. Native `SPCTLK2` is additionally bounded below `$3DFE`, whose 512-byte tail is persistent Earth packet scratch.
- `SPECTALK.OVL` is packed as a variable-length STOA atlas; do not seek fixed pages at runtime. The Spectranext atlas alone reserves a fixed 256-byte bootstrap after its header.
- Any new overlay entry must update this file, the matching `overlay_entry*.asm`, and callers using `overlay_exec()` or inline trampoline bytes.
- If an overlay uses `_overlay_slot`, preserve the caller's RX discard contract; `_overlay_slot` aliases `_rx_line`.
- Native Next overlay pages persist for the process lifetime; every entry and failure exit must initialize writable overlay-local state before it is read again.
- On Spectranext, `overlay/xfs_write_ovl.asm` is linked only into `SPCTLK4.OVL`; `_esx_replace_write` is a private helper and has no atlas entry or resident-map ABI.
- Spectranext XFS directory operations use a 256-byte scratch range but intentionally preserve only the 128-byte source at `$5CB6`, backed up through the input-cache alias `$5B00..$5B7F`. The resident `_input_cache_invalidate` boundary remains caller-owned after runtime esxDOS transactions; do not widen the preserve range to 256 bytes.

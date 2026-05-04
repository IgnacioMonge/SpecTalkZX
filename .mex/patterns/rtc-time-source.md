# RTC Time Source

RTC is an opt-in time source. Detect and use it only when the user selects `tz=rtc`/`!tz rtc`; otherwise keep the numeric SNTP timezone path.

## Rules
- Keep RTC detection cold. `SPCTLK5.OVL` entry 1 is called at startup only when loaded config set `tz=rtc`; entry 2 handles immediate `!tz rtc`; entry 3 handles numeric `!tz`. Numeric `tz=` configs must not probe/use RTC automatically.
- Try documented esxDOS/Next `M_DRVAPI ($92)` first with `C=0`, `B=0` (driver API RTC query), then fall back to `M_GETDATE ($8e)`. Both calls use `IY=$5C3A` around `RST 8`.
- Do not treat DivTIESUS `.ntpdate` as proof of a readable direct RTC ABI. Its source proves PCF8563 write access through ZX-Uno-style `FC3B/FD3B`, but it does not read the chip back.
- Do not trust carry alone on classic esxDOS/divMMC hardware. z88dk's classic wrapper treats the `M_GETDATE` result registers as guessed `BCDE`, and DivIDE without RTC returns fixed `23/04/1982 00:00:00`.
- Validate the returned MS-DOS date/time before accepting it. Current guard accepts years `2024..2035`, month `1..12`, non-zero day, hour `<24`, minute `<60`, and `seconds/2 <30`.
- If the documented API paths fail validation, the current last-resort direct PCF8563 fallback selects ZX-Uno-style `I2CREG=$F8` through `FC3B`, then uses `FD3B` as bit-banged I2C lines (`bit0=write SDA`, `bit1=SCL`), writes `$A2, $02`, repeated-start reads `$A3`, and reads seven bytes from PCF8563 registers `$02..$08`. The input SDA bit is not assumed anymore: the reader tries masks `01,02,04,08,10,20,40,80` and accepts only a valid decoded PCF8563 date/time.
- DivTIESUS hardware confirmation: the direct PCF8563 route works with read mask `P80` after `.ntpdate +2` and a real power-cycle. If startup time appears offset, first refresh the RTC using `.ntpdate +<tz>` and power-cycle before changing Spectalk's timezone model.
- Accept PCF8563 data only when the VL bit is clear and BCD seconds/minutes/hours/day/month/year are plausible (`year 24..35`).
- Use `sntp_tz == TZ_RTC (127)` as the RTC mode sentinel. Do not add another resident state byte unless this contract breaks.
- Keep one resident numeric backup in `sntp_tz_last`. It starts at `+1`, is loaded/saved as `tzlast=`, and is refreshed by every numeric `!tz`. This is the fallback target when configured RTC is unavailable.
- If RTC succeeds while in RTC mode, keep `sntp_tz == TZ_RTC`, seed `time_hour/time_minute/time_second`, reset `last_frames_lo/tick_accum`, mark the status bar dirty for immediate redraw, and make `sntp_init()` a no-op. If startup RTC seed fails, the seed overlay must clear RTC mode back to numeric `sntp_tz=sntp_tz_last`; the `!tz rtc` wrapper then restores the previous numeric timezone and prints `No RTC`.
- Entering RTC mode must clear `sntp_init_sent/sntp_waiting/sntp_queried` so no stale pending SNTP query can run after RTC becomes authoritative.
- Do not apply a numeric timezone to RTC time. RTC is local system time once selected.
- When leaving RTC with `!tz +N/-N/N`, switch `sntp_tz` to numeric and clear `sntp_init_sent/sntp_waiting/sntp_queried`, but do not adjust `time_hour`; keep the last RTC hour visible until NTP validates. Numeric-to-numeric TZ changes may adjust `time_hour` by delta immediately, as before.
- Keep feedback short: `tz=RTC`, `No RTC`, `tz=+N sync` when SNTP can be configured immediately in `STATE_WIFI_OK`, and `tz=+N later` when sync is pending until AT mode is available.
- The resident `!tz` command gate must match `rtc` case-insensitively; the command parser lowercases the command token, not the argument bytes. Use a compact ASCII fold before dispatching to entry 2.
- In RTC mode, `!config` should display `timezone= RTC`; otherwise show the numeric `+NN/-NN` value even if the machine has RTC hardware.
- Save `tz=rtc` only when RTC mode is active, and always save `tzlast=<numeric>` beside it. Save numeric `tz=` and matching `tzlast=` after `!tz +N/-N` so users can permanently opt out of bad/misconfigured RTC hardware.
- The temporary diagnostic display and `rtc_diag[19]` buffer were removed after DivTIESUS confirmation. Reintroduce diagnostics only as a bounded temporary debug build, not as normal resident/overlay state.

## Current Implementation
- `overlay/rtc_seed_ovl.asm`: `SPCTLK5.OVL` entry 1 RTC reader, `M_DRVAPI` then `M_GETDATE`, plus direct DivTIESUS PCF8563 fallback.
- `overlay/spectalk_ovl5.c`: config overlay and entry 2 for `!tz rtc`.
- `overlay/overlay_entry5.asm`: entry 3 for numeric `!tz`, including short feedback and SNTP-pending flag reset.
- `src/spectalk.c`: startup calls RTC only for `tz=rtc`; SNTP bypass only when RTC mode is selected; config load applies `tz=` and `tzlast=`.
- `src/user_cmds.c`: dispatches case-insensitive `!tz rtc` to SPCTLK5 entry 2 and numeric `!tz` to SPCTLK5 entry 3.
- `overlay/spectalk_ovl4.c`: saves `tz=rtc` when `sntp_tz == TZ_RTC`, otherwise saves numeric timezone, and persists `tzlast=`.

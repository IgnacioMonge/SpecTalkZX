# Status Hostname Short ASM

`extract_network_short()` feeds the status bar only. It must preserve the old C contract exactly:

- no dot in hostname: return the original hostname pointer
- only one dot: return the original hostname pointer
- two or more dots: copy bytes after the first dot up to the second dot into `sb_left_part + 45`
- cap the copied segment at 11 bytes and append NUL
- return `sb_left_part + 45`

Do not change the visible shortening rule, cap length, or fallback behavior while doing no-visible-change shrink work. The 2026-05-05 naked ASM rewrite was moved to `C:\tmp\SpecTalkZX-gemini-shrink` and measured there as part of the combined no-visible-change follow-up: TAP `35693B`, BSS guard `0xF308` (`504B` free).

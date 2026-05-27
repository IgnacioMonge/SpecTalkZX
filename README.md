# SpecTalkZX

SpecTalkZX is an IRC client for ZX Spectrum systems using divMMC/esxDOS and a
supported 115200-baud UART/ESP-AT network bridge.

Current release candidate: **v1.3.8: Hermes**.

## Requirements

- ZX Spectrum 48K/128K-compatible machine.
- divMMC/esxDOS-compatible storage.
- Supported UART/ESP-AT bridge at 115200 baud.
- SD card files kept in the same directory:
  - `SpecTalkZX.tap`
  - `SPECTALK.OVL`
  - `SPECTALK.DAT`

## Build

```sh
make NO_COLOR=1
```

Release-profile build:

```sh
make release NO_COLOR=1
```

Build outputs are written to `build/`:

- `build/SpecTalkZX.tap`
- `build/SPECTALK.OVL`
- `build/SPECTALK.DAT`

## Runtime Notes

- `SPECTALK.OVL` contains the 2K overlay pages used by help, bookmarks,
  configuration, About, RTC setup, and related screens.
- `SPECTALK.DAT` contains runtime data such as the font, themes, help text,
  BPE dictionary, and About assets.
- Keep all release files together on the SD card; missing or stale data files
  can cause broken help/about/config rendering.

## Main Features

- IRC connect/join/chat with multiple channel slots.
- Session bookmarks, autoconnect, and autojoin.
- Friend/ignore tracking and notifications.
- `/names`, `/list`, `/mode`, `/reply`, `/notice`, and NickServ helpers.
- RTC clock support.
- Animated `!about` overlay.
- Compact 4px text rendering tuned for real ZX hardware.

See `CHANGELOG.md` for the full v1.3.8 release history and build metrics.

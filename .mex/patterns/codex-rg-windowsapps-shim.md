# Codex rg WindowsApps Shim

## Problem

In the Codex desktop environment on Windows, `Get-Command rg -All` may resolve first to Codex's packaged binary under:

`C:\Program Files\WindowsApps\OpenAI.Codex_...\app\resources\rg.exe`

That file can be readable and still fail to start with `Acceso denegado`. If copying the same binary outside `WindowsApps` makes `rg --version` work, the problem is the AppX-managed execution location rather than ripgrep itself.

## Pattern

Prefer a durable user-local `rg.exe` earlier in PATH, for example:

`C:\Users\ignac\.local\bin\rg.exe`

Verify with:

```powershell
Get-Command rg -All
rg --version
```

The first result should be the user-local copy, not the packaged `WindowsApps` path.

## Avoid

Do not take ownership of, rewrite ACLs in, or patch files in `C:\Program Files\WindowsApps\OpenAI.Codex_*`. That tree is managed by Windows/AppX/TrustedInstaller, can be replaced by updates, and changing it can break the app package instead of fixing future sessions.

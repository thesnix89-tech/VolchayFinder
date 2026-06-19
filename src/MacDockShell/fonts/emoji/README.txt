Apple Color Emoji — not redistributable in public repos or installers.

This folder holds a locally downloaded emoji font for MacDockShell only.
The font is loaded at runtime and must NOT be committed to git.

Download (Windows build):
  powershell -ExecutionPolicy Bypass -File tools\setup_apple_emoji.ps1

Or manually from:
  https://github.com/samuelngs/apple-emoji-ttf/releases
  Asset: AppleColorEmoji-Windows.ttf

Place the file here:
  AppleColorEmoji-Windows.ttf

Optional user override (highest priority):
  %APPDATA%\ENI\MacDockShell\fonts\emoji\AppleColorEmoji-Windows.ttf

Deploy/runtime path (next to the executable):
  <exeDir>\fonts\emoji\AppleColorEmoji-Windows.ttf

Requirements:
  Qt 6.8.3 + AppleColorEmoji-Windows.ttf (COLR build for DirectWrite).
  MacDockShell loads the font via addApplicationFont and wires it as an app
  font fallback (family name: Segoe UI Emoji — Apple glyphs inside the app).
  Without the font file, MacDockShell falls back to the system Segoe UI Emoji.

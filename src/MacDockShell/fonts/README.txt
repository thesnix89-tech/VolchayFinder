SF Pro Text (Apple) — not redistributable in public repos.

Extract from SF-Pro.dmg (7-Zip on Windows):
  7z x SF-Pro.dmg
  7z x "SFProFonts/SF Pro Fonts.pkg" SFProFonts.pkg/Payload -o pkg
  7z x pkg/SFProFonts.pkg/Payload -o fonts
  7z e fonts/Payload~ "*SF-Pro-Text-Regular.otf" -r -o.
  7z e fonts/Payload~ "*SF-Pro-Text-Medium.otf" -r -o.
  7z e fonts/Payload~ "*SF-Pro-Text-Semibold.otf" -r -o.
  7z e fonts/Payload~ "*SF-Pro-Text-Bold.otf" -r -o.

Required files in this folder:
  SF-Pro-Text-Regular.otf
  SF-Pro-Text-Medium.otf
  SF-Pro-Text-Semibold.otf
  SF-Pro-Text-Bold.otf

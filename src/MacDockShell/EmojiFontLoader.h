#pragma once

#include <QString>

namespace EmojiFontLoader {

// Registers Apple Color Emoji as the application emoji font when a local TTF is found.
// Returns true when the font file was loaded successfully.
bool loadAppleEmojiFont(QString* loadedPathOut = nullptr, QString* familyOut = nullptr);

// Family name for QML Text elements that render emoji (loaded Apple font or system fallback).
QString emojiFontFamilyName();

} // namespace EmojiFontLoader

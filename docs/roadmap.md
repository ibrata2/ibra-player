# iBRA PLAYER roadmap

Stages are goals, not completed features. Each item becomes supported only after implementation and verification.

## 1. Runnable foundations

- Establish Windows and Android build projects.
- Set product identity, app icons, bilingual strings, and basic settings persistence.
- Create a real playback screen with open-file support and honest error handling.

## 2. Core playback

- Play, pause, stop, seek, volume, fullscreen, and next/previous.
- Add track and subtitle selection based on backend capabilities.
- Verify common local media files and Arabic subtitle rendering on each platform.
- Add playback state and capability reporting.

## 3. Everyday library

- Playlists, recent files, favorites, and resume position.
- M3U/M3U8 import/export where implemented.
- Privacy controls for history and local data.

## 4. Interface and accessibility

- Arabic and English UI, correct right-to-left layout, and readable typography.
- Windows glass-inspired themes with a usable fallback when effects are unavailable.
- Android adaptive phone/tablet layouts for density, orientation, and resizable windows.
- Keyboard, touch, accessibility, and media-key support.

## 5. In-car integration

- Android Auto media browsing and controls using supported media app APIs.
- Android Automotive OS presentation using approved car UI patterns where supported.
- Test with emulator configurations and available real head units; document verified screen sizes and limitations.

## 6. Packaging and release

- Windows installer and optional portable package.
- Android signed release build and store-ready metadata.
- Dependency inventory, license notices, release notes, and a tested compatibility matrix.

## Acceptance rule

A feature is marked complete only after it has an implementation, a documented platform/format scope, and verification evidence. Unsupported formats or hardware paths remain clearly labeled.

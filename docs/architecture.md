# iBRA PLAYER architecture

## Platform approach

This is one product with platform-appropriate implementations, not a promise that one UI binary runs everywhere.

### Windows desktop

- Native desktop application targeting Windows 10 and 11, 64-bit.
- Evaluate Qt 6/QML for the glass-style interface and a maintained playback backend such as libmpv or FFmpeg-backed Qt Multimedia.
- Keep playback and rendering behind a player-service boundary so the UI is not coupled to decoder internals.
- Package only reviewed dependencies and publish their licenses with releases.

### Android phones and tablets

- Native Android application using Kotlin, Jetpack Compose, and AndroidX Media3 for playback, media sessions, audio focus, and platform integration.
- Use adaptive layouts for compact, medium, and expanded window sizes, landscape/portrait orientation, touch targets, and system insets.
- Keep media library and playback state behind shared domain interfaces; use platform storage access APIs.

### In-car experiences

- Android Auto: expose media browsing and playback through Android media-session APIs and supported car templates. Do not mirror the phone's arbitrary visual player screen while driving.
- Android Automotive OS: add an adapted media experience using the supported car app library and vehicle-safe interaction patterns when the target environment supports it.
- Treat screen resolution and physical dimensions separately: adapt to available window size and density rather than hard-coding a single dashboard resolution.
- Validate with Android Automotive and Android Auto emulators, then real hardware. Availability and supported capabilities differ by vehicle and host.

## Boundaries

1. **Presentation:** desktop UI, Android handheld UI, and car-optimized UI.
2. **Playback:** open/close media, transport controls, seek, volume, track selection, playback errors, and capability reporting.
3. **Library:** playlists, recent items, favorites, and resume positions; local by default.
4. **Preferences:** language, appearance, playback, subtitles, privacy, and platform options.
5. **Packaging:** separate Windows installer/portable builds and Android APK/AAB release builds.

Share stable product behavior, translations, brand assets, and media capability documentation. Keep platform UI, permissions, storage access, audio focus, media sessions, and playback backend integration native where necessary.

## Trust and licensing

- Treat media files and playlist contents as untrusted input.
- Never download or execute unknown codec installers.
- Do not claim DRM-protected sources are playable.
- Record decoder and packaging licenses before distributing binaries. FFmpeg configuration can change the applicable license obligations.
- Keep viewing history on-device unless the user explicitly opts into a future sync feature.

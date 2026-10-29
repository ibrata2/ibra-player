# iBRA PLAYER

برمجة و تطوير ابراهيم الخزرجي — [Telegram](https://t.me/ibraprogram)

A bilingual (Arabic / English) media player for **Windows 10/11 (64-bit)** with a customizable glass interface, plus an Android prototype.

## Windows desktop player (`apps/windows`)

- **Engine:** libmpv (FFmpeg + libass + libplacebo) rendered through the mpv OpenGL render API inside Qt 6. Every control drives the real engine.
- **Formats:** containers and codecs come from the bundled FFmpeg build. Settings → Codecs shows the live decoder list of the installed build.
- **Glass themes:** Red & White (default), White, Black, Red & Black, Clear, plus unlimited custom themes made in **Theme Studio** (colors, opacity, frost, reflection, edge, glow, shadow, radius, button size; live preview; import/export `.ibratheme`). Optional Windows 11 acrylic behind the window; the built-in frosted glass is the lightweight fallback.
- **Player:** custom title bar, File/View/Play/Navigate/Favorites/Help menus, glass control bar with auto-hide, seek preview, A-B loop, 0.25×–4× speed with pitch correction, frame stepping, jump to time, chapters, aspect/zoom/crop/rotate/flip, picture adjustments, screenshots, resume, repeat/shuffle, always on top, full screen, drag & drop.
- **Subtitles:** SRT, ASS/SSA, VTT, PGS, VobSub and more; auto-load matching files; Arabic shaping and right-to-left text; encoding detection (UTF-8, Windows-1256, ISO-8859-6…); style, position and delay; secondary subtitles; optional OpenSubtitles web search.
- **Audio:** track selection, WASAPI exclusive mode, bitstream passthrough (AC-3/E-AC-3/DTS/TrueHD), speaker layout, balance, delay, loudness normalisation, output device choice.
- **Playlists & library:** M3U / M3U8 / PLS open & save, search, drag reorder, recent files, favorites, resume positions, privacy controls.
- **Windows integration:** single instance, Explorer “Play with iBRA PLAYER” / “Add to playlist” menu, optional file associations (user confirms in Default Apps), global media keys. Everything is per-user; no administrator rights are needed.
- **Hardware decoding:** automatic (safe) by default with software fallback; D3D11VA, DXVA2, Intel Quick Sync, NVDEC selectable.

### Download / build

Every push to `main` builds on GitHub Actions (Windows, MSYS2 UCRT64), runs a real-media self-test on Windows, and uploads:

- `iBRA PLAYER Setup <version>.exe` — installer (per-user by default, optional all-users)
- `iBRA PLAYER Portable <version>.zip` — portable build (settings stored next to the exe)

Pushing a tag such as `v1.0.0` publishes them as a GitHub Release.

Local build (MSYS2 UCRT64 shell):

```bash
pacman -S mingw-w64-ucrt-x86_64-{gcc,cmake,ninja,pkgconf,qt6-base,mpv,ntldd}
cmake -S apps/windows -B build/windows -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/windows
bash apps/windows/packaging/deploy.sh build/windows build/stage
```

The same sources build on Linux with Qt 6.4+ and libmpv 0.37+ (used for development tests).

### Verification

`"iBRA PLAYER.exe" --selftest <out-dir> <files…>` plays each file, checks that playback advances and seeking lands, records the engine's media information and saves a screenshot to `report.json`. CI generates test media in H.264, HEVC, AV1, VP8, VP9, MPEG-1/2/4, WMV, FLV, Theora, 4K HDR10 HEVC, MP3, FLAC, WAV, Opus, AAC, ALAC, AC-3, E-AC-3, DTS, TrueHD, WMA and Arabic subtitles, and fails the build if one does not play. See [docs/verification.md](docs/verification.md).

### Not supported

- Encrypted commercial Blu-ray / DRM-protected streams.
- Arbitrary website video URLs (only direct media and streaming URLs such as HLS/DASH/RTSP).
- DVD menus are basic (mpv/libdvdnav navigation).

## Android (`apps/android`)

Early Media3 prototype; see [docs/roadmap.md](docs/roadmap.md).

## License

GPL-3.0-or-later (required by the bundled GPL build of mpv/FFmpeg). Third-party notices are shipped in the installer.

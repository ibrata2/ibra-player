# Verification log — Windows player

## Development build (Linux, Qt 6.4.2, libmpv 0.37, FFmpeg 6.1, Mesa llvmpipe)

Self-test run with `--selftest` on generated media (8 s clips, 640×360 unless noted).
Every file loaded, played (position advanced ≥ 2.2 s in 2.5 s) and seeked to 50 % correctly:

| Video | Audio | Result |
|---|---|---|
| MP4 H.264 + external Arabic SRT (auto-loaded) | AAC | ✓ |
| MKV HEVC + embedded ASS (Arabic with diacritics) + SRT | AC-3 5.1 | ✓ |
| WebM AV1 (dav1d) | Opus | ✓ |
| WebM VP9 + Windows-1256 Arabic SRT (auto-detected) | Opus | ✓ |
| WebM VP8 + WebVTT | Vorbis | ✓ |
| AVI Xvid (MPEG-4 Part 2) | MP3 | ✓ |
| MPG MPEG-2 / VOB MPEG-2 / MPG MPEG-1 | MP2 / AC-3 | ✓ |
| TS / M2TS H.264 | AAC / E-AC-3 | ✓ |
| WMV (WMV8) | WMA v2 | ✓ |
| FLV (Sorenson) | MP3 | ✓ |
| MOV H.264 | ALAC | ✓ |
| 3GP H.264 320×240 | AAC 8 kHz | ✓ |
| OGV Theora | Vorbis | ✓ |
| M4V H.264 / MKV MPEG-4 Part 2 | AAC | ✓ |
| MKV 4K HEVC 10-bit HDR10 (detected as PQ, tone-mapped) | — | ✓ |
| MP3, FLAC, WAV, OGG, Opus, M4A AAC, M4A ALAC, AC-3, E-AC-3, DTS, TrueHD (MKA), WMA, AIFF, FLAC 7.1 | | ✓ |
| PNG still image | | ✓ shown |
| M3U8 and PLS playlists (relative paths, UTF-8 titles) | | ✓ |
| Truncated MP4, random-bytes MKV | | ✓ rejected with an error, no crash |

UI checks (screenshots): all five glass themes in English and Arabic, right-to-left layout,
playlist, settings pages, Theme Studio live preview, media information, context menu, idle and
audio-only screens; subtitles move above the control bar while it is visible.

## Windows build

Each CI run repeats the self-test on Windows Server 2022 with the shipped MSYS2 FFmpeg/mpv build
(Mesa software OpenGL in the test copy only) and adds the result table to the run summary.

Not verifiable in CI (no hardware): GPU hardware decoding paths, WASAPI exclusive/passthrough,
physical DVD/Blu-ray drives, multi-monitor full screen. They use mpv's standard implementations
and are selectable in Settings; the active decoder is shown in Media Information.

# iBRA PLAYER

A bilingual media player project for **Windows desktop** and **Android**, including responsive layouts for phones, tablets, and supported in-car media experiences.

> The project is being built in stages. Features are documented as planned until implemented and verified on real media files and devices.

## Product targets

- **Windows 10/11 (64-bit):** resizable desktop player, keyboard and mouse controls, playlists, subtitles, themes, and installer/portable packaging.
- **Android:** phone and tablet layouts that adapt to screen size and orientation.
- **In-car use:** Android Auto media browsing/playback through Android's supported media app interfaces, and a dedicated Android Automotive OS experience where supported. The car interface will prioritize glanceable controls and driving-safe interaction.

## Repository layout

- `apps/windows` — Windows desktop player
- `apps/android` — Android player and supported in-car integrations
- `shared` — shared product strings, brand assets, and media behavior specifications
- `docs` — architecture, roadmap, and implementation notes

The platforms may use native playback and UI components where required for reliability and platform integration, while sharing the iBRA PLAYER identity and product behavior.

## Current status

This repository contains the initial project foundation and implementation plan. It does **not yet** contain a playable application or installer. See [the roadmap](docs/roadmap.md) for staged delivery and [the architecture](docs/architecture.md) for technology boundaries.

## Development principles

- Do not advertise a codec, subtitle format, hardware decoder, or source type as supported until verified on the target platform.
- Keep media playback, user interface, and preferences separated.
- Do not upload media names or viewing history without explicit user consent.
- Keep Android Auto interactions within the media app experiences permitted by Android; provide a dedicated Android Automotive OS interface separately when applicable.
- Respect all third-party library licenses and document the exact components shipped in release builds.

## Brand

Official product name: **iBRA PLAYER**  
Repository: **ibra-player**

The selected red-and-black play monogram is in [assets/brand/ibra-player.svg](assets/brand/ibra-player.svg).

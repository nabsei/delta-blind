# Changelog

All notable changes to Delta Blind (formerly Delta Match) are documented
here. Format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and versioning follows [Semantic Versioning](https://semver.org/) adapted
for a pre-1.0 beta:

- **PATCH** (0.x.y): bug fixes only, no behavior/feature changes
- **MINOR** (0.x.0): new features or notable user-facing changes
- **MAJOR** (1.0.0+): first stable release, then breaking changes only

## [0.2.0] - 2026-07-20

### Changed
- Renamed from Delta Match to **Delta Blind**, matching the sibling
  Delta -> Delta Zero rename -- both fix the same ambiguity between a
  product name and the "Delta line" family name. `PRODUCT_NAME`, bundle
  ID, and the on-screen title changed accordingly. No DSP or parameter
  changes.

## [0.1.0] - 2026-07-19

### Added
- First public beta build. Loudness-matched A/B compare tool: pick A or B
  to hear it (click-free crossfade on switch), toggle MATCH to have B's
  level continuously (slow-adapting) gain-trimmed to meet A's, so a
  preference between the two reflects a real difference rather than "louder
  sounds better".
- Reuses Delta's sidechain-bus, file-load-A/B, and built-in test-signal
  infrastructure -- same audience, same architecture, a different DSP core
  (RMS-based level matching instead of cross-correlation/null-test).
- Builds as VST3, AU (passes `auval`), and Standalone on macOS and Windows.

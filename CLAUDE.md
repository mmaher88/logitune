# Logitune — Claude Code Rules

## Git workflow

- **Never push directly to master.** All changes go through a PR branch that the maintainer reviews before merging.
- **Never amend published commits** unless explicitly asked.
- **Do not add co-author signatures** to commit messages.
- **Do not include test URLs, site names, or sensitive information** in commit messages.

## Code changes

- After any code change, always kill and relaunch the app to verify. Never assume a running instance reflects new code.
- Launch apps via `nohup <cmd> > /tmp/logfile.log 2>&1 & disown`. Never use `run_in_background` for app launches (causes context crashes).
- Reference Solaar and logid source for HID++ protocol questions. Don't reverse-engineer from scratch what's already documented in those projects.

## Architecture

- HID++ feature variants (Battery, SmartShift, etc.) are handled via capability dispatch tables in `src/core/hidpp/capabilities/`. Adding a new variant = one table entry, zero DeviceManager changes.
- Device descriptors live in `src/core/devices/`. Each device is self-contained with its own CIDs, images, hotspots, and feature flags.
- `FeatureSupport` flags control UI visibility. Set a flag to `false` in the descriptor to hide controls for features that don't work on that device yet.

## Testing

- Run `logitune-tests` after every code change. All tests must pass before committing.
- Smoke test on real hardware (MX Master 3S) before pushing any branch that touches DeviceManager or HID++ code.
- Use the parameterized `DeviceSpec` pattern in `tests/test_device_registry.cpp` when adding device descriptor tests.

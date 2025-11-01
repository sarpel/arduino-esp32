# Repository Guidelines

## Project Structure & Module Organization
- `src/` contains firmware modules (`main.cpp`, `network.cpp`, `i2s_audio.cpp`, helpers like `config_validator.h`); keep new code modular and header-guarded.
- `tests/` stores Unity specs; mirror module names and prefer small, deterministic cases.
- `docs/` houses canonical references (DEVELOPER_GUIDE, OTA docs); revise alongside code so behaviour stays documented.
- `scripts/report_build_size.py` surfaces flash/RAM trends; `.pio/` is generated output and should remain untracked.
- Environments live in `platformio.ini`; derive new boards from `esp32dev` unless a unique profile is required.

## Build, Test, and Development Commands
- `pio run -e esp32dev` compiles the default firmware; append `--target upload --upload-port COM8` for USB flashing.
- `pio run -e esp32dev-ota --target upload` deploys via OTA using the configured host.
- `pio device monitor --port COM8 --baud 115200` captures runtime logging; include relevant excerpts in reviews.
- `python scripts/report_build_size.py .pio/build/esp32dev/firmware.elf` reports size deltas after each build.

## Coding Style & Naming Conventions
- Adopt 4-space indentation with K&R braces to match existing files.
- Classes stay PascalCase, functions camelCase, file-scope constants `kCamelCase`, and compile-time macros ALL_CAPS.
- Guard globals with `static`, favor stack buffers, and route diagnostics through the logging helpers.
- Validate configuration changes via `ConfigValidator` before use.

## Testing Guidelines
- Run `pio test -e esp32dev` before pushing; mention the command in commit or PR notes.
- Place hardware-dependent checks under subfolders (e.g. `tests/hw/`) and document manual steps in the PR.
- Capture serial or OTA evidence when exercising new runtime paths.

## Commit & Pull Request Guidelines
- Follow `type: imperative summary` (`feat: add adaptive buffer telemetry`, `fix: harden wifi reconnect`); keep commits small and focused.
- Reference issue IDs or doc updates in the body and list the build/test commands you executed.
- PRs should note target board, behavioural impact, and relevant logs or size reports.
- Sync code and docs in one reviewable change and confirm CI or local checks before requesting review.

## Security & Configuration Tips
- Never commit credentials; keep `src/config.h` sanitized and describe overrides instead of storing secrets.
- Update `platformio.ini` comments and docs when OTA hosts or ports change.
- Audit new network paths with `ConfigValidator` to ensure bounds and defaults stay safe.

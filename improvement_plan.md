# ESP32 Audio Streamer – Reliability Improvement Plan

Focus: maximize long‑term stability and fault tolerance. Latency is explicitly not a priority. This plan lists concrete risks found in the current codebase and proposes prioritized, low‑risk changes with measurable acceptance criteria.

Repository reviewed: src/*, platformio.ini, README and docs.

---

## 1) Critical Risks (fix first)

- Config validation bugs (compile/runtime blockers)
  - Findings:
    - `src/config_validator.h`: uses `strlen(SERVER_PORT)` and logs port with `%s` in a few places even though `SERVER_PORT` is an integer macro (e.g., validateServerConfig). This is UB/compile error on some toolchains and can block builds.
  - Actions:
    - Treat `SERVER_PORT` as integer. Replace `strlen` checks with range validation (1..65535). Use `%u` in logs.
  - Acceptance:
    - Builds succeed on both `esp32dev` and `seeed_xiao_esp32s3` envs without warnings.
    - On boot with invalid port=0 or >65535, validator rejects config with clear CRITICAL log and the device halts safely.

- Boot loop risk on WiFi failures
  - Findings:
    - `src/network.cpp`: When WiFi cannot connect after `WIFI_MAX_RETRIES`, the device calls `ESP.restart()` unconditionally. With bad credentials or network outage, this causes infinite reboot loops.
  - Actions:
    - Replace unconditional restart with a “safe backoff” mode: stop restarting, extend retry interval (e.g., 60–300s), and keep serial command interface alive.
    - Optionally: enable temporary AP fallback for provisioning after repeated failures (see section 2).
  - Acceptance:
    - With invalid credentials, device remains up (no rapid reboot), retries periodically, and accepts serial commands.

- Watchdog configuration vs. operation timing
  - Findings:
    - `WATCHDOG_TIMEOUT_SEC` is 10s; `WIFI_TIMEOUT` is 30s. Docs warn, but code relies on frequent `esp_task_wdt_reset()` in loop. If any blocking path occurs (DNS stall, driver lock), WDT may reset.
    - No explicit `esp_task_wdt_init/add` call; relying on Arduino core defaults is fragile across core versions.
  - Actions:
    - Explicitly initialize and add the main task to the WDT with a conservative timeout (e.g., 30–60s) aligned with worst‑case WiFi/TCP operations.
    - Audit all paths for >500ms blocking and add `wdt feed` or convert to non‑blocking timers.
  - Acceptance:
    - No watchdog resets during: failed WiFi join (30s), repeated TCP backoff (≤60s), or weak networks.

- Aggressive WiFi “preemptive reconnect” can destabilize
  - Findings:
    - `NetworkManager::monitorWiFiQuality()` forcibly disconnects when RSSI < threshold. This can cause oscillation under marginal RF conditions.
  - Actions:
    - Remove forced disconnect; only log and adjust buffering. Reconnect only on actual link loss.
  - Acceptance:
    - Under marginal RSSI (−80 to −90 dBm), link remains up longer, reconnect count decreases, no thrash.

- I2S APLL reliability on ESP32-S3
  - Findings:
    - `i2s_audio.cpp`: sets `.use_apll = true`. On some boards/clock trees this can fail sporadically.
  - Actions:
    - If driver install fails and `.use_apll = true`, retry once with `.use_apll = false` before giving up.
  - Acceptance:
    - Devices that previously failed I2S init due to APLL come up successfully with fallback.

- TCP write handling and stale connection detection
  - Findings:
    - `writeData()` only checks incomplete writes. It uses `last_successful_write` for a 5s timeout, but does not set lwIP SO_SNDTIMEO at the socket level.
  - Actions:
    - Set `SO_SNDTIMEO` (e.g., 5–10s) on the underlying socket fd; keep existing keepalive settings.
    - On timeout or EWOULDBLOCK, treat as error and trigger reconnect via state machine.
  - Acceptance:
    - When server stalls, client recovers by reconnecting without hanging or WDT resets.

---

## 2) High Priority Improvements

- Safe‑mode and provisioning after repeated failures
  - Rationale: Avoid field truck‑rolls for credentials/server corrections.
  - Actions:
    - Count consecutive WiFi or TCP failures in RTC memory (persist across resets). If >N within M minutes, enter Safe Mode: stop streaming, expand retry interval (5–10 minutes), keep serial command interface; optionally start a captive AP (e.g., `ESP32-AudioStreamer-Setup`) to set SSID/password/server.
  - Acceptance:
    - With bad config, device automatically enters Safe Mode after threshold and remains debuggable; no hot reboot loop.

- Config validation completeness and clarity
  - Actions:
    - In `ConfigValidator`, add checks for SSID/pass length (SSID 1–32, pass 8–63), `SERVER_HOST` non‑empty, pin ranges valid for the board, and ensure `I2S_BUFFER_SIZE` aligns with DMA len (multiples helpful but not required).
    - Log actionable remediation hints once, not every loop.
  - Acceptance:
    - Misconfigurations produce one‑time, readable error block at boot; no repeated spam.

- Log rate limiting and levels
  - Findings: High‑rate WARN/ERROR can starve CPU and serial.
  - Actions:
    - Add per‑site rate limit (token bucket or time gate) for recurring logs (e.g., failed connect, I2S transient).
    - Respect a compile‑time `DEBUG_LEVEL` in `Logger::init()` (currently ignored) and provide runtime downgrade via serial command.
  - Acceptance:
    - Under persistent failure, logs show ≤1 line per second per subsystem; CPU usage for logging <5%.

- Robust reconnection backoff
  - Actions:
    - Add jitter (±20%) to exponential backoff to avoid herd effects.
    - Cap max attempts before entering Safe Mode (above) rather than restarting.
  - Acceptance:
    - Reconnection attempts spread in time; measured reconnect storms are reduced.

- Memory health hardening
  - Actions:
    - Use `heap_caps_get_free_size(MALLOC_CAP_8BIT)` for accurate 8‑bit heap; track minimums in long‑term stats.
    - Raise `MEMORY_CRITICAL_THRESHOLD` action from immediate reboot to staged: stop I2S → close TCP → GC opportunity → if still low, reboot.
  - Acceptance:
    - Under induced fragmentation, system gracefully shuts down streaming and recovers without crash.

---

## 3) Medium Priority Improvements

- TCP/streaming robustness
  - Add `SO_RCVTIMEO` and periodic `client.connected()` revalidation before large writes.
  - Buffering: optionally batch writes to a fixed chunk size (e.g., 1024–4096B) from I2S buffer for fewer syscalls; not for latency, for stability.

- I2S configuration validation & recovery
  - Validate pin mapping fits the active board (S3 vs DevKit). On repeated I2S read timeouts, perform `i2s_zero_dma_buffer()` and re‑start before a full driver uninstall.

- Crash cause and uptime persistence
  - Store last reset reason, error counters, and last N critical logs in RTC/NVS to inspect after reboot.

- Brownout and power fault handling
  - Ensure brownout detector is enabled (Arduino core default may vary). Detect supply dips and log a specific critical message.

- Operational safeguards
  - Add a “maintenance window” command to suspend streaming (keep WiFi up) for OTA/diagnostics (even if OTA not yet added).

---

## 4) Low Priority / Hygiene

- Unify debug configuration
  - `DEBUG_LEVEL` (config.h) vs `Logger::init(LOG_INFO)` and `CORE_DEBUG_LEVEL` (platformio.ini) are divergent. Standardize to one source of truth (prefer build flag → Logger default).

- Trim destabilizing features
  - Remove AdaptiveBuffer‑driven forced reconnects; keep size advisory only. Optionally expose current buffer size via STATUS without changing behavior dynamically.

- Documentation alignment
  - README references docs not present (e.g., IMPLEMENTATION_SUMMARY.md). Either add stubs or update README to current set.

---

## 5) Test & Verification Plan

- Environments: `esp32dev`, `seeed_xiao_esp32s3`.
- Scenarios:
  - Invalid WiFi credentials: no boot loop; Safe Mode reachable; serial commands responsive.
  - Server unreachable: exponential backoff with jitter; no WDT resets; periodic attempts continue indefinitely.
  - Weak RSSI (−85 dBm): no forced disconnects; lower throughput tolerated; connection persists longer than current implementation.
  - I2S driver init failure injected: APLL→PLL fallback succeeds; if not, clear fatal message and halt safely (no reboot loop).
  - Memory pressure: staged shutdown before reboot; capture of stats in RTC/NVS confirmed post‑reboot.

- Metrics collected per 24h run:
  - Reconnect counts (WiFi/TCP) ≤ current baseline, no watchdog resets, no uncontrolled reboots.
  - Free heap min, error counters, and uptime stable across reconnections.

---

## 6) Implementation Notes (scoped, minimal changes)

- Files to touch (surgical):
  - `src/config_validator.h`: fix type checks; add length/range validations; reduce repeated logs.
  - `src/network.cpp`: remove RSSI‑triggered disconnect; replace restart‑on‑max‑retries with safe backoff; add socket timeouts/jitter; Safe Mode entry.
  - `src/i2s_audio.cpp`: APLL fallback; optional quick restart of I2S before full uninstall.
  - `src/main.cpp`: explicit WDT init/add; staged memory critical handling; optionally read RTC crash counters.
  - `src/logger.{h,cpp}`: honor `DEBUG_LEVEL`; add simple rate limiter.

- Deferred/optional (feature‑level):
  - Captive AP provisioning; RTC/NVS persistence; brownout explicit logs.

---

## 7) Known Non‑Goals (for this plan)

- Latency reduction or throughput optimizations.
- Introducing heavy new dependencies or large architectural rewrites.

---

## 8) Quick Wins Checklist

- [ ] Fix `SERVER_PORT` validation/logging type issues.
- [ ] Remove RSSI‑driven WiFi disconnects.
- [ ] Replace restart‑on‑WiFi‑failure with safe backoff loop.
- [ ] Explicit WDT init with ≥30s timeout.
- [ ] I2S APLL→PLL fallback on init failure.
- [ ] Set `SO_SNDTIMEO` for TCP writes; keep keepalive.
- [ ] Honor `DEBUG_LEVEL` and apply log rate limiting.

---

## 9) Longer‑Term Hardening (optional)

- Safe Mode with captive AP provisioning UI.
- Persist error stats and last logs in RTC/NVS across reboots.
- Nightly scheduled soft‑restart to defragment heap on long‑running units (only if verified helpful).
- Add a basic ring buffer for outgoing audio to decouple I2S from TCP hiccups.

---

## Appendix – Pointers to Relevant Code

- Config: `src/config.h`
- Config validation: `src/config_validator.h`
- Main loop and memory checks: `src/main.cpp`
- I2S: `src/i2s_audio.{h,cpp}`
- Network/TCP/WiFi: `src/network.{h,cpp}`
- Logging: `src/logger.{h,cpp}`
- Serial commands: `src/serial_command.{h,cpp}`

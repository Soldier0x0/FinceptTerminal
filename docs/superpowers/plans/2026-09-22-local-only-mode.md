# Local-Only Mode Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make this fork of Fincept Terminal boot straight to the dashboard with no Fincept login, no `api.fincept.in` traffic, Ollama as the default LLM, and no Enterprise/pricing chrome — all behind one `FINCEPT_LOCAL_ONLY` switch that defaults ON.

**Architecture:** A leaf unit `core/config/LocalMode` answers `local_mode::enabled()` from a CMake compile definition plus an env-var override. Roughly a dozen existing gates (`WindowFrame` routing, `AuthManager::initialize`, cloud-sync registration, `QuantLibClient::call`, `UpdateService`, `UpgradeDialog`, `ProviderCatalog`, `ToolBar`, `SettingsScreen`) consult it and early-return or hide. A one-shot `LocalModeBootstrap` fixes the LLM provider rows the upstream `v002` migration seeds. Nothing upstream is deleted.

**Tech Stack:** C++20, Qt 6.8.3 (Widgets, Sql, Test), CMake ≥ 3.27 + Ninja, SQLite via `LlmConfigRepository`.

**Spec:** `docs/superpowers/specs/2026-09-22-local-only-mode-design.md`

## Global Constraints

- Gate, don't delete: no upstream file, class, screen, migration or provider is removed. Every behaviour change is behind `fincept::local_mode::enabled()`.
- CMake option name and default: `option(FINCEPT_LOCAL_ONLY "..." ON)`; compile definition `FINCEPT_LOCAL_ONLY=1` / `=0` applied with `target_compile_definitions(FinceptTerminal PRIVATE ...)` after `add_executable`, never `add_compile_definitions` (it would leak into `tests/`).
- Runtime override: environment variable `FINCEPT_LOCAL_ONLY`; `0|false|off|no` → off, `1|true|on|yes` → on, case-insensitive; anything else → compiled default.
- Default LLM in local mode: provider `ollama`, base URL `http://localhost:11434`, model `llama3.1:8b`.
- Tests follow `fincept-qt/tests/README.md`: one `tst_<unit>` executable per unit, `QTEST_GUILESS_MAIN`, link only `Qt6::Test Qt6::Core`, list app sources explicitly, no OBJECT libraries.
- Include style: project-root-relative (`#include "core/config/LocalMode.h"`), matching the rest of `src/`.
- Logging: `LOG_INFO("<Tag>", ...)` from `core/logging/Logger.h`, as neighbouring code does.
- Build/verify commands on Ubuntu (all from `/workspace/fincept-qt`):
  - Configure: `cmake --preset linux-release -DFINCEPT_BUILD_TESTS=ON`
  - Build: `cmake --build --preset linux-release`
  - Unit tests: `ctest --test-dir build/linux-release --output-on-failure`
  - Smoke: `QT_QPA_PLATFORM=offscreen ./build/linux-release/FinceptTerminal --smoke-test` → must print `[Smoke] exit 0`
  - If Qt is not on the default prefix: append `-DCMAKE_PREFIX_PATH=/path/to/Qt/6.8.3/gcc_64` to the configure command (see `setup.sh`).
- Commit after every task with the message shown in that task. Branch: `cursor/local-only-mode-spec-5a93`.

---

## File Structure

| Path | Responsibility | Task |
|---|---|---|
| `fincept-qt/src/core/config/LocalMode.h` / `.cpp` | **new** — the switch: `resolve()` (pure) and `enabled()` (memoised) | 1 |
| `fincept-qt/tests/tst_local_mode.cpp` | **new** — unit tests for `resolve()` / `enabled()` | 1 |
| `fincept-qt/CMakeLists.txt` | option, compile definition, two new sources, status line | 1, 5 |
| `fincept-qt/tests/CMakeLists.txt` | two new test targets | 1, 4 |
| `fincept-qt/src/auth/AuthManager.cpp` | `initialize()` never loads a session in local mode | 2 |
| `fincept-qt/src/app/WindowFrame.h` / `.cpp` / `WindowFrame_Auth.cpp` | `has_full_access()`, straight-to-dashboard route, `on_auth_state_changed` early return | 2 |
| `fincept-qt/src/app/main.cpp` | skip cloud-sync registration; call LLM bootstrap | 3, 5 |
| `fincept-qt/src/services/quantlib/QuantLibClient.cpp` | fail fast in `call()` | 3 |
| `fincept-qt/src/services/updater/UpdateService.cpp` | no update check | 3 |
| `fincept-qt/src/ui/widgets/EnterprisePromo.cpp` | no upgrade dialog | 3 |
| `fincept-qt/src/services/llm/ProviderCatalog.h` / `.cpp` | hide `fincept`; `default_provider()`, `default_model()`; display name | 4 |
| `fincept-qt/src/services/llm/LlmService.cpp` | fallback uses catalog defaults | 4 |
| `fincept-qt/tests/tst_provider_catalog.cpp` | **new** — catalog tests | 4 |
| `fincept-qt/src/core/config/LocalModeBootstrap.h` / `.cpp` | **new** — one-shot LLM row fix-up | 5 |
| `fincept-qt/src/ui/navigation/ToolBar.cpp` | hide credits/plan/upgrade/logout; LOCAL label; help-menu trim | 6 |
| `fincept-qt/src/screens/settings/SettingsScreen.cpp` | no Cloud Sync sidebar button | 6 |
| `docs/LOCAL_ONLY_MODE.md`, `README.md` | user docs | 7 |

---

### Task 1: The switch — `LocalMode` + CMake option + unit test

**Files:**
- Create: `fincept-qt/src/core/config/LocalMode.h`
- Create: `fincept-qt/src/core/config/LocalMode.cpp`
- Create: `fincept-qt/tests/tst_local_mode.cpp`
- Modify: `fincept-qt/CMakeLists.txt` (option near line 366; source list near line 837; compile definition near line 3298; status line near line 4191)
- Modify: `fincept-qt/tests/CMakeLists.txt` (append a target after `tst_order_validator`)

**Interfaces:**
- Produces: `bool fincept::local_mode::resolve(bool compiled_default, const char* env_value)` and `bool fincept::local_mode::enabled()`. Every later task includes `"core/config/LocalMode.h"` and calls `enabled()`.

- [ ] **Step 1: Write the failing test**

Create `fincept-qt/tests/tst_local_mode.cpp`:

```cpp
// Unit tests for src/core/config/LocalMode.{h,cpp}
//
// LocalMode is the single switch behind every "no Fincept cloud" behaviour in
// this fork. resolve() is pure so its table is pinned here; enabled() is
// memoised, so the one process-wide assertion is that it does not flip.

#include "core/config/LocalMode.h"

#include <QTest>

using fincept::local_mode::enabled;
using fincept::local_mode::resolve;

class TstLocalMode : public QObject {
    Q_OBJECT

  private slots:
    void unset_env_returns_compiled_default();
    void empty_env_returns_compiled_default();
    void env_zero_false_off_no_disable();
    void env_one_true_on_yes_enable();
    void env_is_case_insensitive();
    void garbage_env_returns_compiled_default();
    void enabled_is_stable_across_calls();
};

void TstLocalMode::unset_env_returns_compiled_default() {
    QCOMPARE(resolve(true, nullptr), true);
    QCOMPARE(resolve(false, nullptr), false);
}

void TstLocalMode::empty_env_returns_compiled_default() {
    QCOMPARE(resolve(true, ""), true);
    QCOMPARE(resolve(false, ""), false);
}

void TstLocalMode::env_zero_false_off_no_disable() {
    for (const char* v : {"0", "false", "off", "no"}) {
        QVERIFY2(!resolve(true, v), v);
        QVERIFY2(!resolve(false, v), v);
    }
}

void TstLocalMode::env_one_true_on_yes_enable() {
    for (const char* v : {"1", "true", "on", "yes"}) {
        QVERIFY2(resolve(true, v), v);
        QVERIFY2(resolve(false, v), v);
    }
}

void TstLocalMode::env_is_case_insensitive() {
    QCOMPARE(resolve(true, "FALSE"), false);
    QCOMPARE(resolve(true, "Off"), false);
    QCOMPARE(resolve(false, "TRUE"), true);
    QCOMPARE(resolve(false, "Yes"), true);
}

void TstLocalMode::garbage_env_returns_compiled_default() {
    QCOMPARE(resolve(true, "maybe"), true);
    QCOMPARE(resolve(false, "maybe"), false);
    QCOMPARE(resolve(true, "2"), true);
}

void TstLocalMode::enabled_is_stable_across_calls() {
    const bool first = enabled();
    // Changing the environment after the first call must not change the answer:
    // the whole app reads one value for the life of the process.
    qputenv("FINCEPT_LOCAL_ONLY", first ? "0" : "1");
    QCOMPARE(enabled(), first);
    qunsetenv("FINCEPT_LOCAL_ONLY");
}

QTEST_GUILESS_MAIN(TstLocalMode)
#include "tst_local_mode.moc"
```

Append to `fincept-qt/tests/CMakeLists.txt`, after the `tst_order_validator` block and before the final `message(STATUS ...)`:

```cmake
# src/core/config/LocalMode.{h,cpp} — leaf unit, Qt Core only. The app option
# FINCEPT_LOCAL_ONLY is deliberately NOT defined for this target, so the test
# exercises resolve() with both compiled defaults and enabled() falls back to 0.
fincept_add_test(tst_local_mode
    tst_local_mode.cpp
    "${PROJECT_SOURCE_DIR}/src/core/config/LocalMode.cpp")
```

And update the status line at the bottom of that file:

```cmake
message(STATUS "Fincept tests: tst_broker_modify_fields, tst_result, tst_order_validator, tst_local_mode")
```

- [ ] **Step 2: Run the test to verify it fails**

Run (from `/workspace/fincept-qt`):

```bash
cmake --preset linux-release -DFINCEPT_BUILD_TESTS=ON
cmake --build --preset linux-release --target tst_local_mode
```

Expected: configure or build FAILS with `Cannot find source file: .../src/core/config/LocalMode.cpp` or `fatal error: core/config/LocalMode.h: No such file or directory`.

- [ ] **Step 3: Write the header**

Create `fincept-qt/src/core/config/LocalMode.h`:

```cpp
#pragma once
// LocalMode — the single switch behind every "no Fincept cloud" behaviour in
// this fork. When enabled, the terminal skips the login/PIN/pricing stack,
// never contacts api.fincept.in, defaults the LLM to Ollama, and hides the
// Enterprise/pricing chrome. Upstream behaviour is preserved when disabled.
//
// Compile-time default: FINCEPT_LOCAL_ONLY (CMake option, ON in this fork).
// Runtime override:     FINCEPT_LOCAL_ONLY environment variable.
//
// Leaf unit: depends on Qt Core only, so it is unit-testable and can be
// included from any layer without pulling in services.

namespace fincept::local_mode {

/// Pure decision used by enabled(). `compiled_default` is the CMake option;
/// `env_value` is the raw environment variable (nullptr or "" = unset).
/// "0"/"false"/"off"/"no" → false; "1"/"true"/"on"/"yes" → true
/// (case-insensitive); anything else → compiled_default.
bool resolve(bool compiled_default, const char* env_value);

/// Process-wide answer. Computed once on first call from
/// resolve(FINCEPT_LOCAL_ONLY, getenv("FINCEPT_LOCAL_ONLY")) and never changes.
bool enabled();

} // namespace fincept::local_mode
```

- [ ] **Step 4: Write the implementation**

Create `fincept-qt/src/core/config/LocalMode.cpp`:

```cpp
#include "core/config/LocalMode.h"

#include <QByteArray>
#include <QtGlobal>

// Absent when compiled outside the FinceptTerminal target (unit tests): fall
// back to upstream behaviour so the test binary does not depend on the option.
#ifndef FINCEPT_LOCAL_ONLY
#define FINCEPT_LOCAL_ONLY 0
#endif

namespace fincept::local_mode {

bool resolve(bool compiled_default, const char* env_value) {
    if (env_value == nullptr || *env_value == '\0')
        return compiled_default;
    const QByteArray v = QByteArray(env_value).trimmed().toLower();
    if (v == "0" || v == "false" || v == "off" || v == "no")
        return false;
    if (v == "1" || v == "true" || v == "on" || v == "yes")
        return true;
    return compiled_default;
}

bool enabled() {
    static const bool value = [] {
        const QByteArray env = qgetenv("FINCEPT_LOCAL_ONLY");
        return resolve(FINCEPT_LOCAL_ONLY != 0, env.isNull() ? nullptr : env.constData());
    }();
    return value;
}

} // namespace fincept::local_mode
```

- [ ] **Step 5: Wire CMake**

In `fincept-qt/CMakeLists.txt`, directly after the `option(FINCEPT_BUILD_TESTS ...)` line (≈ line 366) add:

```cmake
# ── Local-only mode (this fork) ──────────────────────────────────────────────
# ON: no Fincept login/PIN/pricing, no api.fincept.in traffic, Ollama default
# LLM, no Enterprise promo. OFF restores upstream behaviour. Runtime override:
# FINCEPT_LOCAL_ONLY=0|1 in the environment. See docs/LOCAL_ONLY_MODE.md.
option(FINCEPT_LOCAL_ONLY
    "Local-only build: no Fincept login/cloud, Ollama default LLM, no Enterprise promo" ON)
```

In the source list, directly after `src/core/config/AppConfig.cpp` (≈ line 837) add:

```cmake
    src/core/config/LocalMode.cpp
```

In the "Optional feature compile definitions (applied after add_executable)" block (≈ line 3298), before `if(FINCEPT_HAS_PRIVATE_HEADERS)` add:

```cmake
if(FINCEPT_LOCAL_ONLY)
    target_compile_definitions(FinceptTerminal PRIVATE FINCEPT_LOCAL_ONLY=1)
else()
    target_compile_definitions(FinceptTerminal PRIVATE FINCEPT_LOCAL_ONLY=0)
endif()
```

In the configuration summary, directly after `message(STATUS "  Tests             : ${FINCEPT_BUILD_TESTS}")` (≈ line 4191) add:

```cmake
message(STATUS "  Local-only mode   : ${FINCEPT_LOCAL_ONLY}")
```

- [ ] **Step 6: Run the test to verify it passes**

```bash
cmake --preset linux-release -DFINCEPT_BUILD_TESTS=ON
cmake --build --preset linux-release --target tst_local_mode
ctest --test-dir build/linux-release --output-on-failure -R tst_local_mode
```

Expected: configure output includes `Local-only mode   : ON`; ctest prints `100% tests passed, 0 tests failed out of 1`.

- [ ] **Step 7: Commit**

```bash
git add fincept-qt/src/core/config/LocalMode.h fincept-qt/src/core/config/LocalMode.cpp \
        fincept-qt/tests/tst_local_mode.cpp fincept-qt/tests/CMakeLists.txt fincept-qt/CMakeLists.txt
git commit -m "feat(local-mode): add FINCEPT_LOCAL_ONLY switch with env override and tests"
```

---

### Task 2: Boot straight to the dashboard

**Files:**
- Modify: `fincept-qt/src/auth/AuthManager.cpp` (`AuthManager::initialize`, ≈ line 223)
- Modify: `fincept-qt/src/app/WindowFrame.h` (private section, ≈ line 270 next to `void on_auth_state_changed();`)
- Modify: `fincept-qt/src/app/WindowFrame.cpp` (constructor routing block, ≈ lines 914–944; add helper impl)
- Modify: `fincept-qt/src/app/WindowFrame_Auth.cpp` (`on_auth_state_changed` top ≈ line 33; two `has_paid_plan()` at ≈ lines 122 and 354)

**Interfaces:**
- Consumes: `fincept::local_mode::enabled()` (Task 1).
- Produces: `bool WindowFrame::has_full_access() const` (private; used only within WindowFrame files).

- [ ] **Step 1: AuthManager never loads a session in local mode**

In `fincept-qt/src/auth/AuthManager.cpp` add the include after `#include "auth/AuthManager.h"`:

```cpp
#include "core/config/LocalMode.h"
```

Replace the start of `AuthManager::initialize()`:

```cpp
void AuthManager::initialize() {
    if (local_mode::enabled()) {
        // No Fincept session in local-only mode: nothing is loaded, validated
        // or refreshed. SessionGuard, CloudSyncEngine, the WindowFrame refresh
        // timers and auto_configure_fincept_llm() all gate on an authenticated
        // session and therefore stay inert.
        LOG_INFO("Auth", "Local-only mode — auth disabled, no session will be loaded");
        session_ = SessionData{};
        set_loading(false);
        emit auth_state_changed();
        return;
    }
    set_loading(true);
    load_session();
```

(The rest of the function is unchanged.)

- [ ] **Step 2: Declare and implement `has_full_access()`**

In `fincept-qt/src/app/WindowFrame.h`, directly before `void on_auth_state_changed();` in the private section add:

```cpp
    /// True when the privileged shell may be shown: local-only mode, or an
    /// authenticated session on a paid plan. Every "paid plan" routing check
    /// in WindowFrame goes through here so the local-mode bypass has one home.
    bool has_full_access() const;
```

In `fincept-qt/src/app/WindowFrame_Auth.cpp` add the include after `#include "auth/AuthManager.h"`:

```cpp
#include "core/config/LocalMode.h"
```

and add, directly after `namespace fincept {`:

```cpp
bool WindowFrame::has_full_access() const {
    return local_mode::enabled() || auth::AuthManager::instance().session().has_paid_plan();
}

```

- [ ] **Step 3: Route auth-state changes through the bypass**

Still in `WindowFrame_Auth.cpp`, change the top of `on_auth_state_changed()`:

```cpp
void WindowFrame::on_auth_state_changed() {
    // Local-only mode: auth state never routes the shell. The constructor
    // already showed the dashboard.
    if (local_mode::enabled())
        return;

    auto& auth = auth::AuthManager::instance();
```

Replace the two paid-plan checks:

```cpp
        if (auth.session().has_paid_plan()) {
            // Defensive: at this point the PIN gate above must have either
```
becomes
```cpp
        if (has_full_access()) {
            // Defensive: at this point the PIN gate above must have either
```

and in `on_terminal_unlocked()`:

```cpp
    if (auth.session().has_paid_plan()) {
        set_shell_visible(true);
        stack_->setCurrentIndex(1);
        // Restore chat bubble based on setting
```
becomes
```cpp
    if (has_full_access()) {
        set_shell_visible(true);
        stack_->setCurrentIndex(1);
        // Restore chat bubble based on setting
```

- [ ] **Step 4: Constructor goes straight to the dashboard**

In `fincept-qt/src/app/WindowFrame.cpp` add the include next to the other `core/` includes:

```cpp
#include "core/config/LocalMode.h"
```

The block that starts `auto& auth_mgr = auth::AuthManager::instance();` (≈ line 918) has this shape — the ~80-line "deferred dashboard navigate / materialise restored panels" logic lives *inside* the first branch, and the `else` (≈ line 1024) just calls `on_auth_state_changed()` to show login:

```cpp
    auto& auth_mgr = auth::AuthManager::instance();
    if (auth_mgr.is_authenticated() || auth_mgr.is_loading()) {
        if (auth_mgr.is_authenticated() && auth::PinManager::instance().has_pin() && !pin_gate_cleared_) {
            ... PIN unlock ...
        } else if (auth_mgr.is_authenticated() && auth::PinManager::instance().has_pin()) {
            ... additional window, already unlocked ...
        } else if (auth_mgr.is_authenticated()) {
            on_auth_state_changed();
        } else {
            // Still loading — show app stack temporarily (loading state)
            set_shell_visible(true);
            stack_->setCurrentIndex(1);
        }
        // Recovery / "Continue from last session" path: ...
        if (!dock_restored && adopted_uuid.is_null()) { ... navigate("dashboard") ... }
        else if (!dock_restored) { ... }
        else { ... materialise restored panels ... }
    } else {
        on_auth_state_changed();
    }
```

Make exactly two edits so the navigate/materialise logic is reused untouched:

(a) Widen the outer condition:

```cpp
    if (local_mode::enabled() || auth_mgr.is_authenticated() || auth_mgr.is_loading()) {
```

(b) Add a new **first** inner branch, before the existing `if (auth_mgr.is_authenticated() && auth::PinManager::instance().has_pin() && !pin_gate_cleared_)`:

```cpp
        if (local_mode::enabled()) {
            // Local-only: no login, no PIN, no pricing gate. Show the shell and
            // restore the last workspace exactly as the paid-plan path in
            // on_auth_state_changed() does; the navigate/materialise logic
            // below then behaves as for a restored paid session.
            LOG_INFO("WindowFrame", "Local-only mode — skipping auth stack, showing shell");
            set_shell_visible(true);
            stack_->setCurrentIndex(1);
            layout::WorkspaceShell::load_last_or_default();
        } else if (auth_mgr.is_authenticated() && auth::PinManager::instance().has_pin() && !pin_gate_cleared_) {
```

Nothing else in the block changes. `core/layout/WorkspaceShell.h` is already included by `WindowFrame.cpp` (line 17).

- [ ] **Step 5: Build and smoke-test**

```bash
cmake --build --preset linux-release
QT_QPA_PLATFORM=offscreen ./build/linux-release/FinceptTerminal --smoke-test 2>&1 | tail -n 5
```

Expected: build succeeds; last lines include `[Smoke] exit 0`. Also confirm the log contains `Local-only mode — auth disabled` and `Local-only mode — skipping auth stack`:

```bash
QT_QPA_PLATFORM=offscreen ./build/linux-release/FinceptTerminal --smoke-test 2>&1 | grep -c "Local-only mode"
```

Expected: `2` or more.

Regression check for the non-local path (same binary):

```bash
FINCEPT_LOCAL_ONLY=0 QT_QPA_PLATFORM=offscreen ./build/linux-release/FinceptTerminal --smoke-test 2>&1 | tail -n 1
```

Expected: `[Smoke] exit 0` (upstream behaviour intact).

- [ ] **Step 6: Commit**

```bash
git add fincept-qt/src/auth/AuthManager.cpp fincept-qt/src/app/WindowFrame.h \
        fincept-qt/src/app/WindowFrame.cpp fincept-qt/src/app/WindowFrame_Auth.cpp
git commit -m "feat(local-mode): boot straight to dashboard, never load a Fincept session"
```

---

### Task 3: Silence the cloud — sync, QuantLib, updates, promo

**Files:**
- Modify: `fincept-qt/src/app/main.cpp` (`init_cloud_sync` lambda ≈ line 538)
- Modify: `fincept-qt/src/services/quantlib/QuantLibClient.cpp` (`call()` ≈ line 135)
- Modify: `fincept-qt/src/services/updater/UpdateService.cpp` (`check_for_updates` ≈ line 149)
- Modify: `fincept-qt/src/ui/widgets/EnterprisePromo.cpp` (`UpgradeDialog::show_now` ≈ line 241, `maybe_show_at_startup` ≈ line 251)

**Interfaces:**
- Consumes: `fincept::local_mode::enabled()`.
- Produces: nothing new; existing signatures unchanged.

- [ ] **Step 1: Skip cloud-sync registration**

In `fincept-qt/src/app/main.cpp` add, alphabetically among the `core/config/` includes:

```cpp
#include "core/config/LocalMode.h"
```

Change the lambda header:

```cpp
    auto init_cloud_sync = []() {
        // Local-only mode: no Fincept account, so there is nothing to sync
        // with. Skip adapter registration entirely so the engine never
        // initialises or touches CloudClient.
        if (fincept::local_mode::enabled()) {
            LOG_INFO("App", "Local-only mode — Fincept Cloud sync disabled");
            return;
        }
        // Drains the durable outbox (push) + pulls cloud→local. NOT a DataHub
```

- [ ] **Step 2: QuantLib fails fast**

In `fincept-qt/src/services/quantlib/QuantLibClient.cpp` add after `#include "core/config/AppConfig.h"`:

```cpp
#include "core/config/LocalMode.h"
```

and add `#include <QMetaObject>` to the Qt includes. Change the start of `call()`:

```cpp
void QuantLibClient::call(const QString& endpoint, const QJsonObject& body, QuantLibCallback callback) {
    if (fincept::local_mode::enabled()) {
        // api.fincept.in/quantlib is a paid Fincept service. Until the local
        // QuantLib backend lands (spec: local-quant) every call fails fast
        // with a readable message instead of an HTTP 401 after a round-trip.
        mcp::ToolResult r;
        r.success = false;
        r.error = QStringLiteral("QuantLib cloud API is disabled in local-only mode.");
        LOG_DEBUG(kQuantLibClientTag, "Local-only mode — refusing call to " + endpoint);
        QMetaObject::invokeMethod(
            this, [callback = std::move(callback), r]() { callback(r); }, Qt::QueuedConnection);
        return;
    }
    // Cache GET endpoints (static reference data) and query-param endpoints
```

`mcp::ToolResult` (`mcp/McpTypes.h` line 42) has members `bool success`, `QJsonValue data`, `QString message`, `QString error` — the two used above exist.

- [ ] **Step 3: No update checks**

In `fincept-qt/src/services/updater/UpdateService.cpp` add after `#include "core/logging/Logger.h"`:

```cpp
#include "core/config/LocalMode.h"
```

Change the start of `check_for_updates`:

```cpp
void UpdateService::check_for_updates(bool silent) {
    if (fincept::local_mode::enabled()) {
        // This fork has no release feed; the upstream check would either 404
        // or offer upstream binaries that undo local-only mode.
        LOG_DEBUG("UpdateService", "Local-only mode — update check skipped");
        return;
    }
    if (in_progress_) {
```

- [ ] **Step 4: No Enterprise promo**

In `fincept-qt/src/ui/widgets/EnterprisePromo.cpp` add after `#include "core/config/AppConfig.h"`:

```cpp
#include "core/config/LocalMode.h"
```

Change both entry points:

```cpp
void UpgradeDialog::show_now(QWidget* parent) {
    if (fincept::local_mode::enabled())
        return;
    if (ent_headless_platform())
        return;
```

```cpp
void UpgradeDialog::maybe_show_at_startup(QWidget* parent) {
    if (fincept::local_mode::enabled())
        return;
    if (!startup_prompt_enabled())
        return;
```

- [ ] **Step 5: Build and smoke-test**

```bash
cmake --build --preset linux-release
QT_QPA_PLATFORM=offscreen ./build/linux-release/FinceptTerminal --smoke-test 2>&1 | grep -E "Fincept Cloud sync disabled|\[Smoke\] exit"
```

Expected:

```
... Local-only mode — Fincept Cloud sync disabled
[Smoke] exit 0
```

- [ ] **Step 6: Commit**

```bash
git add fincept-qt/src/app/main.cpp fincept-qt/src/services/quantlib/QuantLibClient.cpp \
        fincept-qt/src/services/updater/UpdateService.cpp fincept-qt/src/ui/widgets/EnterprisePromo.cpp
git commit -m "feat(local-mode): disable cloud sync, QuantLib cloud, update check and Enterprise promo"
```

---

### Task 4: Ollama is the default provider; `fincept` withdrawn from the catalog

**Files:**
- Modify: `fincept-qt/src/services/llm/ProviderCatalog.h`
- Modify: `fincept-qt/src/services/llm/ProviderCatalog.cpp` (`known_providers` ≈ line 12, `display_name` ≈ line 41, add two functions)
- Modify: `fincept-qt/src/services/llm/LlmService.cpp` (`ensure_config` fallback ≈ line 106)
- Create: `fincept-qt/tests/tst_provider_catalog.cpp`
- Modify: `fincept-qt/tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `fincept::local_mode::enabled()`.
- Produces: `static QString ProviderCatalog::default_provider()`, `static QString ProviderCatalog::default_model(const QString& provider)`. Task 5 uses both.

- [ ] **Step 1: Write the failing test**

Create `fincept-qt/tests/tst_provider_catalog.cpp`:

```cpp
// Unit tests for src/services/llm/ProviderCatalog.{h,cpp} — the parts this fork
// changed for local-only mode. The suite forces FINCEPT_LOCAL_ONLY=1 in the
// environment BEFORE the first local_mode::enabled() call (initTestCase), so
// every slot below observes local mode regardless of how the test was built.

#include "core/config/LocalMode.h"
#include "services/llm/ProviderCatalog.h"

#include <QTest>

using fincept::ai_chat::ProviderCatalog;

class TstProviderCatalog : public QObject {
    Q_OBJECT

  private slots:
    void initTestCase();
    void local_mode_is_active_for_this_suite();
    void known_providers_excludes_fincept_in_local_mode();
    void known_providers_still_lists_ollama_and_groq();
    void default_provider_is_ollama_in_local_mode();
    void default_model_for_ollama_is_llama31_8b();
    void default_model_for_fincept_is_minimax();
    void default_model_for_other_provider_is_first_fallback();
    void fincept_display_name_has_no_recommended_suffix();
};

void TstProviderCatalog::initTestCase() {
    qputenv("FINCEPT_LOCAL_ONLY", "1");
}

void TstProviderCatalog::local_mode_is_active_for_this_suite() {
    QVERIFY(fincept::local_mode::enabled());
}

void TstProviderCatalog::known_providers_excludes_fincept_in_local_mode() {
    QVERIFY(!ProviderCatalog::known_providers().contains(QStringLiteral("fincept")));
}

void TstProviderCatalog::known_providers_still_lists_ollama_and_groq() {
    const auto& p = ProviderCatalog::known_providers();
    QVERIFY(p.contains(QStringLiteral("ollama")));
    QVERIFY(p.contains(QStringLiteral("groq")));
    QCOMPARE(p.size(), 13); // 14 upstream providers minus fincept
}

void TstProviderCatalog::default_provider_is_ollama_in_local_mode() {
    QCOMPARE(ProviderCatalog::default_provider(), QStringLiteral("ollama"));
}

void TstProviderCatalog::default_model_for_ollama_is_llama31_8b() {
    QCOMPARE(ProviderCatalog::default_model(QStringLiteral("ollama")), QStringLiteral("llama3.1:8b"));
}

void TstProviderCatalog::default_model_for_fincept_is_minimax() {
    QCOMPARE(ProviderCatalog::default_model(QStringLiteral("fincept")), QStringLiteral("MiniMax-M2.7"));
}

void TstProviderCatalog::default_model_for_other_provider_is_first_fallback() {
    QCOMPARE(ProviderCatalog::default_model(QStringLiteral("groq")),
             ProviderCatalog::fallback_models(QStringLiteral("groq")).value(0));
    QCOMPARE(ProviderCatalog::default_model(QStringLiteral("groq")), QStringLiteral("llama-3.3-70b-versatile"));
}

void TstProviderCatalog::fincept_display_name_has_no_recommended_suffix() {
    QCOMPARE(ProviderCatalog::display_name(QStringLiteral("fincept")), QStringLiteral("Fincept LLM"));
}

QTEST_GUILESS_MAIN(TstProviderCatalog)
#include "tst_provider_catalog.moc"
```

Append to `fincept-qt/tests/CMakeLists.txt` after the `tst_local_mode` block:

```cmake
# src/services/llm/ProviderCatalog.{h,cpp} — Qt Core + LocalMode only. Two TUs.
fincept_add_test(tst_provider_catalog
    tst_provider_catalog.cpp
    "${PROJECT_SOURCE_DIR}/src/services/llm/ProviderCatalog.cpp"
    "${PROJECT_SOURCE_DIR}/src/core/config/LocalMode.cpp")
```

Update the status line:

```cmake
message(STATUS "Fincept tests: tst_broker_modify_fields, tst_result, tst_order_validator, tst_local_mode, tst_provider_catalog")
```

- [ ] **Step 2: Run the test to verify it fails**

```bash
cmake --preset linux-release -DFINCEPT_BUILD_TESTS=ON
cmake --build --preset linux-release --target tst_provider_catalog
```

Expected: build FAILS with `'default_provider' is not a member of 'fincept::ai_chat::ProviderCatalog'`.

- [ ] **Step 3: Extend the header**

In `fincept-qt/src/services/llm/ProviderCatalog.h`, replace the `known_providers` declaration and add the two new functions:

```cpp
class ProviderCatalog {
  public:
    /// Every selectable provider id. In local-only mode the Fincept-hosted
    /// provider ("fincept") is omitted — it needs a paid api.fincept.in key.
    static const QStringList& known_providers();
    /// Provider used when nothing is configured: "ollama" in local-only mode,
    /// "fincept" otherwise (upstream behaviour).
    static QString default_provider();
    /// Starting model for `provider`: "llama3.1:8b" for ollama, "MiniMax-M2.7"
    /// for fincept, otherwise the first fallback_models() entry (may be empty).
    static QString default_model(const QString& provider);
```

(Keep every other declaration as is.)

- [ ] **Step 4: Implement**

In `fincept-qt/src/services/llm/ProviderCatalog.cpp` add after `#include "services/llm/ProviderCatalog.h"`:

```cpp
#include "core/config/LocalMode.h"
```

Replace `known_providers()`:

```cpp
const QStringList& ProviderCatalog::known_providers() {
    static const QStringList kAll = {"openai",     "anthropic", "gemini",       "groq",    "deepseek",
                                     "openrouter", "minimax",   "kimi",         "ollama",  "xai",
                                     "fincept",    "astraflow", "astraflow_cn", "aihubmix"};
    static const QStringList kLocal = [] {
        QStringList out = kAll;
        out.removeAll(QStringLiteral("fincept"));
        return out;
    }();
    return local_mode::enabled() ? kLocal : kAll;
}

QString ProviderCatalog::default_provider() {
    return local_mode::enabled() ? QStringLiteral("ollama") : QStringLiteral("fincept");
}

QString ProviderCatalog::default_model(const QString& provider) {
    const QString p = provider.toLower();
    if (p == "ollama")
        return QStringLiteral("llama3.1:8b");
    if (p == "fincept")
        return QStringLiteral("MiniMax-M2.7");
    return fallback_models(p).value(0);
}
```

In `display_name()`, change the entry:

```cpp
        {"fincept", "Fincept LLM (recommended)"},
```
to
```cpp
        {"fincept", "Fincept LLM"},
```

- [ ] **Step 5: LlmService falls back to the catalog default**

In `fincept-qt/src/services/llm/LlmService.cpp`, `ensure_config()`, replace:

```cpp
    // Nothing configured — default to Fincept with the session key.
    if (provider_.isEmpty()) {
        provider_ = "fincept";
        model_ = "MiniMax-M2.7";
        base_url_ = {};
        LOG_INFO(kLlmSvcTag, "No LLM provider configured — using Fincept default");
    }
```
with
```cpp
    // Nothing configured — use the catalog default (ollama in local-only mode,
    // fincept upstream). Fincept resolves its key below via AuthManager.
    if (provider_.isEmpty()) {
        provider_ = ProviderCatalog::default_provider();
        model_ = ProviderCatalog::default_model(provider_);
        base_url_ = ProviderCatalog::default_base_url(provider_);
        LOG_INFO(kLlmSvcTag, QString("No LLM provider configured — using %1 default").arg(provider_));
    }
```

`ProviderCatalog.h` is already included in `LlmService.cpp`.

- [ ] **Step 6: Run the tests and build the app**

```bash
cmake --build --preset linux-release
ctest --test-dir build/linux-release --output-on-failure -R "tst_local_mode|tst_provider_catalog"
```

Expected: `100% tests passed, 0 tests failed out of 2`; app builds.

- [ ] **Step 7: Commit**

```bash
git add fincept-qt/src/services/llm/ProviderCatalog.h fincept-qt/src/services/llm/ProviderCatalog.cpp \
        fincept-qt/src/services/llm/LlmService.cpp fincept-qt/tests/tst_provider_catalog.cpp fincept-qt/tests/CMakeLists.txt
git commit -m "feat(local-mode): Ollama default provider, hide fincept from catalog"
```

---

### Task 5: Fix the persisted LLM rows — `LocalModeBootstrap`

**Files:**
- Create: `fincept-qt/src/core/config/LocalModeBootstrap.h`
- Create: `fincept-qt/src/core/config/LocalModeBootstrap.cpp`
- Modify: `fincept-qt/CMakeLists.txt` (source list, after `src/core/config/LocalMode.cpp`)
- Modify: `fincept-qt/src/app/main.cpp` (DB-open `else` branch, after `AccountManager::instance().reload_from_db();` ≈ line 875)

**Interfaces:**
- Consumes: `local_mode::enabled()`; `ProviderCatalog::default_provider()`, `default_model()`, `default_base_url()` (Task 4); `LlmConfigRepository::list_providers()`, `delete_provider(const QString&)`, `save_provider(const LlmConfig&)`; struct `LlmConfig{provider, api_key, base_url, model, is_active, tools_enabled}` from `storage/repositories/LlmConfigRepository.h`.
- Produces: `QString fincept::local_mode::bootstrap_llm_defaults()` — returns the provider left active (empty string when the repository is unreadable).

- [ ] **Step 1: Header**

Create `fincept-qt/src/core/config/LocalModeBootstrap.h`:

```cpp
#pragma once
// LocalModeBootstrap — one-shot fix-ups that need the database, run once per
// launch after migrations and before the first window. Kept separate from
// LocalMode.{h,cpp} so that unit stays a Qt-Core-only leaf.

#include <QString>

namespace fincept::local_mode {

/// Local-only mode only (returns the current active provider unchanged when
/// the mode is off). Idempotent:
///   1. deletes the `fincept` row that migration v002 seeds in llm_configs;
///   2. if no remaining row is active, saves the catalog default provider
///      (ollama, http://localhost:11434, llama3.1:8b) as active.
/// Never touches an active non-Fincept provider the user chose.
/// Returns the provider id left active, or an empty string if the repository
/// could not be read.
QString bootstrap_llm_defaults();

} // namespace fincept::local_mode
```

- [ ] **Step 2: Implementation**

Create `fincept-qt/src/core/config/LocalModeBootstrap.cpp`:

```cpp
#include "core/config/LocalModeBootstrap.h"

#include "core/config/LocalMode.h"
#include "core/logging/Logger.h"
#include "services/llm/ProviderCatalog.h"
#include "storage/repositories/LlmConfigRepository.h"

namespace fincept::local_mode {

namespace {
constexpr const char* kTag = "LocalMode";
}

QString bootstrap_llm_defaults() {
    auto& repo = LlmConfigRepository::instance();

    auto providers = repo.list_providers();
    if (providers.is_err()) {
        LOG_WARN(kTag, "bootstrap_llm_defaults: cannot list providers — " + QString::fromStdString(providers.error()));
        return {};
    }

    QString active;
    for (const auto& c : providers.value())
        if (c.is_active)
            active = c.provider.toLower();

    if (!enabled())
        return active;

    bool had_fincept = false;
    for (const auto& c : providers.value())
        if (c.provider.compare(QLatin1String("fincept"), Qt::CaseInsensitive) == 0)
            had_fincept = true;

    if (had_fincept) {
        auto r = repo.delete_provider(QStringLiteral("fincept"));
        if (r.is_err())
            LOG_WARN(kTag, "bootstrap_llm_defaults: could not delete fincept row — " + QString::fromStdString(r.error()));
        else
            LOG_INFO(kTag, "Removed Fincept LLM provider row (local-only mode)");
        if (active == QLatin1String("fincept"))
            active.clear();
    }

    if (active.isEmpty()) {
        LlmConfig c;
        c.provider = ai_chat::ProviderCatalog::default_provider();
        c.model = ai_chat::ProviderCatalog::default_model(c.provider);
        c.base_url = ai_chat::ProviderCatalog::default_base_url(c.provider);
        c.api_key.clear();
        c.is_active = true;
        c.tools_enabled = true;
        auto r = repo.save_provider(c);
        if (r.is_err()) {
            LOG_WARN(kTag, "bootstrap_llm_defaults: could not seed default provider — " + QString::fromStdString(r.error()));
            return {};
        }
        active = c.provider;
        LOG_INFO(kTag, QString("Seeded default LLM provider %1 (%2 @ %3)").arg(c.provider, c.model, c.base_url));
    }

    return active;
}

} // namespace fincept::local_mode
```

Check `core/result/Result.h`: `Result<T>::error()` returns `std::string` (see `tests/tst_result.cpp`), hence the `QString::fromStdString` conversions above.

- [ ] **Step 3: Register the source and call it from `main.cpp`**

In `fincept-qt/CMakeLists.txt`, directly after `src/core/config/LocalMode.cpp` add:

```cmake
    src/core/config/LocalModeBootstrap.cpp
```

In `fincept-qt/src/app/main.cpp` add the include next to `core/config/LocalMode.h`:

```cpp
#include "core/config/LocalModeBootstrap.h"
```

In the DB-open `else` branch, directly after

```cpp
        fincept::trading::AccountManager::instance().reload_from_db();
```

add

```cpp
        // Local-only mode: drop the Fincept LLM row seeded by migration v002
        // and make sure a free provider (Ollama) is active. Must run after
        // migrations (Database::open) and before any WindowFrame constructs
        // LlmService consumers. No-op in upstream mode.
        {
            const QString active = fincept::local_mode::bootstrap_llm_defaults();
            LOG_INFO("App", QString("Active LLM provider after bootstrap: %1").arg(active.isEmpty() ? "<none>" : active));
        }
```

- [ ] **Step 4: Build, smoke-test, inspect the DB**

```bash
cmake --build --preset linux-release
QT_QPA_PLATFORM=offscreen ./build/linux-release/FinceptTerminal --smoke-test 2>&1 | grep -E "Removed Fincept LLM|Seeded default LLM|Active LLM provider|\[Smoke\] exit"
```

Expected on a fresh profile:

```
... Removed Fincept LLM provider row (local-only mode)
... Seeded default LLM provider ollama (llama3.1:8b @ http://localhost:11434)
... Active LLM provider after bootstrap: ollama
[Smoke] exit 0
```

Run it a second time: expected only `Active LLM provider after bootstrap: ollama` and `[Smoke] exit 0` (idempotent — no "Removed"/"Seeded" lines).

Verify the row directly. On Linux `AppPaths::root()` is `~/.local/share/com.fincept.terminal` and the default profile's DB is `profiles/default/data/fincept.db` under it:

```bash
sqlite3 ~/.local/share/com.fincept.terminal/profiles/default/data/fincept.db \
  "SELECT provider, model, base_url, is_active FROM llm_configs;"
```

Expected: one line `ollama|llama3.1:8b|http://localhost:11434|1` and no `fincept` line. (Install `sqlite3` with `sudo apt install sqlite3` if missing.)

- [ ] **Step 5: Commit**

```bash
git add fincept-qt/src/core/config/LocalModeBootstrap.h fincept-qt/src/core/config/LocalModeBootstrap.cpp \
        fincept-qt/CMakeLists.txt fincept-qt/src/app/main.cpp
git commit -m "feat(local-mode): bootstrap LLM rows — drop fincept, seed ollama as active"
```

---

### Task 6: Remove the upsell chrome

**Files:**
- Modify: `fincept-qt/src/ui/navigation/ToolBar.cpp` (constructor ≈ lines 109–147, `apply_responsive_layout` ≈ line 277, `refresh_user_display` ≈ line 311, `build_help_menu` ≈ line 525)
- Modify: `fincept-qt/src/screens/settings/SettingsScreen.cpp` (`make_btn(QStringLiteral("Cloud Sync"), 15, ...)` ≈ line 158)

**Interfaces:**
- Consumes: `local_mode::enabled()`.
- Produces: nothing new.

- [ ] **Step 1: ToolBar — hide widgets**

In `fincept-qt/src/ui/navigation/ToolBar.cpp` add after `#include "auth/AuthManager.h"`:

```cpp
#include "core/config/LocalMode.h"
```

In the constructor, directly after `hl->addWidget(logout_btn_);` and before `retranslateUi();` add:

```cpp
    if (local_mode::enabled()) {
        // No Fincept account: credits, plan, Enterprise CTA and LOGOUT have
        // nothing to act on. Hidden rather than skipped so separators_ indices
        // used by apply_responsive_layout() stay valid.
        credits_label_->hide();
        plan_btn_->hide();
        upgrade_btn_->hide();
        logout_btn_->hide();
        // separators_ order in this constructor: 0 before FINCEPT, 1 after
        // LIVE, 2 after pushpins, 3 after user, 4 after credits, 5 after
        // UPGRADE, 6 after CHAT. Keep 3 so the row reads "LOCAL | CHAT".
        if (separators_.size() >= 7) {
            separators_[4]->hide();
            separators_[5]->hide();
            separators_[6]->hide();
        }
    }
```

- [ ] **Step 2: ToolBar — keep them hidden on resize**

In `apply_responsive_layout(int w)` change:

```cpp
    if (credits_label_)
        credits_label_->setVisible(show_credits);
```
to
```cpp
    if (credits_label_ && !local_mode::enabled())
        credits_label_->setVisible(show_credits);
```

and

```cpp
    if (separators_.size() >= 7) {
        separators_[4]->setVisible(show_credits);
        separators_[5]->setVisible(show_chat);
    }
```
to
```cpp
    if (separators_.size() >= 7 && !local_mode::enabled()) {
        separators_[4]->setVisible(show_credits);
        separators_[5]->setVisible(show_chat);
    }
```

(Both separators are permanently hidden in local mode by Step 1, so the width-driven toggle must not touch them.)

- [ ] **Step 3: ToolBar — LOCAL label**

In `refresh_user_display()` change the unauthenticated branch:

```cpp
    if (!s.authenticated) {
        user_label_->setText(local_mode::enabled() ? tr("LOCAL") : QStringLiteral("---"));
        credits_label_->setText("---");
        plan_btn_->setText("---");
        return;
    }
```

- [ ] **Step 4: ToolBar — trim the Help menu**

Replace the tail of `build_help_menu()`:

```cpp
    m->addAction(tr("Trademarks"), this, [this]() { emit navigate_to("trademarks"); });
    if (!local_mode::enabled()) {
        // Both actions need a Fincept session or release feed; in local-only
        // mode they would be silent no-ops, which reads as broken.
        m->addSeparator();
        m->addAction(tr("Check for Updates"), this, [this]() { emit action_triggered("check_updates"); });
        m->addSeparator();
        m->addAction(tr("Logout"), this, [this]() { emit action_triggered("logout"); });
    }
    return m;
}
```

- [ ] **Step 5: SettingsScreen — no Cloud Sync entry**

In `fincept-qt/src/screens/settings/SettingsScreen.cpp` add after `#include "core/events/EventBus.h"`:

```cpp
#include "core/config/LocalMode.h"
```

Change:

```cpp
    make_btn(QStringLiteral("Cloud Sync"), 15, QStringLiteral("backup sync account devices domains credits"));
```
to
```cpp
    // Section 15 (CloudSyncSection) stays in the stack so indices are stable;
    // only its sidebar entry is dropped in local-only mode.
    if (!local_mode::enabled())
        make_btn(QStringLiteral("Cloud Sync"), 15, QStringLiteral("backup sync account devices domains credits"));
```

No further handling is needed: `apply_nav_filter()` (≈ line 191), `retranslateUi()` (≈ line 210) and `refresh_theme()` (≈ line 274) all iterate `nav_buttons_` with range-for and null-check `nb.btn`; nothing indexes it by position.

- [ ] **Step 6: Build, smoke-test, visual check**

```bash
cmake --build --preset linux-release
QT_QPA_PLATFORM=offscreen ./build/linux-release/FinceptTerminal --smoke-test 2>&1 | tail -n 1
```

Expected: `[Smoke] exit 0`.

Visual check (needs a display):

```bash
./build/linux-release/FinceptTerminal
```

Expected: dashboard appears immediately; toolbar right side shows `LOCAL` then the CHAT button and no `--- | --- | ▴ UPGRADE | LOGOUT`; Help menu ends at "Trademarks"; Settings sidebar has no "Cloud Sync".

- [ ] **Step 7: Commit**

```bash
git add fincept-qt/src/ui/navigation/ToolBar.cpp fincept-qt/src/screens/settings/SettingsScreen.cpp
git commit -m "feat(local-mode): hide credits/plan/upgrade/logout chrome and Cloud Sync settings"
```

---

### Task 7: Documentation

**Files:**
- Create: `docs/LOCAL_ONLY_MODE.md`
- Modify: `README.md` (insert one paragraph after the first heading)

- [ ] **Step 1: Write `docs/LOCAL_ONLY_MODE.md`**

```markdown
# Local-Only Mode

This fork of Fincept Terminal builds in **local-only mode** by default. The
terminal starts on the dashboard, never contacts `api.fincept.in`, uses a
free LLM, and shows none of the Enterprise / pricing UI.

## What changes

| Area | Upstream | Local-only |
|---|---|---|
| Startup | Login → PIN → pricing gate → dashboard | Dashboard |
| Fincept account, credits, plan | Required for the dashboard | Not used; toolbar shows `LOCAL` |
| Default LLM | Fincept LLM (paid credits) | Ollama at `http://localhost:11434`, model `llama3.1:8b` |
| Fincept Cloud Sync | Settings → Cloud Sync | Hidden, never initialised |
| QuantLib cloud API (`/quantlib/*`) | Paid | Disabled — calls return "QuantLib cloud API is disabled in local-only mode." (a local backend is planned) |
| Update check | Upstream release feed | Off |
| Enterprise promo / UPGRADE button | Shown | Hidden |
| PIN lock | Available | Not available (it is tied to a Fincept session) |

Everything else — Yahoo Finance, FRED, NSE/BSE data, brokers, watchlists,
portfolio, notes, agents, every other LLM provider — is unchanged.

## Turning it off

Build-time (restores upstream behaviour):

    cmake --preset linux-release -DFINCEPT_LOCAL_ONLY=OFF

Run-time (same binary, one launch):

    FINCEPT_LOCAL_ONLY=0 ./FinceptTerminal

`FINCEPT_LOCAL_ONLY=1` forces it on for a binary built with the option off.

## Setting up a free LLM

### Ollama (local, no key, uses your GPU)

    curl -fsSL https://ollama.com/install.sh | sh
    ollama pull llama3.1:8b
    systemctl status ollama      # should be active (running)

Open Settings → LLM Config. Ollama is already the active provider; the model
list is fetched live from `http://localhost:11434/api/tags`, so any model you
`ollama pull` appears there. On a 12 GB GPU `llama3.1:8b`, `qwen2.5:14b` and
`mistral-nemo:12b` all fit.

### Groq (cloud, free tier, fast)

1. Create a key at https://console.groq.com/keys
2. Settings → LLM Config → provider **Groq** → paste the key → pick
   `llama-3.3-70b-versatile` → Set Active.

Any other provider in the list (OpenAI, Anthropic, Gemini, OpenRouter, …)
works the same way with its own key.

## Troubleshooting

- **Chat says connection refused** — Ollama is not running: `systemctl start ollama`.
- **Model list is empty** — nothing pulled yet: `ollama pull llama3.1:8b`.
- **A valuation tab says "QuantLib cloud API is disabled"** — expected until
  the local QuantLib backend lands; the rest of the screen still works.
- **You want the upstream login flow back** — see "Turning it off".
```

- [ ] **Step 2: README paragraph**

`README.md` currently opens with two upstream advert blocks (`> [!IMPORTANT] … Enterprise …` and `> [!TIP] … Quantcept …`). Insert the following as the **very first lines of the file**, followed by one blank line, leaving everything else untouched:

```markdown
> **This fork builds in local-only mode.** No Fincept login, no `api.fincept.in`
> traffic, Ollama as the default LLM, no Enterprise/pricing UI. See
> [docs/LOCAL_ONLY_MODE.md](docs/LOCAL_ONLY_MODE.md) for what changes and how to
> turn it off. Everything below this note is the upstream README.
```

- [ ] **Step 3: Verify the docs are tracked**

```bash
git check-ignore -v docs/LOCAL_ONLY_MODE.md README.md; echo "exit=$?"
```

Expected: `docs/LOCAL_ONLY_MODE.md` is matched only by a `!` (negation) rule — the `.gitignore` already contains `!docs/LOCAL_ONLY_MODE.md` — and `README.md` by `!README.md`.

- [ ] **Step 4: Commit and push**

```bash
git add docs/LOCAL_ONLY_MODE.md README.md
git commit -m "docs: describe local-only mode and free LLM setup"
git push -u origin cursor/local-only-mode-spec-5a93
```

---

## Final verification (after Task 7)

Run the full checklist once from `/workspace/fincept-qt`:

```bash
cmake --preset linux-release -DFINCEPT_BUILD_TESTS=ON
cmake --build --preset linux-release
ctest --test-dir build/linux-release --output-on-failure
QT_QPA_PLATFORM=offscreen ./build/linux-release/FinceptTerminal --smoke-test 2>&1 | tail -n 1
FINCEPT_LOCAL_ONLY=0 QT_QPA_PLATFORM=offscreen ./build/linux-release/FinceptTerminal --smoke-test 2>&1 | tail -n 1
```

Expected: all 5 test suites pass; both smoke runs print `[Smoke] exit 0`.

Network check on the user's desktop (GUI running, idle 2 minutes):

```bash
dig +short api.fincept.in
ss -tnp | grep FinceptTerminal
```

Expected: no connection to any IP returned by `dig`.

## Spec coverage

| Spec section | Task |
|---|---|
| 5.1 switch + CMake | 1 |
| 5.2 boot path, `has_full_access`, `on_auth_state_changed` | 2 |
| 5.3 auth never starts | 2 |
| 5.4 cloud sync / QuantLib / update / promo | 3 |
| 5.5 ProviderCatalog, LlmService fallback | 4 |
| 5.5 LocalModeBootstrap + main.cpp call | 5 |
| 5.6 ToolBar, Help menu, Settings | 6 |
| 5.7 docs | 7 |
| 8 tests (`tst_local_mode`, `tst_provider_catalog`, smoke both modes) | 1, 4, final |

# Local-Only Mode — Design

**Status:** approved for planning
**Series:** 1 of 5 (local-only-mode → india-defaults-and-nav → local-quant → india-portfolio → india-macro-alerts)
**Fork:** `Soldier0x0/FinceptTerminal` (this repository), Ubuntu desktop, single user

## 1. Problem

The open-source build of Fincept Terminal is not usable without Fincept's cloud:

- **Login wall.** `WindowFrame` starts on the auth stack. `AuthManager::initialize()` validates a saved API key against `api.fincept.in`; without one the user sees the login screen. There is no guest path in `LoginScreen`.
- **Pricing gate.** Even after login, the dashboard is only shown when `SessionData::has_paid_plan()` is true (`basic|standard|pro|enterprise`). Free accounts land on `PricingScreen` and must click "Continue with Free Plan" every time. The check is duplicated in `WindowFrame_Auth.cpp` (`on_auth_state_changed`, `on_terminal_unlocked`) and `WindowFrame.cpp` (constructor).
- **Fincept LLM is the default.** Migration `v002` seeds an active `fincept` provider row; `LlmService::ensure_config()` falls back to `provider_ = "fincept"` when nothing is configured; `AuthManager::complete_auth_flow()` re-creates the row on every login.
- **Upsell surfaces.** `UpgradeDialog::maybe_show_at_startup()` fires 1.2 s after every launch; the toolbar shows UPGRADE, plan, credits and LOGOUT; Settings exposes Cloud Sync.
- **Cloud services assume a session.** `SessionGuard` pulses `/session`, `WindowFrame` refreshes user data every 3 minutes and on focus, `CloudSyncEngine` registers eleven adapters, `QuantLibClient` posts to `api.fincept.in/quantlib/*` with the session key, `UpdateService` checks for upstream releases.

The user owns this fork and wants a build that starts on the dashboard, never contacts `api.fincept.in`, defaults to a free LLM (Ollama locally, Groq as free cloud), and shows no upsell — without deleting upstream code, so later `git merge upstream/main` stays feasible.

## 2. Goals

1. `FinceptTerminal` starts directly on the dashboard. No login, no PIN, no pricing screen.
2. Zero network requests to `api.fincept.in` in normal use. Fincept-only features fail fast with a clear, local error instead of an HTTP 401.
3. Default LLM provider is **Ollama** (`http://localhost:11434`, model `llama3.1:8b`). Groq and every other provider remain selectable in Settings → LLM Config. The `fincept` provider is not offered.
4. No Enterprise / pricing / credits / logout UI.
5. Everything above is behind one switch so upstream merges stay mechanical.
6. A Qt Test suite covers the switch's decision logic; the existing `--smoke-test` still passes with the switch on.

## 3. Non-goals (later specs)

- Hiding non-Indian screens or reworking navigation (spec 2).
- Replacing `QuantLibClient` cloud calls with local Python QuantLib (spec 3). This spec only makes them fail cleanly.
- Mutual funds, gold, bonds, FD tracking (spec 4). Macro alerts (spec 5).
- Removing Fincept code, screens, or the `fincept` provider from the codebase. **Gate, don't delete.**
- Re-enabling the PIN lock without a Fincept session. `PinManager` and `LockScreen` gate on `is_authenticated()`; decoupling them is a separate change. In local mode the terminal has no lock screen.
- Changing build presets, Qt version (6.8.3), or Python packaging.

## 4. Approaches considered

| | Approach | Trade-off |
|---|---|---|
| A | **Compile-time option `FINCEPT_LOCAL_ONLY` (default ON) exposed through one runtime query `local_mode::enabled()`, with an env-var override for testing.** | One switch; upstream-mergeable; testable; the same binary can still run in cloud mode for comparison via `FINCEPT_LOCAL_ONLY=0`. Requires touching ~12 call sites. **Chosen.** |
| B | Hard-code the changes (delete auth stack, delete `fincept` provider, delete promo). | Fewer lines now; every upstream merge becomes a conflict fest; no way to A/B against upstream behaviour. |
| C | Fake a paid session (seed `SessionData{authenticated=true, account_type="pro"}`). | Smallest diff, but every guarded service (SessionGuard, CloudSync, QuantLib, refresh timers) would *start* and then 401 against `api.fincept.in`. Violates goal 2. |

## 5. Design

### 5.1 The switch — `core/config/LocalMode`

New leaf unit, no Qt dependencies beyond `<QtGlobal>`/`<QByteArray>`, so it is unit-testable and includable from anywhere:

```
fincept-qt/src/core/config/LocalMode.h
fincept-qt/src/core/config/LocalMode.cpp
```

```cpp
namespace fincept::local_mode {

/// Pure decision. `compiled_default` is FINCEPT_LOCAL_ONLY; `env_value` is the raw
/// FINCEPT_LOCAL_ONLY environment variable (nullptr / empty = unset).
/// "0", "false", "off", "no" (case-insensitive) → false; "1", "true", "on", "yes" → true;
/// anything else (including unset) → compiled_default.
bool resolve(bool compiled_default, const char* env_value);

/// Process-wide answer, computed once on first call from resolve(FINCEPT_LOCAL_ONLY, getenv).
bool enabled();

} // namespace fincept::local_mode
```

CMake:

```cmake
option(FINCEPT_LOCAL_ONLY
    "Local-only build: no Fincept login/cloud, Ollama default LLM, no Enterprise promo" ON)
if(FINCEPT_LOCAL_ONLY)
    target_compile_definitions(FinceptTerminal PRIVATE FINCEPT_LOCAL_ONLY=1)
else()
    target_compile_definitions(FinceptTerminal PRIVATE FINCEPT_LOCAL_ONLY=0)
endif()
```

`LocalMode.cpp` reads `FINCEPT_LOCAL_ONLY` via `#ifdef`; when the macro is absent (unit-test target) it defaults to `0` so the test binary's behaviour does not depend on the app option.

### 5.2 Boot path — straight to dashboard

`WindowFrame` gets a single helper used at every plan check:

```cpp
// WindowFrame.h (private)
/// True when the shell may be shown: local mode, or an authenticated paid session.
bool has_full_access() const;
```

```cpp
bool WindowFrame::has_full_access() const {
    return local_mode::enabled() || auth::AuthManager::instance().session().has_paid_plan();
}
```

Replace the two `auth.session().has_paid_plan()` branches in `WindowFrame_Auth.cpp` (`on_auth_state_changed`, `on_terminal_unlocked`) with `has_full_access()`. `PricingScreen` also reads `has_paid_plan()` but is unreachable in local mode (the toolbar plan button is hidden and no auth path routes to it), so it is left untouched.

Constructor routing (`WindowFrame.cpp`, the block beginning "Show the app or auth stack based on authentication state"): add a first branch

```cpp
if (local_mode::enabled()) {
    LOG_INFO("WindowFrame", "Local-only mode — skipping auth stack");
    set_shell_visible(true);
    stack_->setCurrentIndex(1);
    layout::WorkspaceShell::load_last_or_default();
    // fall through to the existing "navigate to dashboard if no layout restored" logic
} else if (auth_mgr.is_authenticated() || auth_mgr.is_loading()) { ... unchanged ... }
```

`on_auth_state_changed()` returns immediately in local mode — auth state never routes the shell:

```cpp
void WindowFrame::on_auth_state_changed() {
    if (local_mode::enabled())
        return;
    ...
```

The lock screen, PIN setup and `on_terminal_unlocked()` are unreachable in local mode because `is_authenticated()` is always false (§5.3), so no further edits there beyond the `has_full_access()` substitution.

### 5.3 Auth — never start a session

`AuthManager::initialize()`:

```cpp
void AuthManager::initialize() {
    if (local_mode::enabled()) {
        LOG_INFO("Auth", "Local-only mode — auth disabled, no session will be loaded");
        session_ = SessionData{};
        set_loading(false);
        emit auth_state_changed();
        return;
    }
    ... unchanged ...
```

Consequences, all automatic because they already gate on `is_authenticated()` / non-empty `api_key`:

- `SessionGuard` never starts (its `auth_state_changed` slot sees `authenticated=false`).
- `WindowFrame::user_refresh_timer_` and the `applicationStateChanged` refresh early-return.
- `CloudSyncEngine::can_enable()` is false; `update_credentials()` clears credentials.
- `QuantLibClient` sends no `X-API-Key`.
- `auto_configure_fincept_llm()` returns at `session_.api_key.isEmpty()`.

`AuthManager::login/signup/logout/...` are left intact; they are simply unreachable because the auth stack is never shown.

### 5.4 Cloud services — skip registration, fail fast

`main.cpp`, the `init_cloud_sync` lambda that registers the eleven `*CloudAdapter`s and calls `CloudSyncEngine::instance().initialize()`: first statement `if (local_mode::enabled()) return;`. It stays in the `post_chain({...})` list so the deferred-init ordering is unchanged. Nothing else references the engine at boot; `CloudSyncSection` (Settings) is hidden (§5.6).

`QuantLibClient::call()` — first statement:

```cpp
if (local_mode::enabled()) {
    mcp::ToolResult r;
    r.success = false;
    r.error = "QuantLib cloud API is disabled in local-only mode (see spec 3: local-quant).";
    QMetaObject::invokeMethod(this, [callback, r]() { callback(r); }, Qt::QueuedConnection);
    return;
}
```

Callers already handle `success == false` by showing `r.error`, so Equity Valuation / QuantLib screens display that string instead of spinning on a 401.

`UpdateService::check_for_updates(bool silent)` — first statement: `if (local_mode::enabled()) return;`. This fork has no release feed; the upstream check would either 404 or offer upstream binaries.

`UpgradeDialog::maybe_show_at_startup()` and `UpgradeDialog::show_now()` — first statement: `if (local_mode::enabled()) return;`. The two `main.cpp` timers and the toolbar handler then become no-ops without further edits.

### 5.5 LLM — Ollama by default, `fincept` withdrawn

`ProviderCatalog`:

- `known_providers()` returns a list **without** `"fincept"` when `local_mode::enabled()`. Implement by building the full static list and filtering once into a second static.
- `display_name("fincept")` → `"Fincept LLM"` (drop "(recommended)") regardless of mode; the "recommended" copy is Fincept's marketing, not a fact about the provider.
- New: `static QString default_provider();` → `"ollama"` in local mode, `"fincept"` otherwise. New: `static QString default_model(const QString& provider);` → `"llama3.1:8b"` for `ollama`, `"MiniMax-M2.7"` for `fincept`, `fallback_models(provider).value(0)` for everything else.

`LlmService::ensure_config()` — the "Nothing configured" fallback becomes:

```cpp
if (provider_.isEmpty()) {
    provider_ = ProviderCatalog::default_provider();
    model_ = ProviderCatalog::default_model(provider_);
    base_url_ = ProviderCatalog::default_base_url(provider_);
    LOG_INFO(kLlmSvcTag, QString("No LLM provider configured — using %1 default").arg(provider_));
}
```

Persisted state. Migration `v002` has already run on the user's existing DB and seeds `fincept` as active; new installs will run it too. Rather than editing a shipped migration, add a one-shot bootstrap that runs after the DB is open and migrations have applied, in local mode only:

```
fincept-qt/src/core/config/LocalModeBootstrap.h
fincept-qt/src/core/config/LocalModeBootstrap.cpp
```

```cpp
namespace fincept::local_mode {
/// Idempotent. Local mode only (no-op otherwise):
///  1. delete the `fincept` row from llm_configs;
///  2. if no provider row is_active, upsert ollama {base_url: http://localhost:11434,
///     model: llama3.1:8b, api_key: "", is_active: 1}.
/// Returns the provider left active, for logging.
QString bootstrap_llm_defaults();
}
```

Called from `main.cpp` inside the `else` branch that follows `fincept::Database::instance().open(db_path)` succeeding, directly after `fincept::trading::AccountManager::instance().reload_from_db();` — migrations have applied by then and no `WindowFrame` exists yet. Uses only `LlmConfigRepository` (`list_providers`, `delete_provider`, `save_provider`). `save_provider` with `is_active = true` does not clear other rows' `is_active`, so the bootstrap only seeds when nothing is active.

Settings → LLM Config (`LlmConfigSection_Providers.cpp`) builds its provider combo from `ProviderCatalog::known_providers()`, so `fincept` disappears without a UI edit. The `AuthManager::clear_session()` line that deletes the `fincept` provider is left as is.

### 5.6 UI — remove upsell chrome

`ToolBar` (`ui/navigation/ToolBar.cpp`) constructor, after the widgets are created: in local mode `hide()` `credits_label_`, `plan_btn_`, `upgrade_btn_`, `logout_btn_`, and set `user_label_` text to `tr("LOCAL")` in `refresh_user_display()` when unauthenticated in local mode (instead of `"---"`). `apply_responsive_layout()` toggles `credits_label_` visibility by width; guard that toggle with `&& !local_mode::enabled()` so it cannot re-show the label.

`ToolBar::build_help_menu()` — in local mode omit the trailing separator plus the two actions `tr("Check for Updates")` (emits `action_triggered("check_updates")`; a menu check that says nothing reads as broken) and `tr("Logout")` (emits `action_triggered("logout")`). The `WindowFrame` handlers for those action strings stay in place.

`SettingsScreen` — the sidebar button `make_btn(QStringLiteral("Cloud Sync"), 15, ...)` is not created in local mode. `section_factories_[15]` and the `CloudSyncSection` widget stay in the stack so the fixed-size index table is untouched. The `Credentials` section stays visible: despite its name it holds third-party keys (Alpha Vantage, FRED, Polygon, …), which the user needs.

`WindowFrame::setup_auth_screens()` is unchanged — the auth widgets are constructed but never shown. Constructing them costs a few milliseconds and keeps the diff small.

### 5.7 Documentation

`docs/LOCAL_ONLY_MODE.md` (new, ~60 lines): what the switch does, how to turn it off (`-DFINCEPT_LOCAL_ONLY=OFF` or `FINCEPT_LOCAL_ONLY=0` at runtime), how to install Ollama on Ubuntu and pull `llama3.1:8b`, how to add a Groq key in Settings → LLM Config, and what is intentionally unavailable (Fincept LLM, cloud sync, QuantLib cloud until spec 3, PIN lock).

`README.md` — one paragraph at the top: this fork builds in local-only mode by default; link to `docs/LOCAL_ONLY_MODE.md`. No other README edits.

## 6. Data flow (boot, local mode)

```
main()
 ├─ TerminalShell::initialise()  → DB open, migrations (v002 seeds fincept)
 ├─ local_mode::bootstrap_llm_defaults()   → delete fincept row, activate ollama
 ├─ TerminalShell::bootstrap_auth()
 │    └─ AuthManager::initialize() → local mode: empty session, loading=false, emit
 ├─ (skip) CloudSyncEngine registration
 ├─ new WindowFrame(0)
 │    ├─ setup_auth_screens()  (constructed, hidden)
 │    ├─ local mode → shell visible, stack index 1, WorkspaceShell::load_last_or_default()
 │    └─ on_auth_state_changed() → early return
 ├─ UpgradeDialog::maybe_show_at_startup() → early return
 └─ app.exec()
```

No socket is opened to `api.fincept.in` on this path. Yahoo/FRED/NSE/Ollama/Groq connectors are untouched and continue to open their own connections when screens request data.

## 7. Error handling

- Ollama not running: `LlmService` already surfaces the connection-refused error in the chat pane; `LlmConfigSection` already shows an empty model list from `/api/tags`. `docs/LOCAL_ONLY_MODE.md` documents `systemctl status ollama` and `ollama pull llama3.1:8b`.
- Corrupt/missing `llm_configs` row: `bootstrap_llm_defaults()` is idempotent and re-seeds `ollama` if nothing is active; it never overwrites an active non-Fincept provider the user chose.
- A screen calls `QuantLibClient` in local mode: callback receives `success=false` with the fixed message; no network I/O.
- `FINCEPT_LOCAL_ONLY=0` at runtime on a local-only build: full upstream behaviour (login wall, Fincept default) — useful for comparing against upstream, documented as such.

## 8. Testing

**Unit (Qt Test, `-DFINCEPT_BUILD_TESTS=ON`):**

`tests/tst_local_mode.cpp` linking only `src/core/config/LocalMode.cpp`:
- `unset_env_returns_compiled_default` (both `true` and `false`)
- `env_zero_false_off_no_disable` (case-insensitive)
- `env_one_true_on_yes_enable`
- `garbage_env_returns_compiled_default`
- `enabled_is_stable_across_calls`

`ProviderCatalog::default_model` / `default_provider` are pure but `ProviderCatalog.cpp` has no app includes beyond Qt Core; add `tests/tst_provider_catalog.cpp` linking `ProviderCatalog.cpp` + `LocalMode.cpp`:
- `default_provider_is_ollama_in_local_mode` (via `qputenv("FINCEPT_LOCAL_ONLY","1")` before first `enabled()` call — the suite runs in its own process)
- `known_providers_excludes_fincept_in_local_mode`
- `fincept_display_name_has_no_recommended_suffix`
- `default_model_for_ollama_is_llama31_8b`

**Headless:** `./FinceptTerminal --smoke-test` must exit 0 with `FINCEPT_LOCAL_ONLY=1` (default) — it constructs every screen through the router, which now happens without a session.

**Manual (once, on the user's Ubuntu box):**
1. `./FinceptTerminal` → dashboard appears with no login/PIN/pricing.
2. `ss -tnp | grep -i fincept` while idle for 2 minutes → no connection to `api.fincept.in` (resolve its IP first with `dig`).
3. Settings → LLM Config → provider list has no "Fincept LLM"; Ollama is active with `llama3.1:8b`; sending a chat message returns a response from the local model.
4. Toolbar shows no UPGRADE / plan / credits / LOGOUT.
5. Equity Research → Valuation → DCF shows the "disabled in local-only mode" message, not a spinner.

## 9. Files touched

| File | Change |
|---|---|
| `fincept-qt/CMakeLists.txt` | `option(FINCEPT_LOCAL_ONLY ON)`, compile definition, add two source files, status line |
| `fincept-qt/src/core/config/LocalMode.{h,cpp}` | **new** — switch |
| `fincept-qt/src/core/config/LocalModeBootstrap.{h,cpp}` | **new** — LLM row bootstrap |
| `fincept-qt/src/app/main.cpp` | call bootstrap; skip cloud adapter registration |
| `fincept-qt/src/app/WindowFrame.h` | `has_full_access()` |
| `fincept-qt/src/app/WindowFrame.cpp` | constructor route; `has_full_access()` impl |
| `fincept-qt/src/app/WindowFrame_Auth.cpp` | early return; two `has_paid_plan` → `has_full_access` |
| `fincept-qt/src/auth/AuthManager.cpp` | `initialize()` early return |
| `fincept-qt/src/services/cloud/` | none (skipped at registration) |
| `fincept-qt/src/services/quantlib/QuantLibClient.cpp` | fail-fast in `call()` |
| `fincept-qt/src/services/updater/UpdateService.cpp` | early return |
| `fincept-qt/src/services/llm/ProviderCatalog.{h,cpp}` | filter `fincept`; `default_provider`, `default_model`; display name |
| `fincept-qt/src/services/llm/LlmService.cpp` | fallback uses catalog defaults |
| `fincept-qt/src/ui/widgets/EnterprisePromo.cpp` | early returns |
| `fincept-qt/src/ui/navigation/ToolBar.cpp` | hide four widgets; LOCAL label; help-menu filter |
| `fincept-qt/src/screens/settings/SettingsScreen.cpp` | hide Credentials + Cloud Sync sidebar entries |
| `fincept-qt/tests/CMakeLists.txt`, `tests/tst_local_mode.cpp`, `tests/tst_provider_catalog.cpp` | tests |
| `docs/LOCAL_ONLY_MODE.md`, `README.md` | docs |

## 10. Open questions resolved

- **Why not make Groq the default?** It needs an API key the user must paste; Ollama needs nothing but a running daemon, and the user has an RTX 3060 12 GB that runs `llama3.1:8b` comfortably. Groq is one dropdown away.
- **Why keep constructing the auth screens?** Removing them changes `auth_stack_` indices that `show_*()` helpers depend on; not worth the merge risk for a few milliseconds.
- **Why an env override at all?** It makes the Qt Test suite able to exercise both branches in one binary and gives the user a one-line way to see upstream behaviour for comparison.

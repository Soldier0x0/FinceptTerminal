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

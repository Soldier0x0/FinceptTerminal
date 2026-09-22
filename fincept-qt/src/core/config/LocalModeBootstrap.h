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

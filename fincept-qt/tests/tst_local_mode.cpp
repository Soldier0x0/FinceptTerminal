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

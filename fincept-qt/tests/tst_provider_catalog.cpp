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

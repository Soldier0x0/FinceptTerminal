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
            LOG_WARN(kTag,
                     "bootstrap_llm_defaults: could not delete fincept row — " + QString::fromStdString(r.error()));
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
            LOG_WARN(kTag,
                     "bootstrap_llm_defaults: could not seed default provider — " + QString::fromStdString(r.error()));
            return {};
        }
        active = c.provider;
        LOG_INFO(kTag, QString("Seeded default LLM provider %1 (%2 @ %3)").arg(c.provider, c.model, c.base_url));
    }

    return active;
}

} // namespace fincept::local_mode

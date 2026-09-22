#include "core/config/LocalMode.h"

#include <QByteArray>
#include <QtGlobal>

// Absent when compiled outside the FinceptTerminal target (unit tests): fall
// back to upstream behaviour so the test binary does not depend on the option.
#ifndef FINCEPT_LOCAL_ONLY
#    define FINCEPT_LOCAL_ONLY 0
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

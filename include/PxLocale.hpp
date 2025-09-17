
#include <string>
#include <vector>
#include <PxConfig.hpp>

#ifndef PXLOCALE
#define PXLOCALE

#define PXL(...) (PxLocale::LocaleString){ .params = {__VA_ARGS__} }
#define PXLS(x) (PxLocale::LocaleString){ .params = {"raw", x} }

namespace PxLocale {
    struct LocaleString {
        std::vector<std::string> params;
    };

    struct LocaleLang {
        PxConfig::conf lang;
    };

    extern LocaleLang lang;
    std::string Resolve(const LocaleString &locString);
    PxResult::Result<void> Init(std::string which = "");
}

#endif
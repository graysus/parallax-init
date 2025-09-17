
#include <PxLocale.hpp>
#include <PxConfig.hpp>

namespace PxLocale {
    LocaleLang lang;

    std::string Resolve(const LocaleString &locString) {
        if (locString.params.size() == 0) {
            return "ERROR_EMPTY_LCSTRING";
        } else if (locString.params[0] == "raw") {
            if (locString.params.size() == 1) {
                return "ERROR_RAW_TOO_FEW_ARGS";
            }
            return locString.params[1];
        } else {
            std::string arg = "";
            for (size_t i = 1; i < locString.params.size(); i++) {
                arg += locString.params[i]+lang.lang.separator;
            }
            if (arg.size() > 0) {
                arg = arg.substr(0, arg.length()-lang.lang.separator.length());
            }
            auto qread = lang.lang.QuickRead(locString.params[0], arg);
            if (qread == "") {
                return "ENOTFOUND("+locString.params[0]+" "+arg+")";
            } else {
                return qread;
            }
        }
    }
    PxResult::Result<void> Init(std::string which) {
        lang = {};

        std::string langPath = which;
        if (!PxFunction::startsWith(which, "/") && !PxFunction::startsWith(which, "./")) {
            // TODO: prefix
            langPath = "/usr/share/parallax/locale/" + which;
        }

        auto reslang = PxConfig::ReadConfig(langPath);
        PXASSERTM(reslang, "PxLocale::Init");

        lang.lang = reslang.assert();
        lang.lang.separator = "<PXLOCALE-SEP>";

        return PxResult::Null;
    }
}
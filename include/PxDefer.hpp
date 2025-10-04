
#ifndef PXHDEFER
#define PXHDEFER

#include <functional>
#include <PxResult.hpp>
#include <PxLog.hpp>
#include <type_traits>
#include <cstring>

#define DEFER(name, ...) PxDefer::Defer<void> name([&]() { __VA_ARGS__; })
#define DEFER_RV(name, ...) PxDefer::Defer<PxResult::Result<void>> name([&]() -> PxResult::Result<void> { __VA_ARGS__; return PxResult::Null; }, #name)

namespace PxDefer {
    template<typename T> class Defer {
    private:
        bool cancelled = false;
        std::function<T()> execute;
    public:
        inline Defer(std::function<T()> fn) : execute(fn) {

        }
        inline ~Defer() {
            if (!cancelled) {
                execute();
            }
        }
        inline void cancel() {
            cancelled = true;
        }
        inline T finish() {
            if (!cancelled) {
                cancelled = true;
                return execute();
            } else {
                if constexpr (std::is_void<T>::value) {
                    return;
                } else {
                    return T();
                }
            }
        }
    };
    template<typename T> class Defer<PxResult::Result<T>> {
    private:
        bool cancelled = false;
        std::function<PxResult::Result<T>()> execute;
        std::string name;
    public:
        inline Defer(std::function<PxResult::Result<T>()> fn, std::string tagname = "") : execute(fn), name(tagname) {

        }
        inline ~Defer() {
            if (!cancelled) {
                auto res = execute();
                if (res.eno) {
                    PxLog::log.warn("Ignoring failure result in "+name+": "+res.funcName+": "+strerror(res.eno));
                }
            }
        }
        inline void cancel() {
            cancelled = true;
        }
        inline PxResult::Result<T> finish() {
            if (!cancelled) {
                cancelled = true;
                return execute().merge("defer "+name);
            } else {
                if constexpr (std::is_void<T>::value) {
                    return PxResult::Null;
                } else {
                    return PxResult::Result<T>(T());
                }
            }
        }
    };
}

#endif
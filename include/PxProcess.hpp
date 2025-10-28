
#include <PxLog.hpp>
#include <PxResult.hpp>
#include <PxFunction.hpp>
#include <cerrno>
#include <cstring>
#include <unistd.h>
#include <sys/wait.h>

#ifndef PXPROC
#define PXPROC

namespace PxProcess {
	void CloseFD(bool include_io = false);


	template<typename... Ts> struct Count {
		static const int value = 0;
	};

	template<typename T, typename... Ts> struct Count<T, Ts...> {
		static const int value = 1 + Count<Ts...>::value;
	};

	PxResult::Result<void> Exec(std::vector<std::string> argv);

	template<typename... Ts> inline PxResult::Result<void> Exec(Ts... argv) {
		static_assert(Count<Ts...>::value > 1);
		return Exec({argv...});
	}

	template<typename... Ts> PxResult::Result<int> Spawn(bool block, Ts... argv) {
		int f = fork();
		if (f < 0) {
			return PxResult::FResult("PxProcess::Spawn / fork", errno);
		} else if (f == 0) {
			// child
			auto res = ([argv...]() -> PxResult::Result<void> {
				CloseFD(false);
				PXASSERT(Exec(argv...));
			})();

			if (!res.eno) res = PxResult::FResult("What are we doing here??", EINVAL);

			PxLog::log.error(res.funcName + ": "+strerror(res.eno));
			exit(1);
		}

		if (block) {
			int wstatus;
			waitpid(f, &wstatus, 0);
			return wstatus;
		} else {
			return f;
		}
	}
}

#endif

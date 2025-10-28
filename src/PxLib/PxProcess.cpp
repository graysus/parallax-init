
#include <PxProcess.hpp>
#include <filesystem>

namespace PxProcess {
	void CloseFD(bool include_io) {
#ifdef __linux__
		std::vector<int> toClose;
		for (auto &i : std::filesystem::directory_iterator("/proc/self/fd/")) {
			auto name = i.path().filename();
			int fd = std::atoi(name.c_str());
			if (fd < 3 && !include_io) continue;
			if (fd < 0) continue;
			toClose.push_back(fd);
		}

		// Must be iterated separately, since it also closes the directory iterator
		for (auto i : toClose) close(i);
#else
		size_t files = sysconf(_SC_OPEN_MAX);
		for (size_t i = 0; i < files; i++) {
			if (i < 3 && !include_io) continue;
			if (i < 0) continue;
			close(i);
		}
#endif
	}

	PxResult::Result<void> Exec(std::vector<std::string> argv) {
		if (argv.size() <= 1) {
			return PxResult::FResult("PxProcess::Exec (not enough arguments)", EINVAL);
		}

		char **cstrs = (char **)malloc(sizeof(char* const) * (argv.size()+1));
		size_t pos = 0;
		for (std::string i : argv) {
			cstrs[pos] = strdup(i.c_str());
			pos++;
		}
		cstrs[pos] = NULL;
		execv(cstrs[0], cstrs+1);

		return PxResult::FResult("PxProcess::Exec / execv", errno);
	}
};


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
};

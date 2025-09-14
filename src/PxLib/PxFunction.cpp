
#include <cerrno>
#include <cstddef>
#include <string>
#include <sys/ioctl.h>
#include <sys/stat.h>
#if linux
#include <linux/vt.h>
#endif
#include <vector>
#include <chrono>
#include <fcntl.h>
#include <unistd.h>
#include <filesystem>
#include <PxResult.hpp>

namespace PxFunction {
	PxResult::Result<void> chvt(int which) {
		#if linux
		int fd = open("/dev/tty0", O_RDWR);
		if (fd < 0) {
			return PxResult::Result<void>("chvt / open", errno);
		}
		if (ioctl(fd, VT_ACTIVATE, which) < 0) {
			return PxResult::Result<void>("chvt / ioctl VT_ACTIVATE", errno);
		}
		if (ioctl(fd, VT_WAITACTIVE, which) < 0) {
			return PxResult::Result<void>("chvt / ioctl VT_WAITACTIVE", errno);
		}
		#endif
		return PxResult::Null;
	}
	std::vector<std::string> split(std::string input, std::string sep = " ", int splits = -1) {
		std::vector<std::string> parts;
		while (1) {
			auto point = input.find(sep);
			// I love unsigned integers!!!!!!!!!!!!!!!!!!!!
			if (point == std::string::npos || (splits--) == 0) {
				parts.push_back(input);
				break;
			}
			parts.push_back(input.substr(0, point));
			input = input.substr(point+sep.length());
		}
		return parts;
	}
	std::string join(std::vector<std::string> input, std::string sep = " ") {
		std::string output = "";
		for (size_t i = 0; i < input.size(); i++) {
			output += input[i];
			if (i != input.size()-1) output += sep;
		}
		return output;
	}
	std::string rtrim(std::string input, std::string whitespace = " \n\t\r") {
		auto x = input.find_last_not_of(whitespace);
		if (x == std::string::npos) return "";
		return input.erase(x+1);
	}
	std::string ltrim(std::string input, std::string whitespace = " \n\t\r") {
		auto x = input.find_first_not_of(whitespace);
		if (x == std::string::npos) return "";
		return input.erase(0, x);
	}
	std::string trim(std::string input, std::string whitespace = " \n\t\r") {
		return rtrim(ltrim(input, whitespace), whitespace);
	}
	std::vector<std::string> vectorize(int argc, const char** argv) {
		std::vector<std::string> s;
		for (int i = 0; i < argc; i++) {
			s.push_back((std::string)argv[i]);
		}
		return s;
	}
	bool startsWith(std::string s1, std::string s2) {
		return s1.substr(0, s2.length()) == s2;
	}
	bool endsWith(std::string s1, std::string s2) {
		if (s1.length() < s2.length()) return false;
		return s1.substr(s1.length()-s2.length()) == s2;
	}
	unsigned long long now() {
		return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
	}
	void setnonblock(int fd, bool isActive) {
		int flags = fcntl(fd, F_GETFL);
		if (isActive)
			flags |= O_NONBLOCK;
		else
			flags &= ~O_NONBLOCK;
		fcntl(fd, F_SETFL, flags);
	}
	bool waitExist(std::string path, int timeout = 200) {
		for (int i = 0; i < timeout; i++) {
			usleep(10000);
			if (std::filesystem::exists(path)) return true;
		}
		return false;
	}
	PxResult::Result<void> assert(bool cond) {
		if (!cond) {
			return PxResult::Result<void>("assert", EINVAL);
		} else {
			return PxResult::Null;
		}
	}
	PxResult::Result<void> wrap(std::string name, int err) {
		if (err < 0) {
			return PxResult::Result<void>(name, errno);
		} else {
			return PxResult::Null;
		}
	}
	PxResult::Result<void> mkdirs(std::string path, bool modeto) {
		auto dirs = split(path, "/");
		std::string curpath = "/";
		if (endsWith(path, "/")) {
			path = path.substr(0, path.length()-1);
		}
		if (modeto) {
			auto pos = path.rfind("/");
			if (pos != std::string::npos) {
				path = path.substr(0, pos);
			}
		}
		for (auto &i : dirs) {
			curpath += "/"+i;
			if (std::filesystem::is_directory(curpath)) continue;
			if (std::filesystem::exists(curpath)) return PxResult::Result<void>("mkdirs", ENOTDIR);
			if (!std::filesystem::create_directory(curpath)) {
				return PxResult::Result<void>("mkdirs / mkdir", errno);
			}
		}
		return PxResult::Null;
	}
}

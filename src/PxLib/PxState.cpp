
#include <cerrno>
#include <cstdio>
#include <string>
#include <PxResult.hpp>

#define PXS_BUF_SIZE 4096

namespace PxState {
	PxResult::Result<void> fput(std::string filename, std::string content) {
		FILE* file = fopen(filename.c_str(), "w");
		if (file == NULL) {
			return PxResult::Result<void>("PxState::put(\""+filename+"\") / fopen", errno);
		}
		fwrite(content.c_str(), sizeof(char), content.length(), file);
		if (ferror(file)) {
			return PxResult::Result<void>("PxState::put(\""+filename+"\") / fwrite", errno);
		}
		if (fclose(file) != 0) {
			return PxResult::Result<void>("PxState::put(\""+filename+"\") / fclose", errno);
		}
		return PxResult::Null;
	}
	PxResult::Result<std::string> fgetp(std::string filename) {
		FILE* file = fopen(filename.c_str(), "r");
		if (file == NULL) {
			return PxResult::Result<std::string>("PxState::get(\""+filename+"\") / fopen", errno);
		}
		std::string output;
		char buf[PXS_BUF_SIZE];
		while (!feof(file)) {
			size_t nbytes = fread(buf, sizeof(char), PXS_BUF_SIZE, file);
			if (nbytes == 0 || nbytes == -1) break;
			if (ferror(file)) {
				fclose(file);
				return PxResult::Result<std::string>("PxState::get(\""+filename+"\") / fread", errno);
			}
			output += std::string(buf, nbytes);
		}
		if (fclose(file) != 0) {
			return PxResult::Result<std::string>("PxState::get(\""+filename+"\") / fclose", errno);
		}
		return output;
	}
	PxResult::Result<std::string> fget(std::string filename) {
		return fgetp(filename);
	}
};

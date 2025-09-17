#include <PxFunction.hpp>
#include <PxResult.hpp>
#include <PxLog.hpp>
#include <PxState.hpp>
#include <map>
#include <vector>

#ifndef PXCONF
#define PXCONF
namespace PxConfig {
	std::vector<std::string> Escape(std::string baseString, std::string argument = "", int msplit = 9);
	class conf {
	private:
		std::map<std::string, std::vector<std::string>> vec_properties;
	public:
		std::string def_arg;
		int maxargs = 9;

		void RawAdd(std::string key, std::string value) {
			vec_properties[key].push_back(value);
		}

		void ReadValue(const std::string &key, std::vector<std::string> &output, const std::string &argument = "") {
			for (auto &def : vec_properties[key]) {
				auto escaped = Escape(def, def_arg, maxargs-1);
				for (auto &i : escaped) {
					output.push_back(i);
				}
			}
		}

		std::string QuickRead(const std::string &key, const std::string &argument = "") {
			std::vector<std::string> output;
			ReadValue(key, output, argument);
			if (output.size() == 0) return "";
			return output[0];
		}

		std::vector<std::string> QuickReadVec(const std::string &key, const std::string &argument = "") {
			std::vector<std::string> output;
			ReadValue(key, output, argument);
			return output;
		}
	};
	PxResult::Result<void> ConfSetValue(conf *c, std::string key, std::string value);
	PxResult::Result<conf> ReadConfig(std::string filename, std::string argument = "");
};
#endif

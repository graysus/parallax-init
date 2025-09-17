
#include <PxConfig.hpp>

namespace PxConfig {
	std::vector<std::string> Escape(std::string baseString, std::string argument, int msplit) {
		std::string outputString;
		std::vector<std::string> outputVector;
		auto larguments = PxFunction::split(argument, ":", msplit);
		bool escaping = false;
		bool varEscaping = false;
		for (size_t i = 0; i < baseString.length(); i++) {
			char c = baseString.at(i);
			if (escaping) {
				switch (c) {
				case 'n':
					outputString += "\n";
					break;
				case '\\':
					outputString += '\\';
					break;
				case '$':
					outputString += '$';
					break;
				default:
					outputString += '\\';
					outputString += c;
					break;
				}
				escaping = false;
				continue;
			} else if (varEscaping) {
				if ('1' <= c && c <= '9') {
					outputString += larguments[c-'1'];
				} else if (c == '@') {
					outputString += argument;
				}
				varEscaping = false;
				continue;
			}
			if (c == '\\') {
				escaping = true;
			} else if (c == '$') {
				varEscaping = true;
			} else if (c == ';') {
				outputVector.push_back(outputString);
				outputString = "";
			} else outputString += c;
		}
		outputVector.push_back(outputString);
		return outputVector;
	}
	PxResult::Result<void> ConfSetValue(conf *c, std::string key, std::string value) {
		if (key == "MaxArgs") {
			c->maxargs = std::atoi(value.c_str());
			if (c->maxargs <= 0) {
				return PxResult::Result<void>("PxConfig::ReadConfig (MaxArgs is not a positive non-zero number.)", EINVAL);
			}
		} else if (key == "RequiredArg") {
			if (value.length() == 0) {
				return PxResult::Result<void>("PxConfig::ReadConfig (An argument is required, but was not specified.)", EINVAL);
			}
		}
		
		c->RawAdd(key, value);
		return PxResult::Null;
	}
	PxResult::Result<conf> ReadConfig(std::string filename, std::string argument) {
		conf c;
		c.def_arg = argument;
		PxResult::Result<std::string> file_content_state = PxState::fget(filename);
		PXASSERTM(file_content_state, "PxConfig::ReadConfig");
		std::string file_content = file_content_state.assert();
		std::vector<std::string> file_lines = PxFunction::split(file_content, "\n");
		int lineno = 0;
		for (auto& i : file_lines) {
			lineno++;
			// Remove comments and whitespace
			auto line = PxFunction::trim(PxFunction::split(i, "#", 1)[0]);

			// Skip if completely blank
			if (line.length() == 0) continue;

			auto key_value = PxFunction::split(line, "=", 1);
			if (key_value.size() != 2) {
				return PxResult::Result<conf>("PxConfig::ReadConfig (incomplete expression on line "+std::to_string(lineno)+")", EINVAL);
			}
			auto key = PxFunction::trim(key_value[0]);
			auto value = PxFunction::trim(key_value[1]);
			
			ConfSetValue(&c, key, PxFunction::trim(value));
		}
		return c;
	}
}

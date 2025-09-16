
#include <initializer_list>
#include <string>
#include <vector>
#include <PxResult.hpp>

#ifndef PXFUNCTION_HPP
#define PXFUNCTION_HPP

#define VEC(x) PxFunction::tovec(x)

namespace PxFunction {
	std::vector<std::string> split(std::string input, std::string sep = " ", int splits = -1);
	std::string join(std::vector<std::string> input, std::string sep = " ");
	std::string rtrim(std::string input, std::string whitespace = " \n\t\r");
	std::string ltrim(std::string input, std::string whitespace = " \n\t\r");
	std::string trim(std::string input, std::string whitespace = " \n\t\r");
	std::vector<std::string> vectorize(int argc, const char** argv);
	bool startsWith(std::string s1, std::string s2);
	bool endsWith(std::string s1, std::string s2);

	// Returns current time in milliseconds.
	unsigned long long now();

	// Sets a file descriptor to non-blocking
	void setnonblock(int fd, bool isActive = true);

	// Similar to splice() in most languages, but returns no value
	template<typename T> void vecRemoveIndeces(std::vector<T> *vec, int start, int count = 1) {
		vec->erase(std::next(vec->begin(), start), std::next(vec->begin(), start+count));
	}
	// Similar to splice() in most languages, but returns no value
	template<typename T> std::vector<T> vecSplice(std::vector<T> *vec, int start, int count = 1) {
		std::vector<T> ret;
		for (int i = start; i < start+count; i++) {
			ret.push_back((*vec)[i]);
		}
		vec->erase(std::next(vec->begin(), start), std::next(vec->begin(), start+count));
		return ret;
	}
	// Removes at most 1 (starting from the left) of the specified item from a vector.
	// Returns whether it was able to remove the item
	template<typename T> bool vecRemoveItem(std::vector<T> *vec, T &item) {
		for (size_t i = 0; i < vec->size(); i++) {
			if ((*vec)[i] == item) {
				vecRemoveIndeces(vec, i);
				return true;
			}
		}
		return false;
	}

	template<typename T, typename U> bool contains(const std::vector<T> &array, const U &value) {
		for (auto &i : array) {
			if (i == value) {
				return true;
			}
		}
		return false;
	}
	template<typename T, typename U> inline bool contains(const std::initializer_list<T> &array, const U &value) {
		return contains((std::vector<T>)array, value);
	}

	PxResult::Result<void> chvt(int which);
	bool waitExist(std::string path, int timeout = 200);
	PxResult::Result<void> assert(bool cond);
	PxResult::Result<void> wrap(std::string name, int err);
	PxResult::Result<void> mkdirs(std::string path, bool modeto = false);

	template<typename T> inline std::vector<T> tovec(std::initializer_list<T> i) {
		return (std::vector<T>)i;
	}
}

#endif

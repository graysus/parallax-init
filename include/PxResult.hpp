
#include <cerrno>
#include <iostream>
#include <memory>
#include <string>
#include <cstdlib>
#include <cstdio>
#include <unistd.h>

#ifndef PXRESULT_HPP
#define PXRESULT_HPP

#define PXASSERT(val) { auto _V = val; if (_V.eno) return (PxResult::FResult) _V; }
#define PXASSERTM(val, msg) { auto _V = val; if (_V.eno) return (PxResult::FResult) _V.merge(msg); }

namespace PxResult {
	template<typename T> struct Result;
	template<> struct Result<void>;
	template<typename T> struct Result {
		int eno;
		std::string funcName;
		std::shared_ptr<T> object;

		Result(const T& Object) {
			funcName = "";
			eno = 0;
			object = std::make_shared<T>(Object);
		}
		Result(std::string FuncName, int Errno) {
			funcName = FuncName;
			eno = Errno == 0 ? 100000000 : Errno;
			object = nullptr;
		}
		T assert(std::string from = "<unknown>") {
			if (eno) {
				// TODO: print error using log
				errno = eno;
				std::perror((from + " / " + funcName).c_str());
				std::cout.flush();
				if (getpid() == 1) {
					std::cout << "System halted.\n";
					while (1) {
						usleep(1000000);
					}
				}
				exit(1);
			}
			return *object;
		}
		PxResult::Result<T> merge(std::string from) {
			if (eno == 0) {
				return PxResult::Result<T>(assert());
			} else {
				return PxResult::Result<T>(from + " / " + funcName, eno);
			}
		}
		template<typename T2> inline PxResult::Result<T2> cast() {
			return (PxResult::Result<T2>)*this;
		}
		template<typename T2> PxResult::Result<T2> replace(const T2& val) {
			if (eno) return Result<T2>(funcName, eno);
			else	 return Result<T2>(val);
		}
		inline PxResult::Result<void> replace();
	};
	template<> struct Result<void> {
		int eno;
		std::string funcName;
		Result() {
			funcName = "";
			eno = 0;
		}
		template<typename T2> Result(const Result<T2>& res) {
			funcName = res.funcName;
			eno = res.eno;
		}
		Result(std::string FuncName, int Errno) {
			funcName = FuncName;
			eno = Errno;
		}
		void assert(std::string from = "<unknown>") {
			if (eno) {
				errno = eno;
				std::perror((from + " / " + funcName).c_str());
				std::cout.flush();
				if (getpid() == 1) {
					std::cout << "System halted.\n";
					while (1) {
						usleep(1000000);
					}
				}
				exit(1);
			}
		}
		PxResult::Result<void> merge(std::string from) {
			if (eno == 0) {
				return PxResult::Result<void>();
			} else {
				return PxResult::Result<void>(from + " / " + funcName, eno);
			}
		}
		template<typename T> inline PxResult::Result<T> cast() {
			return (PxResult::Result<T>)*this;
		}
		template<typename T> PxResult::Result<T> replace(const T& val) const {
			if (eno) return Result<T>(funcName, eno);
			else	 return Result<T>(val);
		}
		template<typename T> inline PxResult::Result<T> replace() {
			return *this;
		}
	};
	struct FResult : PxResult::Result<void> {
		template<typename T2> inline FResult(const Result<T2>& res) {
			funcName = res.funcName;
			eno = res.eno;
		}
		inline FResult(std::string FuncName, int Errno) {
			funcName = FuncName;
			eno = Errno;
		}
		template<typename T> inline operator Result<T>() {
			return Result<T>(funcName, eno);
		}
		inline PxResult::FResult merge(std::string from) {
			return Result<void>::merge(from);
		}
	};
	template<typename T> inline PxResult::Result<void> Result<T>::replace() {
		return (Result<void>) *this;
	}
	template<typename T> inline PxResult::Result<void> Clear(const PxResult::Result<T>& result) {
		return (PxResult::Result<void>) result;
	}
	template<typename T> inline PxResult::Result<T> Attach(const PxResult::Result<void>& result, const T& object) {
		return result.replace(object);
	}
	template<typename T, typename T2> inline PxResult::Result<T> Reattach(const PxResult::Result<T2>& result, const T& object) {
		return result.replace(object);
	}
	static inline struct {
		inline operator PxResult::Result<void>() {
			return PxResult::Result<void>();
		}
	} Null __attribute__((unused));
}
#endif

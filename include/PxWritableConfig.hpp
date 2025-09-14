#include <PxConfig.hpp>
#include <cstddef>
#include <memory>
#include <string>
#include <PxFunction.hpp>
#include <vector>
#include <set>

namespace PxConfig {
	struct StringToken;
	class Token {
	public:
		virtual std::string get() {
			return "";
		}
		virtual void rawfind(int *n, StringToken **res) {
			
		}
		virtual void rawfindkey(int *n, StringToken **res, std::string key) {
			return;
		}
		virtual bool rawinsto(int *n, StringToken **res) {
			return false;
		}
		virtual bool rawinstokey(int *n, StringToken **res, std::string key) {
			return false;
		}
		virtual void rawkeys(std::set<std::string> *keys) {}
		virtual int rawcount() {return 0;}
		virtual int rawcountkey(std::string key) {return 0;}
		StringToken *find(std::string key, int n) {
			StringToken *res = NULL;
			int in = n;
			rawfindkey(&in, &res, key);
			return res;
		}
		StringToken *insto(std::string key, int n) {
			StringToken *res = NULL;
			int in = n;
			if (!rawinstokey(&in, &res, key)) return NULL;
			return res;
		}
		std::set<std::string> keys() {
			std::set<std::string> result;
			rawkeys(&result);
			return result;
		}
		int countkey(std::string key) {
			return rawcountkey(key);
		}
	};
	struct StringToken : public Token {
		std::string me;
		StringToken(std::string s) {
			me = s;
		}
		void rawfind(int *n, StringToken **res) override {
			if ((*n)-- == 0) {
				*res = this;
			}
		}
		bool rawinsto(int *n, StringToken **res) override {
			(*n)--;
			return false;
		}
		std::string get() override {
			return me;
		}
		int rawcount() override {
			return 1;
		}
	};
	struct WhitespacedToken : public Token {
		std::string ltrim;
		std::shared_ptr<Token> content;
		std::string rtrim;
		WhitespacedToken(std::string Ltrim, std::shared_ptr<Token> Content, std::string Rtrim) {
			ltrim = Ltrim;
			content = Content;
			rtrim = Rtrim;
		}
		std::string get() override {
			return ltrim + content->get() + rtrim;
		}
		void rawfind(int *n, StringToken **res) override {
			content->rawfind(n, res);
		}
		void rawfindkey(int *n, StringToken **res, std::string key) override {
			content->rawfindkey(n, res, key);
		}
		bool rawinsto(int *n, StringToken **res) override {
			return content->rawinsto(n, res);
		}
		bool rawinstokey(int *n, StringToken **res, std::string key) override {
			return content->rawinstokey(n, res, key);
		}
		void rawkeys(std::set<std::string> *keys) override {
			return content->rawkeys(keys);
		}
		int rawcount() override {
			return content->rawcount();
		}
		int rawcountkey(std::string key) override {
			return content->rawcountkey(key);
		}
	};
	struct KeyValueToken : public Token {
		std::shared_ptr<Token> key;
		std::shared_ptr<Token> value;
		KeyValueToken(std::shared_ptr<Token> Key, std::shared_ptr<Token> Value) {
			key = Key;
			value = Value;
		}
		std::string get() override {
			return key->get() + "=" + value->get();
		}
		void rawfindkey(int *n, StringToken **res, std::string key) override {
			if (key == PxFunction::trim(this->key->get())) {
				this->value->rawfind(n, res);
			}
		}
		bool rawinstokey(int *n, StringToken **res, std::string key) override {
			if (key == PxFunction::trim(this->key->get())) {
				return this->value->rawinsto(n, res);
			}
			return false;
		}
		void rawkeys(std::set<std::string> *keys) override {
			keys->insert(PxFunction::trim(key->get()));
		}
		int rawcountkey(std::string key) override {
			if (key != PxFunction::trim(this->key->get())) return 0;
			return value->rawcount();
		}
	};
	struct ArrayToken : public Token {
		std::vector<std::shared_ptr<Token>> tokens;
		ArrayToken(std::vector<std::shared_ptr<Token>> Tokens) {
			tokens = Tokens;
		}
		std::string get() override {
			std::vector<std::string> strings;
			for (auto &i : tokens) {
				strings.push_back(i->get());
			}
			return PxFunction::join(strings, ";");
		}
		void rawfind(int *n, StringToken **res) override {
			for (auto &i : tokens) {
				i->rawfind(n, res);
			}
		}
		void rawfindkey(int *n, StringToken **res, std::string key) override {
			for (auto &i : tokens) {
				i->rawfindkey(n, res, key);
			}
		}
		bool rawinsto(int *n, StringToken **res) override {
			int insidx = -1;
			if (*n == 0) insidx = 0;
			else for (int idx = 0; idx < tokens.size(); idx++) {
				auto i = tokens[idx];
				bool inserted = i->rawinsto(n, res);
				if (inserted) return true;
				if (*n == 0) {
					insidx = idx+1;
					break;
				}
			}
			if (insidx == -1) return false;
			auto vec = PxFunction::vecSplice(&tokens, insidx, tokens.size()-insidx);
			auto t = std::make_shared<StringToken>("");
			tokens.push_back(t);
			*res = t.get();
			for (auto &i : vec) {
				tokens.push_back(i);
			}
			return true;
		}
		bool rawinstokey(int *n, StringToken **res, std::string key) override {
			int insidx = -1;
			if (*n == 0) insidx = 0;
			else for (int idx = 0; idx < tokens.size(); idx++) {
				auto i = tokens[idx];
				bool inserted = i->rawinstokey(n, res, key);
				if (inserted) return true;
				if (*n == 0) {
					insidx = idx+1;
					break;
				}
			}
			if (insidx == -1) return false;
			auto vec = PxFunction::vecSplice(&tokens, insidx, tokens.size()-insidx);
			auto t = std::make_shared<StringToken>("");
			tokens.push_back(std::make_shared<KeyValueToken>(std::make_shared<StringToken>(key), t));
			*res = t.get();
			for (auto &i : vec) {
				tokens.push_back(i);
			}
			return true;
		}
		void rawkeys(std::set<std::string> *keys) override {
			for (auto &i : tokens) {
				i->rawkeys(keys);
			}
		}
		int rawcount() override {
			int c = 0;
			for (auto &i : tokens) {
				c += i->rawcount();
			}
			return c;
		}
		int rawcountkey(std::string key) override {
			int c = 0;
			for (auto &i : tokens) {
				c += i->rawcountkey(key);
			}
			return c;
		}
	};
	struct CommentToken : public Token {
		std::shared_ptr<Token> tkn;
		std::shared_ptr<Token> comment;
		CommentToken(std::shared_ptr<Token> Tkn, std::shared_ptr<Token> Comment) {
			tkn = Tkn;
			comment = Comment;
		}
		std::string get() override {
			return tkn->get() + "#" + comment->get();
		}
		void rawfind(int *n, StringToken **res) override {
			tkn->rawfind(n, res);
		}
		void rawfindkey(int *n, StringToken **res, std::string key) override {
			tkn->rawfindkey(n, res, key);
		}
		bool rawinsto(int *n, StringToken **res) override {
			return tkn->rawinsto(n, res);
		}
		bool rawinstokey(int *n, StringToken **res, std::string key) override {
			return tkn->rawinstokey(n, res, key);
		}
		void rawkeys(std::set<std::string> *keys) override {
			tkn->rawkeys(keys);
		}
		int rawcount() override {
			return tkn->rawcount();
		}
		int rawcountkey(std::string key) override {
			return tkn->rawcountkey(key);
		}
	};
	struct LinesToken : public ArrayToken {
		LinesToken(std::vector<std::shared_ptr<Token>> Tokens) : ArrayToken(Tokens) {}
		std::string get() override {
			std::vector<std::string> strings;
			for (auto &i : tokens) {
				strings.push_back(i->get());
			}
			return PxFunction::join(strings, "\n");
		}
	};

	std::shared_ptr<WhitespacedToken> xtrim(std::string input, std::string whitespace = " \n\t") {
		std::string ltrimmed = PxFunction::ltrim(input, whitespace);
		std::string ltrimming = input.substr(0, input.length()-ltrimmed.length());
		std::string rtrimmed = PxFunction::rtrim(ltrimmed, whitespace);
		std::string rtrimming = ltrimmed.substr(rtrimmed.length());
		return std::make_shared<WhitespacedToken>(ltrimming, std::make_shared<StringToken>(rtrimmed), rtrimming);
	}

	std::shared_ptr<Token> parseWritableLine(std::string input) {
		auto lines = PxFunction::split(input, "\n");
		if (lines.size() > 1) {
			std::vector<std::shared_ptr<Token>> lntoken;
			for (auto &i : lines) {
				lntoken.push_back(parseWritableLine(i));
			}
			return std::make_shared<LinesToken>(lntoken);
		}
		auto spl = PxFunction::split(input, "#", 1);
		if (spl.size() > 1) {
			return std::make_shared<CommentToken>(parseWritableLine(spl[0]), xtrim(spl[1]));
		}
		auto statement = spl[0];
		auto equ = PxFunction::split(statement, "=", 1);
		if (equ.size() > 1) {
			// TODO: escapes
			return std::make_shared<KeyValueToken>(xtrim(equ[0]), parseWritableLine(equ[1]));
		}
		auto val = equ[0];
		auto arr = PxFunction::split(val, ";");
		if (arr.size() > 1) {
			std::vector<std::shared_ptr<Token>> tokens;
			for (auto &i : arr) {
				tokens.push_back(parseWritableLine(i));
			}
			return std::make_shared<ArrayToken>(tokens);
		}
		return xtrim(arr[0]);
	}
}

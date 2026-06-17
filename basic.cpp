#include <cassert>
#include <iostream>
#include <sstream>
#include <string>

enum tokens : char {
	t_print = '?', t_run = 'r', t_num = '0', t_string = '"', t_int = '1',
	t_colon = ':', t_space = ' '
};

std::ostream* out { &std::cout };
std::string line;
std::string::const_iterator cur;
std::string::const_iterator end;
std::string err;

static inline bool is_direct_mode() {
	return cur == end || !isdigit(*cur);
}

bool matches(const std::string& kw) {
	bool result {
		end - cur >= kw.size() && std::equal(kw.begin(), kw.end(), cur)
	};
	if (result) { cur += kw.size(); }
	return result;
}

static inline std::string tokenize() {
	std::string result;
	while (cur != end) {
		if (*cur <= ' ') {
			result += t_space; ++cur;
		} else if (matches("print")) {
			result += t_print;
		} else if (*cur == '"') {
			result += t_string;
			++cur;
			while (cur != end && *cur != '"') {
				result += *cur++;
			}
			result += t_string;
			if (cur != end) { ++cur; }
		} else if (*cur == ':') {
			result += t_colon; ++cur;
		} else {
			err = "unknown line: "; for (; cur != end; ++cur) { err += *cur; }
		}
	}
	// std::cout << "tokenize: '" << result << "'\n";
	return result;
}

void do_print() {
	while (cur != end && *cur != t_colon) {
		switch (*cur) {
			case t_space: ++cur; break;
			case t_string: {
				++cur;
				while (cur != end && *cur != t_string) {
					*out << *cur; ++cur;
				}
				if (cur != end) { ++cur; }
				break;
			}
			default: err = "can't print"; cur = end; return;
		}
	}
	*out << "\n";
}

static inline void interpret() {
	while (cur != end) {
		switch (*cur) {
			case t_space: ++cur; continue;
			case t_print: ++cur; do_print(); break;
			case t_colon: break;
			default: err = "syntax error"; cur = end;
		}
		if (cur == end) { break; }
		if (*cur != t_colon) { err = "':' expected"; cur = end; break; }
		++cur;
	}
}

void run_direct(const std::string& source) {
	err = std::string { };
	line = source; cur = line.begin(); end = line.end();
	line = tokenize();
	if (err.empty()) {
		cur = line.begin(); end = line.end();
		interpret();
	}
}

void test_tokenizer(const std::string& source, const std::string& expected) {
	err = std::string { };
	line = source; cur = line.begin(); end = line.end();
	std::string got { tokenize() };
	if (! err.empty()) { std::cerr << "ERR: '" << err << "'\n"; }
	assert(err.empty());
	assert(got == expected);
}

static inline void tokenizer_tests() {
	test_tokenizer("", "");
	test_tokenizer("printprint", "??");
	test_tokenizer("\"ab", "\"ab\"");
	test_tokenizer("\"\"", "\"\"");
	test_tokenizer("\"", "\"\"");
	test_tokenizer(": :", ": :");
}

void run_test(const std::string& source, const std::string& expected) {
	std::ostringstream oss;
	out = &oss;
	run_direct(source);
	if (! err.empty()) { std::cerr << "ERR: '" << err << "'\n"; }
	assert(err.empty());
	assert(oss.str() == expected);
}

static inline void run_tests() {
	tokenizer_tests();
	run_test("print", "\n");
	run_test("print \"abc\"", "abc\n");
	run_test("print \"abc\" \"def\"", "abcdef\n");
	run_test("print \"a\": print\"b\"::", "a\nb\n");
	run_test("", "");
	run_test(":::", "");
}

int main() {
	run_tests();
	out = &std::cout;
	while (std::getline(std::cin, line)) {
		cur = line.begin(); end = line.end();
		if (is_direct_mode()) {
			run_direct(line);
			if (err.empty()) { std::cout << "ready.\n"; continue; }
			std::cerr << "?? " << err << "\n";
		} else {
			
		}
	}
}

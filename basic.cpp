#include <cassert>
#include <iostream>
#include <map>
#include <sstream>
#include <string>

enum tokens : char {
	t_print = '?', t_run = 'r', t_string = '"',
	t_colon = ':', t_space = ' ', t_rem = '\'', t_esc = '\\',
	t_lbracket = '(', t_rbracket = ')', t_plus = '+'
};

std::map<int, std::string> src;

std::ostream* out { &std::cout };
std::istream* in { &std::cin };
std::string line;
std::string::const_iterator cur;
std::string::const_iterator end;
std::string err;

static inline bool is_direct_mode() {
	return cur == end || !isdigit(*cur);
}

static void test_direct_mode(const std::string& source, bool expected) {
	line = source; cur = line.begin(); end = line.end();
	assert(is_direct_mode() == expected);
}

static inline void is_direct_mode_tests() {
	test_direct_mode("", true);
	test_direct_mode("print", true);
	test_direct_mode("10 print", false);
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
		} else if (matches("rem")) {
			result += t_rem;
		} else if (matches("run")) {
			result += t_run;
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
		} else if (*cur == '(') {
			result += t_lbracket; ++cur;
		} else if (*cur == ')') {
			result += t_rbracket; ++cur;
		} else if (*cur == '+') {
			result += t_plus; ++cur;
		} else if (isdigit(*cur)) {
			result += *cur++;
		} else {
			result += t_esc; result += *cur++;
		}
	}
	// std::cout << "tokenize: '" << result << "'\n";
	return result;
}

static std::string value;

static void do_expression();

static void do_factor() {
	while (cur != end && *cur == t_space) { ++cur; }
	if (cur == end || *cur == t_colon) { 
		err = "no expression"; cur = end; return;
	}
	switch (*cur) {
		case t_string: {
			value = std::string { };
			++cur;
			while (cur != end && *cur != t_string) {
				value += *cur++;
			}
			if (cur != end) { ++cur; }
			break;
		}
		case t_lbracket: {
			++cur;
			do_expression();
			if (cur != end && *cur == t_rbracket) {
				++cur;
			} else {
				err = "unmatched bracket"; cur = end; return;
			}
			break;
		}
		default: err = "no expression"; cur = end; return;
	}
}

static void do_term() {
	do_factor();
}

static void do_expression() {
	do_term();
	if (! err.empty()) { return; }
	for (;;) {
		while (cur != end && *cur == t_space) { ++cur; }
		switch (*cur) {
			case t_plus: {
				std::string first = value;
				++cur;
				do_term();
				if (! err.empty()) { return; }
				value = first + value;
				break;
			}
			default: return;
		}
	}
}

void do_print() {
	while (cur != end && *cur != t_colon) {
		do_expression();
		if (! err.empty()) { return; }
		*out << value;
	}
	*out << "\n";
}

static void interpret();

void do_run() {
	for (const auto& l : src) {
		line = l.second; cur = line.begin(); end = line.end();
		interpret();
	}
}

static void interpret() {
	while (cur != end) {
		switch (*cur) {
			case t_space: ++cur; continue;
			case t_print: ++cur; do_print(); break;
			case t_colon: break;
			case t_rem: cur = end; break;
			case t_run: ++cur; do_run(); cur = end; break;
			default: err = "syntax error: " + line; cur = end;
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

void run() {
	err = std::string { };
	for (;;) {
		if (!err.empty() || !std::getline(*in, line)) { break; }
		cur = line.begin(); end = line.end();
		if (is_direct_mode()) {
			run_direct(line);
		} else {
			int num = 0;
			while (cur != end && isdigit(*cur)) {
				num = num * 10 + *cur++ - '0';
			}
			while (cur != end && *cur <= ' ') { ++cur; }
			src[num] = tokenize();
		}
	}
	if (err.empty()) {
		*out << "ready.\n";
	} else {
		std::cerr << "?? " << err << "\n";
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
	test_tokenizer("rem abc", "' \\a\\b\\c");
	test_tokenizer("run", "r");
}

void run_test(const std::string& source, const std::string& expected) {
	std::ostringstream oss;
	out = &oss;
	std::istringstream iss { source };
	in = &iss;
	run();
	assert(err.empty());
	assert(oss.str() == expected + "ready.\n");
}

static inline void run_tests() {
	is_direct_mode_tests();
	tokenizer_tests();
	run_test("print", "\n");
	run_test("print \"abc\"", "abc\n");
	run_test("print \"abc\" \"def\"", "abcdef\n");
	run_test("print \"a\": print\"b\"::", "a\nb\n");
	run_test("", "");
	run_test(":::", "");
	run_test("rem abc", "");
	run_test("10 print \"a\"\nrun", "a\n");
	run_test("print (\"abc\")", "abc\n");
	run_test("print \"a\" + \"b\" + \"c\"", "abc\n");
}

int main() {
	run_tests();
	out = &std::cout;
	in = &std::cin;
	run();
}

#include <cassert>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <variant>

enum tokens : char {
	t_print = '?', t_run = 'r', t_string = '"', t_colon = ':', t_space = ' ',
	t_rem = '\'', t_esc = '\\', t_lbracket = '(', t_rbracket = ')',
	t_plus = '+', t_num = '#', t_minus = '-', t_times = '*', t_div = '/'
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

std::map<char, char> token_mapper {
	{ ':', t_colon }, { '(', t_lbracket }, { ')', t_rbracket },
	{ '+', t_plus }, { '-', t_minus }, { '*', t_times }, { '/', t_div }
};

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
		} else if (isdigit(*cur)) {
			result += t_num;
			while (cur != end && (isdigit(*cur) || *cur == '.')) {
				result += *cur++;
			}
		} else {
			auto got { token_mapper.find(*cur) };
			if (got != token_mapper.end()) {
				result += got->second;
			} else {
				result += t_esc; result += *cur;
			}
			++cur;
		}
	}
	// std::cout << "tokenize: '" << result << "'\n";
	return result;
}

using value_t = std::variant<std::string, double, int>;

static value_t value;

static void do_expression();

static inline bool is_numeric(const value_t& v = value) {
	return std::holds_alternative<double>(v) ||
		std::holds_alternative<int>(v);
}

static inline double get_numeric(const value_t& v = value) {
	if (std::holds_alternative<double>(v)) {
		return std::get<double>(v);
	} else if (std::holds_alternative<int>(v)) {
		return std::get<int>(v);
	} else { return 0.0/0.0; }
}

static void do_factor() {
	while (cur != end && *cur == t_space) { ++cur; }
	if (cur == end || *cur == t_colon) { 
		err = "no expression"; cur = end; return;
	}
	switch (*cur) {
		case t_minus: {
			++cur;
			do_factor();
			if (is_numeric()) {
				value = - get_numeric();
			} else {
				err = "invalid quantity to negate"; cur = end; return;
			}
			break;
		}
		case t_string: {
			std::string v;
			++cur;
			while (cur != end && *cur != t_string) {
				v += *cur++;
			}
			if (cur != end) { ++cur; }
			value = v;
			break;
		}
		case t_num: {
			std::string v;
			++cur;
			bool contains_dot { false };
			while (cur != end && (isdigit(*cur) || *cur == '.')) {
				v += *cur;
				if (*cur == '.') {
					if (contains_dot) { err = "multiple ."; cur = end; return; }
					contains_dot = true;
				}
				++cur;
			}
			if (contains_dot) {
				value = std::stod(v);
			} else {
				value = std::stoi(v);
			}
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

static void do_mult() {
	value_t first = value;
	++cur;
	do_factor();
	if (! err.empty()) { return; }
	if (is_numeric(first) && is_numeric()) {
		value = get_numeric(first) * get_numeric();
	} else {
		err = "wrong datatypes for *"; cur = end; return;
	}
}

static void do_div() {
	value_t first = value;
	++cur;
	do_factor();
	if (! err.empty()) { return; }
	if (is_numeric(first) && is_numeric()) {
		value = get_numeric(first) / get_numeric();
	} else {
		err = "wrong datatypes for /"; cur = end; return;
	}
}

static void do_term() {
	do_factor();
	while (cur != end) {
		switch (*cur) {
			case t_space: ++cur; break;
			case '*': do_mult(); break;
			case '/': do_div(); break;
			default: return;
		}
	}
}

static inline bool is_string(const value_t& v = value) {
	return std::holds_alternative<std::string>(v);
}

static inline const std::string& get_string(const value_t& v = value) {
	return std::get<std::string>(v);
}

static void do_plus() {
	value_t first = value;
	++cur;
	do_term();
	if (! err.empty()) { return; }
	if (is_string(first) && is_string()) {
		value = get_string(first) + get_string();
	} else if (is_numeric(first) && is_numeric()) {
		value = get_numeric(first) + get_numeric();
	} else {
		err = "wrong datatypes for +"; cur = end; return;
	}
}

static void do_minus() {
	value_t first = value;
	++cur;
	do_term();
	if (! err.empty()) { return; }
	if (is_numeric(first) && is_numeric()) {
		value = get_numeric(first) - get_numeric();
	} else {
		err = "wrong datatypes for -"; cur = end; return;
	}
}

static void do_expression() {
	do_term();
	if (! err.empty()) { return; }
	for (;;) {
		while (cur != end && *cur == t_space) { ++cur; }
		switch (*cur) {
			case t_plus: do_plus(); break;
			case t_minus: do_minus(); break;
			default: return;
		}
	}
}

void do_print() {
	while (cur != end && *cur != t_colon) {
		do_expression();
		if (! err.empty()) { return; }
		if (is_string()) {
			*out << get_string();
		} else if (is_numeric()) {
			double v { get_numeric() };
			if (v >= 0) { *out << ' '; }
			*out << v << ' ';
		} else { err = "can't print datatype"; cur = end; return; }
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
	run_test("print 10 20", " 10  20 \n");
	run_test("print 10.5 20.2", " 10.5  20.2 \n");
	run_test("print 3 + 5", " 8 \n");
	run_test("print -3", "-3 \n");
	run_test("print 4 - 10", "-6 \n");
	run_test("print 3 * 4", " 12 \n");
	run_test("print -3/2", "-1.5 \n");
	run_test("print 3 + 2 * 10", " 23 \n");
}

int main() {
	run_tests();
	out = &std::cout;
	in = &std::cin;
	run();
}

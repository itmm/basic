#include <cassert>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <variant>

enum tokens : char {
	t_print, t_run, t_rem
};

static_assert(t_rem < ' ');

std::map<int, std::string> src;

std::ostream* out { &std::cout };
std::istream* in { &std::cin };
std::string line;
std::string::const_iterator cur;
std::string::const_iterator end;
std::string err;

static inline bool is_direct_mode() {
	return cur >= end || !isdigit(*cur);
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
		end - cur >= (ssize_t) kw.size() && std::equal(kw.begin(), kw.end(), cur)
	};
	if (result) { cur += kw.size(); }
	return result;
}

static inline std::string tokenize() {
	std::string result;
	while (cur < end) {
		if (*cur <= ' ') {
			result += ' '; ++cur;
		} else if (matches("print")) {
			result += t_print;
		} else if (matches("rem")) {
			result += t_rem;
		} else if (matches("run")) {
			result += t_run;
		} else if (*cur == '"') {
			result += '"';
			++cur;
			while (cur < end && *cur != '"') {
				result += *cur++;
			}
			result += '"';
			if (cur < end) { ++cur; }
		} else if (isdigit(*cur) || *cur == '.') {
			while (cur < end && (isdigit(*cur) || *cur == '.')) {
				result += *cur++;
			}
		} else {
			result += *cur++;
		}
	}
	return result;
}

using value_t = std::variant<std::string, double>;

static value_t value;

std::map<std::string, value_t> vars;

struct Array {
		int size;
		std::unique_ptr<int[]> dimens;
		std::unique_ptr<value_t[]> elements;

		static int get_size(const std::vector<int>& ds) {
			int size = 1;
			for (int x : ds) { size *= x + 1; }
			return size;
		}

		Array(const std::vector<int>& ds): size { (int) ds.size() },
			dimens { std::make_unique<int[]>(ds.size()) },
			elements { std::make_unique<value_t[]>(get_size(ds)) }
		{
			for (int i { 0 }; i < (int) ds.size(); ++i) {
				dimens[i] = ds[i];
			}
		}
};

std::map<std::string, Array> arrays;

static void do_expression();

static inline bool is_numeric(const value_t& v = value) {
	return std::holds_alternative<double>(v);
}

static inline double get_numeric(const value_t& v = value) {
	return std::get<double>(v);
}

static inline bool is_string(const value_t& v = value) {
	return std::holds_alternative<std::string>(v);
}

static inline const std::string& get_string(const value_t& v = value) {
	return std::get<std::string>(v);
}

static std::pair<Array&, int> parse_array_expression(const std::string& name) {
	auto got { arrays.find(name) };
	if (got == arrays.end()) {
		std::vector<int> dflt = { 10 };
		got = arrays.insert({ name, Array(dflt) }).first;
	}
	Array& ary { got->second };
	int off = 0;
	for (int i = 0; i < ary.size; ++i) {
		while (cur < end && *cur == ' ') { ++cur; }
		if (cur >= end) { err = "end of line in array expression"; cur = end; return { ary, -1 }; }
		if (i == 0 && *cur != '(') { err = "'(' expected"; cur = end; return { ary, -1 }; }
		if (i > 0 && *cur != ',') { err = "',' expected"; cur = end; return { ary, -1 }; }
		++cur;
		do_expression();
		if (! is_numeric()) { err = "index expected"; cur = end; return { ary, -1 }; }
		int idx = (int) get_numeric();
		if (idx < 0 || idx >= ary.dimens[i]) { err = "out of bounds"; cur = end; return { ary, -1 }; }
		off = off * ary.dimens[i] + idx;
	}
	while (cur < end && *cur == ' ') { ++cur; }
	if (cur >= end || *cur != ')') { err = "')' expected"; cur = end; return { ary, -1 }; }
	++cur;
	return { ary, off };
}

static inline void do_array_lookup(const std::string& name) {
	auto ary { parse_array_expression(name) };
	if (ary.second >= 0) { value = ary.first.elements[ary.second]; }
}

static inline void do_array_assign(const std::string& name) {
	auto ary { parse_array_expression(name) };
	if (ary.second < 0) { return; }
	while (cur < end && *cur == ' ') { ++cur; }
	if (cur == end || *cur != '=') { err = "'=' expected"; cur = end; return; }
	++cur;
	do_expression();
	ary.first.elements[ary.second] = value;
}

static void do_factor() {
	while (cur < end && *cur == ' ') { ++cur; }
	if (cur >= end || *cur == ':') { 
		err = "no expression"; cur = end; return;
	}
	switch (*cur) {
		case '-': {
			++cur;
			do_factor();
			if (is_numeric()) {
				value = - get_numeric();
			} else {
				err = "invalid quantity to negate"; cur = end; return;
			}
			break;
		}
		case '"': {
			std::string v;
			++cur;
			while (cur < end && *cur != '"') {
				v += *cur++;
			}
			if (cur < end) { ++cur; }
			value = v;
			break;
		}
		case '0': case '1': case '2': case '3': case '4': case '5':
		case '6': case '7': case '8': case '9': case '.': {
			std::string v;
			bool contains_dot { false };
			while (cur < end && (isdigit(*cur) || *cur == '.')) {
				v += *cur;
				if (*cur == '.') {
					if (contains_dot) { err = "multiple ."; cur = end; return; }
					contains_dot = true;
				}
				++cur;
			}
			value = std::stod(v);
			break;
		}
		case '(': {
			++cur;
			do_expression();
			if (cur < end && *cur == ')') {
				++cur;
			} else {
				err = "unmatched bracket"; cur = end; return;
			}
			break;
		}
		default:
			if (isalpha(*cur)) {
				std::string name; while (cur < end && isalnum(*cur)) { name += *cur++; }
				if (cur < end && *cur == '$') { name += *cur++; }
				if (cur < end && *cur == '(') {
					do_array_lookup(name);
				} else { value = vars[name]; }
				break;
			}
			err = "no expression"; cur = end; return;
	}
}

static void do_binary_numeric(
	const value_t& first, const std::function<double(double, double)>& op
) {
	if (! err.empty()) { return; }
	if (is_numeric(first) && is_numeric()) {
		value = op(get_numeric(first), get_numeric());
	} else {
		err = "wrong datatypes"; cur = end;
	}
}

static void do_term() {
	do_factor();
	while (cur < end) {
		switch (*cur) {
			case ' ': ++cur; break;
			case '*': case '/': {
				value_t first = value;
				char op { *cur++ };
				do_factor();
				switch (op) {
					case '*':
						do_binary_numeric(
							first, [](double a, double b) { return a * b; }
						);
						break;
					case '/':
						do_binary_numeric(
							first, [](double a, double b) { return a / b; }
						);
						break;
				}
				break;
			}
			default: return;
		}
	}
}

static void do_expression() {
	do_term();
	while (cur < end) {
		switch (*cur) {
			case ' ': ++cur; break;
			case '+': case '-': {
				value_t first = value;
				char op = *cur++;
				do_term();
				switch (op) {
					case '+':
						if (! err.empty()) { return; }
						if (is_string(first) && is_string()) {
							value = get_string(first) + get_string();
						} else {
							do_binary_numeric(
								first, [](double a, double b) { return a + b; }
							);
						}
						break;
					case '-':
						do_binary_numeric(
							first, [](double a, double b) { return a - b; }
						);
						break;
				}
				break;
			}
			default: return;
		}
	}
}

void do_print() {
	while (cur < end && *cur != ':') {
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

static void do_assignment(const std::string& name) {
	++cur;
	do_expression();
	vars[name] = value;
}

static void interpret() {
	while (cur < end) {
		switch (*cur) {
			case t_print: ++cur; do_print(); break;
			case t_run: ++cur; do_run(); cur = end; break;
			case t_rem: cur = end; break;
			case ' ': ++cur; continue;
			case ':': break;
			default: 
				if (isalpha(*cur)) {
					std::string name;
					while (cur < end && isalnum(*cur)) { name += *cur++; }
					if (cur < end && *cur == '$') { name += *cur++; }
					if (cur < end && *cur == '(') {
						do_array_assign(name);
						break;
					} else {
						while (cur < end && *cur == ' ') { ++cur; }
						if (cur < end && *cur == '=') {
							do_assignment(name);
							break;
						}
					}
				}
				err = "syntax error: " + line; cur = end;
		}
		if (cur >= end) { break; }
		if (*cur != ':') { err = "':' expected"; cur = end; break; }
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
			while (cur < end && isdigit(*cur)) {
				num = num * 10 + *cur++ - '0';
			}
			while (cur < end && *cur <= ' ') { ++cur; }
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
	std::string exp; exp += t_print; exp += t_print;
	test_tokenizer("printprint", exp);
	test_tokenizer("\"ab", "\"ab\"");
	test_tokenizer("\"\"", "\"\"");
	test_tokenizer("\"", "\"\"");
	test_tokenizer(": :", ": :");
	exp = { }; exp += t_rem; exp += " abc";
	test_tokenizer("rem abc", exp);
	exp = { }; exp += t_run;
	test_tokenizer("run", exp);
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
	run_test("print 3 + 5 + 7", " 15 \n");
	run_test("print -3", "-3 \n");
	run_test("print 4 - 10", "-6 \n");
	run_test("print 2 * 3 * 4", " 24 \n");
	run_test("print -3/2", "-1.5 \n");
	run_test("print 3 + 2 * 10", " 23 \n");
	run_test("a = 10:print a + 5", " 15 \n");
	run_test("a$ = \"abc\":print a$", "abc\n");
	run_test("print a(3)", "\n");
	run_test("a(3)=7:print a(3)", " 7 \n");
}

int main() {
	run_tests();
	out = &std::cout;
	in = &std::cin;
	run();
}

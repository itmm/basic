#include <cassert>
#include <functional>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <variant>

std::map<int, std::string> src;

std::ostream* out { &std::cout };
std::istream* in { &std::cin };
std::string line;
std::string::const_iterator cur;
std::string::const_iterator end;
std::string err;

static void do_err(const char* file, int line, const std::string& msg) {
	err = std::string { };
	err += file; err += ":"; err += std::to_string(line);
	err += " "; err += msg;
	cur = end;
}

#define ERR(msg) do_err(__FILE__, __LINE__, msg)
#define EXP(what) do_err(__FILE__, __LINE__, what " expected")

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
	return std::string { cur, end };
}

using value_t = std::variant<nullptr_t, std::string, double>;

static value_t value;

std::map<std::string, value_t> vars;

static void do_expression();

static inline bool is_numeric(const value_t& v = value) {
	return ! std::holds_alternative<std::string>(v);
}

static inline double get_numeric(const value_t& v = value) {
	return std::holds_alternative<double>(v) ? std::get<double>(v) : 0.0;
}

static inline bool is_string(const value_t& v = value) {
	return ! std::holds_alternative<double>(v);
}

static inline std::string get_string(const value_t& v = value) {
	return std::holds_alternative<std::string>(v) ? std::get<std::string>(v) : "";
}

static void eat_space() { while (cur < end && *cur <= ' ') { ++cur; } }

static std::string parse_array_expression(std::string name) {
	bool first { true };
	for (;;) {
		eat_space();
		if (cur >= end) { ERR("end of line in array expression"); return { }; }
		if (first && *cur != '(') { EXP("'('"); return { }; }
		if (! first && *cur != ',') { EXP("','"); return { }; }
		++cur;
		do_expression();
		if (! is_numeric()) { EXP("index"); return { }; }
		int idx = (int) get_numeric();
		if (idx < 0) { ERR("out of bounds"); return { }; }
		name += '_' + std::to_string(idx);
		first = false;
		eat_space();
		if (cur >= end || *cur != ',') { break; }
	}
	if (cur >= end || *cur != ')') { EXP("')'"); return { }; }
	++cur;
	return name;
}

static inline void do_array_lookup(const std::string& name) {
	auto ary { parse_array_expression(name) };
	if (! ary.empty()) {
		value = vars[ary];
	}
}

static inline void do_array_assign(const std::string& name) {
	auto ary { parse_array_expression(name) };
	if (! ary.empty()) {
		eat_space();
		if (cur == end || *cur != '=') { EXP("'='"); return; }
		++cur;
		do_expression();
		vars[ary] = value;
	}
}

static std::string parse_ident() {
	eat_space();
	if (cur >= end || !isalpha(*cur)) { EXP("identifier"); return { }; }
	std::string name;
	while (cur < end && isalnum(*cur)) { name += *cur++; }
	if (cur < end && *cur == '$') { name += *cur++; }
	return name;
}

static void do_factor() {
	eat_space();
	if (cur >= end || *cur == ':') { ERR("no expression"); return; }
	switch (*cur) {
		case '-': {
			++cur;
			do_factor();
			if (is_numeric()) {
				value = - get_numeric();
			} else { ERR("invalid quantity to negate"); return; }
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
					if (contains_dot) { ERR("multiple ."); return; }
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
				ERR("unmatched parethesis"); return;
			}
			break;
		}
		default:
			if (isalpha(*cur)) {
				std::string name { parse_ident() };
				if (cur < end && *cur == '(') {
					do_array_lookup(name);
				} else {
					value = vars[name];
				}
				break;
			}
			ERR("no expression"); return;
	}
}

static void do_binary_numeric(
	const value_t& first, const std::function<double(double, double)>& op
) {
	if (! err.empty()) { return; }
	if (is_numeric(first) && is_numeric()) {
		value = op(get_numeric(first), get_numeric());
	} else { ERR("wrong datatypes"); }
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
		} else { ERR("can't print datatype"); return; }
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

static inline void do_list() {
	for (const auto& l : src) {
		*out << l.first << ' ' << l.second << '\n';
	}
}

static void interpret() {
	while (cur < end) {
		switch (*cur) {
			case ' ': ++cur; continue;
			case ':': break;
			default: 
				if (isalpha(*cur)) {
					if (matches("print")) {
						do_print(); break;
					} else if (matches("run")) {
						do_run(); cur = end; break;
					} else if (matches("rem")) {
						cur = end; break;
					} else if (matches("list")) {
						do_list(); break;
					} else {
						std::string name { parse_ident() };
						if (cur < end && *cur == '(') {
							do_array_assign(name);
							break;
						} else {
							eat_space();
							if (cur < end && *cur == '=') {
								do_assignment(name);
								break;
							}
						}
					}
				}
				ERR("syntax error: " + line);
		}
		if (cur >= end) { break; }
		if (*cur != ':') { EXP("':'"); break; }
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
	vars.clear();
	src.clear();
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
			eat_space();
			if (cur == end) {
				src.erase(num);
			} else {
				src[num] = tokenize();
			}
		}
	}
	if (err.empty()) {
		*out << "ready.\n";
	} else {
		std::cerr << "?? " << err << "\n";
	}
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
	run_test("a(3,2)=7:print a(3 , 2)", " 7 \n");
	run_test("print a + b", "\n");
	run_test("print a * b", " 0 \n");
	run_test("5 print 4\nlist\n", "5 print 4\n");
	run_test("run", "");
	run_test("10 print 1\n10\nlist", "");
}

int main() {
	run_tests();
	out = &std::cout;
	in = &std::cin;
	run();
}

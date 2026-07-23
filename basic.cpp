#include <cassert>
#include <functional>
#include <iostream>
#include <map>
#include <sstream>
#include <stack>
#include <string>
#include <variant>

static std::map<int, std::string> src;

static std::string err;

static std::map<int, std::string>::const_iterator cur_line;


class State {
		std::string _line;
		std::string::const_iterator _old_cur;
		std::string::const_iterator _old_end;
		std::map<int, std::string>::const_iterator _old_cur_line;

		static std::string::const_iterator _cur;
		static std::string::const_iterator _end;

	public:

		State(const std::map<int, std::string>::const_iterator& new_line):
			_line { }, _old_cur { _cur },
			_old_end { _end }, _old_cur_line { cur_line }
		{
			_cur = new_line->second.begin(); _end = new_line->second.end();
			cur_line = new_line;
		}

		State(const std::string& line):
			_line { line }, _old_cur { _cur }, _old_end { _end }, 
			_old_cur_line { cur_line }
		{
			_cur = _line.begin(); _end = _line.end();
		}

		void restore() {
			_cur = _old_cur; _end = _old_end; cur_line = _old_cur_line;
		}

		static bool is_finished() { return _cur >= _end; }
		static void finish_line() { _cur = _end; }
		static void eat_space() {
			while (! is_finished() && *_cur <= ' ') { ++_cur; }
		}
	
		static bool matches(char ch) { 
			bool got { ! is_finished() && *_cur == ch };
			if (got) { ++_cur; }
			return got;
		}

		static bool matches(const std::string& kw) {
			bool got {
				_end - _cur >= (ssize_t) kw.size() &&
					std::equal(kw.begin(), kw.end(), _cur)
			};
			if (got) { _cur += kw.size(); }
			return got;
		}

		static std::string get_rest() { return std::string { _cur, _end }; }
		static char cur() { return is_finished() ? '\0' : *_cur; }
		static void advance() { if (! is_finished()) { ++_cur; } }
};

std::string::const_iterator State::_cur;
std::string::const_iterator State::_end;

static void do_err(const char* file, int line, const std::string& msg) {
	err = std::string { };
	err += file; err += ":"; err += std::to_string(line);
	err += " "; err += msg;
	State::finish_line();
}

#define ERR(msg) do_err(__FILE__, __LINE__, msg)
#define EXP(what) do_err(__FILE__, __LINE__, what " expected")

std::stack<State> stack;

static void push_on_stack(
	const std::map<int, std::string>::const_iterator& new_line
) {
	stack.emplace(new_line);
}

static void pop_from_stack() {
	if (stack.empty()) { EXP("nonempty stack"); return; }
	stack.top().restore();
	stack.pop();
}

class Stack_Guard {
		State _safed_state;
	public:
		Stack_Guard(
			const std::map<int, std::string>::const_iterator& new_line
		): _safed_state { new_line } { }
		Stack_Guard(const std::string& line): _safed_state { line } { }
		~Stack_Guard() { _safed_state.restore(); }
};

static inline bool is_direct_mode() {
	return State::is_finished() || !isdigit(State::cur());
}

static void test_direct_mode(const std::string& source, bool expected) {
	Stack_Guard sg { source };
	assert(is_direct_mode() == expected);
}

static inline void is_direct_mode_tests() {
	test_direct_mode("", true);
	test_direct_mode("print", true);
	test_direct_mode("10 print", false);
}

static std::ostream* out { &std::cout };
static std::istream* in { &std::cin };

using value_t = std::variant<nullptr_t, std::string, double>;

static value_t value;

static std::map<std::string, value_t> vars;

static void do_expression();

static inline bool is_numeric(const value_t& v = value) {
	return std::holds_alternative<double>(v);
}

static inline bool is_string(const value_t& v = value) {
	return std::holds_alternative<std::string>(v);
}

static inline bool can_be_numeric(const value_t& v = value) {
	return ! is_string(v);
}

static inline double get_numeric(const value_t& v = value) {
	return std::holds_alternative<double>(v) ? std::get<double>(v) : 0.0;
}

static inline bool can_be_string(const value_t& v = value) {
	return ! is_numeric(v);
}

static inline std::string get_string(const value_t& v = value) {
	return std::holds_alternative<std::string>(v) ?
		std::get<std::string>(v) : "";
}

static inline std::string parse_array_expression(std::string name) {
	for (;;) {
		State::eat_space();
		if (State::is_finished()) {
			ERR("end of line in array expression"); return { };
		}
		do_expression();
		if (! can_be_numeric()) { EXP("index"); return { }; }
		int idx = (int) get_numeric();
		if (idx < 0) { ERR("out of bounds"); return { }; }
		name += '_' + std::to_string(idx);
		State::eat_space();
		if (! State::matches(',')) { break; }
	}
	if (! State::matches(')')) { EXP("')'"); return { }; }
	return name;
}

static std::string parse_ident() {
	State::eat_space();
	if (State::is_finished() || !isalpha(State::cur())) {
		EXP("identifier"); return { };
	}
	std::string name;
	while (! State::is_finished() && isalnum(State::cur())) {
		name += State::cur(); State::advance();
	}
	if (State::matches('$')) { name += '$'; }
	if (State::matches('(')) { name = parse_array_expression(name); }
	return name;
}

static void do_factor() {
	State::eat_space();
	if (State::is_finished() || State::cur() == ':') {
		ERR("no expression"); return;
	}
	switch (State::cur()) {
		case '-': {
			State::advance();
			do_factor();
			if (can_be_numeric()) {
				value = - get_numeric();
			} else { ERR("invalid quantity to negate"); return; }
			break;
		}
		case '"': {
			std::string v;
			State::advance();
			while (! State::is_finished() && State::cur() != '"') {
				v += State::cur(); State::advance();
			}
			if (! State::is_finished()) { State::advance(); }
			value = v;
			break;
		}
		case '0': case '1': case '2': case '3': case '4': case '5':
		case '6': case '7': case '8': case '9': case '.': {
			std::string v;
			bool contains_dot { false };
			while (! State::is_finished() && (isdigit(State::cur()) || State::cur() == '.')) {
				v += State::cur();
				if (State::cur() == '.') {
					if (contains_dot) { ERR("multiple ."); return; }
					contains_dot = true;
				}
				State::advance();
			}
			value = std::stod(v);
			break;
		}
		case '(': {
			State::advance();
			do_expression();
			if (! State::matches(')')) { ERR("unmatched parethesis"); return; }
			break;
		}
		default:
			if (isalpha(State::cur())) {
				std::string name { parse_ident() };
				value = vars[name];
				break;
			}
			ERR("no expression"); return;
	}
}

static void do_binary_numeric(
	const value_t& first, const std::function<double(double, double)>& op
) {
	if (! err.empty()) { return; }
	if (can_be_numeric(first) && can_be_numeric()) {
		value = op(get_numeric(first), get_numeric());
	} else { ERR("wrong datatypes"); }
}

static double cast_bool(bool v) { return v ? -1 : 0; }

static void do_bool_binary(
	const value_t& first,
	const std::function<bool(const std::string&, const std::string&)>& str_op,
	const std::function<bool(double, double)>& num_op
) {
	if (! err.empty()) { return; }
	if (can_be_string(first) && can_be_string()) {
		value = cast_bool(str_op(get_string(first), get_string()));
	} else if (can_be_numeric(first) && can_be_numeric()) {
		value = cast_bool(num_op(get_numeric(first), get_numeric()));
	} else { ERR("wrong datatypes"); }
}

static void do_term() {
	do_factor();
	while (! State::is_finished()) {
		switch (State::cur()) {
			case ' ': State::advance(); break;
			case '*': case '/': {
				value_t first = value;
				char op { State::cur() }; State::advance();
				do_factor();
				switch (op) {
					case '*':
						do_binary_numeric(
							first, [](auto a, auto b) { return a * b; }
						);
						break;
					case '/':
						do_binary_numeric(
							first, [](auto a, auto b) { return a / b; }
						);
						break;
				}
				break;
			}
			default: return;
		}
	}
}

static void do_simple_expression() {
	do_term();
	while (! State::is_finished()) {
		switch (State::cur()) {
			case ' ': State::advance(); break;
			case '+': case '-': {
				value_t first = value;
				char op = State::cur(); State::advance();
				do_term();
				switch (op) {
					case '+':
						if (! err.empty()) { return; }
						if (can_be_string(first) && can_be_string()) {
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

static void do_expression() {
	do_simple_expression();
	auto first = value;
	State::eat_space();
	if (! State::is_finished()) {
		switch (State::cur()) {
			case '<': {
				State::advance();
				if (State::matches('>')) {
					do_expression();
					do_bool_binary(
						first, [](const auto& a, const auto& b) { return a != b; },
						[](auto a, auto b) { return a != b; }
					);
				} else if (State::matches('=')) {
					do_expression();
					do_bool_binary(
						first, [](const auto& a, const auto& b) { return a <= b; },
						[](auto a, auto b) { return a <= b; }
					);
				} else {
					do_expression();
					do_bool_binary(
						first, [](const auto& a, const auto& b) { return a < b; },
						[](auto a, auto b) { return a < b; }
					);
				}
				break;
			}
			case '=': {
				State::advance();
				do_expression();
				do_bool_binary(
					first, [](const auto& a, const auto& b) { return a == b; },
					[](auto a, auto b) { return a == b; }
				);
				break;
			}
			case '>': {
				State::advance();
				if (State::matches('=')) {
					do_expression();
					do_bool_binary(
						first, [](const auto& a, const auto& b) { return a >= b; },
						[](auto a, auto b) { return a >= b; }
					);
				} else {
					do_expression();
					do_bool_binary(
						first, [](const auto& a, const auto& b) { return a > b; },
						[](double a, double b) { return a > b; }
					);
				}
				break;
			}
			default: break;
		}
	}
}

static inline void do_print() {
	bool last_was_semicolon { false };
	while (! State::is_finished() && State::cur() != ':') {
		while (State::matches(',')) { *out << '\t'; }
		do_expression();
		if (! err.empty()) { return; }
		if (can_be_string()) {
			*out << get_string();
		} else if (can_be_numeric()) {
			double v { get_numeric() };
			if (v >= 0) { *out << ' '; }
			*out << v << ' ';
		} else { ERR("can't print datatype"); return; }
		last_was_semicolon = false;
		if (State::matches(';')) { last_was_semicolon = true; }
		else if (! State::is_finished() &&
			State::cur() != ',' && State::cur() != ':'
		) {
			EXP("print separator"); return;
		}
	}
	if (! last_was_semicolon) { *out << "\n"; }
}

static void interpret();

static inline void do_run() {
	cur_line = src.begin();
	while (cur_line != src.end()) {
		State st { cur_line }; ++cur_line;
		interpret();
	}
}

static inline void do_assignment(const std::string& name) {
	do_expression();
	vars[name] = value;
}

static inline void do_list() {
	for (const auto& l : src) {
		*out << l.first << ' ' << l.second << '\n';
	}
}

static inline void do_if() {
	do_expression();
	bool is_true = false;
	if (can_be_numeric()) {
		is_true = get_numeric() != 0;
	}
	if (! State::matches("then")) { EXP("then"); return; }
	if (! is_true) { State::finish_line(); }
}

static inline void do_goto() {
	do_expression();
	if (can_be_numeric()) {
		State::finish_line();
		cur_line = src.find((int) get_numeric());
	} else { EXP("line number"); }
}

static inline void do_gosub() {
	do_expression();
	if (can_be_numeric()) {
		push_on_stack(src.find((int) get_numeric()));
		State::finish_line();
	} else { EXP("line number"); }
}

static inline void do_return() {
	if (! stack.empty()) {
		pop_from_stack();
	} else { ERR("can't return"); }
}

static inline void do_end() {
	State::finish_line();
	cur_line = src.end();
}

static inline void do_input() {
	char ch = ' ';
	for (;;) {
		while (ch > 0 && ch <= ' ' && ch != '\n') {
			if (! in->get(ch)) { ch = 0; }
		}
		std::string v;
		while (ch > ' ') {
			v += ch;
			if (! in->get(ch)) { ch = 0; }
		}
		if (ch == '\n') { ch = ' '; }
		std::string name { parse_ident() };
		if (is_numeric(vars[name])) {
			vars[name] = std::stod(v);
		} else {
			vars[name] = v;
		}
		State::eat_space();
		if (! State::matches(',')) { break; }
	}
}

static void do_ident() {
	std::string name { parse_ident() };
	State::eat_space();
	if (State::matches('=')) {
		do_assignment(name);
	} else { ERR("unknown keyword '" + name + "'"); }
}

static void interpret() {
	while (! State::is_finished()) {
		switch (State::cur()) {
			case ' ': State::advance(); continue;
			case ':': break;
			case 'c':
				if (State::matches("clr")) {
					vars.clear();
				} else { do_ident(); }
				break;
			case 'e':
				if (State::matches("end")) {
					do_end(); 
				} else { do_ident(); }
				break;
			case 'g':
				if (State::matches("gosub")) {
					do_gosub();
				} else if (State::matches("goto")) {
					do_goto();
				} else { do_ident(); }
				 break;
			case 'i':
				if (State::matches("if")) {
					do_if(); continue;
				} else if (State::matches("input")) {
					do_input();
				} else { do_ident(); }
				break;
			case 'l':
				if (State::matches("list")) {
					do_list();
				} else { do_ident(); }
				break;
			case 'n':
				if (State::matches("new")) {
					src.clear();
				} else { do_ident(); }
				break;
			case 'p':
				if (State::matches("print")) {
					do_print();
				} else { do_ident(); }
				break;
			case 'r':
				if (State::matches("rem")) {
					State::finish_line();
				} else if (State::matches("return")) {
					do_return();
				} else if (State::matches("run")) {
					do_run(); State::finish_line();
				} else { do_ident(); }
				break;
			default: 
				if (isalpha(State::cur())) {
					do_ident();
				} else {
					ERR("syntax error");
				}
				break;
		}
		if (State::is_finished()) { break; }
		if (! State::matches(':')) { EXP("':'"); break; }
	}
}

static inline void run_direct(const std::string& source) {
	err = std::string { };
	Stack_Guard sg { source };
	interpret();
}

static void run(std::istream& is, std::ostream& os) {
	in = &is; out = &os;
	vars.clear();
	src.clear();
	err = std::string { };
	std::string line;
	for (;;) {
		if (!err.empty() || !std::getline(*in, line)) { break; }
		Stack_Guard sg { line };
		if (is_direct_mode()) {
			run_direct(line);
		} else {
			int num = 0;
			while (! State::is_finished() && isdigit(State::cur())) {
				num = num * 10 + State::cur() - '0'; State::advance();
			}
			State::eat_space();
			if (State::is_finished()) {
				src.erase(num);
			} else {
				src[num] = State::get_rest();
			}
		}
	}
	if (err.empty()) {
		*out << "ready.\n";
	} else {
		std::cerr << "?? " << err << "\n";
	}
}

static void run_test(const std::string& source, const std::string& expected) {
	std::ostringstream oss;
	std::istringstream iss { source };
	run(iss, oss);
	assert(err.empty());
	// std::cerr << "{" << oss.str() << "}\n";
	assert(oss.str() == expected + "ready.\n");
}

static inline void run_tests() {
	is_direct_mode_tests();
	run_test("", "");
	run_test(":::", "");
	run_test("run", "");
	run_test("rem abc", "");
	run_test("print", "\n");
	run_test("print \"a\": print\"bc\"; \"def\"::", "a\nbcdef\n");
	run_test("print (\"abc\")", "abc\n");
	run_test("print \"a\" + \"b\" + \"c\"", "abc\n");
	run_test("print 10.5; 20.2", " 10.5  20.2 \n");
	run_test("print 3 + 5 + 7; 10", " 15  10 \n");
	run_test("print -3", "-3 \n");
	run_test("print 2 * 3 * 4", " 24 \n");
	run_test("print -3/2", "-1.5 \n");
	run_test("print 3 + 2 * 10", " 23 \n");
	run_test("a$ = \"abc\":print a$", "abc\n");
	run_test("print a(3)", "\n");
	run_test("a(3,2)=7:print a(3 , 2)", " 7 \n");
	run_test("print a + b", "\n");
	run_test("print a * b", " 0 \n");
	run_test("print 1, 2", " 1 \t 2 \n");
	run_test("print 1;", " 1 ");
	run_test(
		"5 print 4\n"
		"list",
		"5 print 4\n"
	);
	run_test(
		"10 print 1\n"
		"10\n"
		"list",
		""
	);
	run_test("a = 3: if a then print \"ok\"", "ok\n");
	run_test("a = 0: if a then print \"ok\"", "");
	run_test("a = 3: clr: print a * a", " 0 \n");
	run_test("10 print \"a\"\nrun", "a\n");
	run_test(
		"10 print a\n"
		"new\n"
		"list",
		""
	);
	run_test(
		"10 a=3: s=0\n"
		"20 if a then s = s + a: a = a - 1: goto 20\n"
		"30 print s\n"
		"run",
		" 6 \n"
	);
	run_test("print 3 < 2", " 0 \n");
	run_test("print 2 < 3", "-1 \n");
	run_test("print 2 < 2", " 0 \n");
	run_test("print 2 <= 2", "-1 \n");
	run_test("print 2 <> 2", " 0 \n");
	run_test("print 2 <> 3", "-1 \n");
	run_test("print 3 > 2", "-1 \n");
	run_test("print 2 > 3", " 0 \n");
	run_test("print 2 >= 3", " 0 \n");
	run_test("print 3 >= 2", "-1 \n");
	run_test("print 3 >= 3", "-1 \n");
	run_test("print \"abc\" < \"abb\"", " 0 \n");
	run_test("print \"abb\" < \"abc\"", "-1 \n");
	run_test("print \"abc\" < \"abc\"", " 0 \n");
	run_test("print \"abc\" <= \"abc\"", "-1 \n");
	run_test("print \"abc\" <> \"abc\"", " 0 \n");
	run_test("print \"abb\" <> \"abc\"", "-1 \n");
	run_test("print \"abc\" > \"abb\"", "-1 \n");
	run_test("print \"abb\" > \"abc\"", " 0 \n");
	run_test("print \"abb\" >= \"abc\"", " 0 \n");
	run_test("print \"abc\" >= \"abb\"", "-1 \n");
	run_test("print \"abc\" >= \"abc\"", "-1 \n");
	run_test("10 input a$(3): print a$(3)\nrun\nabc\n", "abc\n");
	run_test("10 input a$, b$: print b$; a$\nrun\nabc\ndef\n", "defabc\n");
	run_test("10 a = 0: input a: print a\nrun\n123\n", " 123 \n");
	run_test("10 end:print 42\nrun", "");

	run_test(
		"10 gosub 40\n"
		"20 print 12\n"
		"30 end\n"
		"40 print 11\n"
		"50 return\n"
		"run", " 11 \n 12 \n"
	);

	run_test("c = 1: print c", " 1 \n");
	run_test("e = 1: print e", " 1 \n");
	run_test("g = 1: print g", " 1 \n");
	run_test("i = 1: print i", " 1 \n");
	run_test("l = 1: print l", " 1 \n");
	run_test("n = 1: print n", " 1 \n");
	run_test("p = 1: print p", " 1 \n");
	run_test("r = 1: print r", " 1 \n");
}

int main() {
	run_tests();
	run(std::cin, std::cout);
}

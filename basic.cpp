#include <iostream>
#include <string>

enum tokens : char {
	t_print = '?', t_run = 'r', t_num = '0', t_string = '"', t_int = '1',
	t_colon = ':'
};

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
			++cur;
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
			case t_string: {
				++cur;
				while (cur != end && *cur != t_string) {
					std::cout << *cur; ++cur;
				}
				if (cur != end) { ++cur; }
				break;
			}
			default: err = "can't print"; cur = end; return;
		}
	}
	std::cout << "\n";
}

static inline void interpret() {
	while (cur != end) {
		switch (*cur) {
			case t_print: ++cur; do_print(); break;
			case t_colon: break;
			default: err = "?? syntax error"; cur = end;
		}
		if (cur == end) { break; }
		if (*cur != t_colon) { err = "':' expected"; cur = end; break; }
		++cur;
	}
}

int main() {
	std::string line;
	while (std::getline(std::cin, line)) {
		cur = line.begin(); end = line.end();
		if (is_direct_mode()) {
			err = std::string { };
			line = tokenize();
			if (! err.empty()) {
				std::cerr << "??? " << err << "\n"; continue;
			}
			cur = line.begin(); end = line.end();
			interpret();
			if (! err.empty()) {
				std::cerr << "?? " << err << "\n"; continue;
			}
			std::cout << "ready.\n";
		} else {
			
		}
	}
}

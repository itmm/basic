.PHONY: all clean

CXXFLAGS += -std=c++20

all: basic
	./basic </dev/null

clean:
	rm -f basic

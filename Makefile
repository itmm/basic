.PHONY: all clean

CXXFLAGS += -std=c++20 -Wall

all: basic
	./basic </dev/null

clean:
	rm -f basic

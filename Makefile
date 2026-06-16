.PHONY: all clean

all: basic
	./basic </dev/null

clean:
	rm -f basic

CXX ?= c++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -pedantic

SRC := $(wildcard src/*.cpp)
HDR := $(wildcard src/*.hpp)

build/campaign: $(SRC) $(HDR)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -o $@ $(SRC)

run: build/campaign
	./build/campaign

clean:
	rm -rf build

.PHONY: run clean

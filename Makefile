# Windows: mingw32-make, Linux: make
CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++17 -I. -g
GTEST = googletest/googletest
GTEST_FLAGS = -pthread -I$(GTEST)/include -I$(GTEST)

HEADERS = $(wildcard *.h *.tpp sequence/*.h sequence/*.tpp)

ifeq ($(OS),Windows_NT)
BUILD = build/windows
MKDIR = if not exist $(subst /,\,$(BUILD)) mkdir $(subst /,\,$(BUILD)) # из PowerShell рецепты идут через cmd, там нет mkdir -p
LDFLAGS = -static # иначе из PATH может подхватиться чужая libstdc++-6.dll
BENCH_LIBS = -lpsapi # замер памяти процесса
else
BUILD = build/linux
MKDIR = mkdir -p $(BUILD)
endif

GTEST_OBJ = $(BUILD)/gtest-all.o $(BUILD)/gtest_main.o

all: ptr_tests sequence_tests benchmark

$(BUILD)/gtest%.o: $(GTEST)/src/gtest%.cc
	$(MKDIR)
	$(CXX) $(CXXFLAGS) $(GTEST_FLAGS) -c $< -o $@

ptr_tests: tests/ptr_tests.cpp $(GTEST_OBJ) $(HEADERS)
	$(CXX) $(CXXFLAGS) $(GTEST_FLAGS) tests/ptr_tests.cpp $(GTEST_OBJ) -o $@ $(LDFLAGS)

sequence_tests: tests/sequence_tests.cpp $(GTEST_OBJ) $(HEADERS)
	$(CXX) $(CXXFLAGS) $(GTEST_FLAGS) tests/sequence_tests.cpp $(GTEST_OBJ) -o $@ $(LDFLAGS)

benchmark: benchmarks/benchmark.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -O2 -DNDEBUG benchmarks/benchmark.cpp -o $@ $(LDFLAGS) $(BENCH_LIBS)

check: ptr_tests sequence_tests
	./ptr_tests
	./sequence_tests

VALGRIND = valgrind --leak-check=full --show-leak-kinds=all \
           --errors-for-leak-kinds=definite,indirect,possible --track-origins=yes --error-exitcode=1

valgrind: ptr_tests sequence_tests
	$(VALGRIND) ./ptr_tests --gtest_filter=-*ImpossibleSize*:WeakPtrThreadTest.*
	$(VALGRIND) ./sequence_tests

clean:
	rm -rf build ptr_tests sequence_tests benchmark *.exe

.PHONY: all check valgrind clean

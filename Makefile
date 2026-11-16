# Windows: mingw32-make, Linux: make
CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++17 -I. -g
GTEST = googletest/googletest
GTEST_FLAGS = -pthread -I$(GTEST)/include -I$(GTEST)

HEADERS = $(wildcard *.h *.tpp sequence/*.h sequence/*.tpp support/*.h)

ALLOC = support/alloc_counter.cpp

ifeq ($(OS),Windows_NT)
BUILD = build/windows
MKDIR = if not exist $(subst /,\,$(BUILD)) mkdir $(subst /,\,$(BUILD)) # из PowerShell рецепты идут через cmd, там нет mkdir -p
LDFLAGS = -static # иначе из PATH может подхватиться чужая libstdc++-6.dll
BENCH_LIBS = -lpsapi # замер памяти процесса
else
BUILD = build/linux
MKDIR = mkdir -p $(BUILD)
endif

TEST_PARTS = tests/leak_check.cpp $(ALLOC) $(BUILD)/gtest-all.o

all: ptr_tests sequence_tests benchmark

$(BUILD)/gtest-all.o: $(GTEST)/src/gtest-all.cc
	$(MKDIR)
	$(CXX) $(CXXFLAGS) $(GTEST_FLAGS) -c $< -o $@

ptr_tests: tests/ptr_tests.cpp $(TEST_PARTS) $(HEADERS)
	$(CXX) $(CXXFLAGS) $(GTEST_FLAGS) tests/ptr_tests.cpp $(TEST_PARTS) -o $@ $(LDFLAGS)

sequence_tests: tests/sequence_tests.cpp $(TEST_PARTS) $(HEADERS)
	$(CXX) $(CXXFLAGS) $(GTEST_FLAGS) tests/sequence_tests.cpp $(TEST_PARTS) -o $@ $(LDFLAGS)

benchmark: benchmarks/benchmark.cpp $(ALLOC) $(HEADERS)
	$(CXX) $(CXXFLAGS) -O2 -DNDEBUG -DALLOC_COUNTER_SINGLE_THREAD benchmarks/benchmark.cpp $(ALLOC) -o $@ $(LDFLAGS) $(BENCH_LIBS)

check: ptr_tests sequence_tests
	./ptr_tests
	./sequence_tests

VALGRIND = valgrind --soname-synonyms=somalloc=nouserintercepts --leak-check=full --show-leak-kinds=all \
           --errors-for-leak-kinds=definite,indirect,possible --track-origins=yes --error-exitcode=1

valgrind: ptr_tests sequence_tests
	$(VALGRIND) ./ptr_tests --gtest_filter=-*ImpossibleSize*:WeakPtrThreadTest.*
	$(VALGRIND) ./sequence_tests

clean:
	rm -rf build ptr_tests sequence_tests benchmark *.exe

.PHONY: all check valgrind clean

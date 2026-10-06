CC = g++
CFLAGS = -Wall -Wextra -std=c++17 -I.
GTEST_DIR = googletest/googletest
GTEST_FLAGS = -I$(GTEST_DIR)/include -I$(GTEST_DIR)
PTR_FILES = unq_ptr.h unq_ptr.tpp shrd_ptr.h shrd_ptr.tpp
SEQ_FILES = $(wildcard sequence/*.h sequence/*.tpp)
VALGRIND_DIR = build/valgrind
VALGRIND_FLAGS = --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=definite,indirect,possible --track-origins=yes --error-exitcode=1
ifeq ($(OS),Windows_NT)
BENCH_LIBS = -lpsapi
endif

ptr_tests: tests/ptr_tests.cpp $(PTR_FILES) $(GTEST_DIR)/src/gtest-all.cc $(GTEST_DIR)/src/gtest_main.cc
	$(CC) $(CFLAGS) $(GTEST_FLAGS) tests/ptr_tests.cpp $(GTEST_DIR)/src/gtest-all.cc $(GTEST_DIR)/src/gtest_main.cc -o ptr_tests

sequence_tests: tests/sequence_tests.cpp $(PTR_FILES) $(SEQ_FILES) $(GTEST_DIR)/src/gtest-all.cc $(GTEST_DIR)/src/gtest_main.cc
	$(CC) $(CFLAGS) $(GTEST_FLAGS) tests/sequence_tests.cpp $(GTEST_DIR)/src/gtest-all.cc $(GTEST_DIR)/src/gtest_main.cc -o sequence_tests

benchmark: benchmarks/benchmark.cpp $(PTR_FILES)
	$(CC) $(CFLAGS) -O2 -DNDEBUG benchmarks/benchmark.cpp -o benchmark $(BENCH_LIBS)

$(VALGRIND_DIR):
	mkdir -p $(VALGRIND_DIR)

$(VALGRIND_DIR)/ptr_tests: tests/ptr_tests.cpp $(PTR_FILES) $(GTEST_DIR)/src/gtest-all.cc $(GTEST_DIR)/src/gtest_main.cc | $(VALGRIND_DIR)
	$(CC) $(CFLAGS) -g -O0 -pthread $(GTEST_FLAGS) tests/ptr_tests.cpp $(GTEST_DIR)/src/gtest-all.cc $(GTEST_DIR)/src/gtest_main.cc -o $@

$(VALGRIND_DIR)/sequence_tests: tests/sequence_tests.cpp $(PTR_FILES) $(SEQ_FILES) $(GTEST_DIR)/src/gtest-all.cc $(GTEST_DIR)/src/gtest_main.cc | $(VALGRIND_DIR)
	$(CC) $(CFLAGS) -g -O0 -pthread $(GTEST_FLAGS) tests/sequence_tests.cpp $(GTEST_DIR)/src/gtest-all.cc $(GTEST_DIR)/src/gtest_main.cc -o $@

$(VALGRIND_DIR)/benchmark: benchmarks/benchmark.cpp $(PTR_FILES) | $(VALGRIND_DIR)
	$(CC) $(CFLAGS) -g -O0 benchmarks/benchmark.cpp -o $@

valgrind: $(VALGRIND_DIR)/ptr_tests $(VALGRIND_DIR)/sequence_tests $(VALGRIND_DIR)/benchmark
	valgrind $(VALGRIND_FLAGS) $(VALGRIND_DIR)/ptr_tests
	valgrind $(VALGRIND_FLAGS) $(VALGRIND_DIR)/sequence_tests
	set -e; for scenario in owners clones descriptors; do for variant in raw std custom; do \
		valgrind $(VALGRIND_FLAGS) $(VALGRIND_DIR)/benchmark $$scenario $$variant 1000; \
	done; done

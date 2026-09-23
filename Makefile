# mingw32-make для сборки на Windows
# make для сборки на Linux
CC = g++
CFLAGS = -Wall -Wextra -std=c++17 -I.
GTEST_DIR ?= ../../LR-2/googletest/googletest
GTEST_FLAGS = -I$(GTEST_DIR)/include -I$(GTEST_DIR)
PTR_FILES = unq_ptr.h unq_ptr.tpp shrd_ptr.h shrd_ptr.tpp
SEQ_FILES = $(wildcard sequence/*.h sequence/*.tpp)

check: decentral_tests sequence_tests
	./decentral_tests
	./sequence_tests

decentral_tests: tests/decentral_ptr_tests.cpp $(PTR_FILES) $(GTEST_DIR)/src/gtest-all.cc $(GTEST_DIR)/src/gtest_main.cc
	$(CC) $(CFLAGS) $(GTEST_FLAGS) tests/decentral_ptr_tests.cpp $(GTEST_DIR)/src/gtest-all.cc $(GTEST_DIR)/src/gtest_main.cc -o decentral_tests

sequence_tests: tests/sequence_tests.cpp $(PTR_FILES) $(SEQ_FILES) $(GTEST_DIR)/src/gtest-all.cc $(GTEST_DIR)/src/gtest_main.cc
	$(CC) $(CFLAGS) $(GTEST_FLAGS) tests/sequence_tests.cpp $(GTEST_DIR)/src/gtest-all.cc $(GTEST_DIR)/src/gtest_main.cc -o sequence_tests

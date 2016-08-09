
all: atomic_data_test.exe atomic_list.exe atomic_map.exe atomic_vector.exe vector_of_atomic.exe

OPTS = -U__STRICT_ANSI__ -static -Wall -std=c++14 -O2 -mtune=pentium4 -msse2
CC = g++

%.exe : %.cpp atomic_data.h atomic_data_mutex.h makefile
	$(CC) $(OPTS) -o $@ $<

atomic_list.exe : atomic_list.cpp atomic_data.h atomic_data_mutex.h atomic_list.h makefile
	$(CC) $(OPTS) -o $@ $<

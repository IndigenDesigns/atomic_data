
#Note: exe extensions and __STRICT_ANSI__ - are for MinGW on Windows, should be fine on Linux

all: atomic_data_test.exe atomic_map.exe atomic_vector.exe vector_of_atomic.exe atomic_list.exe


OPTS = -D_ISOC99_SOURCE -Wall -march=native -std=c++14 -O2 -msse2 -ffast-math -static
CC = g++


%.exe : %.cpp atomic_data.h atomic_data_mutex.h makefile
	$(CC) $(OPTS) -o $@ $<


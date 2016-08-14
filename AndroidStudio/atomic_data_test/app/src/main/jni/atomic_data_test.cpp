/*

atomic_data: A Multibyte General Purpose Lock-Free Data Structure

This is a test that was used to test atomic_data.
The task is to find the minimum value in an array and increment it.
After some number of iterations we check that every array element is the same
and no increments are lost. This kind of test was very helpful in
testing on an ARM smartphone.

Here we compare the timings with a pure mutex approach.

License: Public-domain Software.

Blog post: http://alexpolt.github.io/atomic-data.html
Alexandr Poltavsky

*/

#include <cstdio>
#include <chrono>
#include <random>

#if defined(ATOMIC_DATA_ARM)
  char const cpu[] = "ARM";
#elif defined(ATOMIC_DATA_X86)
  char const cpu[] = "X86";
#elif defined(ATOMIC_DATA_MIPS)
  char const cpu[] = "MIPS";
#else
  char const cpu[] = "Unknown";
#endif

#include <android/log.h>
char atomic_data_log_buffer[1024];
char *atomic_data_log_ptr;
extern "C" char *atomic_data_log() { return atomic_data_log_buffer; }
#define printf( ... ) atomic_data_log_ptr+=std::sprintf(atomic_data_log_ptr,__VA_ARGS__); __android_log_print( ANDROID_LOG_FATAL, "atomic_data", __VA_ARGS__ )


#include "atomic_data.h"
#include "atomic_data_mutex.h"

namespace {

  using uint = unsigned;

  //edit to change the test setup
  //total number of increments/array cell = iterations * threads_size / array_size
  //we check at the end that all array elements are equal that value
  //read_iterations is to vary reading load

  const uint array_size = 64;
  const uint iterations = 81920;
  const uint threads_size = 8;
  const uint read_iterations = 20;

  static_assert( ( iterations*threads_size/array_size ) * array_size == iterations * threads_size , 
                  "ERROR: iterations * threads_size / array_size is not a whole number, "
                  "please correct the numbers for it to be evenly divisible " );

  //test data structure
  struct array_test {
    uint data[ array_size ];
  };

  //for testing exception safety
  bool flag_throw = false;

}


//Test Update
//look up the minimum value and increment it
bool update( array_test *array_new ) {

  uint min = -1;
  size_t min_index = 0;

  for( size_t i = 0; i < array_size; i++ ) {
    if( array_new->data[ i ] < min ) {
      min = array_new->data[ i ];
      min_index = i;
    }
  }

  //test exception safety
  if( flag_throw && array_new->data[ min_index ] == 10 ) {
    flag_throw = false;
    throw 1;
  }

  array_new->data[ min_index ]++;

  //signal that we are ok for the update
  return true;
}



//Test Read
//look up the minimum value and store in a dummy global
volatile uint min_global = -1;

void read( array_test *array ) {
  volatile uint min = -1;
  for( size_t t = 0; t < read_iterations; t++ ) {
    for( size_t i = 0; i < array_size; i++ )
      min = min <= array->data[ i ] ? min : array->data[ i ];
    min_global = min;
  }
}

template< typename T > void test_atomic_data( T& array0 );

extern "C" void atomic_data_test() {

  //reset log pointer
  atomic_data_log_ptr = atomic_data_log_buffer;

  //an instance of atomic_data
  atomic_data<array_test, threads_size * 2> atomic_array{ new array_test{} };

  //test copy/move/assign
  auto atomic_array_copy = atomic_array;
  auto atomic_array_move = (decltype(atomic_array)&&) atomic_array_copy;
  atomic_array_move = atomic_array;

  //and an instance of atomic_data_mutex to compare perfomance
  atomic_data_mutex<array_test> atomic_array_mutex{ new array_test{} };

  printf( "Test parameters:\n\tCPU: %d core(s)\n\tarray size: %d\n\titerations: %d\n\tthreads: %d\n\tread iterations: %d\n\tIncrements/array cell: %d\n",
    std::thread::hardware_concurrency(), array_size, iterations, threads_size, read_iterations, iterations * threads_size / array_size );

  printf( "\nstart testing atomic_data\n" );
  test_atomic_data( atomic_array );

  printf( "\nstart testing atomic_data_mutex\n" );
  test_atomic_data( atomic_array_mutex );

}

//test function 
//creates thread_size threads with fn functor as a parameter and calcs the time
template< typename T >
void test_atomic_data( T& array0 ) {

  auto fn = [ &array0 ]() {
    size_t i = 0;
    while( i++ < iterations ) {
      try {
        if( i % 3 == 0 ) { array0.update( update ); array0.read( read ); }
        else { array0.read( read ); array0.update( update ); }
      }
      catch( ... ) { printf( "Got a test exception. Try again...\n" ); --i; }
    }

  };

  printf( "start threads (%d update/read iterations)\n", iterations * 8 );

  auto start = std::chrono::high_resolution_clock::now();

  std::thread threads[ threads_size ];
  for( auto& thread : threads ) thread = std::thread{ fn };
  for( auto& thread : threads ) thread.join();

  uint time = (uint) std::chrono::duration_cast<std::chrono::milliseconds>( std::chrono::high_resolution_clock::now() - start ).count();
  printf( "time = %u\n", time );

  uint value_check = iterations * threads_size / array_size;

  printf( "check that array elements are all equal %d: ", value_check );

  bool r = array0.read( [value_check]( array_test* array0 ){ 
    for( uint i = 0; i < array_size; i++ ) {
      if( value_check != array0->data[ i ] ) {
        printf( "failed! data[%u] = %d\n", i, array0->data[ i ] );
        return false;
      }
    }
    return true;
  } );

  if( r ) {
    printf( "Passed!\n" );
  }

}


/*

atomic_data can wrap containers and turn them into concurrent data structures.
Wrapping std::map is going to work but will be much slower on updates than simply 
using a mutex (a lot of memory allocations). But using std::vector is a more
viable option since it can reuse memory on copy.

This test is the same as the atomic_data_test (the one with array and increments),
except that here we use a std::vector as the data type for atomic_data.
Threads look up the minimum value and increment it.

License: Public-domain Software.

Blog post: http://alexpolt.github.io/atomic-data.html
Alexandr Poltavsky

*/


#include <cstdio>
#include <chrono>
#include <vector>

#include "atomic_data.h"
#include "atomic_data_mutex.h"

namespace {

  //edit to change the test setup
  //total number of iterations = iterations * threads_size / array_size
  //read_iterations is vary reading load
  using uint = unsigned;
  using atomic_vector_t = std::vector<uint>;
  const uint array_size = 256;
  const uint iterations = 8192;
  const uint threads_size = 8;
  const uint read_iterations = 20;

  //for testing exception safety
  bool flag_throw = false;

}


//Test Update
//look up the minimum value and increment it
bool update( atomic_vector_t *vector_new ) {

  uint min = -1;
  size_t min_index = 0;

  for( size_t i = 0; i < array_size; i++ ) {
    if( (*vector_new)[ i ] < min ) {
      min = (*vector_new)[ i ];
      min_index = i;
    }
  }

  //test exception safety
  if( flag_throw && (*vector_new)[ min_index ] == 10 ) {
    flag_throw = false;
    throw 1;
  }

  (*vector_new)[ min_index ]++;

  //signal that we are ok for the update
  return true;
}


//Test Read
//look up the minimum value and store in a dummy global
volatile uint min_global = -1;

void read( atomic_vector_t *vector ) {
  volatile uint min = -1;
  for( size_t t = 0; t < read_iterations; t++ ) {
    for( size_t i = 0; i < array_size; i++ )
      min = min <= (*vector)[ i ] ? min : (*vector)[ i ];
    min_global = min;
  }
}

template< typename T > void test_atomic_vector( T& );

int main() {

  uint iterations_per_cell = iterations * threads_size / array_size;
  if( iterations_per_cell * array_size != iterations * threads_size ) {
    printf( "iterations * threads_size / array_size = %.2f - not a whole number\n", float(iterations) * threads_size / array_size );
    printf( "please correct the numbers for it to be evenly divisible\n" );
    printf( "press enter\n" );
    getchar();
    return 1;
  }

  //an instance of atomic_data
  atomic_data<atomic_vector_t, threads_size * 2> atomic_vector{ new atomic_vector_t( array_size ) };

  //test copy/move/assign
  auto atomic_vector_copy = atomic_vector;
  auto atomic_vector_move = (decltype(atomic_vector)&&) atomic_vector_copy;
  atomic_vector_move = atomic_vector;

  //and instance of atomic_data_mutex to compare perfomance
  atomic_data_mutex<atomic_vector_t> atomic_vector_mutex{ new atomic_vector_t( array_size ) };

  printf( "Test parameters:\n\tCPU: %d core(s)\n\tarray size: %d\n\titerations: %d\n\tthreads: %d\n\tread iterations: %d\n\tIncrements/array cell: %d\n",
    std::thread::hardware_concurrency(), array_size, iterations, threads_size, read_iterations, iterations * threads_size / array_size );

  printf( "\nstart testing atomic_vector\n" );
  test_atomic_vector( atomic_vector );

  printf( "\nstart testing atomic_vector_mutex\n" );
  test_atomic_vector( atomic_vector_mutex );

  printf("\npress enter\n");
  getchar();

}

//test function 
//creates thread_size threads with fn functor as a parameter and calcs the time
template< typename T >
void test_atomic_vector( T& atomic_vector ) {

  auto fn = [ &atomic_vector ]() {
    size_t i = 0;
    while( i++ < iterations ) {
      try {
        if( i % 3 == 0 ) { atomic_vector.update( update ); atomic_vector.read( read ); }
        else { atomic_vector.read( read ); atomic_vector.update( update ); }
      }
      catch( ... ) { printf( "Got a test exception. Try again...\n" ); --i; }
    }

  };

  //clear array
  for( auto &i : *atomic_vector ) i = 0;

  printf( "start threads (%d update/read iterations)\n", iterations * 8 );

  auto start = std::chrono::high_resolution_clock::now();

  std::thread threads[ threads_size ];
  for( auto& thread : threads ) thread = std::thread{ fn };
  for( auto& thread : threads ) thread.join();

  uint time = (uint) std::chrono::duration_cast<std::chrono::milliseconds>( std::chrono::high_resolution_clock::now() - start ).count();
  printf( "time = %u\n", time );

  uint value_check = iterations * threads_size / array_size;

  printf( "check that array elements are all equal %d: ", value_check );

  for( uint i = 0; i < array_size; i++ ) {
    if( value_check != (*atomic_vector)[ i ] ) {
      printf( "failed! data[%u] = %d\n", i, (*atomic_vector)[ i ] );
      value_check = 0;
      break;
    }
  }

  if( value_check != 0 ) {
    printf( "Passed!\n" );
  }


}


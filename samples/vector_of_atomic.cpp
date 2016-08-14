/*

atomic_data can be used as a container element. 
Here we use a vector of atomic_data. 
Threads increment a random element of the vector.
At the end we sum all elements and compare it to the number of iterations * threads_size.
Then sort and print out.

License: Public-domain Software.

Blog post: http://alexpolt.github.io/atomic-data.html
Alexandr Poltavsky

*/


#include <cstdio>
#include <vector>
#include <thread>
#include <random>
#include <algorithm>
#include <chrono>

#include "atomic_data.h"

using uint = unsigned;
const uint threads_size = 8;
const uint iterations = 81290;
const uint vector_size = 16;
const uint total = iterations * threads_size;


int main() {

  printf( "start testing vector of atomic_data<int,%u>\n", threads_size*2 );

  std::vector< atomic_data<uint, threads_size*2> > vector0{ vector_size };

  auto fn = [&vector0]() {
    auto seed = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    std::mt19937_64 gen0( seed );
    std::uniform_int_distribution<uint> engine0{0, vector_size-1};

    for( uint i = 0; i < iterations; i++ ){
      uint index = engine0( gen0 );
      vector0[ index ].update( []( uint* data ) {
        (*data)++;
        //for fun
        std::this_thread::yield();
        return true;
      } );
    }
  };

  printf( "starting %u threads\n", threads_size );

  std::thread threads[ threads_size ];
  for( auto& thread : threads ) thread = std::thread{ fn };
  for( auto& thread : threads ) thread.join();

  printf( "checking that the sum of all elements equals %u: ", total );

  uint sum{};

  for( auto &i : vector0 ) sum += i.read( []( uint *data ){ return *data; } );

  printf( sum == total ? "passed!\n" : "failed!\n" );

  printf( "sorting and printing\n" );

  std::sort( begin( vector0 ), end( vector0 ) );

  for( auto &i : vector0 ) printf( "%u ", i.read( []( uint *data ){ return *data; } ) );

  printf( "\ndone\n" );


}


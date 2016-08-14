/*

Here we use an atomic_data< std::map<int, int> >.
Threads use their id to access and increment their map locations.

License: Public-domain Software.

Blog post: http://alexpolt.github.io/atomic-data.html
Alexandr Poltavsky

*/


#include <cstdio>
#include <chrono>
#include <map>

#include "atomic_data.h"
#include "atomic_data_mutex.h"

namespace {

  using uint = unsigned;
  using map = std::map<uint, uint>;

  //edit to change the test setup
  const uint cycles_update = 102400;
  const uint cycles_read = 819200;
  const uint threads_size = 8;
  volatile uint global_dummy;

}

template< typename T > void test_atomic_map( T& );

int main() {

  //an instance of atomic_data<map>
  atomic_data<map, threads_size*2> atomic_map{};

  //the same using mutex
  atomic_data_mutex<map> atomic_map_mutex{};

  printf( "Test parameters:\n\t CPU: %d core(s)\n\t update iterations: %d\n\t read iterations: %d\n\t threads: %d\n\t",
    std::thread::hardware_concurrency(), cycles_update, cycles_read, threads_size );

  printf( "\nstart testing atomic_map\n" );
  test_atomic_map( atomic_map );

  printf( "\nstart testing atomic_map_mutex\n" );
  test_atomic_map( atomic_map_mutex );

  printf("\npress enter\n");
  getchar();

}

//test function 
//creates thread_size threads with a functor as an argument and calcs the time
template< typename T >
void test_atomic_map( T& atomic_map ) {

  auto update = [ &atomic_map ]( uint thread_id ) {

    auto fn = [=]( map* data ) {
      auto i = data->find( thread_id );
      if( i != data->end() ) {
        (*i).second++;
      } else {
        data->emplace( thread_id, 1 );
      }
      return true;
    };

    size_t i = 0;
    while( i++ < cycles_update ) { 
      atomic_map.update( fn ); 
      std::this_thread::yield();
    }
  };


  auto read = [ &atomic_map ]( uint thread_id ) {

    auto fn = [=]( map* data ) {
      auto i = data->find( thread_id );
      if( i != data->end() ) {
        return (*i).second;
      }
      return 0u;
    };

    size_t i = 0;
    while( i++ < cycles_read ) { 
      if( i % 100 == 0 ) std::this_thread::yield();
      global_dummy = atomic_map.read( fn ); 
    }
  };

  //clear the map
  atomic_map.update( []( map* map0 ){
    map0->clear();
    return true;
  });

  printf( "start %d threads\n", threads_size );

  auto start = std::chrono::high_resolution_clock::now();

  std::thread threads[ threads_size ];

  for( uint i = 0; i < threads_size; i+=2 ) {
    threads[ i ] = std::thread{ update, i/2 };
    threads[ i + 1 ]  = std::thread{ read, i/2 };
  }

  for( auto& thread : threads ) thread.join();

  uint time = (uint) std::chrono::duration_cast<std::chrono::milliseconds>( std::chrono::high_resolution_clock::now() - start ).count();
  printf( "time = %u\n", time );

  printf( "check # of increments = %d\n\n", cycles_update );

  atomic_map.read( []( map* map0 ){ 
    for( auto& i : *map0 ) {
      printf( "thread %d -> %d increments\n", i.first, i.second );
    }
  });

}

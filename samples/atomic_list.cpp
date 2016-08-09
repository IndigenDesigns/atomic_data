/*

A linked list using atomic_data. A certain number of elements are pre-inserted into the list.
Then threads perform equal number of insertions and deletions.
If the implementation is correct than at the end the list should be the same size.

For lock-free lists we have to deal with the deletion problem. For this we employ a lock
on the to be deleted node. For testing purposes we set the lock on one of the preinserted elements.
At the end we print out the list and expect that element to remain in the list.

License: Public-domain Software.

Blog post: http://alexpolt.github.io/atomic-data.html
Alexandr Poltavsky

*/


#include <cstdio>
#include <thread>
#include <random>
#include <chrono>

#include "atomic_list.h"


using uint = unsigned;
const uint threads_size = 8;
const uint iterations = 8192;
const uint list_size = 15;


int main() {

  printf( "Test parameters:\n\t CPU: %d core(s)\n\t list size: %d\n\t iterations: %d\n\t threads: %d\n\n",
    std::thread::hardware_concurrency(), list_size, threads_size*iterations, threads_size );

  printf( "start testing atomic_list<int>\n\n" );

  using atomic_list_t = atomic_list<int, threads_size*2>;

  //create an instance of atomic_list
  atomic_list_t atomic_list0;

  //used for generating values for insertion
  std::atomic<uint> counter{ list_size };

  //populate the list with list_size members
  //after test insertions/removals we will check that the size is still list_size
  for( uint i = 0; i < list_size; i++ ) {
    atomic_list_t::iterator it = atomic_list0.insert_weak( i );
    if( i == 3 ) (*it)->lock = true;
  }

  printf( "list before test (the first 0 is the head node):\n" );

  for( auto& node : atomic_list0 ) {
    if( node->lock  )
      printf( "(%d,%s) ", node->data, "locked");
    else
      printf( "%d ", node->data );
  }

  printf( "= *%d* elements\n\n", atomic_list0.size() );

  //insertions
  auto fn_insert = [ &atomic_list0, &counter ]() {

    //generate random position
    std::uniform_int_distribution<uint> engine0{0, list_size+list_size/2};
    auto seed = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    std::mt19937 gen0( seed );

    for( uint i = 0; i < iterations; i++ ){

      uint value = counter.fetch_add(1, std::memory_order_relaxed);

      //do insert in a loop because we might try to insert at a locked node
      do {
        uint index = engine0( gen0 );
        auto it = atomic_list0.begin();
        auto it_next = it;
        for( uint i = 0; i < index && it_next; it = it_next++, ++i );
        auto r = atomic_list0.insert_weak( it, value );
        if( r ) {
          //printf( "insert at %d value %d\n", index, value );
          break;
        }
      } while( true );

      std::this_thread::yield();
    }
  };

  //deletions
  auto fn_remove = [ &atomic_list0 ]() {

    //generate random position
    std::uniform_int_distribution<uint> engine0{0, list_size+list_size/2};
    auto seed = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    std::mt19937 gen0( seed );

    for( uint i = 0; i < iterations; i++ ){

      //do insert in a loop because we might try to remove a locked node
      do {
        uint index = engine0( gen0 );
        auto it = atomic_list0.begin();
        auto it_next = it;
        for( uint i = 0; i < index && it_next; it = it_next++, ++i );
        auto r = atomic_list0.remove_weak( it );
        if( r ) {
          //printf( "remove at %d value %d\n", index, (*r)->data );
          break;
        }
      } while( true );

      std::this_thread::yield();
    }
  };

  printf( "starting %u threads\n\n", threads_size );

  std::thread threads[ threads_size ];

  for( uint i = 0; i < threads_size; i++ ) 
    threads[ i ] = i % 2 == 0 ? std::thread{ fn_insert } : std::thread{ fn_remove };

  for( auto& thread : threads ) thread.join();


  printf("list after test (the first 0 is the head node):\n");

  for( auto& node : atomic_list0 ) {
    if( node->lock  )
      printf( "(%d,%s) ", node->data, "locked");
    else
      printf( "%d ", node->data );
  }

  printf( "= *%d* elements\n\n", atomic_list0.size() );

  printf( "done\n" );

}

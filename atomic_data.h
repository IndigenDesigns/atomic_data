
/*
atomic_data: A Multibyte General Purpose Lock-Free Data Structure

Blog post: http://alexpolt.github.io/atomic-data.html

Alexandr Poltavsky
*/


#pragma once


#include <atomic>
#include <thread>

/*
Template parameters:
  T0 - data type
  N0 - size of the queue
*/

template<typename T0, unsigned N0> 
struct atomic_data {


  //Default Constructor:
  //  object - initial current data, gets deleted in ~atomic_data
  atomic_data( T0* object ) {

    data.store( object );

    //preallocate queue elements
    for( unsigned i = 0; i < queue_size; i++ ) queue[i] = new T0;

    //issue a memory barrier
    std::atomic_thread_fence( std::memory_order_release );
  }

  ~atomic_data() {
    //not exception safe, be accurate
    delete data.load();
    for( unsigned i = 0; i < queue_size; i++ ) delete queue[i];
  }

  //Read method:
  //  fn0 - a functor that accepts a pointer of type T0* (checked by SFINAE)
  //  returns the result of the functor invocation
  template<typename U0>
  auto read( U0 fn ) -> decltype( fn( (T0*) nullptr ) ) {

    while( no_reading.load() ) //a writer signals on hitting a sync barrier
            std::this_thread::yield();

    usage_counter_guard counter{}; //increment/decrement of usage_counter with RAII

    return fn( data.load() );
  }

  //Update
  template<typename U0>
  auto update( U0 fn ) -> decltype( fn( (T0*) nullptr ), (void) 0 ) {

    sync(); //checking for the sync barrier

    usage_counter_guard counter{};

    //allocate an element from the queue
    auto idx = left.add(1) % array_size;

    T0* current_data = nullptr;

    do { //CAS loop

      auto current_data = data.load( std::memory_order_acquire ); //aquire

      *queue[idx] = *current_data; //copy

      fn( queue[idx] ); //update

    } while( data.compare_exchange_weak( current_data, queue[idx], std::memory_order_release ) );

    //return current element to the queue
    idx = right.add(1) % array_size;
    queue[idx] = current_data;

    //if that was the last store for the queue to become full - issue a fence,
    //so that threads can use elements again after the sync barrier
    if( idx % queue_size == 0 )
        std::atomic_thread_fence( std::memory_order_release );

    //~usage_counter_guard()
  }

  
  //Logic for the synchronization barrier
  void sync() {

    //check if it's time for the sync
    if( left.load() % queue_size == 0 ) {
      
      no_reading.store(1); //signal to readers

      //it might seem that there is a race between no_reading and usage_counter
      //but no_reading is just a benign signal, usage_counter is most important
      
      while( usage_counter.load() != 0 )
              std::this_thread::yield();
   
      no_reading.store(0);
    }
  }


  //Helper class to wrap std::atomic with relaxed loads and stores
  template<typename U0>
  class atomic {

    U0 add( U0 value, std::memory_order order = std::memory_order_relaxed ) { return data.fetch_add( value, order ); }
    U0 sub( U0 value, std::memory_order order = std::memory_order_relaxed ) { return data.fetch_sub( value, order ); }

    void store( U0 value, std::memory_order order = std::memory_order_relaxed ) { data.store( value, order ); }
    U0 load( std::memory_order order = std::memory_order_relaxed ) { return data.load( order ); }

    std::atomic<U0> data;
  };

  //RAII for usage_counter
  class usage_counter_guard {
     usage_counter_guard() { usage_counter.add(1); }
    ~usage_counter_guard() { usage_counter.sub(1); }
  };


  static const unsigned queue_size = N0;
  static const unsigned array_size = 2 * N0;

  //note array_size=2*N0: we use double the size to avoid having to zero out cells
  //also not atomic because sync barrier and aquire/release help to avoid races
  static T0* queue[ array_size ]; 

  //pointer to actual data
  atomic<T0*> data{};

  //pointers into the queue
  //relaxed atomic increments and modulus are used to get a position
  static atomic<int> left;
  static atomic<int> right;

  //usage counter is used upon reaching synchronization barrier
  static atomic<int> usage_counter;

  //a singnal to readers used during sync
  static atomic<int> no_reading;

};

template<typename T0, unsigned N0> T0* atomic_data<T0,N0>::queue[array_size]{};
template<typename T0, unsigned N0> atomic_data<T0,N0>::atomic<int> atomic_data<T0,N0>::left{};
template<typename T0, unsigned N0> atomic_data<T0,N0>::atomic<int> atomic_data<T0,N0>::right{queue_size};
template<typename T0, unsigned N0> atomic_data<T0,N0>::atomic<int> atomic_data<T0,N0>::usage_counter{};
template<typename T0, unsigned N0> atomic_data<T0,N0>::atomic<int> atomic_data<T0,N0>::no_reading{};



/*
atomic_data: A Multibyte General Purpose Lock-Free Data Structure

Blog post: http://alexpolt.github.io/atomic-data.html

Alexandr Poltavsky
*/

#include <cstdio>

#include <atomic>
#include <thread>
#include <chrono>

using namespace std::chrono_literals;


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
    while( counter_usage.load() != 0 )
            std::this_thread::sleep_for(1ms);

    //not exception safe, be accurate
    delete data.load();
    for( unsigned i = left.load(); i < queue_size; i++ ) delete queue[i];
  }

  //Read method:
  //  fn0 - a functor that accepts a pointer of type T0* (checked by SFINAE)
  //  returns the result of the functor invocation
  template<typename U0>
  auto read( U0 fn ) -> decltype( fn( (T0*) nullptr ) ) {

    while( no_reading.load() ) //a writer signals on hitting a sync barrier
            std::this_thread::yield();

    counter_guard counter{counter_usage}; //increment/decrement of counter_usage with RAII

    return fn( data.load() );
  }

  //Update
  template<typename U0>
  auto update( U0 fn ) -> decltype( fn( (T0*) nullptr ), (void) 0 ) {
    while( ! update_weak( fn ) );
  }

  //Update weak
  template<typename U0>
  bool update_weak( U0 fn ) {

    auto left_old = left.load();

    if( ! sync( left_old ) ) return false; //checking for the sync barrier

    if( left_old == right.load() ) return false; //queue is empty

    counter_guard counter{counter_usage};

    //allocate an element from the queue
    if( ! left.compare_exchange_weak( left_old, left_old+1, std::memory_order_relaxed ) ) return false;

    //update loop
    T0 *data_old = data.load(), *data_new = queue[left_old % array_size];
    do {

      std::atomic_thread_fence( std::memory_order_acquire );

      *data_new = *data_old; //copy

      fn( data_new ); //update

      std::atomic_thread_fence( std::memory_order_release );

    } while( ! data.compare_exchange_weak( data_old, data_new, std::memory_order_relaxed ) );

    //return current element to the queue
    auto idx = right.add(1) % array_size;

    queue[idx] = data_old;

    //if that was the last store for the queue to become full - issue a fence,
    //so that threads can use elements again after the sync barrier
    if( idx % queue_size == 0 )
        std::atomic_thread_fence( std::memory_order_release );

    return true;
    //~counter_guard()
  }

  
  //Logic for the synchronization barrier
  bool sync(int left) {

    //check if it's time for the sync
    if( left % queue_size == 0 ) {

      counter_guard counter{no_reading}; //signal to readers

      //it might seem that there is a race between no_reading and counter_usage
      //but no_reading is just a benign signal, counter_usage is most important
      
      if( counter_usage.load() == 0 ) return true;

      counter_sync.add(1); //for stats

      std::this_thread::sleep_for(1ms);   

      return false;
    }

    return true;
  }

	//call to get the number of wait events at the sync barrier
  int sync_stat() { return counter_sync.load(); }

  //Helper class to wrap std::atomic with relaxed loads and stores
  template<typename U0>
  struct atomic {

    U0 add( U0 value, std::memory_order order = std::memory_order_relaxed ) { return data.fetch_add( value, order ); }
    U0 sub( U0 value, std::memory_order order = std::memory_order_relaxed ) { return data.fetch_sub( value, order ); }

    void store( U0 value, std::memory_order order = std::memory_order_relaxed ) { data.store( value, order ); }
    U0 load( std::memory_order order = std::memory_order_relaxed ) { return data.load( order ); }
    
	bool compare_exchange_weak( U0& expected, U0 desired, std::memory_order order = std::memory_order_seq_cst ) {
		return data.compare_exchange_weak( expected, desired, order );
	}
	
    std::atomic<U0> data;
  };

  //RAII for counter_usage
  struct  counter_guard {
     counter_guard( atomic<int>& counter_ ) : counter{counter_} { counter.add(1); }
    ~counter_guard() { counter.sub(1); }
    atomic<int>& counter;
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
  static atomic<int> counter_usage;

  //for statistics
  static atomic<int> counter_sync;

  //a singnal to readers used during sync
  static atomic<int> no_reading;

};

template<typename T0, unsigned N0> T0* atomic_data<T0,N0>::queue[array_size]{};
template<typename T0, unsigned N0> atomic_data<T0,N0>::atomic<int> atomic_data<T0,N0>::left{};
template<typename T0, unsigned N0> atomic_data<T0,N0>::atomic<int> atomic_data<T0,N0>::right{{queue_size}};
template<typename T0, unsigned N0> atomic_data<T0,N0>::atomic<int> atomic_data<T0,N0>::counter_usage{};
template<typename T0, unsigned N0> atomic_data<T0,N0>::atomic<int> atomic_data<T0,N0>::counter_sync{};
template<typename T0, unsigned N0> atomic_data<T0,N0>::atomic<int> atomic_data<T0,N0>::no_reading{};


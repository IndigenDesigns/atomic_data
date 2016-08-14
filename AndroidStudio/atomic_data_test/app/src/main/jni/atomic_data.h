#pragma once
/*

atomic_data: A Multibyte General Purpose Lock-Free Data Structure

API:

to create an instance:

  - atomic_data< data_type, queue_size = 8 >
  where queue_size = 2 * number of threads is usually enough (8 is by default)
  synchronization happens once in a queue_size allocations from the queue

methods:

  - void update( F )
  - bool update_weak ( F )
  where F - a functor which accepts a pointer to data type and returns a bool on ok/not ok to perform actual update
  update calls update_weak in a loop, update is not reentrant (due to a sync barrier), update_weak is reentrant
  on success returns true, false otherwise, in particular if you return false from your functor the method will also fail

  - auto read( F )
  where F - a functor which accepts a pointer to data type and returns the return value of the functor or void

  - data_type* operator->() = delete;
  - data_type& operator*() = delete;
  getting the raw pointer to wrapped data is undesired because it changes on every update call

License: Public-domain Software.

More is in the blog post: http://alexpolt.github.io/atomic-data.html
Alexandr Poltavsky
*/

#include <atomic>
#include <thread>

template< typename T0, unsigned N0 = 8 >
struct atomic_data {

  static_assert( N0 != 0 && ( N0 & ( N0 - 1 ) ) == 0, "Queue size must be a power of two!" );

  using uint = unsigned;

  //Default Constructor
  atomic_data( T0* object = new T0{ } ) {
    data = object;
    init.dummy_call();
  }

  //Copy Constructor.
  atomic_data( atomic_data const& r ) {
    T0 *object = r.read( []( T0* object ) { return new T0( *object ); } );
    data = object;
  }

  //Move Constructor. Not Thread Safe.
  atomic_data( atomic_data&& r ) noexcept {
    data = r.data.load();
    r.data = nullptr;
  }

  //Copy Assigment Operator.
  atomic_data& operator=( atomic_data const& r ) {
    T0 *object = r.read( []( T0* object ) { return new T0( *object ); } );
    this->~atomic_data();
    data = object;
    return *this;
  }

  //Move Assigment Operator. Not Thread Safe.
  atomic_data& operator=( atomic_data&& r ) {
    data = r.data.load();
    r.data = nullptr;
    return *this;
  }

  //Destructor. Not Thread Safe.
  ~atomic_data() noexcept {
    delete data.load();
  }


  //Initialization on Program Load
  struct init_static {

    init_static() {
      //preallocate queue elements
      for( uint i = 0; i < queue_size; i++ ) queue[ i ] = new T0;
      right = queue_size;
    }

    ~init_static() {
      for( uint i = left.load(), end = right.load(); i < end; i++ ) {
        delete queue[ i % array_size ];
      }
    }

    //for assuring static initialization
    void dummy_call() {}
  };


  //Read Method
  //fn - a functor which accepts a pointer to data type and returns the return value of the functor or void
  template< typename U0 >
  auto read( U0 fn ) const -> decltype( fn( ( T0* ) nullptr ) ) {
    counter_guard counter{ };
    return fn( data.load() );
  }

  //Update Method
  //where F - a functor which accepts a pointer to data type and returns a bool on ok / not ok to perform actual update
  //update calls update_weak in a loop, update is not reentrant
  template< typename U0 >
  auto update( U0 fn ) -> decltype( fn( ( T0* ) nullptr ), (void) 0 ) {
    while( ! update_weak( fn ) );
  }

  //Update Weak
  template< typename U0 >
  bool update_weak( U0 fn ) {

    auto queue_left = left.load();
    auto queue_right = right.load();

    //if the queue is full, back out
    if( queue_left == queue_right ) {
      yield();
      return false;
    }

    if( !check_barrier( queue_left, queue_right ) ) return false;

    //allocate an element from the queue using CAS
    //we need CAS to not miss the sync barrier
    if( !left.compare_exchange_weak( queue_left, queue_left + 1 ) ) return false;

    //read
    T0 *data_new = queue[ queue_left % array_size ];
    T0 *data_old = data.load();

    //on update failure or exception returns data_new back to the queue
    deallocate_guard dalloc{ data_new };

    //copy
    *data_new = *data_old;

    //update
    if( ! fn( data_new ) ) return false;

    //unrequired on X86 but essential on ARM and other weakly ordered CPUS
    std::atomic_thread_fence( std::memory_order_release );

    //publish
    //when a thread reads data the above fence makes sure that stores before are already in memory
    if( ! data.compare_exchange_weak( data_old, data_new ) ) return false;

    dalloc.reset( data_old );

    return true;
  }


  //Logic for the Synchronization Barrier
  bool check_barrier( uint queue_left, uint queue_right ) const {

    bool is_barrier = ( queue_left % queue_size ) == 0;

    if( is_barrier ) {

      //first make sure all elements are back in the queue
      if( queue_right - queue_left < queue_size ) {
        yield();
        return false;
      }

      //wait for the usage counter to become zero
      if( counter_usage.is_used( queue_right ) ) {
        yield();
        return false;
      }

    }

    return true;
  }

  //Thread Yield
  void yield() const {
    std::this_thread::yield();
  }

  //We don't normally want users to get to the pointer because the pointer changes on every update.
  T0 const* operator->() const = delete;
  T0 const& operator*() const = delete;

  //Helper Class with Relaxed Loads and Stores by Default
  template< typename U0 >
  struct atomic_t {

    U0 add( U0 value, std::memory_order order = std::memory_order_relaxed ) { return data.fetch_add( value, order ); }
    U0 sub( U0 value, std::memory_order order = std::memory_order_relaxed ) { return data.fetch_sub( value, order ); }

    void store( U0 value, std::memory_order order = std::memory_order_relaxed ) { data.store( value, order ); }
    U0 load( std::memory_order order = std::memory_order_relaxed ) const { return data.load( order ); }

    U0 exchange( U0 desired, std::memory_order order = std::memory_order_relaxed ) { return data.exchange( desired, order ); }

    bool compare_exchange_weak( U0& expected, U0 desired, std::memory_order order = std::memory_order_relaxed ) {
      return data.compare_exchange_weak( expected, desired, order );
    }

    void operator=( U0 value ) { store( value ); }

    std::atomic<U0> data;
  };

  using atomic = atomic_t<uint>;

  //Usage Tracking
  //uses two counters that are swapped when the sync barrier is reached
  //it allows for readers continue reading and be wait-free
  struct counter_t {

    uint get() { return index.load(); }
    void inc( uint queue_right ) { counters[ counter_index( queue_right ) ].add( 1 ); }
    void dec( uint queue_right ) { counters[ counter_index( queue_right ) ].sub( 1 ); }
    bool is_used( uint queue_right ) { return counters[ 1 - counter_index( queue_right ) ].load() > 0; }

    //based on the queue right pointer get the right counter
    uint counter_index( uint queue_right ) {
      //queue is 2*N size (see comments at the end)
      //ech part of the queue uses its own counter
      return  ( queue_right % array_size ) < queue_size;
    }

    atomic index;
    atomic counters[ 2 ];
  };

  //Usage Counter RAII Helper 
  struct  counter_guard {
    counter_guard() : queue_right{ right.load() } {
      counter_usage.inc( queue_right );
    }
    ~counter_guard() {
      counter_usage.dec( queue_right );
    }
    uint queue_right;
  };

  //Helper to Return an Allocated Element to the Queue
  struct deallocate_guard {

    deallocate_guard( T0* data_ ) : data{ data_ }  { }

      ~deallocate_guard() {
      //returning to the queue is just and atomic inc
      queue[ right.add( 1 ) % array_size ] = data;

      //unrequired on X86 but essential on ARM and other weakly ordered CPUS
      std::atomic_thread_fence( std::memory_order_release );
    }

    void reset( T0* data_ ) {
      data = data_;
    }

    //we need the counter to wait at the sync barrier for the final store to the queue to finish
    counter_guard counter{ };
    T0 *data;
  };

  static const uint queue_size = N0;
  static const uint array_size = 2 * N0;

  //note array_size = 2 * queue_size: we use double the size which makes implementing the queue a lot easier
  //also not atomic thanks to the sync barrier
  static T0* queue[ array_size ];

  //pointer to current data
  atomic_t<T0*> data;

  //pointers into the queue
  //relaxed atomic increments and modulus are used to get a position
  static atomic left;
  static atomic right;

  //usage counter used to wait by writers of other threads during the synchronization period
  static counter_t counter_usage;

  //dummy variable for static initialization
  static init_static init;

};

//c++ rules make static data a single instance per data type
//it means there is a single backing queue for a data type
template< typename T0, unsigned N0 > T0* atomic_data<T0, N0>::queue[ array_size ];
template< typename T0, unsigned N0 > typename atomic_data<T0, N0>::atomic atomic_data<T0, N0>::left;
template< typename T0, unsigned N0 > typename atomic_data<T0, N0>::atomic atomic_data<T0, N0>::right;
template< typename T0, unsigned N0 > typename atomic_data<T0, N0>::counter_t atomic_data<T0, N0>::counter_usage;
template< typename T0, unsigned N0 > typename atomic_data<T0, N0>::init_static atomic_data<T0, N0>::init;

//comparison operators, makes it possible to use atomic_data in standard containers
template< typename T0, unsigned N0 > bool operator==(const atomic_data<T0, N0>& lhs, const atomic_data<T0, N0>& rhs){ return *lhs.data.load() == *rhs.data.load(); }
template< typename T0, unsigned N0 > bool operator!=(const atomic_data<T0, N0>& lhs, const atomic_data<T0, N0>& rhs){ return !operator==(lhs,rhs); }
template< typename T0, unsigned N0 > bool operator< (const atomic_data<T0, N0>& lhs, const atomic_data<T0, N0>& rhs){ return *lhs.data.load() < *rhs.data.load(); }
template< typename T0, unsigned N0 > bool operator> (const atomic_data<T0, N0>& lhs, const atomic_data<T0, N0>& rhs){ return  operator< (rhs,lhs); }
template< typename T0, unsigned N0 > bool operator<=(const atomic_data<T0, N0>& lhs, const atomic_data<T0, N0>& rhs){ return !operator> (lhs,rhs); }
template< typename T0, unsigned N0 > bool operator>=(const atomic_data<T0, N0>& lhs, const atomic_data<T0, N0>& rhs){ return !operator< (lhs,rhs); }


#pragma once

/*
This is just a version of atomic_data using a mutex for testing purposes.

More is in the blog post: http://alexpolt.github.io/atomic-data.html
Alexandr Poltavsky

License: Public-domain Software.

*/

#include <mutex>


template<typename T0, unsigned N0 = 0>
struct atomic_data_mutex {

  using uint = unsigned;

  atomic_data_mutex( T0* object = new T0{ } ) { data = object; }
  atomic_data_mutex( atomic_data_mutex const& r ) = delete;
  atomic_data_mutex( atomic_data_mutex&& r ) = delete;
  atomic_data_mutex& operator=( atomic_data_mutex const& r ) = delete;
  atomic_data_mutex& operator=( atomic_data_mutex&& r ) = delete;
  ~atomic_data_mutex() { delete data; }

  //Read Method:
  //  fn0: a functor that accepts a pointer of type T0* (checked by SFINAE)
  //  returns: the result of the functor invocation
  template<typename U0>
  auto read( U0 fn ) const -> decltype( fn( ( T0* ) nullptr ) ) {
    std::lock_guard<std::mutex> lock_guard{ lock };
    return fn( data );
  }

  //Update
  template< typename U0 >
  auto update( U0 fn ) -> decltype( fn( ( T0* ) nullptr ), (void) 0 ) {
    while( ! update_weak( fn ) );
  }

  //Update Weak
  template< typename U0 >
  bool update_weak( U0 fn ) {
    std::lock_guard<std::mutex> lock_guard{ lock };
    return fn( data );
  }

  //Lock/Unlock Methods
  uint lock_read() const { return 0; }
  void unlock_read( uint counter ) const {}

  T0* operator->() { return data; }
  T0 const* operator->() const { return data; }
  T0& operator*() { return *data; }
  T0 const & operator*() const { return *data; }
  T0& get() { return *data; }
  T0 const& get() const { return *data; }

  //pointer to current data
  T0* data;

  mutable std::mutex lock;

};



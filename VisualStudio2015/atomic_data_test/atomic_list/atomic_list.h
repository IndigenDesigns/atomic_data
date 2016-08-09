/*

A linked list using atomic_data. This header is part of a test in atomic_list.cpp file.

For lock-free lists we have to deal with the deletion problem. For this we employ a lock
on the to be deleted node. 

Iterator for atomic_list contains a shared_ptr referencing a list node.
You can store and refer to it. If it's deleted from the list then you won't be able to erroneously add 
to it or remove it from the list, because it's going to have the lock member variable set to true.

API:

create instance:

    - atomic_list< data_type, queue_length >
    where queue_length is passed to atomic_data

methods:

  - iterator insert_weak( iterator it, value )
  - iterator insert_weak( value ) //calls insert_weak with this->begin() for iterator
  inserts a node after it and return an iterator to it
  on fail (if the node at it position is locked) returns back an empty iterator that converts to false
  you should implement your strategy for this case (you might call it in a loop and yield on failure)


  - iterator remove_weak( iterator it, value )
  - iterator remove_weak( value ) //calls remove_weak with this->begin() for iterator
  removes a node after it and returns an iterator poiting to it
  to do it it sets a lock variable to true (with the help of atomic_data)
  so all removed nodes have their lock variable set to true and can't be 
  erroneously used for insertion or removal and you can safely store the returned

  - iterator begin()
  - iterator end()
  standard methods, atomic_list can be used in range-based for loops
  but remember that the first element is always present and is the head of the list

License: Public-domain Software.

Blog post: http://alexpolt.github.io/atomic-data.html
Alexandr Poltavsky

*/


#include "atomic_data.h"

template< typename T0, unsigned N0 > struct atomic_list {

  static_assert( N0 > 1, "queue_size for the atomic_list must be greater than 1 because the remove() method requires 2 allocations from the queue." );

  struct node;

  using atomic_node = atomic_data<node, N0>;
  using node_ptr = std::shared_ptr<atomic_node>;
  using size_t = unsigned;

  struct node {
    bool lock;
    T0 data;
    node_ptr next;
  };

  struct iterator {

    iterator operator++() {
      value = value->read( []( node* node0 ){
          return node0->next;
      });
      return *this;
    }

    iterator operator++(int) {
      iterator it{ value };
      value = value->read( []( node* node0 ){
          return node0->next;
      });
      return it;
    }

    bool operator==( iterator const& other ) const { return value == other.value; }
    bool operator!=( iterator const& other ) const { return !operator==( other ); }
    operator bool() { return (bool)value; }

    atomic_node& operator*() const { return *value; }
    atomic_node* operator->() const { return &*value; }

    node_ptr value;
  };


  //empty node is our head of the list
  atomic_list() : list{ new atomic_node{} }{ }


  iterator insert_weak( T0 value ) {
    return insert_weak( {list}, (T0&&) value );
  }

  iterator insert_weak( iterator pos, T0 value ) {

    node_ptr node_new{ new atomic_node{ new node{ false, value, nullptr } }  };

    bool r = pos->update_weak( [ &node_new ]( node* node0 ) {

      if( node0->lock ) return false;

      (*node_new)->next = node0->next;
      node0->next = node_new;

      return true;

    } );

    return r ? iterator{node_new} : iterator{};
  }

  iterator remove_weak() {
    return remove_weak( {list} );
  }

  iterator remove_weak( iterator pos ) {

    node_ptr node_next{};

    bool r = pos->update_weak( [&pos,&node_next]( node* node0 ) {

      if( node0->lock ) return false;

      node_next = node0->next;

      if( ! node_next ) {
        return false;
      }

      bool r = node_next->update_weak( []( node* node0 ) {
        if( node0->lock ) return false;
        node0->lock = true;
        return true;
      });

      if( ! r ) {
        node_next = nullptr;
        return false;
      }

      node0->next = (*node_next)->next;

      return true;

    });


    if( ! r ) {
      if( node_next ) 
        (*node_next)->lock = false;
      return {};
    } 

    return {node_next};
  }

  iterator begin() { return { list }; };
  iterator end() { return {}; };

  size_t size() {
    size_t count = 0;
    for( auto& n : *this ) {
      (void)n;
      count++;
    }
    //minus one for the head
    return count-1;
  }

  node_ptr list;

};



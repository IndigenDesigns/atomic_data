#pragma once

/*

A linked list using atomic_data. This header is part of a test in atomic_list.cpp file.

For lock-free lists we have to deal with the deletion problem. For this we employ a lock
on the to be deleted node. 

Iterator for atomic_list contains a shared_ptr referencing a list node.
You can store and refer to it. If it's deleted from the list then you won't be able to erroneously add 
to it or remove it from the list, because it's going to have the lock member variable set to true.

API:

types:

  - iterator
  holds a shared_ptr to an atomic_data<node>, can be stored and passed around
  accesing node data: auto data = it->read( []( node* ) { ... return node->data; } ) 
  updating node data: it->update( []( node* ) { ... return true; } ) 
  or in a single-threaded case: (*it)->member_of_node

create instance:

    - atomic_list< data_type, queue_length >
    where queue_length is passed to atomic_data

methods:

  - iterator insert_weak( iterator it, value )
  inserts a node after it and returns an iterator to it
  on fail (if the node at it position is locked) returns back an empty iterator that converts to false
  you should implement your strategy for this case (you might call it in a loop and yield on failure)

  - iterator insert( value ) 
  calls insert_weak with this->begin() for iterator, never fails, returns an iterator to inserted element

  - iterator remove_weak( iterator it, value )
  removes a node after it and returns an iterator poiting to it
  to do it it sets a lock variable to true (with the help of atomic_data)
  so all removed nodes have their lock variable set to true and can't be 
  erroneously used for insertion or removal and you can safely store the returned
  iterator

  - iterator remove_weak( value )
  calls remove_weak with this->begin() for iterator

  - iterator begin()
  - iterator end()
  standard methods, atomic_list can be used in range-based for loops
  but remember that the first element is always present and is the head of the list

  - size_t size()
  get the number of nodes in the list

  - void clear()
  remove nodes from the begining until nothing is left

  - bool empty()
  check if the list has elements

License: Public-domain Software.

Blog post: http://alexpolt.github.io/atomic-data.html
Alexandr Poltavsky

*/


#include "atomic_data.h"
#include <memory>

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
      //safely acquire node_ptr to atomic_node using atomic_data->read method
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
    operator bool() const { return (bool) value; }

    atomic_node& operator*() { return *value; }
    atomic_node const& operator*() const { return *value; }
    atomic_node* operator->() { return &*value; }
    atomic_node const* operator->() const { return &*value; }

    node_ptr value;
  };


  //empty node is our head of the list
  atomic_list() : list{ new atomic_node{} }{ }


  iterator insert( T0 value ) {
    auto it = begin();
    return insert_weak( it, (T0&&) value );
  }

  iterator insert_weak( iterator& pos, T0 value ) {

    node_ptr node_new{ new atomic_node{ new node{ false, value, nullptr } }  };

    //atomic_data->update_weak because the node at pos might be locked
    bool r = pos->update_weak( [ &node_new ]( node* node0 ) {

      //if locked get out
      if( node0->lock ) return false;

      //perform insertion
      (*node_new)->next = node0->next;
      node0->next = node_new;

      //okay for update
      return true;

    } );

    return r ? iterator{node_new} : iterator{};
  }

  iterator remove_weak() {
    auto it = begin();
    return remove_weak( it );
  }

  iterator remove_weak( iterator& pos ) {

    node_ptr node_next{};

    //using update_weak on the node before one that we want to remove
    //update_weak is reentrant
    bool r = pos->update_weak( [&pos,&node_next]( node* node0 ) {

      if( node0->lock ) return false;

      node_next = node0->next;

      if( ! node_next ) {
        return false;
      }
      
      //try to lock the to be deleted node (next node)
      bool r = node_next->update_weak( []( node* node0 ) {
        if( node0->lock ) return false;
        node0->lock = true;
        return true;
      });

      if( ! r ) {
        node_next = nullptr;
        return false;
      }

      //delete
      node0->next = (*node_next)->next;

      //signal atomic_data that we're good to go
      return true;

    });


    if( ! r ) {
      //unlock if we successfully locked and still failed to update
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
      (void)n; count++;
    }
    //minus one for the head
    return count-1;
  }

  void clear() {
    auto it = begin();
    while( remove_weak( it ) );
  }

  bool empty() {
    return ! list->read( []( node* node0 ) {
      return node0->next;
    });
  }

  node_ptr list;

};



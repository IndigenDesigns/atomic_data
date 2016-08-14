#pragma once

/*

A singly linked list using atomic_data. This header is part of a test in atomic_list.cpp file.

For lock-free lists we have to deal with the deletion problem. For this we employ a lock (just
a bool field thanks to atomic_data) on the to be deleted node. 

Iterator for atomic_list contains a shared_ptr referencing a list node.
You can store and refer to it. If this node is deleted from the list then you won't be able to erroneously add 
to it or remove it from the list again, because it's going to have the lock member variable set to true.

API:

types:

  - iterator
  holds a shared_ptr to an atomic_data<node>, can be stored and passed around

  accesing node data: auto data = *it;
  check if a node is locked: bool iterator::is_locked();
  check if a node is deleted: bool iterator::is_deleted();
  updating node data: bool iterator::update_weak( value ) ); //fails for various reasons
                      bool iterator::update( value ) ); //fails only if deleted

create instance:

    - atomic_list< data_type, queue_length >
    where queue_length is passed to atomic_data

methods:

  - iterator insert_after_weak( iterator it, value )
  inserts a node after it and returns an iterator to it
  on fail (might happen for various reasons) returns back an empty iterator that converts to false
  you should implement your strategy for this case (you might call it in a loop and yield on failure)

  - iterator push_front( value ) 
  inserts a node at the head, never fails, returns an iterator to inserted element

  - iterator erase_after_weak( iterator it, value )
  removes a node after it and returns an iterator poiting to it
  to do it it sets a locked bool field to true (with the help of atomic_data)
  so all removed nodes have their locked variable set to true and can't be 
  erroneously used for insertion or removal and you can safely store the returned
  iterator

  - iterator pop_front( value )
  removes a node at the head

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
    bool locked;
    bool deleted;
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

    //read data
    T0 operator*() const { 
      return value->read( []( node* node0 ){
          return node0->data;
      });
    }

    bool update( T0 data ) {
      while( true ) {
        if( is_deleted() ) return false;
        if( update_weak( data ) ) break;
      }
      return true;
    }

    //might fail because of CAS or lock
    bool update_weak( T0 data ) {
      return value->update_weak( [&data]( node* node0 ){
        if( node0->locked ) return false;
        node0->data = (T0&&) data;
        return true;
      });
    }

    bool is_locked() {
      return value && value->read( []( node* node0 ){
          return node0->locked;
      });
    }

    bool is_deleted() {
      return value && value->read( []( node* node0 ){
          return node0->deleted;
      });
    }

    node_ptr value;
  };


  //empty node is our head of the list
  atomic_list() : list{ new atomic_node{} }{ }


  iterator push_front( T0 value ) {
    auto it = iterator{ list };
    return insert_after_weak( it, (T0&&) value );
  }

  iterator insert_after_weak( iterator& pos, T0 value ) {

    node_ptr node_new;

    //atomic_data->update_weak because the node at pos might be locked
    bool r = pos.value->update_weak( [ &node_new, &value ]( node* node0 ) {

      //if locked get out
      if( node0->locked ) return false;

      //perform insertion
      node *node_ = new node{ false, false, (T0&&) value, node0->next };
      node_new.reset( new atomic_node{ node_ } );
      node0->next = node_new;

      //okay for update
      return true;

    } );

    return r ? iterator{node_new} : iterator{};
  }

  iterator pop_front() {
    auto pos = iterator{ list };
    iterator it;
    while( ! empty() && ! (it = erase_after_weak( pos )) );
    return it;
  }

  iterator erase_after_weak( iterator& pos ) {

    node_ptr node_next{};

    //using update_weak on the node before one that we want to remove
    //update_weak is reentrant
    bool r = pos.value->update_weak( [&node_next]( node* node0 ) {

      if( node0->locked ) return false;

      node_next = node0->next;

      if( ! node_next ) {
        return false;
      }
      
      node_ptr node_after;

      //try to lock the to be deleted node (next node)
      bool r = node_next->update_weak( [ &node_after ]( node* node0 ) {
        if( node0->locked ) return false;
        node0->locked = true;
        node_after = node0->next;
        return true;
      });

      if( ! r ) {
        node_next = nullptr;
        return false;
      }

      //delete
      node0->next = node_after;

      //signal atomic_data that we're good to go
      return true;

    });


    if( ! r ) {

      //unlock if we successfully locked and still failed to update
      if( node_next ) {
        node_next->update( []( node* node0 ) {
          node0->locked = false;
          return true;
        } );
      }

      return {};
    } 

    //mark as deleted
    node_next->update( []( node* node0 ) {
      node0->deleted = true;
      return true;
    } );

    return {node_next};
  }

  iterator begin() { 
    return ++iterator{ list };
  }

  iterator end() { return {}; }

  size_t size() {
    size_t count = 0;
    for( auto it = begin(); it; ++it ) {
      count++;
    }
    return count;
  }

  void clear() {
    while( pop_front() );
  }

  bool empty() {
    return ! list->read( []( node* node0 ) {
      return (bool) node0->next;
    });
  }

  node_ptr list;

};



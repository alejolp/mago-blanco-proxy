/*
   Mago Blanco - "You Shall Not Pass"
   Alejandro Santos - alejolp@alejolp.com.ar

Copyright (c) 2012 Alejandro Santos

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

   1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.

   2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.

   3. This notice may not be removed or altered from any source
   distribution.
 */

#include <iostream>

#include <boost/version.hpp>
#include <boost/heap/fibonacci_heap.hpp>
#include <boost/pool/pool_alloc.hpp>

#ifndef SESSIONSQUEUE_H_
#define SESSIONSQUEUE_H_

namespace magoblanco {

class sessions_queue_item;
typedef boost::heap::fibonacci_heap<
		sessions_queue_item,
		boost::heap::allocator<boost::pool_allocator<sessions_queue_item> > >
		sessions_queue_fib_heap;

// El handle dentro del arbol para eliminarlo de forma eficiente.
typedef sessions_queue_fib_heap::handle_type session_handle;

class session;
typedef session* session_ptr;

struct sessions_queue_item {
	sessions_queue_item(session_ptr item, std::size_t p) : item_(item), priority(p) {}

	session_ptr item_;
	std::size_t priority;

	bool operator <(const sessions_queue_item& other) const {
		return (priority > other.priority);
	}
};

class sessions_queue {
public:
	sessions_queue();
	virtual ~sessions_queue();

	void push(std::size_t priority, session_ptr s, session_handle* handle) {
		// std::cout << "push de " << priority << std::endl;
		*handle = heap_.push(sessions_queue_item(s, priority));
	}

	session_ptr front() {
		return heap_.top().item_;
	}

	void pop() {
		// std::cout << "pop de " << heap_.top().priority << std::endl;
		heap_.pop();
	}

	bool empty() {
		return heap_.empty();
	}

	void remove(const session_handle* handle) {
		heap_.erase(*handle);
	}

private:
	sessions_queue_fib_heap heap_;
};

} /* namespace magoblanco */
#endif /* SESSIONSQUEUE_H_ */

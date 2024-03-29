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

#include <deque>

#include <boost/noncopyable.hpp>
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/unordered_map.hpp>
#include <boost/unordered_set.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include <boost/pool/pool.hpp>
#include <boost/pool/object_pool.hpp>
#include <boost/pool/pool_alloc.hpp>

#include "configparms.h"
#include "session.h"
#include "sessionsqueue.h"

#ifndef SERVER_H_
#define SERVER_H_

namespace magoblanco {

using boost::asio::ip::tcp;

typedef boost::shared_ptr<boost::asio::io_service::work> work_ptr;

struct Ipv4Hasher {
	std::size_t operator()(const boost::asio::ip::address_v4& e) const
	{
		return static_cast<std::size_t>(e.to_ulong());
	}
};

class conn_table_item_t;

class session;
typedef session* session_ptr;

// Asi se define una tabla hash con un custom allocator.

typedef boost::unordered_map<
		boost::asio::ip::address_v4,
		conn_table_item_t,
		Ipv4Hasher,
		std::equal_to<boost::asio::ip::address_v4>,
		boost::pool_allocator<
			std::pair<
				boost::asio::ip::address_v4,
				conn_table_item_t> > > ipv4_count_map_t;

typedef boost::unordered_set<
		session_ptr,
		boost::hash<session_ptr>,
		std::equal_to<session_ptr>,
		boost::pool_allocator< session_ptr > > clients_list_t;

class conn_table_item_t
{
public:
	conn_table_item_t(boost::pool_allocator< session_ptr >& alloc) : clients_list_(alloc) {
		 active_connections_count_ = 0;
		 total_connection_attempts_ = 0;
		 conn_cant_ = 0;
		 blocked_dirty_ = true;
		 blocked_points_ = 0;
	}

	// Cantidad de sockets abiertos de esta IP
	std::size_t active_connections_count_;

	// Cantidad total de intentos de conexion desde esta IP
	std::size_t total_connection_attempts_;

	// Primera vez que se vio la IP en este segmento de tiempo
	boost::posix_time::ptime conn_first_;

	// Cantidad de intentos de conexion en este segmento de tiempo
	std::size_t conn_cant_;

	// Ultima actividad (connect, close) de esta IP
	boost::posix_time::ptime last_seen_;

	// Penalizacion.
	boost::posix_time::ptime blocked_until_;

	// Para saber cuando hay que quitar la penalizacion
	bool blocked_dirty_;

	// Por cada bloqueo se incrementa el puntaje, haciendo que el proximo bloqueo dure más
	int blocked_points_;

	// Lista de clientes
	clients_list_t clients_list_;

	void reset(boost::posix_time::ptime t) {
		conn_cant_ = 0;
		conn_first_ = t;
		blocked_until_ = t;
	}
};


class server : private boost::noncopyable
{
    public:
        server(magoblanco::configparms &cp, magoblanco::logger& mblog);

        void run();
        void stop();
        void enqueue_for_deletion(session_ptr ss);
        bool on_session_open(session* ss);
        void on_session_close(session* ss);

        bool connect_want(session_ptr ss);
        bool connect_done(session_ptr ss);

        boost::asio::io_service& io_service_accept() { return io_service_accept_; }
        boost::pool_allocator< session_ptr >& get_alloc_session_ptr() { return alloc_session_ptr_; }

        inline magoblanco::configparms& get_config() { return config_; }

    private:

        void process_new_session(session* new_session);
        void process_new_session_handler(session* new_session);

        void connect_force_remove(session_ptr ss);

        void clients_thread_func();
        void start_accept();
        void start_timer();

        void handle_accept(session* new_session,
                const boost::system::error_code& error);
        void handle_timer(const boost::system::error_code&);

        void dispose_sessions();

        void attack_detected(const boost::posix_time::ptime& tact, conn_table_item_t& item, session_ptr ss);

        magoblanco::configparms& config_;
        magoblanco::logger& mblog_;

        boost::asio::io_service io_service_accept_;
        boost::asio::io_service io_service_client_;

        tcp::acceptor acceptor_;

        boost::thread clients_thread_;
        std::deque<session_ptr> clients_for_closing_;
        boost::asio::deadline_timer timer_;

        ipv4_count_map_t active_connections_by_addr_;

        // Para seguirle el rastro a los sockets que están intentando conectarse actualmente
        // al host remoto. El limite de conexiones simultaneas es MAX_CONCURRENT_CONNS_TO_REMOTE
        int clients_connecting_;
        boost::mutex clients_queue_lock_;
        sessions_queue clients_for_connect_;

        boost::pool_allocator< session_ptr > alloc_session_ptr_;
};

} /* namespace magoblanco */
#endif /* SERVER_H_ */

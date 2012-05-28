/*
 * server.h
 *
 *  Created on: 28/05/2012
 *      Author: alejo
 */

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
	conn_table_item_t(boost::pool_allocator< session_ptr >& alloc) : count_(0), conn_cant_(0), blocked_dirty_(true), blocked_points_(0), clients_list_(alloc) {}

	// Cantidad de sockets abiertos de esta IP
	std::size_t count_;

	// Primera vez que se vio la IP en este segmento de tiempo
	boost::posix_time::ptime conn_first_;

	// Cantidad de intentos de conexion en este segmento de tiempo
	int conn_cant_;

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

        void clients_thread_func();
        void start_accept();
        void start_timer();

        void handle_accept(session* new_session,
                const boost::system::error_code& error);
        void handle_timer(const boost::system::error_code&);

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

        std::deque<session_ptr> clients_for_connect_;

        boost::pool_allocator< session_ptr > alloc_session_ptr_;
};

} /* namespace magoblanco */
#endif /* SERVER_H_ */

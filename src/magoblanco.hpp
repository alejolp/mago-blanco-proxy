/*
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

#ifndef GANDALF_HPP_
#define GANDALF_HPP_

using boost::asio::ip::tcp;

typedef boost::shared_ptr<boost::asio::io_service::work> work_ptr;

struct Ipv4Hasher {
	std::size_t operator()(const boost::asio::ip::address_v4& e) const
	{
		return static_cast<std::size_t>(e.to_ulong());
	}
};

// Asi se define una tabla hash con un custom allocator.

class conn_table_item_t;

class session;
typedef session* session_ptr;

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


class server
{
    public:
        server(int port);

        void run();
        void stop();
        void enqueue_for_deletion(session_ptr ss);
        bool on_session_open(session* ss);
        void on_session_close(session* ss);

        bool connect_want(session_ptr ss);
        bool connect_done(session_ptr ss);

        boost::asio::io_service& io_service_accept() { return io_service_accept_; }
        boost::pool_allocator< session_ptr >& get_alloc_session_ptr() { return alloc_session_ptr_; }

    private:

        void clients_thread_func();
        void start_accept();
        void start_timer();

        void handle_accept(session* new_session,
                const boost::system::error_code& error);
        void handle_timer(const boost::system::error_code&);

        void attack_detected(conn_table_item_t& item, session_ptr ss);

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


class session
{
    public:
        session(boost::asio::io_service& io_service, server* se);

        inline tcp::socket& remote_socket()
        {
            return remote_socket_;
        }

        inline tcp::socket& local_socket()
        {
            return local_socket_;
        }

        inline const boost::asio::ip::address_v4& remote_endpoint_addr() const
        {
            return remote_endpoint_addr_;
        }

        void init();
        void start();

        void *operator new(size_t size);
        void operator delete(void *ptr);

        void just_close();

	private:
        void *operator new[](size_t size);
        void operator delete[](void *memory);

        void start_resolve();
        void start_connect(const boost::asio::ip::tcp::endpoint& ep);

        void handle_resolve(const boost::system::error_code& err,
                tcp::resolver::iterator endpoint_iterator);
        void handle_connect(const boost::system::error_code& err);

        void start_remote_read();
        void start_remote_write();
        void start_local_read();
        void start_local_write();

        void handle_remote_read(const boost::system::error_code& error,
                size_t bytes_transferred);
        void handle_remote_write(const boost::system::error_code& error);
        void handle_local_read(const boost::system::error_code& error,
                size_t bytes_transferred);
        void handle_local_write(const boost::system::error_code& error);

        void error_and_close(const boost::system::error_code& err);

private:
        tcp::resolver resolver_;

        boost::asio::ip::address_v4 remote_endpoint_addr_;

        tcp::socket remote_socket_;
        tcp::socket local_socket_;
        enum { data_max_length = 2048 };

        /* Incoming remote data to be sent over the local socket */
        char data_remote_[data_max_length];
        size_t data_remote_size_;

        /* Incoming local data to be sent over the remote socket */
        char data_local_[data_max_length];
        size_t data_local_size_;

        bool closing_;
        bool queued_for_del_;
        server* server_;
        int async_ops_;

public:
        int64_t total_remote_data_;
        int64_t total_local_data_;
};


#endif /* GANDALF_HPP_ */

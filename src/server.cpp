/*
 * server.cpp
 *
 *  Created on: 28/05/2012
 *      Author: alejo
 */

#include "server.h"
#include "session.h"

#include <boost/interprocess/detail/atomic.hpp>

namespace magoblanco {


server::server(magoblanco::configparms &cp, magoblanco::logger& mblog)
: config_(cp), mblog_(mblog), acceptor_(io_service_accept_, tcp::endpoint(tcp::v4(), cp.get_port_local())), timer_(io_service_client_)
{
	clients_connecting_ = 0;
    start_accept();
}

void server::run()
{
    clients_thread_ = boost::thread(
            boost::bind(&server::clients_thread_func, this));

#if BOOST_ASIO_HAS_IOCP
    // FIXME: WSASelect con condicion.
#endif

    mblog_.log("Listo");

    io_service_accept_.run();
    clients_thread_.join();
}

void server::stop()
{
    io_service_accept_.stop();
    io_service_client_.stop();
}


void server::enqueue_for_deletion(session_ptr ss)
{
	/*
	 * THREADING NOTE: esta funcion se llama desde el thread de clientes.
	 */

	connect_force_remove(ss);
    clients_for_closing_.push_back(ss);
}

bool server::on_session_open(session* ss)
{
	/*
	 * THREADING NOTE: Esta funcion se ejecuta en un thread diferente a on_session_close.
	 */

	if (config_.get_verbose())
		std::cerr << ss << " on_session_open " << std::endl;

	{
		std::stringstream logs;
		logs << ss << " Nueva conexion desde " << ss->remote_endpoint_addr() << " active_table_size=" << active_connections_by_addr_.size();
		mblog_.log(logs);
	}

    boost::asio::ip::address_v4 key = ss->remote_endpoint_addr();

    std::pair<ipv4_count_map_t::iterator, bool> e =
    		active_connections_by_addr_.insert(ipv4_count_map_t::value_type(key, conn_table_item_t(alloc_session_ptr_)));

    conn_table_item_t& item = (*e.first).second;
    boost::posix_time::ptime tact(boost::posix_time::second_clock::local_time());

    item.last_seen_ = tact;

    // Existe en la tabla?
    if (!e.second) {
    	// Ya existe.

    	// Mientras esta bloqueada se la rechaza sin mas preguntas.
    	if (tact < item.blocked_until_) {
    		if (config_.get_verbose()) {
    			std::cerr << ss << " IP penalizada por " << (item.blocked_until_ - tact) << " segundos restantes." << std::endl;
    		}

    		{
    			std::stringstream logs;
    			logs << ss << " IP penalizada por " << (item.blocked_until_ - tact) << " segundos restantes.";
    			mblog_.log(logs);
    		}

    		return false;
    	} else if (item.blocked_dirty_) {
    		item.blocked_dirty_ = false;
			item.reset(tact);
		}

    	// Limite maximo de sockets por IP al mismo tiempo.
    	if ((int)item.count_ >= config_.get_MAX_CONNECTIONS_BY_IP()) {
    		if (config_.get_verbose()) {
    			std::cerr << ss << " Limite por IP alcanzado" << std::endl;
    		}

    		{
    			std::stringstream logs;
    			logs << ss << " Limite por IP alcanzado";
    			mblog_.log(logs);
    		}

    		item.blocked_until_ = tact + boost::posix_time::seconds(config_.get_PENALTY_TIME());

    		return false;
    	}

    	// Limite de intentos de conexion por periodo de tiempo.
    	// FIXME: Esta implementacion es muy naive, hacer algo mejorcito.

    	/*
    	 * La forma de detectar un ataque es contar la cantidad de intentos de
    	 * conexion, y medir el tiempo que pasó entre la primera vez y la actual.
    	 * Si la cantidad es igual al limite maximo de cantidad, y el tiempo
    	 * es menor al configurado, significa que le están dando con un martillo
    	 * al server.
    	 *
    	 */

    	item.conn_cant_++;
    	if (item.conn_cant_ > config_.get_MAX_TIME_COUNT()) {
    		if ((tact - item.conn_first_).seconds() <= config_.get_MAX_TIME_DELTA()) {

	    		attack_detected(tact, item, ss);

				return false;
    		}
    	}
    } else {
    	// IP nunca antes vista. Inicializar los campos.
		item.reset(tact);
    }

	item.count_++;
	item.clients_list_.insert(ss);

	{
		std::stringstream logs;
		logs << ss << " Aceptada, count=" << item.count_ << " points=" << item.blocked_points_ << " conn_cant=" << item.conn_cant_;
		mblog_.log(logs);
	}

    return true;
}

void server::attack_detected(const boost::posix_time::ptime& tact, conn_table_item_t& item, session_ptr ss)
{
	/*
	 * THREADING NOTE: Esta funcion es llamada por el thread listener. No hay mutexes bloqueados aun.
	 */

	int block_secs = config_.get_PENALTY_TIME() + item.blocked_points_;

	if (config_.get_verbose()) {
		std::cerr << ss << " Limite por frecuencia alcanzado, bloqueada por " << block_secs << " segundos." << std::endl;
	}

	{
		std::stringstream logs;
		logs << ss << " Limite por frecuencia alcanzado, bloqueada por " << block_secs << " segundos.";
		mblog_.log(logs);
	}

	item.blocked_until_ = tact + boost::posix_time::seconds(block_secs);
	item.blocked_dirty_ = true;
	item.blocked_points_ += config_.get_PENALTY_INC_TIME();

	for (clients_list_t::iterator it = item.clients_list_.begin(); it != item.clients_list_.end(); ++it) {
		(*it)->request_close();
	}
}

void server::on_session_close(session* ss)
{
	/*
	 * THREADING NOTE: Esta funcion se ejecuta en un thread diferente a on_session_open.
	 */

	if (config_.get_verbose())
		std::cerr << ss << " on_session_close " << active_connections_by_addr_.size() << std::endl;

    boost::asio::ip::address_v4 key = ss->remote_endpoint_addr();

    ipv4_count_map_t::iterator e = active_connections_by_addr_.find(key);
    conn_table_item_t& item = (*e).second;

    assert(e != active_connections_by_addr_.end());

	item.count_ -= 1;
	item.clients_list_.erase(ss);

	{
		std::stringstream logs;
		logs << ss << " Cerrada, count=" << item.count_ << " points=" << item.blocked_points_
				<< " conn_cant=" << item.conn_cant_ << " active_table_size=" << active_connections_by_addr_.size();
		mblog_.log(logs);
	}

	// FIXME: Hacer una limpieza de la tabla cada cierto tiempo? Tener en cuenta que map.insert se
	//        llama desde otro thread.
}

bool server::connect_want(session_ptr ss)
{
	/*
	 * THREADING NOTE: Puede ser llamada por ambos threads.
	 */

	boost::mutex::scoped_lock l(clients_queue_lock_);
	bool ret;

	++clients_connecting_;

	if (clients_connecting_ <= get_config().get_MAX_CONCURRENT_CONNS_TO_REMOTE()) {
		ret = true;
	} else {
		clients_for_connect_.push_back(*ss);
		ss->waiting_for_connect_ = true;
		ret = false;
	}

	{
		std::stringstream logs;
		logs << ss << " connect_want: " << ret << ", connecting=" << clients_connecting_;
		mblog_.log(logs);
	}

	return ret;
}

bool server::connect_done(session_ptr ss)
{
	/*
	 * THREADING NOTE: Puede ser llamada por ambos threads.
	 */

	boost::mutex::scoped_lock l(clients_queue_lock_);

	--clients_connecting_;

	if (!clients_for_connect_.empty()) {
		session& next = clients_for_connect_.front();
		clients_for_connect_.pop_front();

		/* Passing the baton, passing the papa caliente */

		next.start_connect();
	}

	{
		std::stringstream logs;
		logs << ss << " connect_done, connecting=" << clients_connecting_;
		mblog_.log(logs);
	}

	return true;
}

void server::connect_force_remove(session_ptr ss)
{
	/*
	 * THREADING NOTE: Puede ser llamada por ambos threads.
	 */

	boost::mutex::scoped_lock l(clients_queue_lock_);

	if (ss->waiting_for_connect_) {
		ss->waiting_for_connect_ = false;
		// O(1) yeah!!
		clients_for_connect_.erase( waiting_for_connect_list_t::s_iterator_to(*ss) );
		--clients_connecting_;
	}
}

void server::clients_thread_func()
{
	/*
	 * THREADING NOTE: Esta es la funcion principal del thread de clientes.
	 */

    work_ptr work(new boost::asio::io_service::work(io_service_client_));

    start_timer();

    io_service_client_.run();
}

void server::start_timer()
{
	/*
	 * THREADING NOTE:El callback del timer se ejecuta en el thread de clientes.
	 */

    timer_.expires_from_now(boost::posix_time::seconds(1));
    timer_.async_wait(boost::bind(&server::handle_timer, this,
        boost::asio::placeholders::error));
}

void server::start_accept()
{
	/*
	 * THREADING NOTE: El callback del accept se ejecuta en el thread del listener.
	 */

    session* new_session = new session(io_service_client_, this);
    acceptor_.async_accept(new_session->remote_socket(),
            boost::bind(&server::handle_accept, this, new_session,
                boost::asio::placeholders::error));
}

void server::handle_accept(session* new_session,
        const boost::system::error_code& error)
{
	/*
	 * THREADING NOTE: Esta funcion se ejecuta en el thread del listener.
	 */

    if (!error)
    {
    	new_session->init();

        if (on_session_open(new_session)) {
            new_session->start();
        } else {
            delete new_session;
        }
    }
    else
    {
    	if (config_.get_verbose())
    		std::cout << "Listener error: " << error.message() << std::endl;

        delete new_session;
    }

    start_accept();
}

void server::dispose_sessions()
{
    while (!clients_for_closing_.empty())
    {
        session_ptr ss = clients_for_closing_.front();
        clients_for_closing_.pop_front();

        on_session_close(ss);
        connect_force_remove(ss);

        if (config_.get_verbose())
        	std::cerr << ss << " cerrada la sesion (" << ss->total_local_data_ << ", " << ss->total_remote_data_ << ")." << std::endl;

        delete ss;
    }
}

void server::handle_timer(const boost::system::error_code&)
{
	/*
	 * THREADING NOTE: esta funcion se llama desde el thread de clientes.
	 */

	dispose_sessions();

    start_timer();
}

} /* namespace magoblanco */

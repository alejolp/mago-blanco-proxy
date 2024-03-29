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
	mblog_.log("Cerrando...");
    io_service_accept_.stop();
    io_service_client_.stop();
    mblog_.log("Cerrado.");
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
	 * THREADING NOTE: Esta funcion se ejecuta en el thread de clientes.
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
    item.total_connection_attempts_++;

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
    	if ((int)item.active_connections_count_ >= config_.get_MAX_CONNECTIONS_BY_IP()) {
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
    	if ((int)item.conn_cant_ > config_.get_MAX_TIME_COUNT()) {
    		if ((tact - item.conn_first_).seconds() <= config_.get_MAX_TIME_DELTA()) {

	    		attack_detected(tact, item, ss);

				return false;
    		}
    	}
    } else {
    	// IP nunca antes vista. Inicializar los campos.
		item.reset(tact);
    }

	item.active_connections_count_++;
	item.clients_list_.insert(ss);

	{
		std::stringstream logs;
		logs << ss << " Aceptada, count=" << item.active_connections_count_ << " points=" << item.blocked_points_ << " conn_cant=" << item.conn_cant_;
		mblog_.log(logs);
	}

    return true;
}

void server::attack_detected(const boost::posix_time::ptime& tact, conn_table_item_t& item, session_ptr ss)
{
	/*
	 * THREADING NOTE: Esta funcion se ejecuta en el thread de clientes.
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
		(*it)->just_close();
	}
}

void server::on_session_close(session* ss)
{
	/*
	 * THREADING NOTE: Esta funcion se ejecuta en el thread de clientes.
	 */

	if (config_.get_verbose())
		std::cerr << ss << " on_session_close " << active_connections_by_addr_.size() << std::endl;

    boost::asio::ip::address_v4 key = ss->remote_endpoint_addr();

    ipv4_count_map_t::iterator e = active_connections_by_addr_.find(key);
    conn_table_item_t& item = (*e).second;

    assert(e != active_connections_by_addr_.end());

	item.active_connections_count_--;
	item.clients_list_.erase(ss);

	{
		std::stringstream logs;
		logs << ss << " Cerrada, data_in_local=" << ss->total_local_data_
				<< ", data_in_remote=" << ss->total_remote_data_
				<< " active_conns_count=" << item.active_connections_count_
				<< " points=" << item.blocked_points_
				<< " conn_cant=" << item.conn_cant_
				<< " active_table_size=" << active_connections_by_addr_.size();
		mblog_.log(logs);
	}

	// FIXME: Hacer una limpieza de la tabla cada cierto tiempo?
}

bool server::connect_want(session_ptr ss)
{
	/*
	 * THREADING NOTE: Esta funcion se ejecuta en el thread de clientes.
	 */

	// FIXME: Este lock esta comentado porque hay un unico thread de clientes.
//	boost::mutex::scoped_lock l(clients_queue_lock_);
	bool ret;

    ipv4_count_map_t::iterator e = active_connections_by_addr_.find(ss->remote_endpoint_addr());
    conn_table_item_t& item = (*e).second;

	++clients_connecting_;

	if (clients_for_connect_.empty() && clients_connecting_ <= get_config().get_MAX_CONCURRENT_CONNS_TO_REMOTE())
	{
		ss->waiting_for_connect_ = false;
		ret = true;
	}
	else
	{
		/*
		 * La prioridad de la nueva conexion. Mientras mas grande es el valor, menor es la prioridad.
		 *
		 */
		std::size_t prioridad =
				item.active_connections_count_
				+ item.conn_cant_
				+ item.blocked_points_
				+ item.total_connection_attempts_;

		clients_for_connect_.push(prioridad, ss, &ss->handle_);
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
	 * THREADING NOTE: Esta funcion se ejecuta en el thread de clientes.
	 */

//	boost::mutex::scoped_lock l(clients_queue_lock_);

	--clients_connecting_;

	if (!clients_for_connect_.empty()) {
		session_ptr next = clients_for_connect_.front();
		clients_for_connect_.pop();

		/* Passing the baton, passing the papa caliente */

		next->waiting_for_connect_ = false;
		next->start_connect();
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
	 * THREADING NOTE: Esta funcion se ejecuta en el thread de clientes.
	 */

//	boost::mutex::scoped_lock l(clients_queue_lock_);

	if (ss->waiting_for_connect_) {
		ss->waiting_for_connect_ = false;
		clients_for_connect_.remove(&ss->handle_);
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
    	process_new_session(new_session);
    }
    else
    {
    	{
    		std::stringstream logs;
    		logs << "Listener error: " << error.message();
    		mblog_.log(logs);
    	}

        delete new_session;
    }

    start_accept();
}

void server::process_new_session(session* new_session)
{
	/*
	 * THREADING NOTE: esta funcion se llama desde el thread listener.
	 */

	// Se dispara un callback para ejecutarse en el thread de clientes.

	io_service_client_.post(boost::bind(&server::process_new_session_handler, this, new_session));
}

void server::process_new_session_handler(session* new_session)
{
	/*
	 * THREADING NOTE: esta funcion se llama desde el thread de clientes.
	 */

	new_session->init();

	if (on_session_open(new_session)) {
		new_session->start();
	} else {
		delete new_session;
	}
}

void server::dispose_sessions()
{
	/*
	 * THREADING NOTE: esta funcion se llama desde el thread de clientes.
	 */

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

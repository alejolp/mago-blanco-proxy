
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

/*
 * FIXME: Creo que hay una mezcla horrible de tabs y espacios en el mismo codigo.
 *
 * FIXME: No hay ni un solo comentario en el codigo. Horror.
 *
 * FIXME: Probar Google sparsehash.
 *
 * FIXME: Agregar WSASelect con condicion en Windows.
 *
 * FIXME: Agregar validacion basica de protocolo.
 *
 * FIXME: La implementacion de frecuencia es muy naive, hacer algo mejorcito.
 *
 * FIXME: Guardar logs y estadisticas. Rotacion de logs.
 *
 * FIXME: Limitar la cantidad de conexiones simultáneas hacia el host remoto.
 *
 * FIXME: Limitar la cantidad de conexiones globales simultáneas.
 *
 * FIXME: Agregar el algoritmo de queues.
 *
 * FIXME: Agregar más threads.
 *
 * Desarrollado con boost version 1.42.
 *
 *
 */

#include <cstdlib>
#include <iostream>
#include <string>
#include <deque>
#include <fstream>
#include <sstream>

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

#include <boost/program_options.hpp>

#include "magoblanco.hpp"
#include "logger.h"

namespace po = boost::program_options;

#define MB_VERSION "1.0"

// FIXME: Variables globales (zomg!!!111one), moverlas a un struct.

int PORT1 = 0;
int PORT2 = 0;
std::string destination;
bool destination_is_localhost = false;
boost::asio::ip::tcp::endpoint destination_ep;
bool verbose = false;
int MAX_CONNECTIONS_BY_IP = 8;
int MAX_TIME_DELTA = 10;
int MAX_TIME_COUNT = 10;
int PENALTY_TIME = 30;
int PENALTY_INC_TIME = 5;
std::string log_file_name;
magoblanco::logger mblog;

po::options_description config("config");
std::string config_file_name;
po::variables_map config_vm;

namespace {
	boost::object_pool<session> session_allocator_;
}

// Manejo de memoria custom para los objetos session.

void* session::operator new(size_t size)
{
	return static_cast<void*>(session_allocator_.malloc());
}

void session::operator delete(void *ptr)
{
	session_allocator_.free(static_cast<session*>(ptr));
}

session::session(boost::asio::io_service& io_service, server* se)
	: resolver_(io_service), remote_socket_(io_service), local_socket_(io_service), data_remote_size_(0), data_local_size_(0), closing_(false), queued_for_del_(false), server_(se), async_ops_(0), total_remote_data_(0), total_local_data_(0)
{
}


void session::init()
{
	remote_endpoint_addr_ =
		remote_socket().remote_endpoint().address().to_v4();
}

void session::start()
{
	start_connect(destination_ep);
}

#if 0
void session::start_resolve()
{
	start_connect(destination_ep);
}

void session::handle_resolve(const boost::system::error_code& err,
		tcp::resolver::iterator endpoint_iterator)
{
	--async_ops_;

	if (!err)
	{
		start_connect(*endpoint_iterator);
	}
	else
	{
		error_and_close(err);
	}
}
#endif

void session::start_connect(const boost::asio::ip::tcp::endpoint& ep)
{
	local_socket_.async_connect(ep,
			boost::bind(&session::handle_connect, this,
				boost::asio::placeholders::error));

	++async_ops_;
}

void session::handle_connect(const boost::system::error_code& err)
{
	--async_ops_;

	if (!err)
	{
		start_remote_read();
		start_local_read();
	}
	else
	{
		error_and_close(err);
	}
}

void session::start_remote_read()
{
	++async_ops_;
	data_remote_size_ = 0;
	remote_socket_.async_read_some(
			boost::asio::buffer(data_remote_, data_max_length),
			boost::bind(&session::handle_remote_read, this,
				boost::asio::placeholders::error,
				boost::asio::placeholders::bytes_transferred));
}

void session::start_remote_write()
{
	++async_ops_;
	boost::asio::async_write(remote_socket_,
			boost::asio::buffer(data_local_, data_local_size_),
			boost::bind(&session::handle_remote_write, this,
				boost::asio::placeholders::error));
}

void session::start_local_read()
{
	++async_ops_;
	data_local_size_ = 0;
	local_socket_.async_read_some(
			boost::asio::buffer(data_local_, data_max_length),
			boost::bind(&session::handle_local_read, this,
				boost::asio::placeholders::error,
				boost::asio::placeholders::bytes_transferred));
}

void session::start_local_write()
{
	++async_ops_;
	boost::asio::async_write(local_socket_,
			boost::asio::buffer(data_remote_, data_remote_size_),
			boost::bind(&session::handle_local_write, this,
				boost::asio::placeholders::error));
}

void session::just_close()
{
	if (!closing_) {
		closing_ = true;

		local_socket_.close();
		remote_socket_.close();

		// FIXME: ¿Iniciar un timer por si las ops. async no terminan
		// nunca? Nah.
	}

	if (async_ops_ == 0) {
		if (!queued_for_del_) {
			queued_for_del_ = true;
			server_->enqueue_for_deletion(this);
		}
	}
}

void session::error_and_close(const boost::system::error_code& err)
{
	if (verbose)
		std::cout << this << " Error: " << err.message() << "\n";

	just_close();
}

void session::handle_remote_read(const boost::system::error_code& error,
		size_t bytes_transferred)
{
	--async_ops_;

	if (verbose)
		std::cout << this << " handle_remote_read" << std::endl;

	if (!error && !closing_)
	{
		data_remote_size_ = bytes_transferred;
		total_remote_data_ += bytes_transferred;
		start_local_write();
	}
	else
	{
		error_and_close(error);
	}
}

void session::handle_remote_write(const boost::system::error_code& error)
{
	--async_ops_;

	if (verbose)
		std::cout << this << " handle_remote_write" << std::endl;

	if (!error && !closing_)
	{
		start_local_read();
	}
	else
	{
		error_and_close(error);
	}
}

void session::handle_local_read(const boost::system::error_code& error,
		size_t bytes_transferred)
{
	--async_ops_;

	if (verbose)
		std::cout << this << " handle_local_read" << std::endl;

	if (!error && !closing_)
	{
		data_local_size_ = bytes_transferred;
		total_local_data_ += bytes_transferred;
		start_remote_write();
	}
	else
	{
		error_and_close(error);
	}
}

void session::handle_local_write(const boost::system::error_code& error)
{
	--async_ops_;

	if (verbose)
		std::cout << this << " handle_local_write" << std::endl;

	if (!error && !closing_)
	{
		start_remote_read();
	}
	else
	{
		error_and_close(error);
	}
}



server::server(int port)
: acceptor_(io_service_accept_, tcp::endpoint(tcp::v4(), port)), timer_(io_service_client_)
{
    start_accept();
}

void server::run()
{
    clients_thread_ = boost::thread(
            boost::bind(&server::clients_thread_func, this));

#if BOOST_ASIO_HAS_IOCP
    // FIXME: WSASelect con condicion.
#endif

    mblog.log("Listo");

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
    clients_for_closing_.push_back(ss); 
}

bool server::on_session_open(session* ss)
{
	/*
	 * THREADING NOTE: Esta funcion se ejecuta en un thread diferente a on_session_close.
	 */

	if (verbose)
		std::cerr << ss << " on_session_open " << std::endl;

	{
		std::stringstream logs;
		logs << ss << " Nueva conexion desde " << ss->remote_endpoint_addr() << " active_table_size=" << active_connections_by_addr_.size();
		mblog.log(logs);
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
    		if (verbose) {
    			std::cerr << ss << " IP penalizada por " << (item.blocked_until_ - tact) << " segundos restantes." << std::endl;
    		}

    		{
    			std::stringstream logs;
    			logs << ss << " IP penalizada por " << (item.blocked_until_ - tact) << " segundos restantes.";
    			mblog.log(logs);
    		}

    		return false;
    	} else if (item.blocked_dirty_) {
    		item.blocked_dirty_ = false;
			item.reset(tact);
		}

    	// Limite maximo de sockets por IP al mismo tiempo.
    	if ((int)item.count_ >= MAX_CONNECTIONS_BY_IP) {
    		if (verbose) {
    			std::cerr << ss << " Limite por IP alcanzado" << std::endl;
    		}

    		{
    			std::stringstream logs;
    			logs << ss << " Limite por IP alcanzado";
    			mblog.log(logs);
    		}

    		item.blocked_until_ = tact + boost::posix_time::seconds(PENALTY_TIME);

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
    	if (item.conn_cant_ > MAX_TIME_COUNT) {
    		if ((tact - item.conn_first_).seconds() <= MAX_TIME_DELTA) {
    			int block_secs = PENALTY_TIME + item.blocked_points_;

				if (verbose) {
					std::cerr << ss << " Limite por frecuencia alcanzado, bloqueada por " << block_secs << " segundos." << std::endl;
				}

	    		{
	    			std::stringstream logs;
	    			logs << ss << " Limite por frecuencia alcanzado, bloqueada por " << block_secs << " segundos.";
	    			mblog.log(logs);
	    		}

	    		item.blocked_until_ = tact + boost::posix_time::seconds(block_secs);
	    		item.blocked_dirty_ = true;
	    		item.blocked_points_ += PENALTY_INC_TIME;

	    		attack_detected(item, ss);

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
		mblog.log(logs);
	}

    return true;
}

void server::attack_detected(conn_table_item_t& item, session_ptr ss)
{
	for (clients_list_t::iterator it = item.clients_list_.begin(); it != item.clients_list_.end(); ++it) {
		(*it)->just_close();
	}
}

void server::on_session_close(session* ss)
{
	/*
	 * THREADING NOTE: Esta funcion se ejecuta en un thread diferente a on_session_open.
	 */

	if (verbose)
		std::cerr << ss << " on_session_close " << active_connections_by_addr_.size() << std::endl;

    boost::asio::ip::address_v4 key = ss->remote_endpoint_addr();

    ipv4_count_map_t::iterator e = active_connections_by_addr_.find(key);
    conn_table_item_t& item = (*e).second;

    assert(e != active_connections_by_addr_.end());

	item.count_ -= 1;
	item.clients_list_.erase(ss);

	{
		std::stringstream logs;
		logs << ss << " Cerrada, count=" << item.count_ << " points=" << item.blocked_points_ << " conn_cant=" << item.conn_cant_ << " active_table_size=" << active_connections_by_addr_.size();
		mblog.log(logs);
	}

	// FIXME: Hacer una limpieza de la tabla cada cierto tiempo? Tener en cuenta que map.insert se
	//        llama desde otro thread.
}

void server::clients_thread_func()
{
    work_ptr work(new boost::asio::io_service::work(io_service_client_));

    start_timer();

    io_service_client_.run();
}

void server::start_timer()
{
    timer_.expires_from_now(boost::posix_time::seconds(1));
    timer_.async_wait(boost::bind(&server::handle_timer, this, 
        boost::asio::placeholders::error));
}

void server::start_accept()
{
    session* new_session = new session(io_service_client_, this);
    acceptor_.async_accept(new_session->remote_socket(),
            boost::bind(&server::handle_accept, this, new_session,
                boost::asio::placeholders::error));
}

void server::handle_accept(session* new_session,
        const boost::system::error_code& error)
{
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
    	if (verbose)
    		std::cout << "Listener error: " << error.message() << std::endl;

        delete new_session;
    }

    start_accept();
}

void server::handle_timer(const boost::system::error_code&)
{
    while (!clients_for_closing_.empty())
    {
        session_ptr ss = clients_for_closing_.front();
        clients_for_closing_.pop_front();

        on_session_close(ss);

        if (verbose)
        	std::cerr << ss << " cerrada la sesion (" << ss->total_local_data_ << ", " << ss->total_remote_data_ << ")." << std::endl;

        delete ss;
    }

    start_timer();
}

int parse_cmd_options(int argc, char* argv[])
{

	config.add_options()
			("help", "Ayuda")
			("version,v", "Muestra la version del programa")
			("verbose", "Activa la salida de debug")
			("config,c", po::value<std::string>(&config_file_name), "Archivo de configuracion")
			("log", po::value<std::string>(&log_file_name), "Archivo de log")
			("listen-port,l", po::value<int>(&PORT1), "Puerto de escucha")
			("remote-port,r", po::value<int>(&PORT2), "Puerto remoto")
			("remote-host,h", po::value<std::string>(&destination)->default_value("localhost"), "Host remoto")
			("max-conns-by-ip", po::value<int>(&MAX_CONNECTIONS_BY_IP)->default_value(MAX_CONNECTIONS_BY_IP), "Numero maximo de conexiones simultaneas desde la misma IP")
			("max-time-delta", po::value<int>(&MAX_TIME_DELTA)->default_value(MAX_TIME_DELTA), "MAX_TIME_DELTA")
			("max-time-count", po::value<int>(&MAX_TIME_COUNT)->default_value(MAX_TIME_COUNT), "MAX_TIME_COUNT")
			("penalty-time", po::value<int>(&PENALTY_TIME)->default_value(PENALTY_TIME), "PENALTY_TIME")
			("penalty-inc-time", po::value<int>(&PENALTY_INC_TIME)->default_value(PENALTY_INC_TIME), "PENALTY_INC_TIME")
			;

    po::store(po::command_line_parser(argc, argv).options(config).run(), config_vm);
    po::notify(config_vm);

    if (config_vm.count("config")) {
		std::ifstream ifs(config_file_name.c_str());

		if (ifs) {
			po::store(po::parse_config_file(ifs, config), config_vm);
			po::notify(config_vm);
		} else {
			std::cerr << "Erorr al abrir el archivo de configuracion." << std::endl;
			return 1;
		}
    }

    /* */

    if (destination == "localhost" || destination.substr(0, 4) == "127.")
    	destination_is_localhost = true;

    /* */

    if (config_vm.count("help")) {
    	std::cerr << config << std::endl;
    	return 1;
    }

    if (config_vm.count("version")) {
		std::cerr << "El Mago Blanco " MB_VERSION " - Thou Shall Not Pass - Alejandro Santos" << std::endl;
		std::cerr << "Website: http://code.google.com/p/mago-blanco-proxy/" << std::endl;
		return 1;
	}

    if (config_vm.count("verbose"))
    	verbose = true;

    return 0;
}

int main(int argc, char* argv[])
{
    try
    {
        if (parse_cmd_options(argc, argv)) {
        	return 1;
        }

        /* Log */

        if (log_file_name.size()) {
        	try {
        		mblog.open(log_file_name);

        	} catch (std::exception e) {
        		std::cerr << "Error al abrir el archivo de log" << std::endl;
        		return 1;
        	}
        } else {
        	std::cerr << "Debe especificar un archivo de log." << std::endl;
        	return 1;
        }

        std::stringstream ss;
        ss << "Iniciando servidor, puerto " << PORT1 << ", destino: " << destination << ":" << PORT2;
        mblog.log(ss);

        server s(PORT1);

        if (destination_is_localhost)
        {
        	destination_ep = boost::asio::ip::tcp::endpoint(boost::asio::ip::address_v4::loopback(), PORT2);
        }
        else
        {
        	tcp::resolver resolver(s.io_service_accept());
            tcp::resolver::query query(argv[2], argv[3]);
            tcp::resolver::iterator iterator = resolver.resolve(query);

            // Si hay algun error al resolver el nombre, dispara un boost::system::system_error

            destination_ep = *iterator;
        }

        s.run();
    }
    catch (std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}



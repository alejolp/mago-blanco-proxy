/*
 * session.cpp
 *
 *  Created on: 28/05/2012
 *      Author: alejo
 */

#include "session.h"
#include "server.h"

namespace magoblanco {

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

configparms& session::get_config()
{
	return server_->get_config();
}

void session::init()
{
	remote_endpoint_addr_ =
		remote_socket().remote_endpoint().address().to_v4();
}

void session::start()
{
	start_connect(get_config().get_destination_ep());
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
	if (get_config().get_verbose())
		std::cout << this << " Error: " << err.message() << "\n";

	just_close();
}

void session::handle_remote_read(const boost::system::error_code& error,
		size_t bytes_transferred)
{
	--async_ops_;

	if (get_config().get_verbose())
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

	if (get_config().get_verbose())
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

	if (get_config().get_verbose())
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

	if (get_config().get_verbose())
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


} /* namespace magoblanco */
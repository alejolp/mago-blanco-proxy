/*
 * session.h
 *
 *  Created on: 28/05/2012
 *      Author: alejo
 */

#include <boost/noncopyable.hpp>
#include <boost/asio.hpp>

#include "configparms.h"


#ifndef SESSION_H_
#define SESSION_H_

namespace magoblanco {

using boost::asio::ip::tcp;

class server;

class session : private boost::noncopyable
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

        magoblanco::configparms& get_config();

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

} /* namespace magoblanco */
#endif /* SESSION_H_ */
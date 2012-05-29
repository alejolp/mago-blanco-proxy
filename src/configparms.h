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

#include <string>

#include <boost/asio.hpp>
#include <boost/program_options.hpp>
#include <boost/noncopyable.hpp>

#include "logger.h"

#ifndef CONFIGPARMS_H_
#define CONFIGPARMS_H_

namespace magoblanco {

namespace po = boost::program_options;

#define MB_VERSION "1.0"

#define MB_DEFAULT_DESTINATION "localhost"
#define MB_DEFAULT_VERBOSE false
#define MB_DEFAULT_MAX_CONNECTIONS_BY_IP 20
#define MB_DEFAULT_MAX_TIME_DELTA 5
#define MB_DEFAULT_MAX_TIME_COUNT 5
#define MB_DEFAULT_PENALTY_TIME 30
#define MB_DEFAULT_PENALTY_INC_TIME 10
#define MB_DEFAULT_MAX_CONCURRENT_CONNS_TO_REMOTE 5

class configparms : private boost::noncopyable {
public:
	configparms();
	virtual ~configparms();

	bool parse_opts(int argc, char** argv);

	inline int get_port_local() const { return PORT_LOCAL; }
	inline int get_port_remote() const { return PORT_REMOTE; }
	inline const std::string& get_destination() const { return destination; }
	inline bool get_destination_is_localhost() const { return destination_is_localhost; }
	inline const boost::asio::ip::tcp::endpoint& get_destination_ep() const { return destination_ep; }
	inline void set_destination_ep(const boost::asio::ip::tcp::endpoint& ep) { destination_ep = ep; }
	inline bool get_verbose() const { return verbose; }
	inline int get_MAX_CONNECTIONS_BY_IP() const { return MAX_CONNECTIONS_BY_IP; }
	inline int get_MAX_TIME_DELTA() const { return MAX_TIME_DELTA; }
	inline int get_MAX_TIME_COUNT() const { return MAX_TIME_COUNT; }
	inline int get_PENALTY_TIME() const { return PENALTY_TIME; }
	inline int get_PENALTY_INC_TIME() const { return PENALTY_INC_TIME; }
	inline const std::string& get_log_file_name() const { return log_file_name; }
	inline int get_MAX_CONCURRENT_CONNS_TO_REMOTE() const { return MAX_CONCURRENT_CONNS_TO_REMOTE; }

private:
	int PORT_LOCAL;
	int PORT_REMOTE;
	std::string destination;
	bool destination_is_localhost;
	boost::asio::ip::tcp::endpoint destination_ep;
	bool verbose;
	int MAX_CONNECTIONS_BY_IP;
	int MAX_TIME_DELTA;
	int MAX_TIME_COUNT;
	int PENALTY_TIME;
	int PENALTY_INC_TIME;
	std::string log_file_name;
	int MAX_CONCURRENT_CONNS_TO_REMOTE;

	po::options_description config;
	std::string config_file_name;
	po::variables_map config_vm;
	po::positional_options_description pos_;
};

} /* namespace magoblanco */
#endif /* CONFIGPARMS_H_ */

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

#include "configparms.h"

namespace magoblanco {

configparms::configparms()
{
	destination = MB_DEFAULT_DESTINATION;
	verbose = MB_DEFAULT_VERBOSE;
	MAX_CONNECTIONS_BY_IP = MB_DEFAULT_MAX_CONNECTIONS_BY_IP;
	MAX_CONCURRENT_CONNS_TO_REMOTE = MB_DEFAULT_MAX_CONCURRENT_CONNS_TO_REMOTE;
	MAX_TIME_DELTA = MB_DEFAULT_MAX_TIME_DELTA;
	MAX_TIME_COUNT = MB_DEFAULT_MAX_TIME_COUNT;
	PENALTY_TIME = MB_DEFAULT_PENALTY_TIME;
	PENALTY_INC_TIME = MB_DEFAULT_PENALTY_INC_TIME;
}

configparms::~configparms() {}

bool configparms::parse_opts(int argc, char** argv)
{
	config.add_options()
		("help", "Ayuda")
		("version,v", "Muestra la version del programa")
		("verbose", "Activa la salida de debug")
		("config,c", po::value<std::string>(&config_file_name), "Archivo de configuracion")
		("log", po::value<std::string>(&log_file_name), "Archivo de log")
		("listen-port,l", po::value<int>(&PORT_LOCAL), "Puerto de escucha")
		("remote-port,r", po::value<int>(&PORT_REMOTE), "Puerto remoto")
		("remote-host,h", po::value<std::string>(&destination)->default_value("localhost"), "Host remoto")
		("max-conns-by-ip", po::value<int>(&MAX_CONNECTIONS_BY_IP)->default_value(MAX_CONNECTIONS_BY_IP), "Numero maximo de conexiones simultaneas desde la misma IP")
		("max-conc-conns-to-remote", po::value<int>(&MAX_CONCURRENT_CONNS_TO_REMOTE)->default_value(MAX_CONCURRENT_CONNS_TO_REMOTE), "Numero maximo de intentos de conexion simultaneos hacia el host remoto")
		("max-time-delta", po::value<int>(&MAX_TIME_DELTA)->default_value(MAX_TIME_DELTA), "MAX_TIME_DELTA")
		("max-time-count", po::value<int>(&MAX_TIME_COUNT)->default_value(MAX_TIME_COUNT), "MAX_TIME_COUNT")
		("penalty-time", po::value<int>(&PENALTY_TIME)->default_value(PENALTY_TIME), "PENALTY_TIME")
		("penalty-inc-time", po::value<int>(&PENALTY_INC_TIME)->default_value(PENALTY_INC_TIME), "PENALTY_INC_TIME")
		;

	pos_.add("config", -1);

	po::store(po::command_line_parser(argc, argv).options(config).positional(pos_).run(), config_vm);
	po::notify(config_vm);

	if (config_vm.count("config")) {
		std::ifstream ifs(config_file_name.c_str());

		if (ifs) {
			po::store(po::parse_config_file(ifs, config), config_vm);
			po::notify(config_vm);
		} else {
			std::cerr << "Erorr al abrir el archivo de configuracion." << std::endl;
			return false;
		}
	}

	/* */

	if (destination == "localhost" || destination.substr(0, 4) == "127.") {
		destination_is_localhost = true;
	}

	/* */

	if (config_vm.count("help")) {
		std::cerr << config << std::endl;
		return false;
	}

	if (config_vm.count("version")) {
		std::cerr << "El Mago Blanco " MB_VERSION " - Thou Shall Not Pass - Alejandro Santos" << std::endl;
		std::cerr << "Website: http://code.google.com/p/mago-blanco-proxy/" << std::endl;
		return false;
	}

	if (config_vm.count("verbose")) {
		verbose = true;
	}

	if (!config_vm.count("listen-port")) {
		std::cerr << "Debe especificar el puerto de escucha con --listen-port" << std::endl;
		return false;
	}

	if (!config_vm.count("remote-port")) {
		std::cerr << "Debe especificar el puerto remoto con --remote-port" << std::endl;
		return false;
	}

	return true;
}

} /* namespace magoblanco */

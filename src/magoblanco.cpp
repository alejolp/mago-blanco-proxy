
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
 * FIXME: Agregar más comentarios en el código.
 *
 * FIXME: Probar Google sparsehash.
 *
 * FIXME: Agregar WSASelect con condicion en Windows.
 *
 * FIXME: Agregar validacion basica de protocolo.
 *
 * FIXME: La implementacion de frecuencia es muy naive, hacer algo mejorcito.
 *
 * FIXME: Limitar la cantidad de conexiones globales simultáneas.
 *
 * FIXME: Agregar el algoritmo de queues.
 *
 * FIXME: Crear un pool de threads y mover el codigo de deteccion de DDoS a los threads del pool.
 *
 * FIXME: Verificar que el uso de socket::close() esté bien, intentar usar shutdown().
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

#include "magoblanco.hpp"
#include "logger.h"
#include "server.h"
#include "session.h"

using boost::asio::ip::tcp;


int main(int argc, char* argv[])
{
	magoblanco::configparms confp;
	magoblanco::logger mblog;

    try
    {
        if (!confp.parse_opts(argc, argv)) {
        	return 1;
        }

        /* Log */

        if (confp.get_log_file_name().size()) {
        	try {
        		mblog.open(confp.get_log_file_name(), confp.get_log_rotation());

        	} catch (std::exception e) {
        		std::cerr << "Error al abrir el archivo de log" << std::endl;
        		return 1;
        	}
        } else {
        	std::cerr << "Debe especificar un archivo de log." << std::endl;
        	return 1;
        }

        {
        	std::stringstream ss;
        	ss << "Iniciando servidor, puerto " << confp.get_port_local() << ", destino: " << confp.get_destination() << ":" << confp.get_port_remote();
        	mblog.log(ss);
        }

        magoblanco::server s(confp, mblog);

        if (confp.get_destination_is_localhost())
        {
        	confp.set_destination_ep(boost::asio::ip::tcp::endpoint(boost::asio::ip::address_v4::loopback(), confp.get_port_remote()));
        }
        else
        {
        	tcp::resolver resolver(s.io_service_accept());
            tcp::resolver::query query(argv[2], argv[3]);
            tcp::resolver::iterator iterator = resolver.resolve(query);

            // Si hay algun error al resolver el nombre, dispara un boost::system::system_error

            confp.set_destination_ep(*iterator);
        }

        s.run();
    }
    catch (std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}



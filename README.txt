
Mago Blanco - "Thou Shall Not Pass"
http://code.google.com/p/mago-blanco-proxy/

Alejandro Santos - http://www.alejolp.com.ar

Pequeño proxy de conexiones TCP IPv4, para intentar resistir un ataque de 
negacion de servicio. Migrar a TCPv6 no deberia ser complicado, boost es
muy util en este sentido.

El objetivo es evitar saturar el servicio detrás del proxy distribuyendo
la carga entre varios cores del servidor.

La configuración se puede hacer por medio de la línea de comandos, o por un
archivo externo, mediante la opción "--config archivo.cfg". Las opciones son
las mismas que las de la línea de comandos, con la siguiente sintaxis:

  opcion=valor

A su vez, la primer opcion implícita del proxy es tambien el archivo de
configuracion, por lo que se puede indicar sin necesidad de usar --config:

  magoblanco.exe archivo.cfg

Actualmente hay dos threads en el proxy. El primer thread se encarga de aceptar
nuevos pedidos de conexion, y nada más. Esto permite que el proxy acepte
nuevas conexiones lo más rápido posible sin saturar el backlog del socket.

El segundo thread se encarga de detectar si la nueva conexión tiene permitido
conectarse al servicio, y hacer de pasarela de datos para cuando se le
permitió el acceso.

El proxy permite limitar la cantidad máxima de pedidos de conexion hacia el
servicio mediante la opcion --max-conc-conns-to-remote. Si el servicio no
es capaz de atender los pedidos de conexion lo suficientemente rápido,
el proxy deja en espera los pedidos y va generando pedidos de conexion
a medida que el servicio responda.

El proxy a su vez tiene un scheduler de pedidos de conexiones. Los pedidos
de conexion en espera se almacenan en una cola de prioridad, donde se les
asigna un puntaje. La cola de prioridad es una "Fibonacci Heap".

Las conexiones con el menor puntaje tienen prioridad para conectarse al 
servicio, mientras que las de mayor puntaje deben esperar a que las primeras
lo hagan.  

Al ser un proxy, las conexiones entrantes deben llegar primero al proxy,
para luego ser redirigidas al servicio real.

IMPORTANTE: El servicio debe tener la opción de cambiar de número de 
puerto de escucha, y de poder hacerle bind en la interfaz de red loopback.

Este proxy a su vez permite ser ejecutado en una PC dedicada. Esto se hace
indicando la dirección IP real en donde el servició se esté ejecutando.

Se valida la cantidad de conexiones activas desde una misma IP al mismo
tiempo, y tambien la cantidad de intentos de conexion en un período de
tiempo. Si en alguno de estos casos se pasa el límite definido, se cierra
el socket.

Cuando se detecta que una misma IP está realizando demasiados intentos de 
conexión, se la filtra durante PENALTY_TIME y se le suma un puntaje de
PENALTY_INC_TIME segundos, de forma que la próxima vez que se filtre sea
de (PENALTY_TIME + puntaje) segundos.

La forma de detectar un ataque es por el momento bastante simple. Si desde una
misma IP hubo más de MAX_TIME_COUNT intentos de conexión en menos de 
MAX_TIME_DELTA segundos, se filtra esta IP y se cierran todas las demás 
conexiones desde la misma IP.

El manejo de sockets se realiza con boost::asio, que utiliza epoll en Linux
y IOCP en Windows, ambos mecanismos eficientes al manejar la notificación
de eventos en cientos de sockets simultáneos.

La cantidad de conexiones simultáneas es tanta como esté configurado el 
sistema operativo.

La tabla de información por IP es una tabla hash boost::unordered_map.

El manejo de memoria de la tabla hash esté redefinido, usando
boost::allocator_pool, y la creacion de los objetos session se hace con
boost::object_pool.

Los objetos session representan cada conexion abierta.

Se puso especial esfuerzo en hacer que el código sea O(1) donde sea posible.
La cola de prioridad es O(log n), con n=cantidad de eventos en espera.

LICENCE:
--------
 
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

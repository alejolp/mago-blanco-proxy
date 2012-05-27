
Mago Blanco - "Thou Shall Not Pass"
Alejandro Santos - http://www.alejolp.com.ar

Pequeño proxy de conexiones TCP IPv4, para intentar resistir un ataque de 
negacion de servicio. Migrar a TCPv6 no deberia ser complicado, boost es
muy util en este sentido.

El objetivo es distribuir la carga entre varios cores del servidor para evitar
saturar el servicio detrás del proxy.

La configuración se puede hacer por medio de la línea de comandos, o por un
archivo externo, mediante la opción "--config archivo.cfg". Las opciones son
las mismas que las de la línea de comandos, con la siguiente sintaxis:

  opcion=valor

Al ser un proxy, las conexiones entrantes deben llegar primero al proxy,
para luego ser redirigidas al servicio real. El servicio debe tener la 
opción de cambiar de número de puerto de escucha, y de poder hacerle
bind en la interfaz de red loopback.

Este proxy a su vez permite ser ejecutado en una PC dedicada. Esto se hace
indicando la dirección IP real en donde el servició se esté ejecutando.

Una vez iniciado, se levantan dos threads: uno que solo se encarga de aceptar
nuevas conexiones, y otro thread que se encarga de hacer la pasarela de
envio y recepcion de datos. Lo que llega por un socket lo manda por el otro
y viceversa.

Tener un thread dedicado solo a aceptar conexiones hace que se puedan
aceptar tan rapido como sea posible, sin saturar el backlog del socket.

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

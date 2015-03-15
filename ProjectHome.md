## English ##
Simple proxy/firewall for filtering DDoS SYN attacks on a TCP service. Tested on Windows and Linux systems. The source code is C++, [boost C++ libs](http://www.boost.org/) are a compilation dependency.

The docs are mostly in spanish. For more information you can contact us at the [mailing list](http://groups.google.com/group/mago-blanco-proxy) (english and spanish).

## Español ##
Pequeño proxy de conexiones TCP IPv4, para intentar resistir un ataque de negacion de servicio. El objetivo es distribuir la carga entre varios cores del servidor para evitar saturar el servicio detrás del proxy.

Funciona en Windows y Linux, el programa está hecho en C++ y depende de [boost](http://www.boost.org/) para compilarse.

La documentación se encuentra en el archivo [README.txt](http://code.google.com/p/mago-blanco-proxy/source/browse/README.txt) dentro del código fuente.

La lista de correos es: [mago-blanco-proxy](http://groups.google.com/group/mago-blanco-proxy).

La licencia es la "[Zlib Licence](http://www.opensource.org/licenses/Zlib)".

```
$ ./magoblanco --help
  --help                                Ayuda
  -v [ --version ]                      Muestra la version del programa
  --verbose                             Activa la salida de debug
  -c [ --config ] arg                   Archivo de configuracion
  --log arg                             Archivo de log
  --log-rotation arg (=60)              Rotacion de logs en minutos, 0 para 
                                        desactivar.
  -l [ --listen-port ] arg              Puerto de escucha
  -r [ --remote-port ] arg              Puerto remoto
  -h [ --remote-host ] arg (=localhost) Host remoto
  --max-conns-by-ip arg (=20)           Numero maximo de conexiones simultaneas
                                        desde la misma IP
  --max-conc-conns-to-remote arg (=5)   Numero maximo de intentos de conexion 
                                        simultaneos hacia el host remoto
  --max-time-delta arg (=5)             MAX_TIME_DELTA
  --max-time-count arg (=5)             MAX_TIME_COUNT
  --penalty-time arg (=30)              PENALTY_TIME
  --penalty-inc-time arg (=10)          PENALTY_INC_TIME
```
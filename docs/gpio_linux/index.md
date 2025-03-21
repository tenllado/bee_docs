# Control GPIO en Linux

En esta práctica utilizaremos los drivers que proporciona linux para controlar
el GPIO. Puedes hacer esta práctica si tienes una raspberry-pi con Raspbian,
consiguiendo manejar dispositivos sencillos, como los pulsadores y los leds que
incorpora la placa BEE, desde un programa de usuario escrito en C.

## Introducción

Linux expone los controladores del GPIO como dispositivos orientados a
caracteres */dev/gpiochip#*. Usar estos dispositivos ofrece las siguientes
ventajas:

- Portabilidad: el código es prácticamente independiente del hardware, a
  excepción de los GPIOS/pines que hay que usar
- No requiere privilegios de root
- Ofrece un mecanismo para tratar eventos hw desde el espacio de usuario
- Mantiene la semántica UNIX tradicional de *todo es un fichero*

Como con cualquier otro dispositivo, podemos trabajar con el gpio como si fuese
un fichero, utilizando las llamadas al sistema *open()*, *read()*, *write()*,
*ioctl()*, *close()*.

En esta práctica usaremos la versión 2 del ABI, la 1 se considera obsoleta
(*deprecated*). La fuente más fiable de referencia/documentación es el propio
fichero de cabecera */usr/include/linux/gpio.h*.

## Utilidades de línea de comandos

La librería libgpiod nos ofrece unos programas de ejemplo que podemos utilizar
como utilidades de línea de comandos para interactuar con los controladores de
GPIO.

Para disponer de estas herramientas debemos instalar algunos paquetes de
raspbian:

```sh 
sudo apt install gpiod libgpiod-dev libgpiod-doc
```

Una vez instalados estos paquetes tendremos disponibles las utilidades
descritas en las siguientes secciones.

### gpiodetect

Esta utilidad nos da una lista de los dispositivos de caracteres para control
del gpio disponibles en nuestra plataforma:

```sh 
pi@raspberrypi:~ $ gpiodetect
gpiochip0 [pinctrl-bcm2835] (54 lines)
gpiochip1 [brcmvirt-gpio] (2 lines)
gpiochip2 [raspberrypi-exp-gpio] (8 lines)
pi@raspberrypi:~ $
```
### gpioinfo 

Esta utilidad lista los pines controlados por uno de los controladores:

```sh 
pi@raspberrypi:~ $ gpioinfo gpiochip0
gpiochip0 - 54 lines:
    line   0:      unnamed       unused   input  active-high
    line   1:      unnamed       unused   input  active-high
    line   2:      unnamed       unused   input  active-high
    line   3:      unnamed       unused   input  active-high
    ...
pi@raspberrypi:~ $
```

### gpiofind

Utilidad que nos da el número de línea de gpio para líneas identificadas con un
  nombre en el *device-tree*

### gpioset

Utilidad que permite asignar un valor a un conjunto de líneas del gpio mientras
ejecuta el comando. Con *-m modo* podemos configurar lo que hace el comando tras
dar el valor a las líneas. Los posibles modos son:

- wait: espera a que el usuario pulse enter
- exit: termina inmediatamente
- time: duerme por el periodo de tiempo especificado con el flag -s o el flag -u
- signal: espera hasta recibir SIGINT o SIGTERM
 
Por ejemplo, para encender 3 leds conectados a los pines 20, 21 y 26, 
hasta que el usuario pulse enter:

```sh 
pi@raspberrypi:~ $ gpioset -m wait 0 20=1 21=1 26=1
pi@raspberrypi:~ $
```

El siguiente script es un ejemplo de uso de gpioset que hace oscilar una luz
entre tres leds.

```bash
#!/bin/bash

ini=0x01
dir=0
while true
do
	bit26=$(( (ini & 0x4) >> 2 ))
	bit21=$(( (ini & 0x2) >> 1 ))
	bit20=$((ini & 0x1))
	gpioset -m time -s 1 0 26=$bit26 21=$bit21 20=$bit20
	if [ $dir -eq 0 ]; then
		ini=$(((ini << 1) & 0x7))
	else
		ini=$(((ini >> 1) & 0x7))
	fi
	if [ $ini -eq 0 ]; then
		dir=$(((dir + 1) & 1))
		ini=0x2
	fi
done
```

### gpioget 

Esta utilidad nos da el estado actual de las líneas solicitadas en el chip de
control indicado en la linea de comandos

### gpiomon

Esta utilidad nos permite monitorizar el estado de unos pines. Usa la llamada al
sistema *poll*, con las siguientes opciones:

- -n NUM: termina después de \texttt{NUM} eventos
- -s: no imprime información de evento
- -r: procesa sólo eventos de flanco de subida
- -f: procesa sólo eventos de flanco de bajada
- -F FMT: especifica el formato de salida (consulta la página de manual)

Por ejemplo, para monitorizar tres pulsadores conectados a las líneas 19, 6 y 5:

```bash
pi@raspberrypi:~ $ gpiomon -f 0 19 6 5
event: FALLING EDGE offset: 19 timestamp: [ 2740.887807571]
event: FALLING EDGE offset: 6 timestamp: [  2742.291116096]
event: FALLING EDGE offset: 5 timestamp: [  2744.609921761]
^Cpi@raspberrypi:~ $
```

## Driver GPIO

Vamos a ver cómo podemos manejar los pines del GPIO desde un programa de usuario
escrito en C utilizando el driver de GPIO proporcionado por Linux. Para ello
debemos comprender qué son las operaciones ioctl en el estándar POSIX, que serán
vitales para la comunicación con el driver. Luego iremos viendo como podemos
utilizar estas operaciones para interactuar con el controlador GPIO.

### Operaciones ioctl

En los sistemas POSIX las operaciones sobre un dispositivo que no sean read o 
write se realizan con la llamada al sistema *ioctl*, cuyo prototipo es:

```c 
int ioctl(int fd, unsigned long request, void *argp);
```

dónde:

- *fd*: descriptor de fichero devuelto por open
- *request*: petición de operación, dependiente del dispositivo.
- *argp*: dirección a un buffer que depende del tipo de operación.

No hay un estándar para los códigos de las peticiones, pero un convenio que es
ampliamente usado es el siguiente:

- Dos bits para indicar la dirección: 00 (nada),  01 (lectura), 10 (escritura) y
  11 (lectura y escritura).
- 14 bits que indican el tamaño del dato pasado como argumento
- 8 bits de tipo de operación
- 8 bits de número de operación

El sistema define unas macros para ayudar a la codificación de estas peticiones:
_IOR(type, nr, arg), _IOW(type, nr, arg), _IOWR(type, nr, arg) e _IO(type, nr).

### Obtener información del controlador GPIO

Para obtener información del controlador podemos utilizar la petición *ioctl*
GPIO_GET_CHIP_INFO, que requiere como parámetro adicional la dirección de una
estructura del siguiente tipo:

```c 
struct gpiochip_info {
  char name[32];    /* Nombre del controlador en el kernel */
  char label[32];   /* Nombre funcional, de producto */
  __u32 lines;      /* número de líneas que maneja */
};
```

Un ejemplo de uso sería el siguiente:

```c 
int fd;
struct gpiochip_info info;
fd = open("/dev/gpiochip0", O_RDONLY);
ioctl(fd, GPIO_GET_CHIPINFO_IOCTL, &info);
close(fd);
printf("label: %s\n", info.label);
printf("name: %s\n", info.name);
printf("number of lines: %u\n", info.lines);
```

Examinar el ejemplo [gpio_info](src/gpio_info.c) y comprobar su funcionamiento.

### Pines de entrada y salida

El driver de GPIO nos permite agrupar pines en *líneas*, que pueden recibir un
nombre lógico definido por el programador(por ejemplo sevenseg). Para crear una
de estas *líneas de pines*, una vez abierto el dispositivo, debemos hacer una
petición ioctl de tipo *line_request*:

```c 
struct gpio_v2_line_request req;

ioctl(fd, GPIO_V2_GET_LINE_IOCTL, &req);
```

El registro *req* tiene la siguiente estructura: 

```c 
struct gpio_v2_line_request {
    __u32 offsets[GPIO_V2_LINES_MAX];  //pines a manejar      (in)
    char consumer[GPIO_MAX_NAME_SIZE]; //nombre por usuario   (in)
    struct gpio_v2_line_config config; //configuración        (in)
    __u32 num_lines;                   //num pines en offsets (in)
    __u32 event_buffer_size;           //para eventos         (in)
    __u32 padding[5];                  //no usado
    __s32 fd;                          //fd de salida         (out)
};
```

El campo *offsets* debe contener los números de los pines que pertenecen a la
línea, indicando el número total de pines de la línea en *num_lines*. El campo
*config* se utiliza para indicar la configuración de cada uno de los pines que
pertenecen a la línea. El campo *fd* es de salida, en el retorno de la llamada
ioctl contendrá un nuevo descriptor de fichero que podemos usar para operar
sobre los pines de la línea (leer, escribir, etc).

El campo *config* tiene la siguiente estructura:

```c 
struct gpio_v2_line_config {
	__aligned_u64 flags; // flags por defecto para los pines
	__u32 num_attrs;     // número de atributos en attrs
	__u32 padding[5];    // no usado
	struct gpio_v2_line_config_attribute
     attrs[GPIO_V2_LINE_NUM_ATTRS_MAX]; // array de atributos
};
```

El campo *flags* permite establecer una configuración por defecto para los pines
de la línes. Se trata de una máscara de bits a la que se le debe asignar una
combinación, con or a nivel de bit (operadores *bitwise*), de las siguientes
macros:

- `GPIO_V2_LINE_FLAG_USED`
- `GPIO_V2_LINE_FLAG_ACTIVE_LOW`
- `GPIO_V2_LINE_FLAG_INPUT`
- `GPIO_V2_LINE_FLAG_OUTPUT`
- `GPIO_V2_LINE_FLAG_EDGE_RISING`
- `GPIO_V2_LINE_FLAG_EDGE_FALLING`
- `GPIO_V2_LINE_FLAG_OPEN_DRAIN`
- `GPIO_V2_LINE_FLAG_OPEN_SOURCE`
- `GPIO_V2_LINE_FLAG_BIAS_PULL_UP`
- `GPIO_V2_LINE_FLAG_BIAS_PULL_DOWN`
- `GPIO_V2_LINE_FLAG_BIAS_DISABLED`

Los atributos de la línea nos permiten cambiar la configuración de un
subconjunto de los pines de la línea o indicar información complementaria a los
flags necesaria para completar su configuración (por ejemplo valor del pin en
pines de salida). El campo *attrs* es un array de atributos definidos para la
línea, indicando el número de atributos definidos en el campo *num_attrs*. Cada
atributo se define con la siguiente estructura:

```c 
struct gpio_v2_line_config_attribute {
	struct gpio_v2_line_attribute attr; //atributo
	__aligned_u64 mask;                 //pines a los que aplica
};
```

El campo *mask* indica los pines de la linea afectados por el atributo, dónde
cada bit corresponde a un índice del array de offsets de la estructura 
*struct gpio_v2_line_request*. El campo *attr* se representa con la siguiente
estructura:

```c 
struct gpio_v2_line_attribute {
    __u32 id; //GPIO_V2_LINE_ATTR_ID_{FLAGS,OUTPUT_VALUES,DEBOUNCE}
    __u32 padding;
    union {
        __aligned_u64 flags;     // flag
        __aligned_u64 values;    // valor ini. 1-activo, 0-inactivo
        __u32 debounce_period_us;// tiempo de debounce
    };
};
```

El campo *id* indica el tipo de atributo, y su valor determina el campo de la
unión que contiene la información correspondiente:

- *GPIO_V2_LINE_ATTR_ID_FLAGS*: aplicable a cualquier grupo de pines. El campo
  *flags* de la unión indica una configuración alternativa para este grupo de
  pines.

- *GPIO_V2_LINE_ATTR_ID_OUTPUT*: aplicable a grupos de pines configurados como
  salida (por defecto o con atributos anteriores). El campo *values* de la unión
  indica el valor que se asigna a los pines afectados.

- *GPIO_V2_LINE_ATTR_ID_DEBOUNCE*: aplicable a pines configurados como entrada
  (por defecto o con atributos anteriores). El campo *debounce_period_us* de la
  unión indica el periodo en microsegundos con el que se muestrea el pin para
  eliminar rebotes (cualquier cambio más rápido es filtrado).

Para leer o escribir en los pines de la línea debemos realizaremos nuevas
operaciones ioctl sobre el descriptor de fichero inicializado en la operación
ioctl *line_request*.

Para escribir un valor en pines configurados como salida usaremos la operación
ioctl `GPIO_V2_LINE_SET_VALUES_IOCTL`:

```c 
struct gpio_v2_line_values values;
...
ioctl(req.fd, GPIO_V2_LINE_SET_VALUES_IOCTL, &values);
```

mientras que para leer utilizaremos la operación *GPIO_V2_LINE_GET_VALUES_IOCTL*:

```c 
struct gpio_v2_line_values values;
...
ioctl(req.fd, GPIO_V2_LINE_GET_VALUES_IOCTL, &values);
```

En ambos casos *values* tiene la siguiente estructura:

```c 
struct gpio_v2_line_values {
	__aligned_u64 bits; //máscara de bits con el valor los pines
	__aligned_u64 mask; //máscara de bits que leer/escribir
};
```

Cada bit de ambos campos se refiere a una de las posiciones del campo
*offsets* de la línea.

Veamos un ejemplo. El programa [gpio_blink_v2.c](src/gpio_blink_v2.c) usa el
driver de caracteres del GPIO para hacer parpadear los leds que se indican por
la línea de comandos, con un periodo de 0.5Hz (1 s encendidos, 1 s apagados).
Podemos probar este programa en la raspberry, conectando tres pines a los tres
leds de la placa BEE.

Como ejercicio, se propone al estudiante modificar el código del programa
anterior para que de la sensación de que una luz va pasando de un led a otro,
dando la vuelta cuando llegue a los extremos.

### Eventos en pines de entrada

El driver del GPIO convierte las interrupciones en los pines del GPIO en eventos
software que se insertan en el descriptor de fichero (fd) asociado a la linea.
Esto permite:

- Usar programación multihilo, destiando un hilo a atender los eventos de
  entrada de la línea.
- Usar mecanismos de multiplexación de entrada/salida para monitorizar varios
  descriptores/líneas (uando select, poll o epoll).

La detección de eventos debe ser habilitada en los pines de entrada, añadiendo
uno los flags `GPIO_V2_LINE_FLAG_EDGE_RISING` y/o
`GPIO_V2_LINE_FLAG_EDGE_FALLING`. Además, se puede utilizar el atributo 
`GPIO_V2_LINE_ATTR_ID_DEBOUNCE` para configurar en el pin una eliminación de
rebotes.

Por cada evento podremos leer del descriptor de fichero una estructura del tipo:

```c 
struct gpio_v2_line_event {
	__aligned_u64 timestamp_ns; //Marca de tiempo del evento
	__u32 id;                   //Tipo de evento
	__u32 offset;               //offset del pin correspondiente
	__u32 seqno;                //núm. de secuencia global
	__u32 line_seqno;           //núm. de secuencia en este pin
	__u32 padding[6];           //reservado
};
```
dónde el campo id nos identifica el evento detectado mientras que *offset*
identifica el pin en el que se producido el evento. El campo *seqno* nos indica
el órden global en el que el evento se ha detectado, mientras que *line_seqno*
nos indica el órden entre los eventos de la línea.

El programa de ejemplo [gpio_toggle.c](src/gpio_toggle.c) usa el driver GPIO
para controlar el estado de unos leds con unos pulsadores. El programa recibe
como parámetros una lista con un número par de pines. La primera mitad se
configuran como pines entradas, activando la detección de eventos por flancos de
bajada, con un debounce de 10 ms. La segunda mitad se configuran como pines de
salida, inicialmente activos (a 1). El programa se queda esperando por eventos
de flanco de bajada en las entradas. Cuando se recibe un evento, se conmuta el
estado del pin de salida correspondiente. El programa puede probarse conectando
los 3 leds de la placa de expansión a 3 gpios (salidas) y los pulsadores a otros
3 gpios (entradas).

Como ejercicio se propone al estudiante modificar el programa para que los
pulsadores determinen si los leds parpadean o no. Es decir, inicialmente estarán
los 3 leds parpadeando a una frecuencia fija (por ejemplo 0.5 Hz), si se pulsa
uno de los pulsadores el led correspondiente se quedará apagado. Si se vuelve a
pulsar, el led volverá a parpadear síncronamente con el resto.


El dispositivo de caracteres GPIO soporta algunas operaciones ioctl adicionales:

- `GPIO_V2_GET_LINEINFO_WATCH_IOCTL`, monitorizar cambios en un línea:
    - Se pasa un argumento `struct gpio_v2_line_info`
    - Se indican las líneas que se desean monitorizar
    - Los eventos de cambio se obtinen del *fd* del gpiochip correspondiente

- `GPIO_V2_LINE_SET_CONFIG_IOCTL`, cambiar la configuración de una línea:
    - Se pasa un argumento `struct gpio_v2_line_config`
    - No es necesario liberar la línea antes


## GPIO mapeado en memoria

Linux incorpora el dispositivo /dev/mem que permite acceder a la memoria física
del computador. El offset en este fichero es interpretado como una dirección
física y el driver controla los rangos accesibles (configurable con la opción de
compilación del kernel *CONFIG_STRICT_DEVMEM*) en función de la arquitectura del
computador. Sólo root tiene acceso a este dispositvo, ya que usarlo implica
saltarse prácticamente todos los mecanismos de control y las abstracciones del
sistema operativo.

Raspbian incorpora también el dispositivo /dev/gpiomem, que es un dispositivo
que permite el acceso al rango de direcciones físicas controlado por el GPIO.
Todos los usuarios que pertenezcan al grupo gpio tienen acceso a este fichero,
además de root. Este fichero permite acceder *en crudo* a los registros del gpio
puenteando al driver gpio. Aunque no se recomienda su uso (mejor utilizar el
driver gpio), puede ser útil usarlo para estudiar el funcionamiento del
controlador gpio.

Como ejemplo vamos a ver cómo podemos implementar el ejemplo gpio_blink
utilizando este dispositivo. Para ello primero debemos revisar la documentación
de Broadcom para ver cómo funciona el controlador GPIO. Resumimos aquí los
aspectos más destacados.

En el controlador GPIO la función de cada pin se configura en los registros de
selección *GPFSELn*:

| Registro | **Dir. Física** | **Pines GPI**   |
| -------- | --------------- | --------------- |
| GPFSEL0  | 0x3F200000      | GPIO0 - GPIO9   |
| GPFSEL1  | 0x3F200004      | GPIO10 - GPIO19 |
| GPFSEL2  | 0x3F200008      | GPIO20 - GPIO29 |
| GPFSEL3  | 0x3F20000C      | GPIO30 - GPIO39 |
| GPFSEL4  | 0x3F200010      | GPIO40 - GPIO49 |
| GPFSEL5  | 0x3F200014      | GPIO50 - GPIO53 |

Se utilizan tres bits por pin, con la codificación indicada en la siguiente
tabla:

|     | **Función (según doc. Broadcom**) |
| --- | --------------------------------- |
| 000 | Entrada                           |
| 001 | Salida                            |
| 100 | Función alternativa 0             |
| 101 | Función alternativa 1             |
| 110 | Función alternativa 2             |
| 111 | Función alternativa 3             |
| 011 | Función alternativa 4             |
| 010 | Función alternativa 5             |

Para operar sobre los pines de salida hay dos pares de registros:

| Valor | Pin   | Registro | Dirección (ARM) |
| ----- | ----- | -------- | --------------- |
| 1     | 0-31  | GPFSET0  | 0x3F20001C      |
| 1     | 32-53 | GPFSET1  | 0x3F200020      |
| 0     | 0-31  | GPFCLR0  | 0x3F200028      |
| 0     | 32-53 | GPFCLR1  | 0x3F20002C      |

Así, para poner a 1 un pin de salida debemos escribir un 1 en la posición
correspondiente del registro GPSET que corresponda. Y para poner dicho pin a 0
deberemos escribir un 1 en el la misma posición del registro GPCLR
correspondiente. Escribir un 0 no modifica el valor del pin.

El programa [gpio_blink_mem](src/gpio_blink_mem.c) proyectan el dispositivo
*/dev/gpiomem* en el mapa virtual de memoria del proceso utilizando mmap, con el
fin de acceder diréctamente a los registros del controlador. Se utilizan unos
arrays para almacenar los offsets de los registros:

```c 
uint32_t gpfsel_offset[] = {0x00,0x04,0x08,0x0C,0x10,0x14};
uint32_t gpfset_offset[] = {0x1C, 0x20};
uint32_t gpfclr_offset[] = {0x28, 0x2C};
```

Estos offsets se suman a la dirección base que es un puntero a byte,
convirtiendo el resultado a un puntero a *uint32_t* por conveniencia:

```c 
	uint8_t *gpiomem;
	volatile uint32_t * fpsel;
  ...
	gpiomem = mmap(NULL, 4096, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0x3f200000);
  ...
	n = get_int(argv[i+1], 10, desc);
	fpsel = (uint32_t *)(gpiomem + gpfsel_offset[n / 10]);
```

Las escrituras en los registros se hacen desreferenciado los punteros, que se
han declarado como volátiles:

```c 
	*fpsel |= (0x1 << (n % 10)*3);
```

Aunque este mecanismo permite controlar los gpios, debemos notar que el código
es muy dependiente del hardware, utilizando offsets y selecciones de registros
en función del número de pin o la operación a realizar; y escogiendo
adecuadamente los códigos a escribir en los registros siguiendo la documentación
del fabricante del SoC. Notemos además que el estado de los leds se mantiene
cuando el programa termina, y puede entrar en conflicto con cualquier otro
proceso que use el driver del sistema para el gpio. Este código es mucho más
propenso a errores que no serán identificados por el sistema (no hay valor de
retorno de error).

La librería [bcm2835](https://www.airspayce.com/mikem/bcm2835/) usa entrada
salida mapeada en memoria y ofrece un API más sencillo y práctico para el manejo
de los controladores de la raspberry pi, pero sigue saltándose todos los
controles del sistema operativo, por lo que resulta más adecuando aprender a
utilizar el driver GPIO estudiado en la sección anterior.



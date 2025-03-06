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

El driver de GPIO nos permite definir *líneas de pines*, que representan grupos
de pines con un nombre de usuario (por ejemplo sevenseg), y que comparten la
misma configuración. Una vez creada la línea se obtiene un descriptor de fichero
que la representa, con el que podremos manejar este grupo de pines.

Para crear una de estas *líneas de pines*, una vez abierto el dispositivo,
debemos hacer una petición ioctl de tipo *line_request*:

```c 
ioctl(fd, GPIO_V2_GET_LINE_IOCTL, &req);
```

dónde *req* es un `struct gpio_v2_line_request`:

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
línea, indicando el número total de pines de la línea en *num_lines*. La
configuración de los pines de la línea se indica en el campo *config*. A la
salida, el campo *fd* es un nuevo descriptor de fichero que podemos usar para
operar sobre los pines (leer/escribir).

El campo *config* del es del tipo:

```c 
struct gpio_v2_line_config {
	__aligned_u64 flags; // flags por defecto para los pines
	__u32 num_attrs;     // número de atributos en attrs
	__u32 padding[5];    // no usado
	struct gpio_v2_line_config_attribute
     attrs[GPIO_V2_LINE_NUM_ATTRS_MAX]; // array de atributos
};
```

El campo *flags* de esta estructura es una máscara de bits que permite
seleccionar la configuración de los pines de la línea, combinando con una or las
siguientes macros:

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



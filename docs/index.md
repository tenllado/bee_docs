# BEE Docs

## Placa de expansión BEE

La placa BEE fué desarrollada como una placa de expansión para la primera
versión de la Raspberry Pi, con el fin de facilitar montar puestos de
laboratorio económicos en torno a este *computador de una sola placa*.

La placa BEE proporciona una serie de periféricos que son fácilmente conectables
a los pines GPIO de la raspberry pi. En lugar de proporcionar una conexión fija
entre los periféricos y los GPIOS de del microcontrolador, es el usuario el que
debe conectar cada uno de los periféricos a los pines deseados usando cables de
puente dupont hembra-hembra.

Los pines de la raspberry pi se han dejado disponibles en distintas tiras de
pines en la propia placa de expansión. Algunas de estas tiras se han colocado
junto a los circuitos de los periféricos para los que esos pines se usarán con
mayor frecuencia. Para esos casos la conexión entre el periférico y la raspberry
pi puede hacerse sólo con jumpers.

En todo momento se ha evitado que un pin pueda ser conectado a más de un
dispositivo externo, para evitar errores frecuentes entre los alumnos que
generan cortocircuitos.

Se ha desarrollado hasta el momento dos versiones de la placa BEE. La primera
versión (v1) es más pequeña y económica de manejar, y contiene un conjunto más
reducido de dispositivos pensados inicialmente para el desarrollo de dos
asignaturas del Grado de Ingeniería Electrónica de Comunicaciones.

La segunda versión (v2) extiende el conjunto de dispositivos incluidos para
ampliar el espectro de asignaturas que pueden sacar provecho de esta económica y
versátil placa.

Los detalles de estos dos modelos se presentan en las próximas secciones.

## BEE v1

La primera versión de la BEE incluye el siguiente conjunto básico de
dispositivos:

- Un MCP3008. Se trata de un conversor analógico digital (ADC) de 10 bits con 8
  canales. Su montaje en la placa permite una conexión sencilla de sus entradas
  a las salidas de sensores, y su interfaz digital SPI puede ser conectada
  mediante 5 *jumpers* a los pines SPI de la Raspberry Pi por el canal CE0.

- Sockets para la conexión rápida de sensores a la entrada del ADC, con
  posibilidad de polarizar dichos sensores a 0 o alimentación (3.3 V o 5 V).

- Un MCO4911. Se trata de un conversor digital analógico (DAC) de 10 bits, que
  también puede ser conectado al controlador SPI usando los mismos 5 *jumpers*,
  quedando conectado al canal CE1.

- 3 circuitos simples de pulsador, que pueden ser conectados a pines digitales
  de entrada del microcontrolador para recibir acciones de usuario.

- 3 circuitos simples de led polarizado, que permiten encender el led desde un
  programa si se conecta el circuito a un pin de salida del microcontrolador.
 
- Un conector para cables FTDI-232-R con conexión directa a los pines del puerto
  serie de la raspberry pi. Facilita la comunicación serie desde un PC con la
  raspberry pi.
  
- Un conector para JTAG estándar de 20 pines, conectado a los pines de
  depuración en circuito de la raspberry pi.

- 1 Zumbador piezoeléctrico que puede ser conectado con *jumpers* a algunos de
  los pines pwm de la Raspberry PI.

Estos dispositivos están incluidos para dar soporte al desarrollo de prácticas
de dos asignaturas del departamento:

- Estructura de Computadores: en la que se realizan prácticas de programación de
  entrada salida *bare-metal*, usando pines digitales de entrada y salida,
  conectados a leds y pulsadores, manejo de puerto serie y conexión a
  dispositivos por SPI o I2C.

- Control de Sistemas: se utilizan sensores analógicos y digitales, ADC, DAC y
  controladores PWM.

Las siguientes imágenes muestran un modelo 3D de la BEE v1, una foto aislada de
un montaje real de la placa y su conexión a la Raspberry Pi en el montaje
utilizado el laboratorio del Grado de Ingeniería Electrónica de Comunicaciones
de la Universidad Complutense de Madrid:

![BEE v1](img/bee_v1_3dmodel.png)

![BEE v1 foto](img/bee_v1_foto.jpg)

![BEE v1 y raspi](img/raspi_bee_v1_montaje.jpg)

### Esquemático de la BEE v1

La siguiente figura muestra el esquemático de la primera versión de la placa
BEE:

![BEE v1_schematic](img/BEE_v1_schematic.png)

Cada uno de los bloques de periféricos se explica y documenta en las secciones
correspondientes.

## BEE v2

La segunda versión de la placa extiende los dispositivos incluidos para dar
soporte a un mayor número de asignaturas de nuestro departamento, como por
ejemplo Arquitectura Interna Linux y Android, dónde se programan drivers para
varios tipos de dispositivos en estos sistemas. Asimismo se incorpora una red de
polarización mucho más versátil, con el objetivo de facilitar la polarización de
sensores analógicos que se quieran conectar a la entrada del ADC, proporcionando
las resistencias de polarización más habituales, posibilidades de combinarlas en
serie o en paralelo y microinterruptores para polarizarlas a gnd o alimentación.

Concretamente, en esta versión de la placa se han añadido a los dispositivos
incluidos en la versión 1 la siguiente lista de periféricos: 

- Un desplazador con buffer conectado a un display de 7 segmentos (con punto
  decimal), permite escribir en el display 7 segmentos desde la raspberry-pi
  usando pines genéricos de entrada y salida.

- Un led RGB polarizado, que puede ser operado desde la raspberry pi con pines
  genéricos de entrada salida.

- Una red de polarización de sensores, con micro interruptores y resistencias
  habituales para una polarización de sensores analógicos que quieran conectarse
  a las entradas del ADC. 

La siguiente imagen muestra un modelo 3D de la segunda iteración de la placa:

![BEE v2](img/bee_v2_3dmodel.png)

### Esquemático de la BEE v2

La siguiente figura muestra el esquemático de la primera versión de la placa
BEE:

![BEE v2_schematic](img/BEE_v2_schematic.png)

Cada uno de los bloques de periféricos se explica y documenta en las secciones
correspondientes.


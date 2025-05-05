# PRÁCTICA 1. MOTORES Parte 1
## Motores en robótica

### Objetivos

1. Familiarizarse con la Raspberry Pi y su entorno.
2. Generación de una señal para control de un motor paso a paso.
3. Generación de una señal (PWM) para control de un servomotor.

La finalidad de esta práctica es aprender a generar señales en el orden adecuado para mover un motor paso a paso y aplicar una señal de PWM para controlar un motor de continua (servomotor de rotación continua). Estos tipos de motores son muy utilizados en Robótica. En concreto, el servo de rotación continua se utilizará para controlar un robot móvil sencillo.

> **Nota:** Antes de ejecutar un programa, verifique con su profesor que las conexiones están realizadas correctamente.

---

## Desarrollo de la práctica

### Primer contacto con la Raspberry

Para familiarizarse con el entorno de programación de la Raspberry Pi, primero vamos a realizar dos tareas sencillas.

#### Tarea 1: Conocer el entorno de la Raspberry

Alimenta la Raspberry Pi y conecta el portátil a ella (vea cómo conectar con la Raspberry Pi). Una vez conectado, cambia al directorio `wiringPi/examples`. Allí hay un archivo denominado `blink.c`.

Lo que haremos será comprobar que está funcionando correctamente la Raspberry: compila el programa escribiendo:


~~~
bash
make blink
~~~

Observa que se crea un ejecutable.

#### Tarea 2: "Hola Mundo" hardware

El objetivo de esta tarea es programar el parpadeo de un LED. Para ello, en primer lugar, habrá que realizar el montaje indicado en la siguiente figura:

![Esquema de conexión del LED](figuras\ConexionesLED.png)

Conecta el LED al Pin GPIO 17 (el Wiring Pin 0). La resistencia utilizada es de 220 Ω.

Dependiendo de la biblioteca software que usemos para programar la Raspberry, la numeración de los pines cambia. En la siguiente figura pueden verse las distintas numeraciones según el conector físico de 40 pines. La placa de conexiones Bee 2.0 sigue la numeración BCM. De este modo, el pin serigrafiado en la placa como B17 corresponde al pin físico 11 o al WiringPi 0.

![Raspberry Pinout](figuras/raspberrypi-gpio-wiringpi-pinout.png)

Si ahora ejecutamos el programa anterior:
~~~
sudo ./blink
~~~

Se verá que el LED parpadea.

**Edita el programa, cambia la frecuencia de parpadeo, vuelve a compilarlo y observa que su frecuencia cambia.**

También puede conectarse el LED en otro Pin de la Raspberry, pero en ese caso hay que cambiar el pin asignado en el archivo blink.c:
~~~
#define 0 LED
~~~

### Control de motores de rotación continua

El servomotor no debe ser alimentado directamente por la Raspberry Pi porque en caso de que requiera realizar un esfuerzo grande puede requerir un consumo que no puede proporcionar la Raspberry. La alimentación del servo se realizará externamente y SOLO el pin de control del motor (PWM) se conectará a la Raspberry.

![Esquema de conexión del servomotor](figuras/Servomotor.png)




#### Tarea 3: Control por pwm de servomotres usando C++}
Para controlar los motores por pwm en C o C++ se puede usar la biblioteca WiringPi. Al igual que en python conviene tener en cuenta los siguientes aspectos:
- Es necesario incluir la librería en el código:
  ~~~
         #include <wiringPi.h>
   ~~~
- La biblioteca WiringPi usa otra numeración de pines, como se indicaba anteriormente. Los pines correspondientes según la numeración WiringPi a los *pwm_0* y *pwm_1* son:
    ~~~
        #define PinMotor0 23 //pwm_1
        #define PinMotor1 26 //for pwm_0
    ~~~
- Es necesario inicializar la librería
- Hay que configurar el pin como un pin pwm de salida
- Para dar un valor de pwm a la salida correspondiente basta con usar el comando

**Establece el rango de valores de PWM sirven para girar en un sentido u otro. Con este rango podrás escoger un valor adecuado para usarlo en el futuro control del robot.**


### Referencias útiles#
\begin{itemize}
    [Control de servomotor usando Python 1](https://www.digikey.es/en/maker/blogs/2021/how-to-control-servo-motors-with-a-raspberry-pi}{Control de servomotor usando Pyhton 1)
    [Control de servomotor usando Python 2](https://www.learnrobotics.org/blog/raspberry-pi-servo-motor/}{Control de servomotor usando Pyhton 2)
    [Referencia de WiringPi](http://wiringpi.com/reference/raspberry-pi-specifics/)
    [ Utilidad gpio de Adafruit](https://learn.adafruit.com/adafruits-raspberry-pi-lesson-8-using-a-servo-motor/software)

# PRÁCTICA 2. MOTORES Parte 2

La finalidad de esta práctica es aplicar una señal de PWM para
controlar los actuadores (servomotores de rotación continua) de un robot móvil sencillo.
Una vez que se mueven adecuadamente los dos motores del robot de tipo diferencial
construido, se realizarán varios movimientos para mostrar que se es capaz de controlar
adecuadamente el robot.

El robot utilizará dos servos de rotación contínua. Aseguraos de que están alineados adecuadamente y las ruedas están fijadas para que el robot pueda moverse en línea recta. Si el robot tiene una deriva alta, su control será más complejo e impedirá un correcto funcionamiento en futuras prácticas en las que se utilizará.


1. Colocad sobre la plataforma el circuito anterior y la Rasp Pi para mover los motores a la vez. 
2. Si el diseño y sujeción de los motores es el adecuado debe ser capaz de moverse en línea recta sin desviarse. Como mínimo debería recorrer un tramo de unos 2 m con una desviación máxima de un 10% hacia cualquiera de los dos lados.

## Control en lazo abierto

Una vez que se ha comprobado que sabe crear un programa utilizando la librería wiringPi, se ha configurado y calibrado los servos de las ruedas del robot y movido el robot de forma básica para verificar que la posición de los motores es la adecuada, el objetivo será crear un programa que mueva el robot una distancia y ángulo deseados. Como el robot no tiene realimentación de la posición cualquier error, deslizamiento o desviación no se detectará.

Para mover el robot una distancia deseada, mediremos la velocidad con que se desplaza el robot (velocidad de las ruedas) y moveremos el robot (siempre a la misma velocidad) durante el tiempo que corresponda para la distancia deseada. Es decir:
1. Poned las ruedas a girar y registrad el tiempo que tarda en dar 10 vueltas. Ese tiempo será T_i
2. Para obtener la velocidad de cada rueda por segundo bastará con realizar $v_i=\frac{T_i}{10}$ vueltas por segundo.
3. Es necesario medir el radio de las ruedas $R_i$ para poder calcular la longitud de su circunferencia $\Phi_i=2\cdot \pi \cdot R_i$ y así saber la distancia que recorre el robot en una vuelta.
4. Con estos datos ya se puede pasar una distancia recorrida por una rueda $D_i$ a número de vueltas $N_{vi}=\frac{D_i}{\Phi_i}$. Con la velocidad de la rueda $v_i$ que ya se ha calculado se puede sabeer el tiempo que ha de estar girando la rueda $Tiempo_i=\frac{N_{vi}}{v_i}$.
5. Este tiempo $Tiempo_i$ es que se debe esperar antes de parar los motores para que el robot mueva las ruedas la distancia deseada.


Con los cálculos anteriores puede obtener el tiempo para que cada rueda gire la distancia $D_i$ deseada ($i$ es derecha, $r$, o izquierda, $l$). Esta distancia $D_i$ se obtendrá de las ecuaciones $D_r$ y $D_l$ del modelo de un robot diferencial, donde la distancia a desplazarse (o el ángulo que queremos que gire) es la proporcionada por el usuario.

### Movimiento en línea recta.
Programad  el robot para que se mueva una distancia determinada a partir del tiempo que está en funcionamiento los motores (control en lazo abierto). Probadlo con la distancia del apartado anterior y ved si es capaz de parar a los 2 m.

### Giro en lazo abierto
Programad el robot para que gire el número de ángulos determinado. Probadlo con $90$ grados.

## Movimiento complejo del robot

A continuación realizaremos, utilizando las ecuaciones anteriores, un movimiento algo más complejo donde se combinen simultáneamente movimientos rectos y giros. El movimiento que el robot realizará será un rectángulo.

### Movimientos complejos
El robot debería quedar en la misma posición que al principio. Estimar el error cometido. ¿Está dentro de una bola de error de radio 10 cm?

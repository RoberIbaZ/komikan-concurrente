# Torneo de Komikan concurrente

Servidor que recibe N clientes, los empareja de a dos (Puma contra Perros) y
juega las partidas en a lo sumo M mesas a la vez. Cada mesa tiene un hilo juez
y 13 hilos ficha (1 puma y 12 perros) que mueven sin turno fijo.

## Compilar

```
make            # ./torneo
make tsan       # ./torneo_tsan, con ThreadSanitizer
make clean
```

Flags: `gcc -Wall -Wextra -std=c17 -pthread`.

## Uso

```
./torneo N M [config.txt]
```

- `N`: cantidad de clientes (>= 1). Si es impar, el ultimo se retira sin jugar.
- `M`: maximo de mesas activas (>= 1). Los pares que sobran esperan bloqueados.
- `config.txt`: opcional. Sin el se usan los valores por defecto.

Ctrl+C (SIGINT) deja de aceptar clientes, aborta las mesas activas, espera a
todos los hilos, libera todo y termina con codigo 130.

Cada mesa deja su registro (tablero tras cada movimiento) en `logs/mesa_<id>.log`.

## Formato de config.txt

Una entrada `clave=valor` por linea, sin espacios. `#` inicia un comentario.
Una clave desconocida o un valor fuera de rango es un error y el programa no parte.

| clave            | por defecto     | descripcion                                         |
|------------------|-----------------|-----------------------------------------------------|
| `min_ms`         | 50              | retardo minimo entre movimientos de una ficha        |
| `max_ms`         | 300             | retardo maximo (debe ser >= `min_ms`)                |
| `seed`           | hora actual     | semilla, para repetir una corrida                    |
| `prob_fallo`     | 0               | probabilidad [0,1] de que una ficha falle por ciclo  |
| `llegada_max_ms` | 200             | retardo maximo de llegada de cada cliente            |
| `pos_puma`       | 12              | nodo inicial del puma (0..30)                        |
| `pos_perros`     | 10,14,15,...,24 | 12 nodos separados por coma                          |

Nodos: 0..24 son la cuadricula 5x5 (`fila*5 + col`) y 25..30 el trono, que se
une al tablero por el nodo 2. Ejemplo completo en `pruebas/config_ejemplo.txt`.

## Pruebas

```
make test
```

Corre `pruebas/correr_pruebas.sh` (torneo completo: N impar, M=1, N grande,
limite de mesas, argumentos y configs invalidas, SIGINT, fallos simulados,
ThreadSanitizer y fugas) y `pruebas_juego/probar.sh` (tablero y partidas
sueltas). Las fugas se revisan con `leaks` en macOS y con valgrind en Linux.

En Docker, desde la carpeta del repo:

```
docker run --rm --security-opt seccomp=unconfined -v "$PWD:/work" -w /work <imagen con gcc y valgrind> make test
```

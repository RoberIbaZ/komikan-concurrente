#!/bin/bash
cd "$(dirname "$0")" || exit 1
F="-Wall -Wextra -std=c17 -pthread -g"
S="../tablero.c ../juez.c stub_servidor.c test_partida.c"
mkdir -p bin
FALLOS=0

# prueba <nombre> <segundos> <comando...>
prueba() {
    nombre=$1; seg=$2; shift 2
    (cd bin && perl -e 'alarm shift; exec @ARGV' "$seg" "$@") > bin/salida.txt 2>&1
    if [ $? -eq 0 ]; then echo "PASS $nombre"; else echo "FAIL $nombre"; FALLOS=$((FALLOS+1)); fi
    grep -E "RESUMEN|latencia|test_tablero|leaks for" bin/salida.txt | sed 's/^/     /'
}

gcc $F ../tablero.c test_tablero.c -o bin/test_tablero || exit 1
gcc $F $S -o bin/test_partida || exit 1
gcc $F -fsanitize=thread $S -o bin/test_tsan || exit 1

prueba tablero 10 ./test_tablero
prueba "1 partida" 60 ./test_partida -n 1 -m 5 -M 20
prueba "8 partidas" 120 ./test_partida -n 8 -m 5 -M 20
prueba "fallos" 120 ./test_partida -n 8 -m 5 -M 20 -f 0.01
prueba "abortar" 20 ./test_partida -n 1 -m 100 -M 300 -a 300
prueba "TSan" 180 ./test_tsan -n 4 -m 5 -M 20 -f 0.002
grep -q "WARNING: ThreadSanitizer" bin/salida.txt && { echo "FAIL TSan reporto algo"; FALLOS=$((FALLOS+1)); }
prueba "leaks" 120 leaks --atExit -- ./test_partida -n 2 -m 5 -M 20 -f 0.002

[ $FALLOS -eq 0 ] && echo "TODO OK" || echo "$FALLOS fallos"
exit $FALLOS

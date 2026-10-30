#!/bin/bash
cd "$(dirname "$0")/.." || exit 1
FALLOS=0
mkdir -p /tmp/torneo_pruebas
OUT=/tmp/torneo_pruebas/salida.txt

# ok <nombre> <condicion>: PASS si el comando de la condicion tiene exito
ok() {
    if eval "$2"; then echo "PASS $1"; else echo "FAIL $1"; FALLOS=$((FALLOS+1)); fi
}
# correr <segundos> <comando...>: guarda la salida en $OUT y deja el codigo en $RC
correr() {
    seg=$1; shift
    perl -e 'alarm shift; exec @ARGV' "$seg" "$@" > $OUT 2>&1
    RC=$?
}

make -s torneo tsan || exit 1

correr 120 ./torneo 10 3 pruebas/config_rapido.txt
ok "N=10 M=3" "[ $RC -eq 0 ] && grep -q 'Resumen: partidas=5 ' $OUT"

correr 120 ./torneo 7 2 pruebas/config_rapido.txt
ok "N=7 M=2 (impar)" "[ $RC -eq 0 ] && grep -q 'no tiene rival y se retira' $OUT && grep -q 'Resumen: partidas=3 ' $OUT"

correr 20 ./torneo 1 1 pruebas/config_rapido.txt
ok "N=1 M=1" "[ $RC -eq 0 ] && grep -q 'no tiene rival' $OUT && grep -q 'Resumen: partidas=0 ' $OUT"

correr 300 ./torneo 40 4 pruebas/config_rapido.txt
ok "N=40 M=4" "[ $RC -eq 0 ] && grep -q 'Resumen: partidas=20 ' $OUT"

# nunca mas de M mesas activas: recorre la salida contando Inicio (+1) y Fin (-1)
MAX=$(awk '/Inicio de partida/{a++; if(a>max)max=a} /\] Fin:/{a--} END{print max+0}' $OUT)
ok "maximo $MAX mesas simultaneas (M=4)" "[ $MAX -le 4 ] && [ $MAX -ge 1 ]"

correr 10 ./torneo
ok "sin argumentos" "[ $RC -ne 0 ]"
correr 10 ./torneo 0 3
ok "N=0" "[ $RC -ne 0 ]"
correr 10 ./torneo 5 abc
ok "M no numerico" "[ $RC -ne 0 ]"
correr 10 ./torneo 4 2 pruebas/no_existe.txt
ok "config inexistente" "[ $RC -ne 0 ]"
correr 10 ./torneo 4 2 pruebas/config_invalido.txt
ok "config invalida" "[ $RC -ne 0 ]"

printf 'min_ms=5\nclave_rara=1\n' > /tmp/torneo_pruebas/rara.txt
correr 10 ./torneo 4 2 /tmp/torneo_pruebas/rara.txt
ok "clave desconocida (linea 2)" "[ $RC -ne 0 ] && grep -q ':2:' $OUT"

# SIGINT a mitad: debe terminar en menos de 3 s
# un proceso en segundo plano hereda SIGINT ignorado; perl lo restaura
perl -e '$SIG{INT}="DEFAULT"; exec @ARGV' ./torneo 40 4 pruebas/config_lenta.txt > $OUT 2>&1 &
PID=$!
sleep 1
INICIO=$(date +%s)
kill -INT $PID
wait $PID
RC=$?
DUR=$(( $(date +%s) - INICIO ))
ok "SIGINT (cierre en ${DUR}s, codigo $RC)" "[ $DUR -lt 3 ] && [ $RC -eq 130 ] && grep -q 'SIGINT recibido' $OUT && grep -q 'Resumen' $OUT"

correr 300 ./torneo 10 3 pruebas/config_fallos.txt
ok "fallos simulados" "[ $RC -eq 0 ] && grep -q 'Resumen: partidas=5 ' $OUT"

correr 300 ./torneo_tsan 10 3 pruebas/config_rapido.txt
ok "ThreadSanitizer" "[ $RC -eq 0 ] && ! grep -q 'WARNING: ThreadSanitizer' $OUT"

if [ "$(uname)" = Darwin ]; then
    correr 300 leaks --atExit -- ./torneo 10 3 pruebas/config_rapido.txt
    ok "leaks" "grep -q ' 0 leaks for 0 total leaked bytes' $OUT"
    # con SIGINT: leaks lanza el programa como hijo; se le manda SIGINT por nombre
    (perl -e '$SIG{INT}="DEFAULT"; exec @ARGV' leaks --atExit -- ./torneo 40 4 pruebas/config_lenta.txt > $OUT 2>&1 &)
    sleep 2; pkill -INT -x torneo; sleep 4
    ok "leaks con SIGINT" "grep -q ' 0 leaks for 0 total leaked bytes' $OUT"
fi

[ $FALLOS -eq 0 ] && echo "TODO OK" || echo "$FALLOS fallos"
exit $FALLOS

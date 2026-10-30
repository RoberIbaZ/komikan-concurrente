CC = gcc
CFLAGS = -Wall -Wextra -std=c17 -pthread -g -D_DEFAULT_SOURCE
FUENTES = main.c servidor.c config.c juez.c tablero.c

torneo: $(FUENTES) komikan.h servidor.h tablero.h
	$(CC) $(CFLAGS) $(FUENTES) -o torneo

tsan: $(FUENTES) komikan.h servidor.h tablero.h
	$(CC) $(CFLAGS) -fsanitize=thread $(FUENTES) -o torneo_tsan

test: torneo tsan
	./pruebas/correr_pruebas.sh
	./pruebas_juego/probar.sh

clean:
	rm -rf torneo torneo_tsan torneo.dSYM torneo_tsan.dSYM logs

.PHONY: tsan test clean

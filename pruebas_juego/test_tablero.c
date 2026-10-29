#include <stdio.h>
#include "../tablero.h"

static int fallos;
#define CHECK(c) do { if (!(c)) { printf("FALLO linea %d: %s\n", __LINE__, #c); fallos++; } } while (0)

int main(void)
{
    int aristas = 0;

    CHECK(NUM_NODOS == 31);
    for (int a = 0; a < NUM_NODOS; a++)
        for (int b = 0; b < NUM_NODOS; b++) {
            CHECK(tablero_adyacentes(a, b) == tablero_adyacentes(b, a));
            aristas += tablero_adyacentes(a, b);
        }
    CHECK(aristas / 2 == 66);

    CHECK(tablero_salto(0, 1) == 2);
    CHECK(tablero_salto(0, 6) == 12);
    CHECK(tablero_salto(12, 7) == 2);
    CHECK(tablero_salto(2, 26) == 29);
    CHECK(tablero_salto(28, 29) == 30);
    CHECK(tablero_salto(25, 26) == 27);
    CHECK(tablero_salto(0, 2) == -1);    /* no adyacentes */
    CHECK(tablero_salto(0, 5) == 10);
    CHECK(tablero_salto(1, 6) == 11);
    CHECK(tablero_salto(1, 2) == 3);
    CHECK(tablero_salto(4, 3) == 2);
    CHECK(tablero_salto(24, 20) == -1);  /* no hay casilla mas alla */
    CHECK(tablero_salto(1, 5) == -1);    /* diagonal en nodo impar */
    CHECK(!tablero_adyacentes(24, 25));
    CHECK(!tablero_adyacentes(-1, 3));

    Config cfg = { .pos_puma = -1 };
    int puma, perros[NUM_PERROS];
    for (int i = 0; i < NUM_PERROS; i++) cfg.pos_perros[i] = -1;
    CHECK(tablero_posiciones_iniciales(&cfg, &puma, perros) == 0 && puma == 12);
    cfg.pos_perros[0] = 12;
    CHECK(tablero_posiciones_iniciales(&cfg, &puma, perros) == -1);

    int celda[NUM_NODOS];
    for (int i = 0; i < NUM_NODOS; i++) celda[i] = -1;
    celda[12] = 0;
    celda[17] = 3;
    tablero_imprimir(stdout, celda);
    printf(fallos ? "test_tablero: %d fallos\n" : "test_tablero: OK\n", fallos);
    return fallos != 0;
}

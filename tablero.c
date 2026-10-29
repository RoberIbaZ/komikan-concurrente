#include <stdlib.h>
#include <string.h>
#include "tablero.h"

/* 0..24: cuadricula (fila*5+col); 25..30: trono */
static const int FILA[NUM_NODOS] = {
    0,0,0,0,0, 1,1,1,1,1, 2,2,2,2,2, 3,3,3,3,3, 4,4,4,4,4, -1,-1,-1,-2,-2,-2 };
static const int COL[NUM_NODOS] = {
    0,1,2,3,4, 0,1,2,3,4, 0,1,2,3,4, 0,1,2,3,4, 0,1,2,3,4, 1,2,3,0,2,4 };
static const int TRONO[10][2] = {
    {2,25},{2,26},{2,27},{25,26},{26,27},{25,28},{26,29},{27,30},{28,29},{29,30} };
static const int PERROS_DEFECTO[NUM_PERROS] = { 10,14,15,16,17,18,19,20,21,22,23,24 };

int tablero_nodo(int fila, int col)
{
    for (int i = 0; i < NUM_NODOS; i++)
        if (FILA[i] == fila && COL[i] == col)
            return i;
    return -1;
}

void tablero_coords(int nodo, int *fila, int *col)
{
    *fila = FILA[nodo];
    *col = COL[nodo];
}

int tablero_adyacentes(int a, int b)
{
    if (a < 0 || b < 0 || a >= NUM_NODOS || b >= NUM_NODOS || a == b)
        return 0;
    for (int i = 0; i < 10; i++)
        if ((TRONO[i][0] == a && TRONO[i][1] == b) || (TRONO[i][0] == b && TRONO[i][1] == a))
            return 1;
    if (a >= 25 || b >= 25)
        return 0;
    int df = abs(FILA[a] - FILA[b]), dc = abs(COL[a] - COL[b]);
    return df + dc == 1 || (df == 1 && dc == 1 && (FILA[a] + COL[a]) % 2 == 0);
}

int tablero_salto(int a, int b)
{
    if (!tablero_adyacentes(a, b))
        return -1;
    int c = tablero_nodo(2 * FILA[b] - FILA[a], 2 * COL[b] - COL[a]);
    return c >= 0 && tablero_adyacentes(b, c) ? c : -1;
}

int tablero_posiciones_iniciales(const Config *cfg, int *puma, int perros[NUM_PERROS])
{
    int usado[NUM_NODOS] = { 0 };
    int p = cfg->pos_puma == -1 ? 12 : cfg->pos_puma;

    if (p < 0 || p >= NUM_NODOS)
        return -1;
    usado[p] = 1;
    for (int i = 0; i < NUM_PERROS; i++) {
        int d = cfg->pos_perros[i] == -1 ? PERROS_DEFECTO[i] : cfg->pos_perros[i];
        if (d < 0 || d >= NUM_NODOS || usado[d])
            return -1;
        usado[d] = 1;
        perros[i] = d;
    }
    *puma = p;
    return 0;
}

void tablero_imprimir(FILE *f, const int celda[NUM_NODOS])
{
    char lin[7][24];

    memset(lin, ' ', sizeof lin);
    for (int i = 0; i < NUM_NODOS; i++)
        lin[FILA[i] + 2][2 + COL[i] * 4] =
            celda[i] < 0 ? '.' : (celda[i] == 0 ? 'P' : 'D');
    for (int r = 0; r < 7; r++)
        fprintf(f, "%.23s%s\n", lin[r], r < 2 ? " (trono)" : "");
}

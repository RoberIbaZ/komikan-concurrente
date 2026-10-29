#ifndef TABLERO_H
#define TABLERO_H

#include <stdio.h>
#include "komikan.h"

int  tablero_nodo(int fila, int col);          /* -1 si no existe */
void tablero_coords(int nodo, int *fila, int *col);
int  tablero_adyacentes(int a, int b);
/* Destino de saltar desde a sobre b, o -1 si no es una recta valida. */
int  tablero_salto(int a, int b);
/* 0 si las posiciones (con los -1 completados por defecto) son validas. */
int  tablero_posiciones_iniciales(const Config *cfg, int *puma, int perros[NUM_PERROS]);
/* celda[n]: -1 vacio, 0 puma, >0 perro */
void tablero_imprimir(FILE *f, const int celda[NUM_NODOS]);

#endif

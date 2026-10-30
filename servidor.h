#ifndef SERVIDOR_H
#define SERVIDOR_H

#include "komikan.h"

/* Atiende n clientes con a lo sumo m mesas simultaneas. Retorna 0 si ok. */
int  servidor_correr(int n, int m, const Config *cfg);

/* Pide cerrar el torneo (llamada desde el hilo de senales). */
void servidor_apagar(void);

/* Destruye mx y cv; solo despues de terminar el hilo de senales. */
void servidor_liberar(void);

#endif

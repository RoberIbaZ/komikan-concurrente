/* Reemplaza a config.c y servidor.c SOLO para probar la Parte A. */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include "../komikan.h"

atomic_int apagando = 0;

int config_cargar(const char *ruta, Config *cfg)
{
    (void)ruta;
    *cfg = (Config){ .min_ms = 50, .max_ms = 300, .seed = 1, .pos_puma = -1 };
    for (int i = 0; i < NUM_PERROS; i++)
        cfg->pos_perros[i] = -1;
    return 0;
}

void config_imprimir(const Config *cfg)
{
    printf("min=%d max=%d seed=%u prob_fallo=%.3f\n", cfg->min_ms, cfg->max_ms,
           cfg->seed, cfg->prob_fallo);
}

int fallo_simulado(const Config *cfg, unsigned *seedp)
{
    return cfg->prob_fallo > 0 && (double)rand_r(seedp) / RAND_MAX < cfg->prob_fallo;
}

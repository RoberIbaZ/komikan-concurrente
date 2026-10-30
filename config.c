#include "komikan.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Convierte s a entero en [min,max]. Retorna 0 si es valido. */
static int entero(const char *s, long min, long max, long *out)
{
    char *fin;
    errno = 0;
    long v = strtol(s, &fin, 10);
    if (fin == s || *fin != '\0' || errno || v < min || v > max)
        return -1;
    *out = v;
    return 0;
}

/* Aplica una linea "clave=valor". Retorna 0 si es valida. */
static int aplicar(Config *cfg, char *clave, char *valor)
{
    long v;
    if (strcmp(clave, "min_ms") == 0) {
        if (entero(valor, 0, 3600000, &v)) return -1;
        cfg->min_ms = v;
    } else if (strcmp(clave, "max_ms") == 0) {
        if (entero(valor, 0, 3600000, &v)) return -1;
        cfg->max_ms = v;
    } else if (strcmp(clave, "llegada_max_ms") == 0) {
        if (entero(valor, 0, 3600000, &v)) return -1;
        cfg->llegada_max_ms = v;
    } else if (strcmp(clave, "seed") == 0) {
        if (entero(valor, 0, 4294967295L, &v)) return -1;
        cfg->seed = v;
    } else if (strcmp(clave, "prob_fallo") == 0) {
        char *fin;
        cfg->prob_fallo = strtod(valor, &fin);
        if (fin == valor || *fin != '\0' || cfg->prob_fallo < 0 || cfg->prob_fallo > 1)
            return -1;
    } else if (strcmp(clave, "pos_puma") == 0) {
        if (entero(valor, 0, NUM_NODOS - 1, &v)) return -1;
        cfg->pos_puma = v;
    } else if (strcmp(clave, "pos_perros") == 0) {
        char *resto = valor;
        for (int i = 0; i < NUM_PERROS; i++) {
            char *coma = strchr(resto, ',');
            if (coma)
                *coma = '\0';
            if (entero(resto, 0, NUM_NODOS - 1, &v)) return -1;
            cfg->pos_perros[i] = v;
            if (!coma)
                return i == NUM_PERROS - 1 ? 0 : -1;
            resto = coma + 1;
        }
        return -1; /* sobraron valores */
    } else {
        return -1; /* clave desconocida */
    }
    return 0;
}

int config_cargar(const char *ruta, Config *cfg)
{
    cfg->min_ms = 50;
    cfg->max_ms = 300;
    cfg->seed = (unsigned)time(NULL);
    cfg->prob_fallo = 0;
    cfg->llegada_max_ms = 200;
    cfg->pos_puma = -1;
    for (int i = 0; i < NUM_PERROS; i++)
        cfg->pos_perros[i] = -1;
    if (!ruta)
        return 0;

    FILE *f = fopen(ruta, "r");
    if (!f) {
        fprintf(stderr, "config: no se pudo abrir '%s'\n", ruta);
        return -1;
    }
    char linea[512];
    int nro = 0, error = 0;
    while (!error && fgets(linea, sizeof linea, f)) {
        nro++;
        char *com = strchr(linea, '#');
        if (com)
            *com = '\0';
        linea[strcspn(linea, "\r\n")] = '\0';
        if (linea[strspn(linea, " \t")] == '\0')
            continue; /* linea vacia */
        char *igual = strchr(linea, '=');
        if (!igual) {
            error = 1;
        } else {
            *igual = '\0';
            /* sin espacios: "clave=valor" */
            error = aplicar(cfg, linea, igual + 1) != 0;
        }
        if (error)
            fprintf(stderr, "config: %s:%d: linea invalida\n", ruta, nro);
    }
    fclose(f);
    if (error)
        return -1;
    if (cfg->min_ms > cfg->max_ms) {
        fprintf(stderr, "config: min_ms no puede ser mayor que max_ms\n");
        return -1;
    }
    return 0;
}

void config_imprimir(const Config *cfg)
{
    printf("Configuracion: min_ms=%d max_ms=%d seed=%u prob_fallo=%.3f llegada_max_ms=%d\n",
           cfg->min_ms, cfg->max_ms, cfg->seed, cfg->prob_fallo, cfg->llegada_max_ms);
    if (cfg->pos_puma >= 0) {
        printf("  pos_puma=%d pos_perros=", cfg->pos_puma);
        for (int i = 0; i < NUM_PERROS; i++)
            printf("%s%d", i ? "," : "", cfg->pos_perros[i]);
        printf("\n");
    }
}

int fallo_simulado(const Config *cfg, unsigned *seedp)
{
    return rand_r(seedp) % 10000 < cfg->prob_fallo * 10000;
}

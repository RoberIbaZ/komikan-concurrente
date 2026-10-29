/* uso: test_partida [-n partidas] [-s seed] [-f prob_fallo] [-m min_ms]
 *                   [-M max_ms] [-a ms_para_abortar_la_mesa_0] */
#define _POSIX_C_SOURCE 200809L
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include "../komikan.h"

#define MAX_MESAS 64

static int fds_abiertos(void)
{
    int n = 0;
    for (int fd = 0; fd < 256; fd++)
        if (fcntl(fd, F_GETFD) != -1)
            n++;
    return n;
}

static double ahora_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

int main(int argc, char **argv)
{
    Config cfg;
    int n = 1, abortar_ms = -1, opt, cuenta[6] = { 0 }, mal = 0;
    Mesa *mesas[MAX_MESAS];
    pthread_t jueces[MAX_MESAS];

    config_cargar(NULL, &cfg);
    while ((opt = getopt(argc, argv, "n:s:f:m:M:a:")) != -1) {
        switch (opt) {
        case 'n': n = atoi(optarg); break;
        case 's': cfg.seed = (unsigned)atoi(optarg); break;
        case 'f': cfg.prob_fallo = atof(optarg); break;
        case 'm': cfg.min_ms = atoi(optarg); break;
        case 'M': cfg.max_ms = atoi(optarg); break;
        case 'a': abortar_ms = atoi(optarg); break;
        default: return 2;
        }
    }
    if (n < 1 || n > MAX_MESAS)
        return 2;
    config_imprimir(&cfg);

    int fds = fds_abiertos();
    for (int i = 0; i < n; i++) {
        mesas[i] = mesa_crear(i, &cfg, 100 + i, 200 + i);
        if (!mesas[i] || pthread_create(&jueces[i], NULL, juez_run, mesas[i]) != 0) {
            fprintf(stderr, "no se pudo crear la mesa %d\n", i);
            return 1;
        }
    }
    double t_abort = 0;
    if (abortar_ms >= 0) {
        usleep(abortar_ms * 1000);
        t_abort = ahora_ms();
        mesa_abortar(mesas[0]);
    }
    for (int i = 0; i < n; i++) {
        pthread_join(jueces[i], NULL);
        if (i == 0 && abortar_ms >= 0) {
            double lat = ahora_ms() - t_abort;
            printf("latencia de aborto: %.0f ms\n", lat);
            if (mesa_resultado(mesas[0]) != GANA_NADIE || lat > 1000)
                mal = 1;
        }
        cuenta[mesa_resultado(mesas[i])]++;
        mesa_destruir(mesas[i]);
    }
    printf("RESUMEN nadie=%d puma=%d perros=%d puma_aband=%d perros_aband=%d empate=%d\n",
           cuenta[GANA_NADIE], cuenta[GANA_PUMA], cuenta[GANA_PERROS],
           cuenta[GANA_PUMA_ABANDONO], cuenta[GANA_PERROS_ABANDONO], cuenta[EMPATE]);
    if (fds_abiertos() != fds) {
        printf("FALLO: descriptores abiertos %d -> %d\n", fds, fds_abiertos());
        mal = 1;
    }
    return mal;
}

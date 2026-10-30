#define _POSIX_C_SOURCE 200809L
#include "servidor.h"

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

atomic_int apagando = 0;

typedef struct {
    int id, puma, perros;
} Par;

static pthread_mutex_t mx = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cv = PTHREAD_COND_INITIALIZER;
static sem_t *mesas;            /* cuenta las mesas libres (empieza en M) */
static const Config *cfg;
static int n_clientes;
static int *cola, ini, fin;     /* clientes esperando pareja */
static int llegados;
static Mesa **activas;          /* activas[id-1] != NULL mientras la mesa juega */
static int partidas, v_puma, v_perros, abandonos, empates, abortadas;

static void *cliente(void *arg)
{
    int i = *(int *)arg;
    unsigned seed = cfg->seed + i;
    struct timespec t;

    clock_gettime(CLOCK_REALTIME, &t);
    long ms = rand_r(&seed) % (cfg->llegada_max_ms + 1);
    t.tv_sec += ms / 1000;
    t.tv_nsec += (ms % 1000) * 1000000L;
    if (t.tv_nsec >= 1000000000L) {
        t.tv_sec++;
        t.tv_nsec -= 1000000000L;
    }

    pthread_mutex_lock(&mx);
    while (!apagando && pthread_cond_timedwait(&cv, &mx, &t) != ETIMEDOUT)
        ;
    if (!apagando) {
        cola[fin++] = i;
        llegados++;
        printf("Cliente %d llega al servidor\n", i);
        pthread_cond_broadcast(&cv);
    }
    pthread_mutex_unlock(&mx);
    return NULL;
}

static void *partida(void *arg)
{
    Par *p = arg;
    Mesa *mesa = mesa_crear(p->id, cfg, p->puma, p->perros);

    if (!mesa) {
        fprintf(stderr, "[Mesa %d] No se pudo crear la mesa\n", p->id);
        sem_post(mesas);
        return NULL;
    }
    pthread_mutex_lock(&mx);
    activas[p->id - 1] = mesa;
    pthread_mutex_unlock(&mx);

    juez_run(mesa);

    Resultado r = mesa_resultado(mesa);
    printf("[Mesa %d] Cliente %d (Puma) vs Cliente %d (Perros): %s\n", p->id, p->puma,
           p->perros, resultado_str(r));

    pthread_mutex_lock(&mx);
    partidas++;
    if (r == GANA_PUMA)
        v_puma++;
    else if (r == GANA_PERROS)
        v_perros++;
    else if (r == GANA_PUMA_ABANDONO || r == GANA_PERROS_ABANDONO)
        abandonos++;
    else if (r == EMPATE)
        empates++;
    else
        abortadas++;
    activas[p->id - 1] = NULL; /* desde aqui servidor_apagar ya no la ve */
    pthread_mutex_unlock(&mx);

    mesa_destruir(mesa);
    sem_post(mesas);
    return NULL;
}

void servidor_apagar(void)
{
    atomic_store(&apagando, 1);
    pthread_mutex_lock(&mx);
    pthread_cond_broadcast(&cv);
    if (activas)
        for (int i = 0; i < n_clientes / 2; i++)
            if (activas[i])
                mesa_abortar(activas[i]);
    if (mesas)
        sem_post(mesas); /* despierta al emparejador si espera mesa */
    pthread_mutex_unlock(&mx);
}

int servidor_correr(int n, int m, const Config *c)
{
    char nombre[64];
    snprintf(nombre, sizeof nombre, "/torneo_%d", (int)getpid());
    sem_t *sem = sem_open(nombre, O_CREAT | O_EXCL, 0600, m);
    if (sem == SEM_FAILED) {
        perror("sem_open");
        return -1;
    }
    sem_unlink(nombre);

    cfg = c;
    n_clientes = n;
    cola = calloc(n, sizeof *cola);
    int *ids = calloc(n, sizeof *ids);
    pthread_t *hilos_cliente = calloc(n, sizeof *hilos_cliente);
    Mesa **mesas_activas = calloc(n / 2 + 1, sizeof *mesas_activas);
    Par *pares = calloc(n / 2 + 1, sizeof *pares);
    pthread_t *hilos_partida = calloc(n / 2 + 1, sizeof *hilos_partida);
    if (!cola || !ids || !hilos_cliente || !mesas_activas || !pares || !hilos_partida) {
        perror("calloc");
        return -1;
    }
    activas = mesas_activas;
    mesas = sem;

    int creados = 0;
    for (int i = 0; i < n; i++) {
        ids[i] = i + 1;
        if (pthread_create(&hilos_cliente[creados], NULL, cliente, &ids[i]) == 0) {
            creados++;
        } else {
            perror("pthread_create");
            pthread_mutex_lock(&mx);
            llegados++; /* este cliente nunca llegara; no hay que esperarlo */
            pthread_mutex_unlock(&mx);
        }
    }

    int npartidas = 0;
    pthread_mutex_lock(&mx);
    while (1) {
        while (!apagando && fin - ini < 2 && llegados < n)
            pthread_cond_wait(&cv, &mx);
        if (apagando)
            break;
        if (fin - ini < 2) {
            if (fin - ini == 1)
                printf("Cliente %d no tiene rival y se retira\n", cola[ini]);
            break;
        }
        Par *p = &pares[npartidas];
        p->id = npartidas + 1;
        p->puma = cola[ini++];
        p->perros = cola[ini++];
        pthread_mutex_unlock(&mx);

        while (sem_wait(sem) == -1 && errno == EINTR)
            ;
        if (apagando) {
            sem_post(sem);
            pthread_mutex_lock(&mx);
            break;
        }
        if (pthread_create(&hilos_partida[npartidas], NULL, partida, p) == 0) {
            npartidas++;
        } else {
            perror("pthread_create");
            sem_post(sem);
        }
        pthread_mutex_lock(&mx);
    }
    pthread_mutex_unlock(&mx);

    for (int i = 0; i < creados; i++)
        pthread_join(hilos_cliente[i], NULL);
    for (int i = 0; i < npartidas; i++)
        pthread_join(hilos_partida[i], NULL);

    printf("Resumen: partidas=%d victorias_puma=%d victorias_perros=%d abandonos=%d "
           "empates=%d abortadas=%d\n",
           partidas, v_puma, v_perros, abandonos, empates, abortadas);

    pthread_mutex_lock(&mx);
    activas = NULL;
    mesas = NULL;
    pthread_mutex_unlock(&mx);
    sem_close(sem);
    free(cola);
    free(ids);
    free(hilos_cliente);
    free(mesas_activas);
    free(pares);
    free(hilos_partida);
    return 0;
}

void servidor_liberar(void)
{
    pthread_mutex_destroy(&mx);
    pthread_cond_destroy(&cv);
}

#define _POSIX_C_SOURCE 200809L
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

#include "servidor.h"

static sigset_t set;
static int hubo_sigint;

static void *hilo_senales(void *arg)
{
    int sig;
    (void)arg;
    sigwait(&set, &sig);
    hubo_sigint = 1;
    printf("SIGINT recibido: cerrando torneo...\n");
    servidor_apagar();
    return NULL;
}

static int leer_entero(const char *s)
{
    char *fin;
    long v = strtol(s, &fin, 10);
    return (*s == '\0' || *fin != '\0' || v < 1 || v > 100000) ? -1 : (int)v;
}

int main(int argc, char **argv)
{
    if (argc < 3 || argc > 4) {
        fprintf(stderr, "Uso: %s N M [config.txt]\n", argv[0]);
        return 1;
    }
    int n = leer_entero(argv[1]), m = leer_entero(argv[2]);
    if (n < 1 || m < 1) {
        fprintf(stderr, "N y M deben ser enteros >= 1\n");
        return 1;
    }
    Config cfg;
    if (config_cargar(argc == 4 ? argv[3] : NULL, &cfg) != 0)
        return 1;
    config_imprimir(&cfg);

    /* SIGINT bloqueado antes de crear hilos: todos lo heredan bloqueado */
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    pthread_sigmask(SIG_BLOCK, &set, NULL);
    pthread_t senales;
    pthread_create(&senales, NULL, hilo_senales, NULL);

    int rc = servidor_correr(n, m, &cfg);

    pthread_cancel(senales);
    pthread_join(senales, NULL);
    servidor_liberar();
    if (rc != 0)
        return 1;
    return hubo_sigint ? 130 : 0;
}

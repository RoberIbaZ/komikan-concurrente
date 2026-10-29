#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <pthread.h>
#include "tablero.h"

#define MAX_MOVIMIENTOS 2000
#define NUM_FICHAS      (1 + NUM_PERROS)   /* 0 = puma, 1..12 = perros */
#define PROB_QUIETO     20   /* % de ciclos en que un perro no se mueve */
#define PROB_CAPTURA    50   /* % de veces que el puma toma una captura disponible */
enum { ACTIVA, CAPTURADA, FALLIDA };

typedef struct { Mesa *m; int id; unsigned seed; } Arg;

/* Un solo mutex y una sola cond por mesa protegen todo el estado. Solo la
 * ficha con el turno puede pedir un movimiento; el juez lo valida y aplica. */
struct Mesa {
    int id, cliente_puma, cliente_perros;
    Config cfg;
    pthread_mutex_t mx;
    pthread_cond_t cv;
    int celda[NUM_NODOS], pos[NUM_FICHAS], estado[NUM_FICHAS];
    int turno;                              /* ficha que tiene el tablero, -1 libre */
    int hay_sol, sol_ficha, sol_origen, sol_destino;
    int movs, capturas, terminada, abortar;
    Resultado res;
    FILE *log;
    pthread_t hilos[NUM_FICHAS];
    Arg args[NUM_FICHAS];
};

static pthread_mutex_t g_print = PTHREAD_MUTEX_INITIALIZER;

static void out(const char *fmt, ...)
{
    va_list ap;

    pthread_mutex_lock(&g_print);
    va_start(ap, fmt);
    vfprintf(stdout, fmt, ap);
    va_end(ap);
    fputc('\n', stdout);
    fflush(stdout);
    pthread_mutex_unlock(&g_print);
}

static void log_linea(Mesa *m, const char *fmt, ...)
{
    va_list ap;

    if (!m->log)
        return;
    va_start(ap, fmt);
    vfprintf(m->log, fmt, ap);
    va_end(ap);
    fputc('\n', m->log);
}

const char *resultado_str(Resultado r)
{
    switch (r) {
    case GANA_NADIE:           return "Partida abortada, sin ganador";
    case GANA_PUMA:            return "Gana el Puma";
    case GANA_PERROS:          return "Ganan los Perros";
    case GANA_PUMA_ABANDONO:   return "Gana el Puma por abandono de un Perro";
    case GANA_PERROS_ABANDONO: return "Ganan los Perros por abandono del Puma";
    case EMPATE:               return "Empate (limite de movimientos)";
    }
    return "Resultado desconocido";
}

/* ------------------------------ fichas ------------------------------ */

static void esperar_fin_de_pausa(Mesa *m, int ms)
{
    struct timespec ts;

    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += ms / 1000;
    ts.tv_nsec += (long)(ms % 1000) * 1000000L;
    if (ts.tv_nsec >= 1000000000L) {
        ts.tv_sec++;
        ts.tv_nsec -= 1000000000L;
    }
    while (!m->terminada)
        if (pthread_cond_timedwait(&m->cv, &m->mx, &ts) == ETIMEDOUT)
            break;
}

static int dist2(int a, int b)
{
    int fa, ca, fb, cb;

    tablero_coords(a, &fa, &ca);
    tablero_coords(b, &fb, &cb);
    return (fa - fb) * (fa - fb) + (ca - cb) * (ca - cb);
}

/* Elige un movimiento (con mx tomado). Retorna 1 si hay uno. */
static int elegir(Mesa *m, Arg *a, int *destino)
{
    int pos = m->pos[a->id], pp = m->pos[0];
    int opc[NUM_NODOS], n = 0, buenas[NUM_NODOS], nb = 0;

    if (a->id == 0) {
        for (int b = 0; b < NUM_NODOS; b++) {
            int c = tablero_salto(pos, b);
            if (m->celda[b] > 0 && c >= 0 && m->celda[c] < 0)
                opc[n++] = c;
        }
        if (n > 0 && rand_r(&a->seed) % 100 < PROB_CAPTURA) {
            *destino = opc[rand_r(&a->seed) % n];
            return 1;
        }
        n = 0;
    }
    if (a->id > 0 && rand_r(&a->seed) % 100 < PROB_QUIETO)
        return 0;
    for (int b = 0; b < NUM_NODOS; b++) {
        if (!tablero_adyacentes(pos, b) || m->celda[b] >= 0)
            continue;
        opc[n++] = b;
        int c = tablero_salto(pp, b);
        int capturable = c >= 0 && (m->celda[c] < 0 || c == pos);
        if (a->id > 0 && !capturable && dist2(b, pp) < dist2(pos, pp))
            buenas[nb++] = b;
    }
    if (nb > 0) {
        *destino = buenas[rand_r(&a->seed) % nb];
        return 1;
    }
    if (n > 0)
        *destino = opc[rand_r(&a->seed) % n];
    return n > 0;
}

static void *hilo_ficha(void *p)
{
    Arg *a = p;
    Mesa *m = a->m;
    int mn = m->cfg.min_ms < 0 ? 0 : m->cfg.min_ms;
    int mx = m->cfg.max_ms < mn ? mn : m->cfg.max_ms;

    pthread_mutex_lock(&m->mx);
    while (!m->terminada && m->estado[a->id] == ACTIVA) {
        esperar_fin_de_pausa(m, mn + (int)(rand_r(&a->seed) % (unsigned)(mx - mn + 1)));
        while (m->turno != -1 && !m->terminada)
            pthread_cond_wait(&m->cv, &m->mx);
        if (m->terminada || m->estado[a->id] != ACTIVA)
            break;
        m->turno = a->id;
        if (fallo_simulado(&m->cfg, &a->seed)) {
            m->estado[a->id] = FALLIDA;      /* muere sin soltar el turno */
            pthread_cond_broadcast(&m->cv);
            pthread_mutex_unlock(&m->mx);
            pthread_exit(NULL);
        }
        int d;
        if (elegir(m, a, &d)) {
            m->sol_ficha = a->id;
            m->sol_origen = m->pos[a->id];
            m->sol_destino = d;
            m->hay_sol = 1;
            pthread_cond_broadcast(&m->cv);
            while (m->hay_sol && !m->terminada)
                pthread_cond_wait(&m->cv, &m->mx);
        }
        m->turno = -1;
        pthread_cond_broadcast(&m->cv);
    }
    pthread_mutex_unlock(&m->mx);
    return NULL;
}

/* ------------------------------- juez ------------------------------- */

static void log_tablero(Mesa *m)
{
    if (m->log)
        tablero_imprimir(m->log, m->celda);
}

static int puma_bloqueado(Mesa *m)
{
    int pos = m->pos[0];

    for (int b = 0; b < NUM_NODOS; b++) {
        int c = tablero_salto(pos, b);
        if (tablero_adyacentes(pos, b) && m->celda[b] < 0)
            return 0;
        if (m->celda[b] > 0 && c >= 0 && m->celda[c] < 0)
            return 0;
    }
    return 1;
}

/* Valida y aplica la solicitud; retorna 1 si es valida. */
static int aplicar(Mesa *m)
{
    int f = m->sol_ficha, o = m->sol_origen, d = m->sol_destino, medio = -1;

    if (m->pos[f] != o || m->celda[d] >= 0)
        return 0;
    if (!tablero_adyacentes(o, d)) {
        for (int b = 0; f == 0 && b < NUM_NODOS; b++)
            if (m->celda[b] > 0 && tablero_salto(o, b) == d)
                medio = b;
        if (medio < 0)
            return 0;
    }
    m->celda[o] = -1;
    m->celda[d] = f;
    m->pos[f] = d;
    m->movs++;
    if (medio < 0) {
        log_linea(m, "mov %d: ficha %d mueve %d->%d", m->movs, f, o, d);
        return 1;
    }
    int v = m->celda[medio];
    m->celda[medio] = -1;
    m->pos[v] = -1;
    m->estado[v] = CAPTURADA;
    m->capturas++;
    log_linea(m, "mov %d: Puma captura perro #%d (%d->%d->%d) [capturas: %d]", m->movs,
          v, o, medio, d, m->capturas);
    out("[Mesa %d] Puma captura perro #%d (%d->%d->%d) [capturas: %d]", m->id, v,
        o, medio, d, m->capturas);
    return 1;
}

static int fallida(Mesa *m)
{
    for (int i = 0; i < NUM_FICHAS; i++)
        if (m->estado[i] == FALLIDA)
            return i;
    return -1;
}

/* Con mx tomado. Retorna el resultado de la partida. */
static Resultado arbitrar(Mesa *m)
{
    struct timespec ts;
    int f;

    for (;;) {
        while (!m->hay_sol && !m->abortar && !atomic_load(&apagando) && fallida(m) < 0) {
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_nsec += 100 * 1000000L;
            if (ts.tv_nsec >= 1000000000L) {
                ts.tv_sec++;
                ts.tv_nsec -= 1000000000L;
            }
            pthread_cond_timedwait(&m->cv, &m->mx, &ts);
        }
        if (m->abortar || atomic_load(&apagando)) {
            out("[Mesa %d] Partida abortada", m->id);
            return GANA_NADIE;
        }
        if ((f = fallida(m)) >= 0) {
            if (m->turno == f) {
                m->turno = -1;
                log_linea(m, "liberado el turno retenido por la ficha %d", f);
            }
            out("[Mesa %d] Fallo detectado en la ficha %d", m->id, f);
            return f == 0 ? GANA_PERROS_ABANDONO : GANA_PUMA_ABANDONO;
        }
        if (!aplicar(m)) {
            out("[Mesa %d] Fallo detectado: movimiento invalido de la ficha %d",
                m->id, m->sol_ficha);
            return m->sol_ficha == 0 ? GANA_PERROS_ABANDONO : GANA_PUMA_ABANDONO;
        }
        log_tablero(m);
        m->hay_sol = 0;
        pthread_cond_broadcast(&m->cv);
        if (m->capturas >= CAPTURAS_VICTORIA)
            return GANA_PUMA;
        if (puma_bloqueado(m))
            return GANA_PERROS;
        if (m->movs >= MAX_MOVIMIENTOS)
            return EMPATE;
    }
}

void *juez_run(void *arg)
{
    Mesa *m = arg;
    char ruta[64];
    int creados = 0;

    mkdir("logs", 0755);
    snprintf(ruta, sizeof ruta, "logs/mesa_%d.log", m->id);
    m->log = fopen(ruta, "w");
    log_linea(m, "Mesa %d: min_ms=%d max_ms=%d seed=%u prob_fallo=%.4f", m->id,
          m->cfg.min_ms, m->cfg.max_ms, m->cfg.seed, m->cfg.prob_fallo);
    log_tablero(m);
    out("[Mesa %d] Inicio de partida (clientes: puma=%d, perros=%d)", m->id,
        m->cliente_puma, m->cliente_perros);

    while (creados < NUM_FICHAS &&
           pthread_create(&m->hilos[creados], NULL, hilo_ficha, &m->args[creados]) == 0)
        creados++;
    pthread_mutex_lock(&m->mx);
    if (creados < NUM_FICHAS)
        m->res = GANA_NADIE;
    else if (puma_bloqueado(m))
        m->res = GANA_PERROS;
    else
        m->res = arbitrar(m);
    m->terminada = 1;
    pthread_cond_broadcast(&m->cv);
    pthread_mutex_unlock(&m->mx);
    for (int i = 0; i < creados; i++)
        pthread_join(m->hilos[i], NULL);

    log_linea(m, "RESULTADO: %s (movimientos: %d, capturas: %d)", resultado_str(m->res),
          m->movs, m->capturas);
    pthread_mutex_lock(&g_print);
    printf("[Mesa %d] Fin: %s (movimientos: %d, capturas: %d)\n", m->id,
           resultado_str(m->res), m->movs, m->capturas);
    tablero_imprimir(stdout, m->celda);
    fflush(stdout);
    pthread_mutex_unlock(&g_print);
    if (m->log)
        fclose(m->log);
    m->log = NULL;
    return NULL;
}

/* ------------------------------ mesa ------------------------------- */

Mesa *mesa_crear(int id, const Config *cfg, int cliente_puma, int cliente_perros)
{
    int pp, pd[NUM_PERROS];
    Mesa *m;

    if (!cfg || tablero_posiciones_iniciales(cfg, &pp, pd) != 0)
        return NULL;
    m = calloc(1, sizeof *m);
    if (!m)
        return NULL;
    pthread_mutex_init(&m->mx, NULL);
    pthread_cond_init(&m->cv, NULL);
    m->id = id;
    m->cfg = *cfg;
    m->cliente_puma = cliente_puma;
    m->cliente_perros = cliente_perros;
    m->turno = -1;
    m->res = GANA_NADIE;
    for (int i = 0; i < NUM_NODOS; i++)
        m->celda[i] = -1;
    m->pos[0] = pp;
    m->celda[pp] = 0;
    for (int i = 0; i < NUM_PERROS; i++) {
        m->pos[i + 1] = pd[i];
        m->celda[pd[i]] = i + 1;
    }
    for (int i = 0; i < NUM_FICHAS; i++) {
        m->args[i] = (Arg){ m, i, cfg->seed * 2654435761u + (unsigned)(id * 97 + i * 31 + 1) };
    }
    return m;
}

void mesa_abortar(Mesa *m)
{
    pthread_mutex_lock(&m->mx);
    m->abortar = 1;
    pthread_cond_broadcast(&m->cv);
    pthread_mutex_unlock(&m->mx);
}

Resultado mesa_resultado(const Mesa *m)
{
    return m->res;
}

void mesa_destruir(Mesa *m)
{
    if (!m)
        return;
    pthread_cond_destroy(&m->cv);
    pthread_mutex_destroy(&m->mx);
    free(m);
}

/*
 * komikan.h - contrato compartido entre el nucleo del juego (tablero.c,
 * fichas.c, juez.c) y el servidor (main.c, servidor.c, config.c).
 */
#ifndef KOMIKAN_H
#define KOMIKAN_H

#include <signal.h>
#include <stdatomic.h>

#define NUM_NODOS   31   /* 25 de la cuadricula 5x5 + 6 del trono */
#define NUM_PERROS  12
#define CAPTURAS_VICTORIA 6

/* ---------- configuracion (config.c) ---------- */

typedef struct {
    int      min_ms;                 /* retardo minimo por movimiento */
    int      max_ms;                 /* retardo maximo por movimiento */
    unsigned seed;                   /* semilla para reproducibilidad */
    double   prob_fallo;             /* prob. de que una ficha falle en cada ciclo [0,1] */
    int      llegada_max_ms;         /* retardo maximo de llegada de cada cliente */
    int      pos_puma;               /* nodo inicial del puma */
    int      pos_perros[NUM_PERROS]; /* nodos iniciales de los perros */
} Config;

/* Llena cfg con valores por defecto y, si ruta != NULL, los sobreescribe con
 * los del archivo (formato clave=valor). Retorna 0 si ok, -1 si hay error. */
int  config_cargar(const char *ruta, Config *cfg);
void config_imprimir(const Config *cfg);

/* Hook de inyeccion de fallos: retorna 1 si la ficha debe "fallar" en este
 * ciclo segun cfg->prob_fallo. seedp es la semilla propia del hilo. */
int  fallo_simulado(const Config *cfg, unsigned *seedp);

/* ---------- estado global del servidor (servidor.c) ---------- */

/* 1 cuando llego SIGINT: nadie debe iniciar trabajo nuevo. */
extern atomic_int apagando;

/* ---------- mesa / juez (juez.c) ---------- */

typedef enum {
    GANA_NADIE = 0,      /* partida abortada (SIGINT) */
    GANA_PUMA,
    GANA_PERROS,
    GANA_PUMA_ABANDONO,  /* fallo un perro */
    GANA_PERROS_ABANDONO, /* fallo el puma */
    EMPATE               /* se alcanzo el limite de movimientos */
} Resultado;

typedef struct Mesa Mesa;   /* opaca para el servidor */

/* Crea la mesa con su tablero y primitivas. NULL si falla. */
Mesa     *mesa_crear(int id, const Config *cfg, int cliente_puma, int cliente_perros);

/* Cuerpo del hilo juez: lanza las fichas, arbitra hasta el fin y hace join
 * de todas. Recibe un Mesa* y retorna NULL. Al volver, la partida termino. */
void     *juez_run(void *mesa);

/* Pide terminar la partida lo antes posible (thread-safe, no bloquea). */
void      mesa_abortar(Mesa *m);

Resultado mesa_resultado(const Mesa *m);
const char *resultado_str(Resultado r);

/* Destruye primitivas y libera memoria. Solo despues de join del juez. */
void      mesa_destruir(Mesa *m);

#endif

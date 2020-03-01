
/*
 * main.c
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "cache.h"
#include "main.h"

static FILE *traceFile;
static int debug = FALSE;

int main(argc, argv) int argc;
char **argv;
{
  // Lectura de los argumentos de la línea de comando y establece los parámetros de la memoria cache
  parse_args(argc, argv);
  // Inicializa la memoria cache
  init_cache();
  // Pasa uno por uno las instrucciones de los archivos *.trace al simulador del cache
  play_trace(traceFile);
  // Imprime los resultados estadísticos de la simulación el cache
  print_stats();
}

/************************************************************/
/*
Parsea los argumentos pasados en la línea de comando, estos
argumentos son útiles para configurar el modelo de cache
que se quiere simular. Entre los diferentes argumentos están:
* -h: imprime el mensaje de ayuda (documentación)
* -bs <bs>: establece el tamaño de bloque de la memoria cache
* -us <us>: establece el tamaño de la memoria cache unificada
* -is <is>: establece el tamaño de la memoria cache de instrucciones
* -ds <ds>: establece el tamaño de la memoria cache de datos
* -a <a>: establece el tipo de asociatividad de la memoria cache
* -wb: establece la política de escritura del cache a write-back
* -wt: establece la política de escritura del cache a write-through
* -wa: establece la política de alocación de memoria a write-allocate
* -nw: establece la política de alocación de memoria a no-write-allocate 
*/
void parse_args(argc, argv) int argc;
char **argv;
{
  int arg_index, i, value;

  // explica al usuario de la línea de comando como llamar al programa
  if (argc < 2)
  {
    printf("usage:  sim <options> <trace file>\n");
    exit(-1);
  }

  /* parse the command line arguments */
  // primero busca para ver si entre toda la línea de argumentos pasados
  // no hay una llamada a -h, si lo hay sale de la ejecución con exit(0)
  for (i = 0; i < argc; i++)
    if (!strcmp(argv[i], "-h"))
    {
      printf("\t-h:  \t\tthis message\n\n");
      printf("\t-bs <bs>: \tset cache block size to <bs>\n");
      printf("\t-us <us>: \tset unified cache size to <us>\n");
      printf("\t-is <is>: \tset instruction cache size to <is>\n");
      printf("\t-ds <ds>: \tset data cache size to <ds>\n");
      printf("\t-a <a>: \tset cache associativity to <a>\n");
      printf("\t-wb: \t\tset write policy to write back\n");
      printf("\t-wt: \t\tset write policy to write through\n");
      printf("\t-wa: \t\tset allocation policy to write allocate\n");
      printf("\t-nw: \t\tset allocation policy to no write allocate\n");
      printf("\t--debug: \t\tset info prints for debugging\n");
      exit(0);
    }

  arg_index = 1;
  // ojo: vamos hasta argc - 1 porque el úlimo elemento debe ser el archivo *.trace
  while (arg_index != argc - 1)
  {

    /* set the cache simulator parameters */

    if (!strcmp(argv[arg_index], "-bs"))
    {
      value = atoi(argv[arg_index + 1]);
      set_cache_param(CACHE_PARAM_BLOCK_SIZE, value);
      arg_index += 2;
      continue;
    }

    if (!strcmp(argv[arg_index], "-us"))
    {
      value = atoi(argv[arg_index + 1]);
      set_cache_param(CACHE_PARAM_USIZE, value);
      arg_index += 2;
      continue;
    }

    if (!strcmp(argv[arg_index], "-is"))
    {
      value = atoi(argv[arg_index + 1]);
      set_cache_param(CACHE_PARAM_ISIZE, value);
      arg_index += 2;
      continue;
    }

    if (!strcmp(argv[arg_index], "-ds"))
    {
      value = atoi(argv[arg_index + 1]);
      set_cache_param(CACHE_PARAM_DSIZE, value);
      arg_index += 2;
      continue;
    }

    if (!strcmp(argv[arg_index], "-a"))
    {
      value = atoi(argv[arg_index + 1]);
      set_cache_param(CACHE_PARAM_ASSOC, value);
      arg_index += 2;
      continue;
    }

    if (!strcmp(argv[arg_index], "-wb"))
    {
      set_cache_param(CACHE_PARAM_WRITEBACK, value);
      arg_index += 1;
      continue;
    }

    if (!strcmp(argv[arg_index], "-wt"))
    {
      set_cache_param(CACHE_PARAM_WRITETHROUGH, value);
      arg_index += 1;
      continue;
    }

    if (!strcmp(argv[arg_index], "-wa"))
    {
      set_cache_param(CACHE_PARAM_WRITEALLOC, value);
      arg_index += 1;
      continue;
    }

    if (!strcmp(argv[arg_index], "-nw"))
    {
      set_cache_param(CACHE_PARAM_NOWRITEALLOC, value);
      arg_index += 1;
      continue;
    }

    if (!strcmp(argv[arg_index], "--debug"))
    {
      set_cache_param(CACHE_PARAM_DEBUG, value);
      debug = TRUE;
      arg_index += 1;
      continue;
    }

    printf("error:  unrecognized flag %s\n", argv[arg_index]);
    exit(-1);
  }

  dump_settings();

  /* open the trace file */
  // cuando sale del ciclo while, arg_index es el índice
  // del elemento donde debe leer la dirección del archivo
  // *.trace, recordar que traceFile esta declarado de forma
  // global por lo que no hace falta regresar nada como
  // resultado de la función
  traceFile = fopen(argv[arg_index], "r");

  return;
}
/************************************************************/

/************************************************************/

void play_trace(inFile)
    FILE *inFile;
{
  unsigned addr, data, access_type;
  int num_inst = 0;

  // la función read_trace_element regresa 0 cuando se alcanza
  // el final (EOF) del archivo leído. Por eso se puede utilizar
  // al interior de un while
  while (read_trace_element(inFile, &access_type, &addr))
  {

    // los valors de access type están definidos en main.h
    // estas instrucciones se ejecutan por cada línea de los
    // archivos *.trace
    switch (access_type)
    {
    case TRACE_DATA_LOAD:
    case TRACE_DATA_STORE:
    case TRACE_INST_LOAD:
      perform_access(addr, access_type);
      break;

    default:
      printf("skipping access, unknown type(%d)\n", access_type);
    }

    num_inst++;
    if (!(num_inst % PRINT_INTERVAL) && debug)
      printf("processed %d references\n", num_inst);
  }

  flush();
}
/************************************************************/

/************************************************************/
// esta función lee una línea a la vez de los archivos *.trace
int read_trace_element(inFile, access_type, addr)
    FILE *inFile;
unsigned *access_type, *addr;
{
  int result;
  char c;

  // la función fscanf asigna el resultado de %u a access_type
  // %x a addr y %c a c. El formato de lo que está leyendo es
  // "2 408ed4" por lo que el %c corresponde a \n o espacios
  // que se eliminan en el ciclo while interno
  result = fscanf(inFile, "%u %x%c", access_type, addr, &c);
  while (c != '\n')
  {
    result = fscanf(inFile, "%c", &c);
    if (result == EOF)
      break;
  }
  if (result != EOF)
    return (1);
  else
    return (0);
}
/************************************************************/

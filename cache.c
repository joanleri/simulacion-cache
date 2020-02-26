/*
 * cache.c
 */


#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "cache.h"
#include "main.h"

/* cache configuration parameters */
// En esta sección se están declarando variables globales
// que sirven para simular el cache. Estas se están
// inizializando a los valores default definidos en
// cache.h
static int cache_split = FALSE;
static int cache_usize = DEFAULT_CACHE_SIZE;
static int cache_isize = DEFAULT_CACHE_SIZE; 
static int cache_dsize = DEFAULT_CACHE_SIZE;
static int cache_block_size = DEFAULT_CACHE_BLOCK_SIZE;
static int words_per_block = DEFAULT_CACHE_BLOCK_SIZE / WORD_SIZE;
static int cache_assoc = DEFAULT_CACHE_ASSOC;
static int cache_writeback = DEFAULT_CACHE_WRITEBACK;
static int cache_writealloc = DEFAULT_CACHE_WRITEALLOC;
static int address_size = DEFAULT_ADDRESS_SIZE;

/* cache model data structures */
static Pcache ptr_icache; // apuntador a cache de instrucciones
static Pcache ptr_dcache; // apuntador a cache de datos
static cache icache; // cache de instrucciones, o cache unico en caso unificado
static cache dcache; // cache de datos
static cache_stat cache_stat_inst; // estadísticas del cache de instrucciones
static cache_stat cache_stat_data; // estadísticas del cache de datos

/************************************************************/
// esta función es llamada en múltiples ocasiones desde main.c
// específicamente, se llama por cada argumento válido en la 
// línea de comandos. Esta función modifica las variables
// locales declaradas al inicio de este archivo
void set_cache_param(param, value)
  int param;
  int value;
{

  switch (param) {
  case CACHE_PARAM_BLOCK_SIZE:
    cache_block_size = value;
    words_per_block = value / WORD_SIZE;
    break;
  case CACHE_PARAM_USIZE:
    cache_split = FALSE;
    cache_usize = value;
    break;
  case CACHE_PARAM_ISIZE:
    cache_split = TRUE;
    cache_isize = value;
    break;
  case CACHE_PARAM_DSIZE:
    cache_split = TRUE;
    cache_dsize = value;
    break;
  case CACHE_PARAM_ASSOC:
    cache_assoc = value;
    break;
  case CACHE_PARAM_WRITEBACK:
    cache_writeback = TRUE;
    break;
  case CACHE_PARAM_WRITETHROUGH:
    cache_writeback = FALSE;
    break;
  case CACHE_PARAM_WRITEALLOC:
    cache_writealloc = TRUE;
    break;
  case CACHE_PARAM_NOWRITEALLOC:
    cache_writealloc = FALSE;
    break;
  default:
    printf("error set_cache_param: bad parameter value\n");
    exit(-1);
  }

}
/************************************************************/

/************************************************************/
// esta función solo es llamada una vez en el archivo main.c
// e inicializa las estructuras de cache y cache statistics
// (definidas en el archivo cache.h)
void init_cache()
{
  // initialize cache stats
  init_cache_stats(&cache_stat_inst);
  init_cache_stats(&cache_stat_data);

  // partiendo de que se necesita solo un cache
  // se emplea cache de instrucciones como el cache
  // unificado
  icache.size = cache_isize;
  icache.associativity = cache_assoc;
  icache.n_sets = (icache.size) / (cache_block_size * icache.associativity);
  icache.index_mask = get_index_mask(icache.n_sets, cache_block_size, address_size);
  icache.index_mask_offset = LOG2(cache_block_size);
  icache.LRU_head = (Pcache_line *)malloc(sizeof(Pcache_line) * icache.n_sets);
  initialize_null(icache.LRU_head, icache.n_sets);
  icache.LRU_tail = (Pcache_line *)malloc(sizeof(Pcache_line) * icache.n_sets);
  initialize_null(icache.LRU_tail, icache.n_sets);
  icache.set_contents = (int *)malloc(sizeof(int) * icache.n_sets);
  initialize_zeros(icache.set_contents, icache.n_sets);

  if (cache_split) {
    // tenemos que inicializar un cache de datos
    dcache.size = cache_dsize;
    dcache.associativity = cache_assoc;
    dcache.n_sets = (dcache.size) / (cache_block_size * dcache.associativity);
    dcache.index_mask = get_index_mask(dcache.n_sets, cache_block_size, address_size);
    dcache.index_mask_offset = LOG2(cache_block_size);
    dcache.LRU_head = (Pcache_line *)malloc(sizeof(Pcache_line) * dcache.n_sets);
    initialize_null(dcache.LRU_head, dcache.n_sets);
    dcache.LRU_tail = (Pcache_line *)malloc(sizeof(Pcache_line) * dcache.n_sets);
    initialize_null(dcache.LRU_tail, dcache.n_sets);
    dcache.set_contents = (int *)malloc(sizeof(int) * dcache.n_sets);
    initialize_zeros(dcache.set_contents, dcache.n_sets);

    // initializing separate pointers
    ptr_icache = &icache;
    ptr_dcache = &dcache;
  } else {
    // directing both pointers to unified cache (icache variable)
    ptr_icache = &icache;
    ptr_dcache = &icache;
  }

  print_cache_status();
}
/************************************************************/

/************************************************************/
// esta función es llamada en cada iteración de la función 
// play_trace() del archivo main.c. Simula una referencia a
// memoria del cache
void perform_access(addr, access_type)
  unsigned addr, access_type;
{
  /*
  * ACCESS TYPE
  * 0 - Data load reference
  * 1 - Data store reference
  * 2 - Instruction load reference
  */

  countAccesses(access_type);
  int index = getLineIndex(addr, access_type);
  int is_hit = isHit(addr, access_type, index);
  if (access_type < 2) {
    cache_stat_data.misses += !is_hit;
  } else {
    cache_stat_inst.misses += !is_hit;
  }

}
/************************************************************/

/************************************************************/
// es llamada una vez antes de finalizar la función play_trace()
// del archivo main.c. Sirve para eliminar todos los contenidos de
// la memoria cache.
void flush()
{
  printf("Flush variables' values\n");
  emptyCacheLine(icache.LRU_head);
  emptyCacheLine(icache.LRU_tail);
  emptyCacheLine(dcache.LRU_head);
  emptyCacheLine(dcache.LRU_tail);
  emptyInt(icache.set_contents);
  emptyInt(dcache.set_contents);
}
/************************************************************/

/************************************************************/
// esta función es útil para simular asociatividad de la memoria
// cache. Específicamente, se utiliza para eliminar una línea de
// cache contenida en un banco. Esto puede suceder tanto cuando
// se quiere eliminar como cuando se hace referencia a una línea
// bajo el esquema de reposición Least Recently Used.
void delete(head, tail, item)
  Pcache_line *head, *tail; // apuntadores a apuntadores de líneas
  Pcache_line item; // apuntador de línea
{
  if (item->LRU_prev) {
    item->LRU_prev->LRU_next = item->LRU_next;
  } else {
    /* item at head */
    *head = item->LRU_next;
  }

  if (item->LRU_next) {
    item->LRU_next->LRU_prev = item->LRU_prev;
  } else {
    /* item at tail */
    *tail = item->LRU_prev;
  }
}
/************************************************************/

/************************************************************/
// Inserta una línea de cache en la cabeza de la lista 
// a.k.a. cabeza del banco
void insert(head, tail, item)
  Pcache_line *head, *tail; // apuntadores a apuntadores de líneas
  Pcache_line item; // apuntador a línea
{
  // el item apunta en next a la dirección que apuntaba la cabeza del banco
  item->LRU_next = *head;
  // pointer casting
  item->LRU_prev = (Pcache_line)NULL;

  // verificando si el item insertado no era el primero del banco
  if (item->LRU_next)
    // ya hay otro elemento en el banco
    // hacemos que el item que estaba antes en la cabeza apunte
    // al nuevo item agregado
    item->LRU_next->LRU_prev = item;
  else
    // de lo contario, la cola debe apuntar al nuevo item insertado
    *tail = item;

  // finalmente la cabeza apunta al item
  *head = item;
}
/************************************************************/

/************************************************************/
// imprime la configuración de la memoria cache a la consola
void dump_settings()
{
  printf("*** CACHE SETTINGS ***\n");
  if (cache_split) {
    printf("  Split I- D-cache\n");
    printf("  I-cache size: \t%d\n", cache_isize);
    printf("  D-cache size: \t%d\n", cache_dsize);
  } else {
    printf("  Unified I- D-cache\n");
    printf("  Size: \t%d\n", cache_usize);
  }
  printf("  Associativity: \t%d\n", cache_assoc);
  printf("  Block size: \t\t%d\n", cache_block_size);
  printf("  Write policy: \t%s\n", 
	 cache_writeback ? "WRITE BACK" : "WRITE THROUGH");
  printf("  Allocation policy: \t%s\n",
	 cache_writealloc ? "WRITE ALLOCATE" : "WRITE NO ALLOCATE");
}
/************************************************************/

/************************************************************/
// imprime a la consola las estadísticas recolectadas de la
// simulación del cache a través de los archivos *.trace. 
// Estas estadísticas deben de ser actualizadas dentro de 
// perform_access().
void print_stats()
{
  printf("\n*** CACHE STATISTICS ***\n");

  printf(" INSTRUCTIONS\n");
  printf("  accesses:  %d\n", cache_stat_inst.accesses);
  printf("  misses:    %d\n", cache_stat_inst.misses);
  if (!cache_stat_inst.accesses)
    printf("  miss rate: 0 (0)\n"); 
  else
    printf("  miss rate: %2.4f (hit rate %2.4f)\n", 
	 (float)cache_stat_inst.misses / (float)cache_stat_inst.accesses,
	 1.0 - (float)cache_stat_inst.misses / (float)cache_stat_inst.accesses);
  printf("  replace:   %d\n", cache_stat_inst.replacements);

  printf(" DATA\n");
  printf("  accesses:  %d\n", cache_stat_data.accesses);
  printf("  misses:    %d\n", cache_stat_data.misses);
  if (!cache_stat_data.accesses)
    printf("  miss rate: 0 (0)\n"); 
  else
    printf("  miss rate: %2.4f (hit rate %2.4f)\n", 
	 (float)cache_stat_data.misses / (float)cache_stat_data.accesses,
	 1.0 - (float)cache_stat_data.misses / (float)cache_stat_data.accesses);
  printf("  replace:   %d\n", cache_stat_data.replacements);

  printf(" TRAFFIC (in words)\n");
  printf("  demand fetch:  %d\n", cache_stat_inst.demand_fetches + 
	 cache_stat_data.demand_fetches);
  printf("  copies back:   %d\n", cache_stat_inst.copies_back +
	 cache_stat_data.copies_back);
}
/************************************************************/

/************************************************************/
/* helper function to calculate inex mask*/
int get_index_mask(int n_sets, int words_per_block, int address_size) {
  int w = LOG2(words_per_block);
  int x = LOG2(n_sets);
  unsigned mask = 0;

  for (int i = 0; i < x; i++) {
    mask += pow(2, i);
  }

  return mask << w;
}

/* helper function to initialize array with zeros */
void initialize_zeros(int *array, int number_of_items) {
  for (int i = 0; i < number_of_items; i++) {
    array[i] = 0;
  }
}

/* helper function to initialize array with NULL */
void initialize_null(Pcache_line *array, int number_of_items) {
  for (int i = 0; i < number_of_items; i++) {
    array[i] = (Pcache_line)NULL;
  }
}

/* helper function to init cache_stat's members with zeros */
void init_cache_stats(Pcache_stat c_stats) {
  c_stats->accesses = 0;
  c_stats->misses = 0;
  c_stats->replacements = 0;
  c_stats->demand_fetches = 0;
  c_stats->copies_back = 0;
}

/* Responsible of empty memory from a Pcache_line */
void emptyCacheLine(Pcache_line *line) {
  free(line);
  printf("Free memory from Cache Line\n");
}

/* Responsible of empty memory from a int */
void emptyInt(int *number) {
  free(number);
  printf("Free memory from Integer\n");  
}

/* helper function to print binary representation of a number */
void print_binary_representation(unsigned number) {
  if (number > 1) {
    print_binary_representation(number / 2);
  }
  printf("%d", number % 2);
}

/* helper function to print arrays elements */
void print_array_ints(int *array, int number_of_items) {
  printf("[");
  for (int i = 0; i < number_of_items; i++) {
    printf("%d", array[i]);
    if (i < (number_of_items - 1)) {
      printf(", ");
    }
  }
  printf("]\n");
}

/* helper function to print array cache line address */
void print_array_lines(Pcache_line *array, int number_of_items) {
  printf("[");
  for (int i = 0; i < number_of_items; i++) {
    if (array[i] != NULL) {
      printf("LINE");
    } else {
      printf("NULL");
    }
    if (i < (number_of_items - 1)) {
      printf(", ");
    }
  }
  printf("]\n");
}

/* helper function to debug cache */
void print_cache_status() {
  printf("\n****** CACHE STATUS *******\n\n");
  printf(" INSTRUCTIONS CACHE\n");
  printf("  size:  %d\n", icache.size);
  printf("  associativity:  %d\n", icache.associativity);
  printf("  sets:  %d\n", icache.n_sets);
  printf("  mask:  ");
  print_binary_representation(icache.index_mask);
  printf("\n");
  printf("  mask offset:  %d\n", icache.index_mask_offset);
  // printf("  LRU_head:  ");
  // print_array_lines(icache.LRU_head, icache.n_sets);
  // printf("  LRU_tail:  ");
  // print_array_lines(icache.LRU_tail, icache.n_sets);
  // printf("  set_contents:  ");
  // print_array_ints(icache.set_contents, icache.n_sets);
  if (cache_split) {
    printf(" DATA CACHE\n");
    printf("  size:  %d\n", dcache.size);
    printf("  associativity:  %d\n", dcache.associativity);
    printf("  sets:  %d\n", dcache.n_sets);
    printf("  mask:  ");
    print_binary_representation(dcache.index_mask);
    printf("\n");
    printf("  mask offset:  %d\n", dcache.index_mask_offset);
    // printf("  LRU_head:  ");
    // print_array_lines(dcache.LRU_head, dcache.n_sets);
    // printf("  LRU_tail:  ");
    // print_array_lines(dcache.LRU_tail, dcache.n_sets);
    // printf("  set_contents:  ");
    // print_array_ints(dcache.set_contents, dcache.n_sets);
  }
  printf("\n*** END OF CACHE STATUS ***\n\n");
}

void countAccesses(unsigned number) {
  if (number < 2) {
    cache_stat_data.accesses++;
  } else {
    cache_stat_inst.accesses++;
  }
}

/* helper function to get line index */
int getLineIndex(addr, access_type)
  unsigned addr, access_type;
{
  if (access_type < 2) {
    return (addr & ptr_dcache->index_mask) >> ptr_dcache->index_mask_offset;
  } else {
    return (addr & ptr_icache->index_mask) >> ptr_icache->index_mask_offset;
  }
}

/* helper function to get tag from memory address */
unsigned getTag(addr, nLines)
  unsigned addr, nLines;
{
  int offset = LOG2(nLines) + LOG2(words_per_block);
  return addr >> offset;
}

/* helper function to check for hits */
int isHit(addr, access_type, index)
  unsigned addr, access_type;
  int index;
{
  Pcache_line element;
  unsigned tag;
  if (access_type < 2) {
    element = ptr_dcache->LRU_head[index];
    tag = getTag(addr, ptr_dcache->n_sets);
  } else {
    element = ptr_icache->LRU_head[index];
    tag = getTag(addr, ptr_icache->n_sets);
  }
  while (element != NULL && tag != element->tag) {
    element = element->LRU_next;
  }
  return element != NULL && tag == element->tag;
}

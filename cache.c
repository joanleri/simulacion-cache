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
static int debug = DEFAULT_DEBUG;

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
  case CACHE_PARAM_DEBUG:
    debug = TRUE;
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
  // printf("Initializing cache...\n");
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
  * 0 - Data load reference - lectura
  * 1 - Data store reference - escritura
  * 2 - Instruction load reference
  */
  // printf("Performing access type %d - ", access_type);
  // conteo del número de veces que se accede a memoria por el
  // procesador
  countAccesses(access_type);

  // obtenemos en qué línea/banco le corresponde a la
  // dirección de memoria 
  int index = getLineIndex(addr, access_type);
  // printf("Using line index %d - ", index);

  // se verifica si el cache correspondiente contiene
  // una línea con ese tag o no
  int is_hit = isHit(addr, access_type, index);

  // printf("%s...\n", (is_hit ? "HIT" : "MISS"));

  // conteo de número de misses
  if (access_type < 2) {
    cache_stat_data.misses += !is_hit;
  } else {
    cache_stat_inst.misses += !is_hit;
  }

  // bloque de código para cuando no hubo un hit
  if (!is_hit) {
    if (access_type == 0) {
      // lectura de bloque 
      Pinsertion_response ptr_response = full_insert(addr, ptr_dcache, index);
      cache_stat_data.replacements += ptr_response->replacement;
      cache_stat_data.demand_fetches += words_per_block;
      cache_stat_data.copies_back += ptr_response->dirty_bit * words_per_block;
      free(ptr_response);
    } else if (access_type == 1) {
      // escritura a memoria
      if (cache_writealloc) {
        // traer a cache y escribir de acuerdo con política de hit write
        // TODO: aquí hay que insertarlo sucio
        Pinsertion_response ptr_response = full_insert(addr, ptr_dcache, index);
        cache_stat_data.replacements += ptr_response->replacement;
        cache_stat_data.demand_fetches += words_per_block;

        if (cache_writeback) {
          // incrementamos en uno la estadística de copies back
          // si la línea removida había sido modificada y tenemos
          // política de write back
          Pcache_line ptr_inserted_line = get_referenced_line(ptr_dcache, addr, index);
          ptr_inserted_line->dirty = TRUE;
          cache_stat_data.copies_back += ptr_response->dirty_bit * words_per_block;
        } else {
          cache_stat_data.copies_back += 1; // += words_per_block;?
        }
        free(ptr_response);
      } else {
        cache_stat_data.copies_back += 1; // += words_per_block;?
      }
    } else if (access_type == 2) {
        Pinsertion_response ptr_response = full_insert(addr, ptr_icache, index);
        cache_stat_inst.replacements += ptr_response->replacement;
        cache_stat_inst.demand_fetches += words_per_block;
        if (!cache_split) {
          // Cargar una instrucción puede borrar un dato 
          // por lo que hay que revisar también el dirty bit
          cache_stat_data.copies_back += ptr_response->dirty_bit * words_per_block /* * cache_writeback? */;
        }
        free(ptr_response);
    }
  }

  // bloque de código si hubo hit
  if (is_hit) {
    if (access_type == 0) {
      // busque datos para lectura en cache y estaban
      reinsert_at_head(ptr_dcache, addr, index);
    } else if (access_type == 1) {
      // quise escribir en localidad de memoria y estaba en cache
      reinsert_at_head(ptr_dcache, addr, index);
      if (cache_writeback) {
        // escribo solo en cache por lo que hay que modificar el dirty bit
        Pcache_line wrote_line = get_referenced_line(ptr_dcache, addr, index);
        wrote_line->dirty = 1;
        reinsert_at_head(ptr_dcache, addr, index);
      } else {
        // entonces se tiene writethrough por lo que se puede ignorar
        // dirty bit
        reinsert_at_head(ptr_dcache, addr, index);
        cache_stat_data.copies_back += 1;
      }
    } else if (access_type == 2) {
      // busque una instrucción en cache y estaba
      reinsert_at_head(ptr_icache, addr, index);
    }
  }
}
/************************************************************/

/************************************************************/
// es llamada una vez antes de finalizar la función play_trace()
// del archivo main.c. Sirve para eliminar todos los contenidos de
// la memoria cache.
void flush()
{
  if (debug) {
    printf("Flushing cache...\n");
  }
  free_structure(ptr_icache);
  free_cache_resources(ptr_icache);

  if (cache_split) {
    free_structure(ptr_dcache);
    free_cache_resources(ptr_dcache);
  }
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
  if (debug) {
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
  } else {
    if (cache_split) {
      printf("%d,", cache_isize);
      printf("%d,", cache_dsize);
      printf("%d,", 0);
    } else {
      printf("%d,", 0);
      printf("%d,", 0);
      printf("%d,", cache_usize);
    }
    printf("%d,", cache_assoc);
    printf("%d,", cache_block_size);
    printf("%s,", cache_writeback ? "WRITE BACK" : "WRITE THROUGH");
    printf("%s,", cache_writealloc ? "WRITE ALLOCATE" : "WRITE NO ALLOCATE");
  }
}
/************************************************************/

/************************************************************/
// imprime a la consola las estadísticas recolectadas de la
// simulación del cache a través de los archivos *.trace. 
// Estas estadísticas deben de ser actualizadas dentro de 
// perform_access().
void print_stats()
{
  if (debug) {
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
    printf("\n");
  } else {
    printf("%d,", cache_stat_inst.accesses);
    printf("%d,", cache_stat_inst.misses);
    if (!cache_stat_inst.accesses)
      printf("0,0,"); 
    else
      printf("%2.4f,%2.4f,", 
    (float)cache_stat_inst.misses / (float)cache_stat_inst.accesses,
    1.0 - (float)cache_stat_inst.misses / (float)cache_stat_inst.accesses);
    printf("%d,", cache_stat_inst.replacements);

    printf("%d,", cache_stat_data.accesses);
    printf("%d,", cache_stat_data.misses);
    if (!cache_stat_data.accesses)
      printf("0,0,"); 
    else
      printf("%2.4f,%2.4f,", 
    (float)cache_stat_data.misses / (float)cache_stat_data.accesses,
    1.0 - (float)cache_stat_data.misses / (float)cache_stat_data.accesses);
    printf("%d,", cache_stat_data.replacements);

    printf("%d,", cache_stat_inst.demand_fetches + 
    cache_stat_data.demand_fetches);
    printf("%d", cache_stat_inst.copies_back +
    cache_stat_data.copies_back);
    printf("\n");
  }
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
  if (debug) {
    printf("Free memory from Cache Line\n");
  }
}

/* Responsible of empty memory from a int */
void emptyInt(int *number) {
  free(number);
  if (debug) {
    printf("Free memory from Integer\n");  
  }
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
  printf("  LRU_head:  ");
  print_array_lines(icache.LRU_head, icache.n_sets);
  printf("  LRU_tail:  ");
  print_array_lines(icache.LRU_tail, icache.n_sets);
  printf("  set_contents:  ");
  print_array_ints(icache.set_contents, icache.n_sets);
  if (cache_split) {
    printf(" DATA CACHE\n");
    printf("  size:  %d\n", dcache.size);
    printf("  associativity:  %d\n", dcache.associativity);
    printf("  sets:  %d\n", dcache.n_sets);
    printf("  mask:  ");
    print_binary_representation(dcache.index_mask);
    printf("\n");
    printf("  mask offset:  %d\n", dcache.index_mask_offset);
    printf("  LRU_head:  ");
    print_array_lines(dcache.LRU_head, dcache.n_sets);
    printf("  LRU_tail:  ");
    print_array_lines(dcache.LRU_tail, dcache.n_sets);
    printf("  set_contents:  ");
    print_array_ints(dcache.set_contents, dcache.n_sets);
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
  int offset = LOG2(nLines) + LOG2(words_per_block * WORD_SIZE);
  // printf("Tag: ");
  // print_binary_representation(addr >> offset);
  // printf("...\n");
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

/* allocate an empty cache line */
Pcache_line get_empty_line() {
    Pcache_line ptr_new_line = (Pcache_line)malloc(sizeof(cache_line));
    ptr_new_line->dirty = 0;
    ptr_new_line->LRU_next = NULL;
    ptr_new_line->LRU_prev = NULL;
    return ptr_new_line;
}

/* checks cache associativity and inserts 
 * returns TRUE (1) if a replacemnt is done
 * FALSE (0) otherwise 
*/
Pinsertion_response full_insert(unsigned addr, Pcache ptr_cache, int line_number) {

  // allocate a new response to return at the end of the function
  Pinsertion_response ptr_response = get_new_insertion_response();

  // get a new line that will be inserted in cache
  Pcache_line ptr_new_line = get_empty_line();

  // add tag to new line for cache
  ptr_new_line->tag = getTag(addr, ptr_cache->n_sets);
  
  // enter if there is no more room for the new line
  // a line needs to be removed
  if (ptr_cache->set_contents[line_number] >= ptr_cache->associativity) {
    Pcache_line ptr_line_delete = ptr_cache->LRU_tail[line_number];
    // we indicate that a replacement has occured as a product of the insertion
    ptr_response->replacement = TRUE;
    // we set the response's dirty bit to that of the evicted line (LRU)
    ptr_response->dirty_bit = ptr_line_delete->dirty;
    // we delete the line
    delete(&ptr_cache->LRU_head[line_number], &ptr_cache->LRU_tail[line_number], ptr_line_delete); 
  } else {
    ptr_cache->set_contents[line_number] += 1;
  }

  // insert the line asked
  insert(&ptr_cache->LRU_head[line_number], &ptr_cache->LRU_tail[line_number], ptr_new_line);
  return ptr_response;
}

/* get a pointer to a new Pinsertion_response struct */
Pinsertion_response get_new_insertion_response() {
  Pinsertion_response ptr_response = (Pinsertion_response)malloc(sizeof(insertion_response));
  ptr_response->dirty_bit = 0;
  ptr_response->replacement = 0;
  return ptr_response;
}

/* get line from set with correct tag and returns a Pcache_line */
Pcache_line get_referenced_line(Pcache ptr_cache, unsigned addr, int set_index) {
  Pcache_line element = ptr_cache->LRU_head[set_index];
  unsigned tag = getTag(addr, ptr_cache->n_sets);
  while (element != NULL && tag != element->tag) {
    element = element->LRU_next;
  }
  if (element == NULL) {
    printf("=== Error: Line NOT found in cache set ===");
    abort();
  }
  return element;
}

/* remove cache line and reinsert it at LRU head */
void reinsert_at_head(Pcache ptr_cache, unsigned addr, int set_index) {
  Pcache_line line_used = get_referenced_line(ptr_cache, addr, set_index);
  delete(&ptr_cache->LRU_head[set_index], &ptr_cache->LRU_tail[set_index], line_used);
  insert(&ptr_cache->LRU_head[set_index], &ptr_cache->LRU_tail[set_index], line_used);
}

/* free cache LRU_head, LRU_tail, set_contents */
void free_cache_resources(Pcache ptr_cache) {
  free(ptr_cache->LRU_head);
  free(ptr_cache->LRU_tail);
  free(ptr_cache->set_contents);
}

void free_structure(cache *data) {
  for (int i = 0; i < data->n_sets; i++) {
    // printf("Flushig cache set no. %d...\n", i + 1);
    Pcache_line ptr_next_element = data->LRU_head[i];
    Pcache_line ptr_actual_element;
    for (int j = 0; j < data->set_contents[i]; j++) {
      // printf("  flushing line no. %d...\n", j + 1);
      ptr_actual_element = ptr_next_element;
      cache_stat_inst.copies_back += ptr_actual_element->dirty * words_per_block;
      ptr_next_element = ptr_actual_element->LRU_next;
      free(ptr_actual_element);
    }
  }
}
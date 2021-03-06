
/*
 * cache.h
 */

#define TRUE 1
#define FALSE 0

/* default cache parameters--can be changed */
#define WORD_SIZE 4
#define WORD_SIZE_OFFSET 2
#define DEFAULT_CACHE_SIZE (8 * 1024)
#define DEFAULT_CACHE_BLOCK_SIZE 16
#define DEFAULT_CACHE_ASSOC 1
#define DEFAULT_CACHE_WRITEBACK TRUE
#define DEFAULT_CACHE_WRITEALLOC TRUE
#define DEFAULT_ADDRESS_SIZE 32
#define DEFAULT_DEBUG FALSE

/* constants for settting cache parameters */
#define CACHE_PARAM_BLOCK_SIZE 0
#define CACHE_PARAM_USIZE 1
#define CACHE_PARAM_ISIZE 2
#define CACHE_PARAM_DSIZE 3
#define CACHE_PARAM_ASSOC 4
#define CACHE_PARAM_WRITEBACK 5
#define CACHE_PARAM_WRITETHROUGH 6
#define CACHE_PARAM_WRITEALLOC 7
#define CACHE_PARAM_NOWRITEALLOC 8
#define CACHE_PARAM_DEBUG 9

/* structure definitions */
// definición de la estructura de una línea de cache
// además de una etiqueta y un dirty bit contiene
// dos apuntadores a líneas de cache ya que se
// implementa como una lista doblemente ligada
// útil cuando se use para cache con asociatividad
typedef struct cache_line_
{
  unsigned tag;
  int dirty;

  struct cache_line_ *LRU_next;
  struct cache_line_ *LRU_prev;
} cache_line, *Pcache_line;

// definción de estructura que modela a la memoria cache
// contiene tamaño, asociatividad, número de sets,
// máscara de índice y máscara de offset. Estos
// son los parámetros que se inicializan desde la función
// init_cache() del main.c.
// El apuntador *LRU_head sirve de referencia al arreglo
// de cabezas de cada lista (banco) de líneas de cache.
// El apuntador *LRU_tail sirve de referencia al arreglo
// de finalización de cada lista (banco) del cache. Es
// solamente necesario para implementar LRU de forma rápida.
// contents sirve para llevar un conteo de la cardinalidad
// de cada banco para asegurarse que ninguno exceda la
// asociatividad expecificada. Es un arreglo de enteros
typedef struct cache_
{
  int size;              /* cache size */
  int associativity;     /* cache associativity */
  int n_sets;            /* number of cache sets */
  unsigned index_mask;   /* mask to find cache index */
  int index_mask_offset; /* number of zero bits in mask */
  Pcache_line *LRU_head; /* head of LRU list for each set */
  Pcache_line *LRU_tail; /* tail of LRU list for each set */
  int *set_contents;     /* number of valid entries in set */
  // int contents;			/* number of valid entries in cache (no le veo la utilidad) */
} cache, *Pcache;

typedef struct cache_stat_
{
  int accesses;       /* number of memory references */
  int misses;         /* number of cache misses */
  int replacements;   /* number of misses that cause replacments */
  int demand_fetches; /* number of fetches */
  int copies_back;    /* number of write backs */
} cache_stat, *Pcache_stat;

typedef struct insertion_response_
{
  int replacement; /* True if last insertion produce a replacement */
  int dirty_bit;   /* Value of dirty bit of line replaced */
} insertion_response, *Pinsertion_response;

/* function prototypes */
void set_cache_param();
void init_cache();
void perform_access();
void flush();
void delete ();
void insert();
void dump_settings();
void print_stats();
int get_index_mask();
void initialize_zeros();
void initialize_null();
void init_cache_stats();
void emptyPointer();
void emptyCache();
void emptyCacheLine();
void emptyInt();
void binary_representation();
void print_binary_representation();
void print_array_ints();
void print_array_lines();
void print_cache_status();
void countAccesses();
int getLineIndex();
unsigned getTag();
int isHit();
Pcache_line get_empty_line();
Pinsertion_response full_insert();
Pinsertion_response get_new_insertion_response();
Pcache_line get_referenced_line();
void reinsert_at_head();
void free_cache_resources();
void free_structure();

/* macros */
#define LOG2(x) ((int)rint((log((double)(x))) / (log(2.0))))

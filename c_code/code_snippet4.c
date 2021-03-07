
/* rpmalloc.c  -  Memory allocator
 *
 * This library is put in the public domain; you can redistribute it and/or modify it without any restrictions.
 *
 */

#include "rpmalloc.h"

///////
/// Build time configurable limits
//////

#ifndef HEAP_ARRAY_SIZE
//! Size of heap hashmap
#define HEAP_ARRAY_SIZE           47
#endif
#ifndef ENABLE_THREAD_CACHE
//! Enable per-thread cache
#define ENABLE_THREAD_CACHE       1
#endif
#ifndef ENABLE_GLOBAL_CACHE
//! Enable global cache shared between all threads, requires thread cache
#define ENABLE_GLOBAL_CACHE       1
#endif
#ifndef ENABLE_VALIDATE_ARGS
//! Enable validation of args to public entry points
#define ENABLE_VALIDATE_ARGS      0
#endif
#ifndef ENABLE_STATISTICS
//! Enable statistics collection
#define ENABLE_STATISTICS         0
#endif
#ifndef ENABLE_ASSERTS
//! Enable asserts
#define ENABLE_ASSERTS            0
#endif
#ifndef ENABLE_OVERRIDE
//! Override standard library malloc/free and new/delete entry points
#define ENABLE_OVERRIDE           0
#endif
#ifndef ENABLE_PRELOAD
//! Support preloading
#define ENABLE_PRELOAD            0
#endif
#ifndef DISABLE_UNMAP
//! Disable unmapping memory pages
#define DISABLE_UNMAP             0
#endif
#ifndef DEFAULT_SPAN_MAP_COUNT
//! Default number of spans to map in call to map more virtual memory (default values yield 4MiB here)
#define DEFAULT_SPAN_MAP_COUNT    64
#endif

#if ENABLE_THREAD_CACHE
#ifndef ENABLE_UNLIMITED_CACHE
//! Unlimited thread and global cache
#define ENABLE_UNLIMITED_CACHE    0
#endif
#ifndef ENABLE_UNLIMITED_THREAD_CACHE
//! Unlimited cache disables any thread cache limitations
#define ENABLE_UNLIMITED_THREAD_CACHE ENABLE_UNLIMITED_CACHE
#endif
#if !ENABLE_UNLIMITED_THREAD_CACHE
#ifndef THREAD_CACHE_MULTIPLIER
//! Multiplier for thread cache (cache limit will be span release count multiplied by this value)
#define THREAD_CACHE_MULTIPLIER 16
#endif
#ifndef ENABLE_ADAPTIVE_THREAD_CACHE
//! Enable adaptive size of per-thread cache (still bounded by THREAD_CACHE_MULTIPLIER hard limit)
#define ENABLE_ADAPTIVE_THREAD_CACHE  0
#endif
#endif
#endif

#if ENABLE_GLOBAL_CACHE && ENABLE_THREAD_CACHE
#if DISABLE_UNMAP
#undef ENABLE_UNLIMITED_GLOBAL_CACHE
#define ENABLE_UNLIMITED_GLOBAL_CACHE 1
#endif
#ifndef ENABLE_UNLIMITED_GLOBAL_CACHE
//! Unlimited cache disables any global cache limitations
#define ENABLE_UNLIMITED_GLOBAL_CACHE ENABLE_UNLIMITED_CACHE
#endif
#if !ENABLE_UNLIMITED_GLOBAL_CACHE
//! Multiplier for global cache (cache limit will be span release count multiplied by this value)
#define GLOBAL_CACHE_MULTIPLIER (THREAD_CACHE_MULTIPLIER * 6)
#endif
#else
#  undef ENABLE_GLOBAL_CACHE
#  define ENABLE_GLOBAL_CACHE 0
#endif

#if !ENABLE_THREAD_CACHE || ENABLE_UNLIMITED_THREAD_CACHE
#  undef ENABLE_ADAPTIVE_THREAD_CACHE
#  define ENABLE_ADAPTIVE_THREAD_CACHE 0
#endif

#if DISABLE_UNMAP && !ENABLE_GLOBAL_CACHE
#  error Must use global cache if unmap is disabled
#endif

#if defined( _WIN32 ) || defined( __WIN32__ ) || defined( _WIN64 )
#  define PLATFORM_WINDOWS 1
#  define PLATFORM_POSIX 0
#else
#  define PLATFORM_WINDOWS 0
#  define PLATFORM_POSIX 1
#endif

/// Platform and arch specifics
#if defined(_MSC_VER) && !defined(__clang__)
#  ifndef FORCEINLINE
#    define FORCEINLINE inline __forceinline
#  endif
#  define _Static_assert static_assert
#else
#  ifndef FORCEINLINE
#    define FORCEINLINE inline __attribute__((__always_inline__))
#  endif
#endif
#if PLATFORM_WINDOWS
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <Windows.h>
#  if ENABLE_VALIDATE_ARGS
#    include <Intsafe.h>
#  endif
#else
#  include <unistd.h>
#  include <stdio.h>
#  include <stdlib.h>
#  if defined(__APPLE__)
#    include <mach/mach_vm.h>
#    include <mach/vm_statistics.h>
#    include <pthread.h>
#  endif
#  if defined(__HAIKU__)
#    include <OS.h>
#    include <pthread.h>
#  endif
#endif

#include <stdint.h>
#include <string.h>
#include <errno.h>

#if defined(_WIN32) && (!defined(BUILD_DYNAMIC_LINK) || !BUILD_DYNAMIC_LINK)
#include <fibersapi.h>
static DWORD fls_key;
static void NTAPI
_rpmalloc_thread_destructor(void* value) {
	if (value)
		rpmalloc_thread_finalize();
}
#endif

#if PLATFORM_POSIX
#  include <sys/mman.h>
#  include <sched.h>
#  ifdef __FreeBSD__
#    include <sys/sysctl.h>
#    define MAP_HUGETLB MAP_ALIGNED_SUPER
#  endif
#  ifndef MAP_UNINITIALIZED
#    define MAP_UNINITIALIZED 0
#  endif
#endif
#include <errno.h>

#if ENABLE_ASSERTS
#  undef NDEBUG
#  if defined(_MSC_VER) && !defined(_DEBUG)
#    define _DEBUG
#  endif
#  include <assert.h>
#else
#  undef  assert
#  define assert(x) do {} while(0)
#endif
#if ENABLE_STATISTICS
#  include <stdio.h>
#endif

//////
///
/// Atomic access abstraction (since MSVC does not do C11 yet)
///
//////

#if defined(_MSC_VER) && !defined(__clang__)

typedef volatile long      atomic32_t;
typedef volatile long long atomic64_t;
typedef volatile void*     atomicptr_t;

static FORCEINLINE int32_t atomic_load32(atomic32_t* src) { return *src; }
static FORCEINLINE void    atomic_store32(atomic32_t* dst, int32_t val) { *dst = val; }
static FORCEINLINE int32_t atomic_incr32(atomic32_t* val) { return (int32_t)InterlockedIncrement(val); }
static FORCEINLINE int32_t atomic_decr32(atomic32_t* val) { return (int32_t)InterlockedDecrement(val); }
#if ENABLE_STATISTICS || ENABLE_ADAPTIVE_THREAD_CACHE
static FORCEINLINE int64_t atomic_load64(atomic64_t* src) { return *src; }
static FORCEINLINE int64_t atomic_add64(atomic64_t* val, int64_t add) { return (int64_t)InterlockedExchangeAdd64(val, add) + add; }
#endif
static FORCEINLINE int32_t atomic_add32(atomic32_t* val, int32_t add) { return (int32_t)InterlockedExchangeAdd(val, add) + add; }
static FORCEINLINE void*   atomic_load_ptr(atomicptr_t* src) { return (void*)*src; }
static FORCEINLINE void    atomic_store_ptr(atomicptr_t* dst, void* val) { *dst = val; }
static FORCEINLINE void    atomic_store_ptr_release(atomicptr_t* dst, void* val) { *dst = val; }
static FORCEINLINE int     atomic_cas_ptr(atomicptr_t* dst, void* val, void* ref) { return (InterlockedCompareExchangePointer((void* volatile*)dst, val, ref) == ref) ? 1 : 0; }
static FORCEINLINE int     atomic_cas_ptr_acquire(atomicptr_t* dst, void* val, void* ref) { return atomic_cas_ptr(dst, val, ref); }

#define EXPECTED(x) (x)
#define UNEXPECTED(x) (x)

#else

#include <stdatomic.h>

typedef volatile _Atomic(int32_t) atomic32_t;
typedef volatile _Atomic(int64_t) atomic64_t;
typedef volatile _Atomic(void*) atomicptr_t;

static FORCEINLINE int32_t atomic_load32(atomic32_t* src) { return atomic_load_explicit(src, memory_order_relaxed); }
static FORCEINLINE void    atomic_store32(atomic32_t* dst, int32_t val) { atomic_store_explicit(dst, val, memory_order_relaxed); }
static FORCEINLINE int32_t atomic_incr32(atomic32_t* val) { return atomic_fetch_add_explicit(val, 1, memory_order_relaxed) + 1; }
static FORCEINLINE int32_t atomic_decr32(atomic32_t* val) { return atomic_fetch_add_explicit(val, -1, memory_order_relaxed) - 1; }
#if ENABLE_STATISTICS || ENABLE_ADAPTIVE_THREAD_CACHE
static FORCEINLINE int64_t atomic_load64(atomic64_t* val) { return atomic_load_explicit(val, memory_order_relaxed); }
static FORCEINLINE int64_t atomic_add64(atomic64_t* val, int64_t add) { return atomic_fetch_add_explicit(val, add, memory_order_relaxed) + add; }
#endif
static FORCEINLINE int32_t atomic_add32(atomic32_t* val, int32_t add) { return atomic_fetch_add_explicit(val, add, memory_order_relaxed) + add; }
static FORCEINLINE void*   atomic_load_ptr(atomicptr_t* src) { return atomic_load_explicit(src, memory_order_relaxed); }
static FORCEINLINE void    atomic_store_ptr(atomicptr_t* dst, void* val) { atomic_store_explicit(dst, val, memory_order_relaxed); }
static FORCEINLINE void    atomic_store_ptr_release(atomicptr_t* dst, void* val) { atomic_store_explicit(dst, val, memory_order_release); }
static FORCEINLINE int     atomic_cas_ptr(atomicptr_t* dst, void* val, void* ref) { return atomic_compare_exchange_weak_explicit(dst, &ref, val, memory_order_relaxed, memory_order_relaxed); }
static FORCEINLINE int     atomic_cas_ptr_acquire(atomicptr_t* dst, void* val, void* ref) { return atomic_compare_exchange_weak_explicit(dst, &ref, val, memory_order_acquire, memory_order_relaxed); }

#define EXPECTED(x) __builtin_expect((x), 1)
#define UNEXPECTED(x) __builtin_expect((x), 0)

#endif

////////////
///
/// Statistics related functions (evaluate to nothing when statistics not enabled)
///
//////

#if ENABLE_STATISTICS
#  define _rpmalloc_stat_inc(counter) atomic_incr32(counter)
#  define _rpmalloc_stat_dec(counter) atomic_decr32(counter)
#  define _rpmalloc_stat_add(counter, value) atomic_add32(counter, (int32_t)(value))
#  define _rpmalloc_stat_add64(counter, value) atomic_add64(counter, (int64_t)(value))
#  define _rpmalloc_stat_add_peak(counter, value, peak) do { int32_t _cur_count = atomic_add32(counter, (int32_t)(value)); if (_cur_count > (peak)) peak = _cur_count; } while (0)
#  define _rpmalloc_stat_sub(counter, value) atomic_add32(counter, -(int32_t)(value))
#  define _rpmalloc_stat_inc_alloc(heap, class_idx) do { \
	int32_t alloc_current = atomic_incr32(&heap->size_class_use[class_idx].alloc_current); \
	if (alloc_current > heap->size_class_use[class_idx].alloc_peak) \
		heap->size_class_use[class_idx].alloc_peak = alloc_current; \
	atomic_incr32(&heap->size_class_use[class_idx].alloc_total); \
} while(0)
#  define _rpmalloc_stat_inc_free(heap, class_idx) do { \
	atomic_decr32(&heap->size_class_use[class_idx].alloc_current); \
	atomic_incr32(&heap->size_class_use[class_idx].free_total); \
} while(0)
#else
#  define _rpmalloc_stat_inc(counter) do {} while(0)
#  define _rpmalloc_stat_dec(counter) do {} while(0)
#  define _rpmalloc_stat_add(counter, value) do {} while(0)
#  define _rpmalloc_stat_add64(counter, value) do {} while(0)
#  define _rpmalloc_stat_add_peak(counter, value, peak) do {} while (0)
#  define _rpmalloc_stat_sub(counter, value) do {} while(0)
#  define _rpmalloc_stat_inc_alloc(heap, class_idx) do {} while(0)
#  define _rpmalloc_stat_inc_free(heap, class_idx) do {} while(0)
#endif

///
/// Preconfigured limits and sizes
///

//! Granularity of a small allocation block (must be power of two)
#define SMALL_GRANULARITY         16
//! Small granularity shift count
#define SMALL_GRANULARITY_SHIFT   4
//! Number of small block size classes
#define SMALL_CLASS_COUNT         65
//! Maximum size of a small block
#define SMALL_SIZE_LIMIT          (SMALL_GRANULARITY * (SMALL_CLASS_COUNT - 1))
//! Granularity of a medium allocation block
#define MEDIUM_GRANULARITY        512
//! Medium granularity shift count
#define MEDIUM_GRANULARITY_SHIFT  9
//! Number of medium block size classes
#define MEDIUM_CLASS_COUNT        61
//! Total number of small + medium size classes
#define SIZE_CLASS_COUNT          (SMALL_CLASS_COUNT + MEDIUM_CLASS_COUNT)
//! Number of large block size classes
#define LARGE_CLASS_COUNT         32
//! Maximum size of a medium block
#define MEDIUM_SIZE_LIMIT         (SMALL_SIZE_LIMIT + (MEDIUM_GRANULARITY * MEDIUM_CLASS_COUNT))
//! Maximum size of a large block
#define LARGE_SIZE_LIMIT          ((LARGE_CLASS_COUNT * _memory_span_size) - SPAN_HEADER_SIZE)
//! ABA protection size in orhpan heap list (also becomes limit of smallest page size)
#define HEAP_ORPHAN_ABA_SIZE      512
//! Size of a span header (must be a multiple of SMALL_GRANULARITY and a power of two)
#define SPAN_HEADER_SIZE          128

_Static_assert((SMALL_GRANULARITY & (SMALL_GRANULARITY - 1)) == 0, "Small granularity must be power of two");
_Static_assert((SPAN_HEADER_SIZE & (SPAN_HEADER_SIZE - 1)) == 0, "Span header size must be power of two");

#if ENABLE_VALIDATE_ARGS
//! Maximum allocation size to avoid integer overflow
#undef  MAX_ALLOC_SIZE
#define MAX_ALLOC_SIZE            (((size_t)-1) - _memory_span_size)
#endif

#define pointer_offset(ptr, ofs) (void*)((char*)(ptr) + (ptrdiff_t)(ofs))
#define pointer_diff(first, second) (ptrdiff_t)((const char*)(first) - (const char*)(second))

#define INVALID_POINTER ((void*)((uintptr_t)-1))

#define SIZE_CLASS_LARGE SIZE_CLASS_COUNT
#define SIZE_CLASS_HUGE ((uint32_t)-1)

////////////
///
/// Data types
///
//////

//! A memory heap, per thread
typedef struct heap_t heap_t;
//! Span of memory pages
typedef struct span_t span_t;
//! Span list
typedef struct span_list_t span_list_t;
//! Span active data
typedef struct span_active_t span_active_t;
//! Size class definition
typedef struct size_class_t size_class_t;
//! Global cache
typedef struct global_cache_t global_cache_t;

//! Flag indicating span is the first (master) span of a split superspan
#define SPAN_FLAG_MASTER 1U
//! Flag indicating span is a secondary (sub) span of a split superspan
#define SPAN_FLAG_SUBSPAN 2U
//! Flag indicating span has blocks with increased alignment
#define SPAN_FLAG_ALIGNED_BLOCKS 4U

#if ENABLE_ADAPTIVE_THREAD_CACHE || ENABLE_STATISTICS
struct span_use_t {
	//! Current number of spans used (actually used, not in cache)
	atomic32_t current;
	//! High water mark of spans used
	atomic32_t high;
#if ENABLE_STATISTICS
	//! Number of spans transitioned to global cache
	atomic32_t spans_to_global;
	//! Number of spans transitioned from global cache
	atomic32_t spans_from_global;
	//! Number of spans transitioned to thread cache
	atomic32_t spans_to_cache;
	//! Number of spans transitioned from thread cache
	atomic32_t spans_from_cache;
	//! Number of spans transitioned to reserved state
	atomic32_t spans_to_reserved;
	//! Number of spans transitioned from reserved state
	atomic32_t spans_from_reserved;
	//! Number of raw memory map calls
	atomic32_t spans_map_calls;
#endif
};
typedef struct span_use_t span_use_t;
#endif

#if ENABLE_STATISTICS
struct size_class_use_t {
	//! Current number of allocations
	atomic32_t alloc_current;
	//! Peak number of allocations
	int32_t alloc_peak;
	//! Total number of allocations
	atomic32_t alloc_total;
	//! Total number of frees
	atomic32_t free_total;
	//! Number of spans in use
	atomic32_t spans_current;
	//! Number of spans transitioned to cache
	int32_t spans_peak;
	//! Number of spans transitioned to cache
	atomic32_t spans_to_cache;
	//! Number of spans transitioned from cache
	atomic32_t spans_from_cache;
	//! Number of spans transitioned from reserved state
	atomic32_t spans_from_reserved;
	//! Number of spans mapped
	atomic32_t spans_map_calls;
};
typedef struct size_class_use_t size_class_use_t;
#endif

// A span can either represent a single span of memory pages with size declared by span_map_count configuration variable,
// or a set of spans in a continuous region, a super span. Any reference to the term "span" usually refers to both a single
// span or a super span. A super span can further be divided into multiple spans (or this, super spans), where the first
// (super)span is the master and subsequent (super)spans are subspans. The master span keeps track of how many subspans
// that are still alive and mapped in virtual memory, and once all subspans and master have been unmapped the entire
// superspan region is released and unmapped (on Windows for example, the entire superspan range has to be released
// in the same call to release the virtual memory range, but individual subranges can be decommitted individually
// to reduce physical memory use).
struct span_t {
	//! Free list
	void*       free_list;
	//! Total block count of size class
	uint32_t    block_count;
	//! Size class
	uint32_t    size_class;
	//! Index of last block initialized in free list
	uint32_t    free_list_limit;
	//! Number of used blocks remaining when in partial state
	uint32_t    used_count;
	//! Deferred free list
	atomicptr_t free_list_deferred;
	//! Size of deferred free list, or list of spans when part of a cache list
	uint32_t    list_size;
	//! Size of a block
	uint32_t    block_size;
	//! Flags and counters
	uint32_t    flags;
	//! Number of spans
	uint32_t    span_count;
	//! Total span counter for master spans
	uint32_t    total_spans;
	//! Offset from master span for subspans
	uint32_t    offset_from_master;
	//! Remaining span counter, for master spans
	atomic32_t  remaining_spans;
	//! Alignment offset
	uint32_t    align_offset;
	//! Owning heap
	heap_t*     heap;
	//! Next span
	span_t*     next;
	//! Previous span
	span_t*     prev;
};
_Static_assert(sizeof(span_t) <= SPAN_HEADER_SIZE, "span size mismatch");

// Control structure for a heap, either a thread heap or a first class heap if enabled
struct heap_t {
	//! Owning thread ID
	uintptr_t    owner_thread;
	//! Free list of active span
	void*        free_list[SIZE_CLASS_COUNT];
	//! Double linked list of partially used spans with free blocks for each size class.
	//  Previous span pointer in head points to tail span of list.
	span_t*      partial_span[SIZE_CLASS_COUNT];
#if RPMALLOC_FIRST_CLASS_HEAPS
	//! Double linked list of fully utilized spans with free blocks for each size class.
	//  Previous span pointer in head points to tail span of list.
	span_t*      full_span[SIZE_CLASS_COUNT];
#endif
#if ENABLE_THREAD_CACHE
	//! List of free spans (single linked list)
	span_t*      span_cache[LARGE_CLASS_COUNT];
#endif
	//! List of deferred free spans (single linked list)
	atomicptr_t  span_free_deferred;
#if ENABLE_ADAPTIVE_THREAD_CACHE || ENABLE_STATISTICS
	//! Current and high water mark of spans used per span count
	span_use_t   span_use[LARGE_CLASS_COUNT];
#endif
#if RPMALLOC_FIRST_CLASS_HEAPS
	//! Double linked list of large and huge spans allocated by this heap
	span_t*      large_huge_span;
#endif
	//! Number of full spans
	size_t       full_span_count;
	//! Mapped but unused spans
	span_t*      span_reserve;
	//! Master span for mapped but unused spans
	span_t*      span_reserve_master;
	//! Number of mapped but unused spans
	size_t       spans_reserved;
	//! Next heap in id list
	heap_t*      next_heap;
	//! Next heap in orphan list
	heap_t*      next_orphan;
	//! Memory pages alignment offset
	size_t       align_offset;
	//! Heap ID
	int32_t      id;
	//! Finalization state flag
	int          finalize;
	//! Master heap owning the memory pages
	heap_t*      master_heap;
	//! Child count
	atomic32_t   child_count;
#if ENABLE_STATISTICS
	//! Number of bytes transitioned thread -> global
	atomic64_t   thread_to_global;
	//! Number of bytes transitioned global -> thread
	atomic64_t   global_to_thread;
	//! Allocation stats per size class
	size_class_use_t size_class_use[SIZE_CLASS_COUNT + 1];
#endif
};

// Size class for defining a block size bucket
struct size_class_t {
	//! Size of blocks in this class
	uint32_t block_size;
	//! Number of blocks in each chunk
	uint16_t block_count;
	//! Class index this class is merged with
	uint16_t class_idx;
};
_Static_assert(sizeof(size_class_t) == 8, "Size class size mismatch");

struct global_cache_t {
	//! Cache list pointer
	atomicptr_t cache;
	//! Cache size
	atomic32_t size;
	//! ABA counter
	atomic32_t counter;
};

////////////
///
/// Global data
///
//////

//! Initialized flag
static int _rpmalloc_initialized;
//! Configuration
static rpmalloc_config_t _memory_config;
//! Memory page size
static size_t _memory_page_size;
//! Shift to divide by page size
static size_t _memory_page_size_shift;
//! Granularity at which memory pages are mapped by OS
static size_t _memory_map_granularity;
#if RPMALLOC_CONFIGURABLE
//! Size of a span of memory pages
static size_t _memory_span_size;
//! Shift to divide by span size
static size_t _memory_span_size_shift;
//! Mask to get to start of a memory span
static uintptr_t _memory_span_mask;
#else
//! Hardwired span size (64KiB)
#define _memory_span_size (64 * 1024)
#define _memory_span_size_shift 16
#define _memory_span_mask (~((uintptr_t)(_memory_span_size - 1)))
#endif
//! Number of spans to map in each map call
static size_t _memory_span_map_count;
//! Number of spans to release from thread cache to global cache (single spans)
static size_t _memory_span_release_count;
//! Number of spans to release from thread cache to global cache (large multiple spans)
static size_t _memory_span_release_count_large;
//! Global size classes
static size_class_t _memory_size_class[SIZE_CLASS_COUNT];
//! Run-time size limit of medium blocks
static size_t _memory_medium_size_limit;
//! Heap ID counter
static atomic32_t _memory_heap_id;
//! Huge page support
static int _memory_huge_pages;
#if ENABLE_GLOBAL_CACHE
//! Global span cache
static global_cache_t _memory_span_cache[LARGE_CLASS_COUNT];
#endif
//! All heaps
static atomicptr_t _memory_heaps[HEAP_ARRAY_SIZE];
//! Orphaned heaps
static atomicptr_t _memory_orphan_heaps;
#if RPMALLOC_FIRST_CLASS_HEAPS
//! Orphaned heaps (first class heaps)
static atomicptr_t _memory_first_class_orphan_heaps;
#endif
//! Running orphan counter to avoid ABA issues in linked list
static atomic32_t _memory_orphan_counter;
#if ENABLE_STATISTICS
//! Active heap count
static atomic32_t _memory_active_heaps;
//! Number of currently mapped memory pages
static atomic32_t _mapped_pages;
//! Peak number of concurrently mapped memory pages
static int32_t _mapped_pages_peak;
//! Number of mapped master spans
static atomic32_t _master_spans;
//! Number of currently unused spans
static atomic32_t _reserved_spans;
//! Running counter of total number of mapped memory pages since start
static atomic32_t _mapped_total;
//! Running counter of total number of unmapped memory pages since start
static atomic32_t _unmapped_total;
//! Number of currently mapped memory pages in OS calls
static atomic32_t _mapped_pages_os;
//! Number of currently allocated pages in huge allocations
static atomic32_t _huge_pages_current;
//! Peak number of currently allocated pages in huge allocations
static int32_t _huge_pages_peak;
#endif

////////////
///
/// Thread local heap and ID
///
//////

//! Current thread heap
#if (defined(__APPLE__) || defined(__HAIKU__)) && ENABLE_PRELOAD
static pthread_key_t _memory_thread_heap;
#else
#  ifdef _MSC_VER
#    define _Thread_local __declspec(thread)
#    define TLS_MODEL
#  else
#    define TLS_MODEL __attribute__((tls_model("initial-exec")))
#    if !defined(__clang__) && defined(__GNUC__)
#      define _Thread_local __thread
#    endif
#  endif
static _Thread_local heap_t* _memory_thread_heap TLS_MODEL;
#endif

static inline heap_t*
get_thread_heap_raw(void) {
#if (defined(__APPLE__) || defined(__HAIKU__)) && ENABLE_PRELOAD
	return pthread_getspecific(_memory_thread_heap);
#else
	return _memory_thread_heap;
#endif
}

//! Get the current thread heap
static inline heap_t*
get_thread_heap(void) {
	heap_t* heap = get_thread_heap_raw();
#if ENABLE_PRELOAD
	if (EXPECTED(heap != 0))
		return heap;
	rpmalloc_initialize();
	return get_thread_heap_raw();
#else
	return heap;
#endif
}

//! Fast thread ID
static inline uintptr_t
get_thread_id(void) {
#if defined(_WIN32)
	return (uintptr_t)((void*)NtCurrentTeb());
#elif defined(__GNUC__) || defined(__clang__)
	uintptr_t tid;
#  if defined(__i386__)
	__asm__("movl %%gs:0, %0" : "=r" (tid) : : );
#  elif defined(__MACH__)
	__asm__("movq %%gs:0, %0" : "=r" (tid) : : );
#  elif defined(__x86_64__)
	__asm__("movq %%fs:0, %0" : "=r" (tid) : : );
#  elif defined(__arm__)
	__asm__ volatile ("mrc p15, 0, %0, c13, c0, 3" : "=r" (tid));
#  elif defined(__aarch64__)
	__asm__ volatile ("mrs %0, tpidr_el0" : "=r" (tid));
#  else
	tid = (uintptr_t)get_thread_heap_raw();
#  endif
	return tid;
#else
	return (uintptr_t)get_thread_heap_raw();
#endif
}

//! Set the current thread heap
static void
set_thread_heap(heap_t* heap) {
#if (defined(__APPLE__) || defined(__HAIKU__)) && ENABLE_PRELOAD
	pthread_setspecific(_memory_thread_heap, heap);
#else
	_memory_thread_heap = heap;
#endif
	if (heap)
		heap->owner_thread = get_thread_id();
}

////////////
///
/// Low level memory map/unmap
///
//////

//! Map more virtual memory
//  size is number of bytes to map
//  offset receives the offset in bytes from start of mapped region
//  returns address to start of mapped region to use
static void*
_rpmalloc_mmap(size_t size, size_t* offset) {
	assert(!(size % _memory_page_size));
	assert(size >= _memory_page_size);
	_rpmalloc_stat_add_peak(&_mapped_pages, (size >> _memory_page_size_shift), _mapped_pages_peak);
	_rpmalloc_stat_add(&_mapped_total, (size >> _memory_page_size_shift));
	return _memory_config.memory_map(size, offset);
}

//! Unmap virtual memory
//  address is the memory address to unmap, as returned from _memory_map
//  size is the number of bytes to unmap, which might be less than full region for a partial unmap
//  offset is the offset in bytes to the actual mapped region, as set by _memory_map
//  release is set to 0 for partial unmap, or size of entire range for a full unmap
static void
_rpmalloc_unmap(void* address, size_t size, size_t offset, size_t release) {
	assert(!release || (release >= size));
	assert(!release || (release >= _memory_page_size));
	if (release) {
		assert(!(release % _memory_page_size));
		_rpmalloc_stat_sub(&_mapped_pages, (release >> _memory_page_size_shift));
		_rpmalloc_stat_add(&_unmapped_total, (release >> _memory_page_size_shift));
	}
	_memory_config.memory_unmap(address, size, offset, release);
}

//! Default implementation to map new pages to virtual memory
static void*
_rpmalloc_mmap_os(size_t size, size_t* offset) {
	//Either size is a heap (a single page) or a (multiple) span - we only need to align spans, and only if larger than map granularity
	size_t padding = ((size >= _memory_span_size) && (_memory_span_size > _memory_map_granularity)) ? _memory_span_size : 0;
	assert(size >= _memory_page_size);
#if PLATFORM_WINDOWS
	//Ok to MEM_COMMIT - according to MSDN, "actual physical pages are not allocated unless/until the virtual addresses are actually accessed"
	void* ptr = VirtualAlloc(0, size + padding, (_memory_huge_pages ? MEM_LARGE_PAGES : 0) | MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	if (!ptr) {
		assert(ptr && "Failed to map virtual memory block");
		return 0;
	}
#else
	int flags = MAP_PRIVATE | MAP_ANONYMOUS | MAP_UNINITIALIZED;
#  if defined(__APPLE__)
	int fd = (int)VM_MAKE_TAG(240U);
	if (_memory_huge_pages)
		fd |= VM_FLAGS_SUPERPAGE_SIZE_2MB;
	void* ptr = mmap(0, size + padding, PROT_READ | PROT_WRITE, flags, fd, 0);
#  elif defined(MAP_HUGETLB)
	void* ptr = mmap(0, size + padding, PROT_READ | PROT_WRITE, (_memory_huge_pages ? MAP_HUGETLB : 0) | flags, -1, 0);
#  else
	void* ptr = mmap(0, size + padding, PROT_READ | PROT_WRITE, flags, -1, 0);
#  endif
	if ((ptr == MAP_FAILED) || !ptr) {
		assert("Failed to map virtual memory block" == 0);
		return 0;
	}
#endif
	_rpmalloc_stat_add(&_mapped_pages_os, (int32_t)((size + padding) >> _memory_page_size_shift));
	if (padding) {
		size_t final_padding = padding - ((uintptr_t)ptr & ~_memory_span_mask);
		assert(final_padding <= _memory_span_size);
		assert(final_padding <= padding);
		assert(!(final_padding % 8));
		ptr = pointer_offset(ptr, final_padding);
		*offset = final_padding >> 3;
	}
	assert((size < _memory_span_size) || !((uintptr_t)ptr & ~_memory_span_mask));
	return ptr;
}

//! Default implementation to unmap pages from virtual memory
static void
_rpmalloc_unmap_os(void* address, size_t size, size_t offset, size_t release) {
	assert(release || (offset == 0));
	assert(!release || (release >= _memory_page_size));
	assert(size >= _memory_page_size);
	if (release && offset) {
		offset <<= 3;
		address = pointer_offset(address, -(int32_t)offset);
#if PLATFORM_POSIX
		//Padding is always one span size
		release += _memory_span_size;
#endif
	}
#if !DISABLE_UNMAP
#if PLATFORM_WINDOWS
	if (!VirtualFree(address, release ? 0 : size, release ? MEM_RELEASE : MEM_DECOMMIT)) {
		assert(address && "Failed to unmap virtual memory block");
	}
#else
	if (release) {
		if (munmap(address, release)) {
			assert("Failed to unmap virtual memory block" == 0);
		}
	}
	else {
#if defined(POSIX_MADV_FREE)
		if (posix_madvise(address, size, POSIX_MADV_FREE))
#endif
		if (posix_madvise(address, size, POSIX_MADV_DONTNEED)) {
			assert("Failed to madvise virtual memory block as free" == 0);
		}
	}
#endif
#endif
	if (release)
		_rpmalloc_stat_sub(&_mapped_pages_os, release >> _memory_page_size_shift);
}


////////////
///
/// Span linked list management
///
//////

#if ENABLE_THREAD_CACHE

static void
_rpmalloc_span_unmap(span_t* span);

//! Unmap a single linked list of spans
static void
_rpmalloc_span_list_unmap_all(span_t* span) {
	size_t list_size = span->list_size;
	for (size_t ispan = 0; ispan < list_size; ++ispan) {
		span_t* next_span = span->next;
		_rpmalloc_span_unmap(span);
		span = next_span;
	}
	assert(!span);
}

//! Add span to head of single linked span list
static size_t
_rpmalloc_span_list_push(span_t** head, span_t* span) {
	span->next = *head;
	if (*head)
		span->list_size = (*head)->list_size + 1;
	else
		span->list_size = 1;
	*head = span;
	return span->list_size;
}

//! Remove span from head of single linked span list, returns the new list head
static span_t*
_rpmalloc_span_list_pop(span_t** head) {
	span_t* span = *head;
	span_t* next_span = 0;
	if (span->list_size > 1) {
		assert(span->next);
		next_span = span->next;
		assert(next_span);
		next_span->list_size = span->list_size - 1;
	}
	*head = next_span;
	return span;
}

//! Split a single linked span list
static span_t*
_rpmalloc_span_list_split(span_t* span, size_t limit) {
	span_t* next = 0;
	if (limit < 2)
		limit = 2;
	if (span->list_size > limit) {
		uint32_t list_size = 1;
		span_t* last = span;
		next = span->next;
		while (list_size < limit) {
			last = next;
			next = next->next;
			++list_size;
		}
		last->next = 0;
		assert(next);
		next->list_size = span->list_size - list_size;
		span->list_size = list_size;
		span->prev = 0;
	}
	return next;
}

#endif

//! Add a span to double linked list at the head
static void
_rpmalloc_span_double_link_list_add(span_t** head, span_t* span) {
	if (*head) {
		span->next = *head;
		(*head)->prev = span;
	} else {
		span->next = 0;
	}
	*head = span;
}

//! Pop head span from double linked list
static void
_rpmalloc_span_double_link_list_pop_head(span_t** head, span_t* span) {
	assert(*head == span);
	span = *head;
	*head = span->next;
}

//! Remove a span from double linked list
static void
_rpmalloc_span_double_link_list_remove(span_t** head, span_t* span) {
	assert(*head);
	if (*head == span) {
		*head = span->next;
	} else {
		span_t* next_span = span->next;
		span_t* prev_span = span->prev;
		prev_span->next = next_span;
		if (EXPECTED(next_span != 0)) {
			next_span->prev = prev_span;
		}
	}
}


////////////
///
/// Span control
///
//////

static void
_rpmalloc_heap_cache_insert(heap_t* heap, span_t* span);

static void
_rpmalloc_heap_finalize(heap_t* heap);

static void
_rpmalloc_heap_set_reserved_spans(heap_t* heap, span_t* master, span_t* reserve, size_t reserve_span_count);

//! Declare the span to be a subspan and store distance from master span and span count
static void
_rpmalloc_span_mark_as_subspan_unless_master(span_t* master, span_t* subspan, size_t span_count) {
	assert((subspan != master) || (subspan->flags & SPAN_FLAG_MASTER));
	if (subspan != master) {
		subspan->flags = SPAN_FLAG_SUBSPAN;
		subspan->offset_from_master = (uint32_t)((uintptr_t)pointer_diff(subspan, master) >> _memory_span_size_shift);
		subspan->align_offset = 0;
	}
	subspan->span_count = (uint32_t)span_count;
}

//! Use reserved spans to fulfill a memory map request (reserve size must be checked by caller)
static span_t*
_rpmalloc_span_map_from_reserve(heap_t* heap, size_t span_count) {
	//Update the heap span reserve
	span_t* span = heap->span_reserve;
	heap->span_reserve = (span_t*)pointer_offset(span, span_count * _memory_span_size);
	heap->spans_reserved -= span_count;

	_rpmalloc_span_mark_as_subspan_unless_master(heap->span_reserve_master, span, span_count);
	if (span_count <= LARGE_CLASS_COUNT)
		_rpmalloc_stat_inc(&heap->span_use[span_count - 1].spans_from_reserved);

	return span;
}

//! Get the aligned number of spans to map in based on wanted count, configured mapping granularity and the page size
static size_t
_rpmalloc_span_align_count(size_t span_count) {
	size_t request_count = (span_count > _memory_span_map_count) ? span_count : _memory_span_map_count;
	if ((_memory_page_size > _memory_span_size) && ((request_count * _memory_span_size) % _memory_page_size))
		request_count += _memory_span_map_count - (request_count % _memory_span_map_count);
	return request_count;
}

//! Setup a newly mapped span
static void
_rpmalloc_span_initialize(span_t* span, size_t total_span_count, size_t span_count, size_t align_offset) {
	span->total_spans = (uint32_t)total_span_count;
	span->span_count = (uint32_t)span_count;
	span->align_offset = (uint32_t)align_offset;
	span->flags = SPAN_FLAG_MASTER;
	atomic_store32(&span->remaining_spans, (int32_t)total_span_count);
}

//! Map an aligned set of spans, taking configured mapping granularity and the page size into account
static span_t*
_rpmalloc_span_map_aligned_count(heap_t* heap, size_t span_count) {
	//If we already have some, but not enough, reserved spans, release those to heap cache and map a new
	//full set of spans. Otherwise we would waste memory if page size > span size (huge pages)
	size_t aligned_span_count = _rpmalloc_span_align_count(span_count);
	size_t align_offset = 0;
	span_t* span = (span_t*)_rpmalloc_mmap(aligned_span_count * _memory_span_size, &align_offset);
	if (!span)
		return 0;
	_rpmalloc_span_initialize(span, aligned_span_count, span_count, align_offset);
	_rpmalloc_stat_add(&_reserved_spans, aligned_span_count);
	_rpmalloc_stat_inc(&_master_spans);
	if (span_count <= LARGE_CLASS_COUNT)
		_rpmalloc_stat_inc(&heap->span_use[span_count - 1].spans_map_calls);
	if (aligned_span_count > span_count) {
		span_t* reserved_spans = (span_t*)pointer_offset(span, span_count * _memory_span_size);
		size_t reserved_count = aligned_span_count - span_count;
		if (heap->spans_reserved) {
			_rpmalloc_span_mark_as_subspan_unless_master(heap->span_reserve_master, heap->span_reserve, heap->spans_reserved);
			_rpmalloc_heap_cache_insert(heap, heap->span_reserve);
		}
		_rpmalloc_heap_set_reserved_spans(heap, span, reserved_spans, reserved_count);
	}
	return span;
}

//! Map in memory pages for the given number of spans (or use previously reserved pages)
static span_t*
_rpmalloc_span_map(heap_t* heap, size_t span_count) {
	if (span_count <= heap->spans_reserved)
		return _rpmalloc_span_map_from_reserve(heap, span_count);
	return _rpmalloc_span_map_aligned_count(heap, span_count);
}

//! Unmap memory pages for the given number of spans (or mark as unused if no partial unmappings)
static void
_rpmalloc_span_unmap(span_t* span) {
	assert((span->flags & SPAN_FLAG_MASTER) || (span->flags & SPAN_FLAG_SUBSPAN));
	assert(!(span->flags & SPAN_FLAG_MASTER) || !(span->flags & SPAN_FLAG_SUBSPAN));

	int is_master = !!(span->flags & SPAN_FLAG_MASTER);
	span_t* master = is_master ? span : ((span_t*)pointer_offset(span, -(intptr_t)((uintptr_t)span->offset_from_master * _memory_span_size)));
	assert(is_master || (span->flags & SPAN_FLAG_SUBSPAN));
	assert(master->flags & SPAN_FLAG_MASTER);

	size_t span_count = span->span_count;
	if (!is_master) {
		//Directly unmap subspans (unless huge pages, in which case we defer and unmap entire page range with master)
		assert(span->align_offset == 0);
		if (_memory_span_size >= _memory_page_size) {
			_rpmalloc_unmap(span, span_count * _memory_span_size, 0, 0);
			_rpmalloc_stat_sub(&_reserved_spans, span_count);
		}
	} else {
		//Special double flag to denote an unmapped master
		//It must be kept in memory since span header must be used
		span->flags |= SPAN_FLAG_MASTER | SPAN_FLAG_SUBSPAN;
	}

	if (atomic_add32(&master->remaining_spans, -(int32_t)span_count) <= 0) {
		//Everything unmapped, unmap the master span with release flag to unmap the entire range of the super span
		assert(!!(master->flags & SPAN_FLAG_MASTER) && !!(master->flags & SPAN_FLAG_SUBSPAN));
		size_t unmap_count = master->span_count;
		if (_memory_span_size < _memory_page_size)
			unmap_count = master->total_spans;
		_rpmalloc_stat_sub(&_reserved_spans, unmap_count);
		_rpmalloc_stat_sub(&_master_spans, 1);
		_rpmalloc_unmap(master, unmap_count * _memory_span_size, master->align_offset, (size_t)master->total_spans * _memory_span_size);
	}
}

//! Move the span (used for small or medium allocations) to the heap thread cache
static void
_rpmalloc_span_release_to_cache(heap_t* heap, span_t* span) {
	assert(heap == span->heap);
	assert(span->size_class < SIZE_CLASS_COUNT);
#if ENABLE_ADAPTIVE_THREAD_CACHE || ENABLE_STATISTICS
	atomic_decr32(&heap->span_use[0].current);
#endif
	_rpmalloc_stat_inc(&heap->span_use[0].spans_to_cache);
	_rpmalloc_stat_inc(&heap->size_class_use[span->size_class].spans_to_cache);
	_rpmalloc_stat_dec(&heap->size_class_use[span->size_class].spans_current);
	_rpmalloc_heap_cache_insert(heap, span);
}

//! Initialize a (partial) free list up to next system memory page, while reserving the first block
//! as allocated, returning number of blocks in list
static uint32_t
free_list_partial_init(void** list, void** first_block, void* page_start, void* block_start,
                       uint32_t block_count, uint32_t block_size) {
	assert(block_count);
	*first_block = block_start;
	if (block_count > 1) {
		void* free_block = pointer_offset(block_start, block_size);
		void* block_end = pointer_offset(block_start, (size_t)block_size * block_count);
		//If block size is less than half a memory page, bound init to next memory page boundary
		if (block_size < (_memory_page_size >> 1)) {
			void* page_end = pointer_offset(page_start, _memory_page_size);
			if (page_end < block_end)
				block_end = page_end;
		}
		*list = free_block;
		block_count = 2;
		void* next_block = pointer_offset(free_block, block_size);
		while (next_block < block_end) {
			*((void**)free_block) = next_block;
			free_block = next_block;
			++block_count;
			next_block = pointer_offset(next_block, block_size);
		}
		*((void**)free_block) = 0;
	} else {
		*list = 0;
	}
	return block_count;
}

//! Initialize an unused span (from cache or mapped) to be new active span, putting the initial free list in heap class free list
static void*
_rpmalloc_span_initialize_new(heap_t* heap, span_t* span, uint32_t class_idx) {
	assert(span->span_count == 1);
	size_class_t* size_class = _memory_size_class + class_idx;
	span->size_class = class_idx;
	span->heap = heap;
	span->flags &= ~SPAN_FLAG_ALIGNED_BLOCKS;
	span->block_size = size_class->block_size;
	span->block_count = size_class->block_count;
	span->free_list = 0;
	span->list_size = 0;
	atomic_store_ptr_release(&span->free_list_deferred, 0);

	//Setup free list. Only initialize one system page worth of free blocks in list
	void* block;
	span->free_list_limit = free_list_partial_init(&heap->free_list[class_idx], &block,
		span, pointer_offset(span, SPAN_HEADER_SIZE), size_class->block_count, size_class->block_size);
	//Link span as partial if there remains blocks to be initialized as free list, or full if fully initialized
	if (span->free_list_limit < span->block_count) {
		_rpmalloc_span_double_link_list_add(&heap->partial_span[class_idx], span);
		span->used_count = span->free_list_limit;
	} else {
#if RPMALLOC_FIRST_CLASS_HEAPS
		_rpmalloc_span_double_link_list_add(&heap->full_span[class_idx], span);
#endif
		++heap->full_span_count;
		span->used_count = span->block_count;
	}
	return block;
}

static void
_rpmalloc_span_extract_free_list_deferred(span_t* span) {
	// We need acquire semantics on the CAS operation since we are interested in the list size
	// Refer to _rpmalloc_deallocate_defer_small_or_medium for further comments on this dependency
	do {
		span->free_list = atomic_load_ptr(&span->free_list_deferred);
	} while ((span->free_list == INVALID_POINTER) || !atomic_cas_ptr_acquire(&span->free_list_deferred, INVALID_POINTER, span->free_list));
	span->used_count -= span->list_size;
	span->list_size = 0;
	atomic_store_ptr_release(&span->free_list_deferred, 0);
}

static int
_rpmalloc_span_is_fully_utilized(span_t* span) {
	assert(span->free_list_limit <= span->block_count);
	return !span->free_list && (span->free_list_limit >= span->block_count);
}

static int
_rpmalloc_span_finalize(heap_t* heap, size_t iclass, span_t* span, span_t** list_head) {
	span_t* class_span = (span_t*)((uintptr_t)heap->free_list[iclass] & _memory_span_mask);
	if (span == class_span) {
		// Adopt the heap class free list back into the span free list
		void* block = span->free_list;
		void* last_block = 0;
		while (block) {
			last_block = block;
			block = *((void**)block);
		}
		uint32_t free_count = 0;
		block = heap->free_list[iclass];
		while (block) {
			++free_count;
			block = *((void**)block);
		}
		if (last_block) {
			*((void**)last_block) = heap->free_list[iclass];
		} else {
			span->free_list = heap->free_list[iclass];
		}
		heap->free_list[iclass] = 0;
		span->used_count -= free_count;
	}
	//If this assert triggers you have memory leaks
	assert(span->list_size == span->used_count);
	if (span->list_size == span->used_count) {
		_rpmalloc_stat_dec(&heap->span_use[0].current);
		_rpmalloc_stat_dec(&heap->size_class_use[iclass].spans_current);
		// This function only used for spans in double linked lists
		if (list_head)
			_rpmalloc_span_double_link_list_remove(list_head, span);
		_rpmalloc_span_unmap(span);
		return 1;
	}
	return 0;
}


////////////
///
/// Global cache
///
//////

#if ENABLE_GLOBAL_CACHE

//! Insert the given list of memory page spans in the global cache
static void
_rpmalloc_global_cache_insert(global_cache_t* cache, span_t* span, size_t cache_limit) {
	assert((span->list_size == 1) || (span->next != 0));
	int32_t list_size = (int32_t)span->list_size;
	//Unmap if cache has reached the limit. Does not need stronger synchronization, the worst
	//case is that the span list is unmapped when it could have been cached (no real dependency
	//between the two variables)
	if (atomic_add32(&cache->size, list_size) > (int32_t)cache_limit) {
#if !ENABLE_UNLIMITED_GLOBAL_CACHE
		_rpmalloc_span_list_unmap_all(span);
		atomic_add32(&cache->size, -list_size);
		return;
#endif
	}
	void* current_cache, *new_cache;
	do {
		current_cache = atomic_load_ptr(&cache->cache);
		span->prev = (span_t*)((uintptr_t)current_cache & _memory_span_mask);
		new_cache = (void*)((uintptr_t)span | ((uintptr_t)atomic_incr32(&cache->counter) & ~_memory_span_mask));
	} while (!atomic_cas_ptr(&cache->cache, new_cache, current_cache));
}

//! Extract a number of memory page spans from the global cache
static span_t*
_rpmalloc_global_cache_extract(global_cache_t* cache) {
	uintptr_t span_ptr;
	do {
		void* global_span = atomic_load_ptr(&cache->cache);
		span_ptr = (uintptr_t)global_span & _memory_span_mask;
		if (span_ptr) {
			span_t* span = (span_t*)span_ptr;
			//By accessing the span ptr before it is swapped out of list we assume that a contending thread
			//does not manage to traverse the span to being unmapped before we access it
			void* new_cache = (void*)((uintptr_t)span->prev | ((uintptr_t)atomic_incr32(&cache->counter) & ~_memory_span_mask));
			if (atomic_cas_ptr(&cache->cache, new_cache, global_span)) {
				atomic_add32(&cache->size, -(int32_t)span->list_size);
				return span;
			}
		}
	} while (span_ptr);
	return 0;
}

//! Finalize a global cache, only valid from allocator finalization (not thread safe)
static void
_rpmalloc_global_cache_finalize(global_cache_t* cache) {
	void* current_cache = atomic_load_ptr(&cache->cache);
	span_t* span = (span_t*)((uintptr_t)current_cache & _memory_span_mask);
	while (span) {
		span_t* skip_span = (span_t*)((uintptr_t)span->prev & _memory_span_mask);
		atomic_add32(&cache->size, -(int32_t)span->list_size);
		_rpmalloc_span_list_unmap_all(span);
		span = skip_span;
	}
	assert(!atomic_load32(&cache->size));
	atomic_store_ptr(&cache->cache, 0);
	atomic_store32(&cache->size, 0);
}

//! Insert the given list of memory page spans in the global cache
static void
_rpmalloc_global_cache_insert_span_list(span_t* span) {
	size_t span_count = span->span_count;
#if ENABLE_UNLIMITED_GLOBAL_CACHE
	_rpmalloc_global_cache_insert(&_memory_span_cache[span_count - 1], span, 0);
#else
	const size_t cache_limit = (GLOBAL_CACHE_MULTIPLIER * ((span_count == 1) ? _memory_span_release_count : _memory_span_release_count_large));
	_rpmalloc_global_cache_insert(&_memory_span_cache[span_count - 1], span, cache_limit);
#endif
}

//! Extract a number of memory page spans from the global cache for large blocks
static span_t*
_rpmalloc_global_cache_extract_span_list(size_t span_count) {
	span_t* span = _rpmalloc_global_cache_extract(&_memory_span_cache[span_count - 1]);
	assert(!span || (span->span_count == span_count));
	return span;
}

#endif
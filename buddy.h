#pragma once

#include <stdbool.h>

#define KB(n) (n * 1024ull)
#define MB(n) (KB(n) * 1024ull)
#define GB(n) (MB(n) * 1024ull)

#define TEMP_ALLOC_BUFSIZE MB(8)

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned int uint;
typedef unsigned long long int u64;
typedef char i8;
typedef short i16;
typedef int i32;
typedef long long int i64;
typedef float f32;
typedef double f64;
typedef u64 uptr;

// MARK: Common

// Zeros out memory. Size is in BYTES.
void zero_memory(void *p, u64 size);
// Copies memory from source to dest. Size is in BYTES.
void copy_memory(void *dest, void *source, u64 size);
// Print NULL terminated string to stdout.
void println(char *s);

// TITLE: Memory

// MARK: Allocators

typedef enum AllocatorMessage
{
    ALLOCATOR_MSG_FREE,
    ALLOCATOR_MSG_ALLOC,
    ALLOCATOR_MSG_ZERO_ALLOC,
    ALLOCATOR_MSG_REALLOC,
} AllocatorMessage;

// The allocator is a generic structure with an internal allocation process.
// Some functions take an allocator as an argument to give the user mote control
// over how memory gets allocated and used in the program. Most allocators have
// a `get_x_allocator` function. You can read more about how they work and what
// they are used for below.
typedef struct Allocator
{
    void *data;
    void *(*proc)(struct Allocator a, AllocatorMessage msg, u64 size, void *old_ptr);
} Allocator;

// Allocates memory. Returns NULL if the allocation fails.
void *alloc(Allocator a, u64 size);
// Same as `alloc`, but zeroes out memory as well.
void *alloc_zero(Allocator a, u64 size);
// Reallocates memory with new size. Returns NULL if the allocation fails.
void *alloc_realloc(Allocator a, void *p, u64 new_size);
// Frees memory allocated by a.
void alloc_free(Allocator a, void *p);

// Currently a wrapper for malloc. Will soon change to a custom heap allocator.
Allocator get_heap_allocator();
// Allocates memory with heap allocator. Returns NULL if allocation fails.
void *heap_alloc(u64 size);
// Same as `heap_alloc`, but zeroes memory as well.
void *heap_zero_alloc(u64 size);
// Reallocates pointer to new size with heap allocator. Returns NULL on failure.
void *heap_realloc(void *old_ptr, u64 size);
// Frees heap allocated pointer.
void heap_free(void *ptr);

// MARK: Temporary Allocator

// The temporary allocator uses predefined global memory with size
// TEMP_ALLOC_BUFSIZE, which is by default 4MB. This is used for short term
// memory with a scoped lifetime. You can reset the allocator to 0 with
// `reset_temp_memory()`. The temp allocator cannot free memory, but has
// `mark()` and `restore()` to push and pop chunks off the allocator stack.
Allocator get_temporary_allocator();
// Resets the temporary memory back to size 0.
void reset_temp_memory();
// Allocates memory using the temporary allocator. Returns NULL if the
// allocation fails.
void *temp_alloc(u64 size);
// Same as `temp_alloc`, but zeroes out memory as well.
void *temp_zero_alloc(u64 size);
// Reallocates to new size. Does not free the original memory.
void *temp_realloc(void *p, u64 size);
// Create a mark in the temporary allocator that can be returned to later.
// This should be used for allocations that dont leave the current scope.
// Returns the mark ID, restore with `temo_restore_mark()`.
u64 temp_mark();
// Restores to mark ID. Any memory allocated after the mark will be discarded.
void temp_restore_mark(u64 id);

// MARK: Arena

// Arenas a chunks of memory you can allocate to and free all at once. They make
// it easy to handle memory across multiple function calls as you can free
// everything allocated to the arena all at once.
typedef struct Arena
{
    u8 *mem;
    u64 size;
    u64 pos;
} Arena;

// Allocates an arena with the given max size. Returns NULL on allocation fail.
Arena *arena_new(Allocator a, u64 size);
// Allocates a region in the arena. Returns NULL if arena is full.
void *arena_alloc(Arena *a, u64 size);
// Same as `arena_alloc`, but zeroes out memory as well.
void *arena_zero_alloc(Arena *a, u64 size);
// Get an allocator using the given arena.
Allocator get_arena_allocator(Arena *a);

// TITLE: Strings

// MARK: String

// Strings are simply a pointer with a length and an error value. There is no
// NULL terminator. When using string functions, they may set err=true if
// something went wrong. This allows future string methods to return gracefully
// on error, and you just have to check the final string result for an error.
typedef struct String
{
    char *s;
    u64 length;
    bool err;
} String;

#define ERROR_STRING ((String){.s = NULL, .length = 0, .err = true})

// Returns the byte length of a NULL-terminated C string.
uint cstr_len(char *s);
// Allocates a new string using the temporary allocator. See `str_alloc` to use
// a custom allocator. Returns ERROR_STRING if allocation fails or s is NULL.
String str_temp(char *s);
// Allocates and returns a copy of the string using allocator. Returns
// ERROR_STRING if allocation fails or s har an error.
String str_alloc(Allocator a, String s);
// Allocates and returns a copy of the string using allocator. Returns
// ERROR_STRING if allocation fails.
String str_alloc_cstr(Allocator a, char *s);
// Returns a string view of the original string. Returns ERROR_STRING if the
// range is out of bounds or the original string has an error.
String str_view(String s, uint start, uint end);
// Returns true if both strings are equal. Returns false if not, or if one has
// an error.
bool str_equal(String a, String b);
// Returns count of character c in string s.
uint str_count(String s, char c);
// Converts the original string s to uppercase, returns the same string for
// convenience.
String str_upper(String s);
// Converts the original string s to lowercase, returns the same string for
// convenience.
String str_lower(String s);
// Replaces all occurances of old char with new in the original string. Returns
// the same string for convenience.
String str_replace_char(String s, char old, char new_c);
// Allocates and returns a new copy of s with the old substrings replaced with
// new. Returns ERROR_STRING if either string has an error or allocation fails.
String str_replace_str(Allocator a, String s, String old, String new_s);
// Reverses the original string. Returns same string for convenience. Returns
// ERROR_STRING if s has an error.
String str_reverse(String s);

// MARK: StringBuilder

// StringBuilder creates a string from multiple others. It automatically
// reallocates the internal memory with the allocator given on init.
typedef struct StringBuilder
{
    Allocator a;
    u64 size;
    u64 length;
    char *mem;
    bool err;
} StringBuilder;

#define ERROR_STRING_BUILDER ((StringBuilder){\
        .size = 0,\
        .length = 0,\
        .mem = NULL, \
        .err = true,\
        })

// Returns a new allocated string builder with the given max size. Returns
// ERROR_STRING_BUILDER if allocation fails.
StringBuilder str_builder_new(Allocator a, u64 size);
// Appends string to the builder. Returns true on success. Returns false if s
// has an error or internal reallocation fails.
bool str_builder_append(StringBuilder *sb, String s);
// Same as `str_builder_append`.
bool str_builder_append_cstr(StringBuilder *sb, char *s);
// Returns the string builder as a string.
String str_builder_to_string(StringBuilder *sb);

// TITLE: OS

// MARK: IO

// Array of bytes with length.
typedef struct ByteArray
{
    u8 *bytes;
    u64 length;
    bool err;
} ByteArray;

#define ERROR_BYTE_ARRAY ((ByteArray){.err = true, .length = 0, .bytes = NULL})

// Exit program with status. Flushes standard input and output.
void os_exit(u8 status);
// Write bytes to standard output.
void os_write_out(u8 *bytes, u64 length);
// Write bytes to standard error output.
void os_write_err(u8 *bytes, u64 length);
// Returns bytes read from standard input, with the bytes pointer pointing to
// the given buffer. Returns ERROR_BYTE_ARRAY on failed read.
ByteArray os_read_input(u8 *buffer, u64 max_length);
// Returns bytes read from standard input, allocating a byte array using the
// given allocator. Returns ERROR_BYTE_ARRAY on allocation fail or failed read.
ByteArray os_read_all_input(Allocator a, u64 max_length);

// MARK: Files

// File
typedef struct File
{
    Allocator allocator;
    String name;

    u64 size;
    u64 size_on_disk; // Aligned to os page size

    // permissions, path, date, ...

    bool open;
    bool writeable;
    bool readable;

    int fd;
    bool err;
} File;


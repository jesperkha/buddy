#pragma once

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

typedef u8 bool;
#define true 1
#define false 0

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

#define ERROR_STRING_BUILDER ((StringBuilder){.mem = NULL, .length = 0, .err = true, .size = 0})

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
ByteArray os_read_input_all(Allocator a, u64 max_length);

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

#ifdef BUDDY_IMPLEMENTATION

#include <unistd.h>
#include <fcntl.h>

// MARK: Internal utils

#define panic(msg)              \
    {                           \
        println("buddy: " msg); \
        os_exit(1);             \
    }

// Root of all evil
#define _STRING(_s) \
    (String) { .err = false, .s = s, .length = cstr_len(s) }

#define assert_not_null(_var, _msg) \
    {                               \
        if ((_var) == NULL)         \
            panic(_msg);            \
    }

void zero_memory(void *p, u64 size)
{
    assert_not_null(p, "zero_memory: p is NULL");
    u8 *_p = (u8 *)p;
    for (int i = 0; i < size; i++)
        _p[i] = 0;
}

void copy_memory(void *dest, void *source, u64 size)
{
    assert_not_null(dest, "copy_memory: destination is NULL");
    assert_not_null(source, "copy_memory: source is NULL");
    u8 *_dst = (u8 *)dest;
    u8 *_src = (u8 *)source;
    for (int i = 0; i < size; i++)
        _dst[i] = _src[i];
}

// MARK: Allocator

void *alloc(Allocator a, u64 size)
{
    assert_not_null(a.proc, "alloc: a is NULL");
    return a.proc(a, ALLOCATOR_MSG_ALLOC, size, NULL);
}

void *alloc_zero(Allocator a, u64 size)
{
    assert_not_null(a.proc, "alloc: a is NULL");
    return a.proc(a, ALLOCATOR_MSG_ZERO_ALLOC, size, NULL);
}

void *alloc_realloc(Allocator a, void *p, u64 new_size)
{
    assert_not_null(a.proc, "alloc: a is NULL");
    return a.proc(a, ALLOCATOR_MSG_REALLOC, new_size, p);
}

// Temporary allocator using by default 4MB of max memory. Does not support
// freeing or reallocation as this doesnt really makes sense for a temporary
// allocator.

#define BLOCK_SIGNATURE (0xDEADDECAFC0FFEE7ull)

u8 _temp_alloc_buffer[TEMP_ALLOC_BUFSIZE];
u8 *_temp_alloc_head = _temp_alloc_buffer;

typedef struct TempMemoryHeader
{
    u64 signature;
    u64 size;
} TempMemoryHeader;

void reset_temp_memory()
{
    _temp_alloc_head = _temp_alloc_buffer;
}

// Temp memory allocation using global buffer. Does not support free as this
// doesnt really make sense for a temporary allocator. Does not free old memory
// on reallocation. Change initial buffer size by setting TEMP_ALLOC_BUFSIZE.
static void *temporary_allocator_proc(Allocator a, AllocatorMessage msg, u64 size, void *old_ptr)
{
    switch (msg)
    {
    case ALLOCATOR_MSG_ALLOC:
    {
        u64 actual_size = size + sizeof(TempMemoryHeader);
        if ((_temp_alloc_head - _temp_alloc_buffer) + actual_size > TEMP_ALLOC_BUFSIZE)
            return NULL;

        TempMemoryHeader header = {
            .signature = BLOCK_SIGNATURE,
            .size = size,
        };

        u8 *p = _temp_alloc_head;
        copy_memory(p, &header, sizeof(TempMemoryHeader));

        _temp_alloc_head += actual_size;
        return p + sizeof(TempMemoryHeader);
    }

    case ALLOCATOR_MSG_ZERO_ALLOC:
    {
        void *p = temporary_allocator_proc(a, ALLOCATOR_MSG_ALLOC, size, NULL);
        if (p == NULL)
            return p;
        zero_memory(p, size);
        return p;
    }

    case ALLOCATOR_MSG_REALLOC:
    {
        void *p = temporary_allocator_proc(a, ALLOCATOR_MSG_ALLOC, size, NULL);
        if (p == NULL)
            return p;

        TempMemoryHeader *header = (TempMemoryHeader *)((u8 *)old_ptr - sizeof(TempMemoryHeader));
        if (header->signature != BLOCK_SIGNATURE)
            return NULL;

        copy_memory(p, old_ptr, header->size);
        return p;
    }

    case ALLOCATOR_MSG_FREE:
        panic("temporary allocator cannot free memory, only reset");
    }

    panic("temporary_allocator_proc: unknown allocation message");
    return NULL;
}

Allocator get_temporary_allocator()
{
    Allocator a;
    a.proc = temporary_allocator_proc;
    return a;
}

void *temp_alloc(u64 size)
{
    Allocator a = get_temporary_allocator();
    return alloc(a, size);
}

void *temp_zero_alloc(u64 size)
{
    Allocator a = get_temporary_allocator();
    return alloc_zero(a, size);
}

void *temp_realloc(void *p, u64 size)
{
    Allocator a = get_temporary_allocator();
    return alloc_realloc(a, p, size);
}

u64 temp_mark()
{
    return _temp_alloc_head - _temp_alloc_buffer;
}

void temp_restore_mark(u64 id)
{
    _temp_alloc_head = _temp_alloc_buffer + id;
}

// MARK: Arena

Arena *arena_new(Allocator a, u64 size)
{
    Arena *arena = alloc(a, size + sizeof(Arena));
    if (arena == NULL)
        return arena;

    arena->mem = (u8 *)arena + sizeof(Arena);
    arena->pos = 0;
    arena->size = size;
    return arena;
}

void *arena_alloc(Arena *a, u64 size)
{
    assert_not_null(a, "arena_alloc: a is NULL");
    if (a->pos + size > a->size)
        return NULL;

    void *p = a->mem + a->pos;
    a->pos += size;
    return p;
}

void *arena_zero_alloc(Arena *a, u64 size)
{
    void *p = arena_alloc(a, size);
    if (p == NULL)
        return p;

    zero_memory(p, size);
    return p;
}

static void *arena_allocator_proc(Allocator a, AllocatorMessage msg, u64 size, void *old_ptr)
{
    switch (msg)
    {
    case ALLOCATOR_MSG_ALLOC:
    {
        Arena *arena = a.data;
        if (arena == NULL)
            return arena;

        return arena_alloc(arena, size);
    }

    case ALLOCATOR_MSG_ZERO_ALLOC:
    {
        Arena *arena = a.data;
        if (arena == NULL)
            return arena;

        return arena_zero_alloc(arena, size);
    }

    case ALLOCATOR_MSG_REALLOC:
        panic("arena allocator cannot reallocate memory");
    case ALLOCATOR_MSG_FREE:
        panic("arena allocator cannot free memory");
    }

    panic("arena_allocator_proc: got unknown allocator message");
    return NULL;
}

Allocator get_arena_allocator(Arena *a)
{
    Allocator al;
    al.data = a;
    al.proc = arena_allocator_proc;
    return al;
}

// MARK: String

uint cstr_len(char *s)
{
    assert_not_null(s, "cstr_len: s is NULL");
    uint count = 0;
    char *p = s;
    while (*p != 0)
    {
        p++;
        count++;
    }
    return count;
}

String str_view(String s, uint start, uint end)
{
    if (s.err || start >= s.length || end >= s.length)
        return ERROR_STRING;

    return (String){
        .s = s.s + start,
        .length = end - start,
    };
}

String str_temp(char *s)
{
    return str_alloc_cstr(get_temporary_allocator(), s);
}

String str_alloc(Allocator a, String s)
{
    if (s.err)
        return ERROR_STRING;

    char *mem = alloc(a, s.length);
    if (mem == NULL)
        return ERROR_STRING;

    copy_memory(mem, s.s, s.length);
    return (String){
        .err = false,
        .length = s.length,
        .s = mem,
    };
}

String str_alloc_cstr(Allocator a, char *s)
{
    if (s == NULL)
        return ERROR_STRING;

    u64 len = cstr_len(s);
    char *mem = alloc(a, len);
    if (mem == NULL)
        return ERROR_STRING;

    copy_memory(mem, s, len);
    return (String){
        .err = false,
        .length = len,
        .s = mem,
    };
}

uint str_count(String s, char c)
{
    if (s.err)
        return 0;
    uint count = 0;
    for (int i = 0; i < s.length; i++)
        if (s.s[i] == c)
            count++;
    return count;
}

#define IS_UPPER(c) (c >= 'A' && c <= 'Z')
#define IS_LOWER(c) (c >= 'a' && c <= 'z')
#define ASCII_CASE_DIFFERENCE 32

String str_upper(String s)
{
    if (s.err)
        return ERROR_STRING;
    for (int i = 0; i < s.length; i++)
        if (IS_LOWER(s.s[i]))
            s.s[i] -= ASCII_CASE_DIFFERENCE;
    return s;
}

String str_lower(String s)
{
    if (s.err)
        return ERROR_STRING;
    for (int i = 0; i < s.length; i++)
        if (IS_UPPER(s.s[i]))
            s.s[i] += ASCII_CASE_DIFFERENCE;
    return s;
}

bool str_equal(String a, String b)
{
    if (a.err || b.err || a.length != b.length)
        return false;
    for (int i = 0; i < a.length; i++)
        if (a.s[i] != b.s[i])
            return false;
    return true;
}

String str_replace_char(String s, char old, char new_c)
{
    if (s.err)
        return ERROR_STRING;
    for (int i = 0; i < s.length; i++)
        if (s.s[i] == old)
            s.s[i] = new_c;
    return s;
}

String str_replace_str(Allocator a, String s, String old, String new_s)
{
    // TODO: str_replace_str()
    return ERROR_STRING;
}

String str_reverse(String s)
{
    if (s.err)
        return ERROR_STRING;

    uint l = s.length / 2;
    for (int i = 0; i < l; i++)
    {
        char left = s.s[i];
        uint right_idx = s.length - i - 1;
        s.s[i] = s.s[right_idx];
        s.s[right_idx] = left;
    }

    return s;
}

// MARK: StringBuilder

StringBuilder str_builder_new(Allocator a, u64 size)
{
    char *mem = alloc(a, size);
    if (mem == NULL)
        return ERROR_STRING_BUILDER;

    return (StringBuilder){
        .err = false,
        .length = 0,
        .size = size,
        .mem = mem,
        .a = a,
    };
}

bool str_builder_append(StringBuilder *sb, String s)
{
    assert_not_null(sb, "str_builder_append: sb is NULL");
    if (s.err || sb->err)
        return false;

    // Reallocate internal buffer on overflow
    if (sb->length + s.length > sb->size)
    {
        u64 new_size = sb->size * 2;
        // Resize until it fits the string
        while (new_size < sb->size + s.length)
            new_size *= 2;

        char *new_mem = alloc_realloc(sb->a, sb->mem, new_size);
        if (new_mem == NULL)
            return false;

        sb->mem = new_mem;
        sb->size = new_size;
    }

    copy_memory(sb->mem + sb->length, s.s, s.length);
    sb->length += s.length;
    return true;
}

bool str_builder_append_cstr(StringBuilder *sb, char *s)
{
    assert_not_null(s, "str_builder_append_cstr: s is NULL");
    return str_builder_append(sb, _STRING(s));
}

String str_builder_to_string(StringBuilder *sb)
{
    assert_not_null(sb, "str_builder_to_string: sb is NULL");
    return (String){
        .err = false,
        .length = sb->length,
        .s = sb->mem,
    };
}

// MARK: OS

void os_write_out(u8 *bytes, u64 length)
{
    assert_not_null(bytes, "_os_write: bytes is NULL");
    write(STDOUT_FILENO, bytes, length);
}

void os_write_err(u8 *bytes, u64 length)
{
    assert_not_null(bytes, "_os_write: bytes is NULL");
    write(STDOUT_FILENO, bytes, length);
}

ByteArray os_read_input(u8 *buffer, u64 max_length)
{
    assert_not_null(buffer, "os_read_input: buffer is NULL");
    u64 len = read(STDIN_FILENO, buffer, max_length);
    if (len < 0)
        return ERROR_BYTE_ARRAY;

    return (ByteArray){
        .err = false,
        .bytes = buffer,
        .length = len,
    };
}

ByteArray os_read_input_all(Allocator a, u64 max_length)
{
    // TODO: os_read_input_all
    return (ByteArray){0};
}

void _os_flush_input()
{
    char c;
    while (read(STDIN_FILENO, &c, 1) > 0 && c != '\n' && c != EOF)
        ;
}

void _os_flush_output()
{
    // Write nothing to flush
    write(STDERR_FILENO, "", 0);
    write(STDOUT_FILENO, "", 0);
}

void os_exit(u8 status)
{
    _os_flush_output();
    _os_flush_input();
    _exit(status);
}

void println(char *s)
{
    assert_not_null(s, "print_cstr: s is NULL");
    os_write_out((u8 *)s, cstr_len(s));
    os_write_out((u8 *)"\n", 1);
}

// String: split, find, dup, trim, iter, concat, StringBuilder, StringArray
//
// Path: append, root, home, backDir, toString, toWindows, to absolute,
// getFilename, getFileExtension
//
// File: open, close, read, write, append, view, toString, copy
//
// Alloc: alloc, free, realloc, view, temp, page
//
// print, format, print_error, log
//
// input, char, string, raw term input
//
// Time: now, add, format, difference

#endif

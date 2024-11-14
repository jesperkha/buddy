#pragma once

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#define KB(n) (n * 1024ull)
#define MB(n) (KB(n) * 1024ull)
#define GB(n) (MB(n) * 1024ull)

#define TEMP_ALLOC_BUFSIZE MB(8)

#define panic(msg)                  \
    {                               \
        printf("buddy: " msg "\n"); \
        exit(1);                    \
    }

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

// MARK: Common definitions

// Zeros out memory. Size is in BYTES.
void zero_memory(void *p, u64 size);
// Copies memory from source to dest. Size is in BYTES.
void copy_memory(void *dest, void *source, u64 size);

// MARK: Allocator definitions

typedef enum AllocatorMessage
{
    ALLOCATOR_MSG_FREE,
    ALLOCATOR_MSG_ALLOC,
    ALLOCATOR_MSG_ZERO_ALLOC,
    ALLOCATOR_MSG_REALLOC,
} AllocatorMessage;

typedef struct Allocator
{
    void *data;
    u64 size;
    u64 pos;
    void *(*proc)(struct Allocator *a, AllocatorMessage msg, u64 size, void *old_ptr);
} Allocator;

// Allocates memory using the given allocator. Returns NULL if the allocation
// fails.
void *alloc(Allocator *a, u64 size);
// Same as `alloc`, but zeros out memory as well.
void *alloc_zero(Allocator *a, u64 size);

// The temporary allocator uses predefined global memory with size
// TEMP_ALLOC_BUFSIZE, which is by default 4MB.
Allocator get_temporary_allocator();
// Resets the temporary memory back to size 0.
void reset_temp_memory();
// Allocates memory using the temporary allocator. Returns NULL if the
// allocation fails.
void *temp_alloc(u64 size);
// Same as `temp_alloc`, but zeros out memory as well.
void *temp_zero_alloc(u64 size);

// MARK: String delcarations

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
String str_new(char *s);
// Allocates and returns a copy of the string using allocator. Returns
// ERROR_STRING if allocation fails or s har an error.
String str_alloc(Allocator *a, String s);
// Allocates and returns a copy of the string using allocator. Returns
// ERROR_STRING if allocation fails.
String str_alloc_cstr(Allocator *a, char *s);
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
String str_replace_str(String s, String old, String new_s, Allocator a);
// Reverses the original string. Returns same string for convenience. Returns
// ERROR_STRING if s has an error.
String str_reverse(String s);

#ifdef BUDDY_IMPLEMENTATION

void zero_memory(void *p, u64 size)
{
    if (p == NULL)
        panic("zero_memory: p is NULL");
    u8 *_p = (u8 *)p;
    for (int i = 0; i < size; i++)
        _p[i] = 0;
}

void copy_memory(void *dest, void *source, u64 size)
{
    if (dest == NULL)
        panic("copy_memory: destination is NULL");
    if (source == NULL)
        panic("copy_memory: source is NULL");
    u8 *_dst = (u8 *)dest;
    u8 *_src = (u8 *)source;
    for (int i = 0; i < size; i++)
        _dst[i] = _src[i];
}

// MARK: Allocator implementation

void *alloc(Allocator *a, u64 size)
{
    return a->proc(a, ALLOCATOR_MSG_ALLOC, size, NULL);
}

void *alloc_zero(Allocator *a, u64 size)
{
    return a->proc(a, ALLOCATOR_MSG_ZERO_ALLOC, size, NULL);
}

// Temporary allocator using by default 4MB of max memory. Does not support
// freeing or reallocation as this doesnt really makes sense for a temporary
// allocator.

u8 _temp_alloc_buffer[TEMP_ALLOC_BUFSIZE];
u8 *_temp_alloc_head = _temp_alloc_buffer;

void reset_temp_memory()
{
    _temp_alloc_head = _temp_alloc_buffer;
}

static void *temporary_allocator_proc(Allocator *a, AllocatorMessage msg, u64 size, void *old_ptr)
{
    switch (msg)
    {
    case ALLOCATOR_MSG_ALLOC:
    {
        if ((_temp_alloc_head - _temp_alloc_buffer) + size > TEMP_ALLOC_BUFSIZE)
            return NULL;

        void *p = _temp_alloc_head;
        _temp_alloc_head += size;
        return p;
    }

    case ALLOCATOR_MSG_ZERO_ALLOC:
    {
        void *p = temporary_allocator_proc(a, ALLOCATOR_MSG_ALLOC, size, NULL);
        if (p == NULL)
            return p;
        zero_memory(p, size);
        return p;
    }

    case ALLOCATOR_MSG_FREE:
        panic("temporary allocator cannot free memory, only reset");
        break;

    case ALLOCATOR_MSG_REALLOC:
        panic("temporary allocator cannot reallocate memory");
        break;
    }

    panic("temporary allocator got unknown allocation message");
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
    return alloc(&a, size);
}

void *temp_zero_alloc(u64 size)
{
    Allocator a = get_temporary_allocator();
    return alloc_zero(&a, size);
}

// MARK: String implementation

uint cstr_len(char *s)
{
    if (s == NULL)
        panic("cstr_len: s is NULL");
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

String str_new(char *s)
{
    if (s == NULL)
        return ERROR_STRING;
    u64 len = cstr_len(s);
    char *mem = temp_alloc(len);
    if (mem == NULL)
        return ERROR_STRING;
    copy_memory(mem, s, len);
    return (String){
        .err = false,
        .length = len,
        .s = mem,
    };
}

String str_alloc(Allocator *a, String s)
{
    // TODO: make allocator first
    return ERROR_STRING;
}

String str_alloc_cstr(Allocator *a, char *s)
{
    // TODO: make allocator first
    return ERROR_STRING;
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

String str_replace_str(String s, String old, String new_s, Allocator a)
{
    // TODO: implement replace string string
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

#include "buddy.h"

#include <malloc.h> // Temporary for heap alloc

#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

// MARK: Internal utils

#define panic(msg)          \
    {                       \
        out("buddy: " msg); \
        os_exit(1);         \
    }

// Root of all evil, internal only
// DO NOT USE FOR STRING LITERALS EVER
#define _STRING(_s) \
    (String) { .err = false, .s = _s, .length = cstr_len(_s) }

#define assert_not_null(_var, _msg) \
    {                               \
        if ((_var) == NULL)         \
            panic(_msg);            \
    }

void zero_memory(void *p, u64 size)
{
    assert_not_null(p, "zero_memory: p is NULL");
    for (int i = 0; i < size; i++)
        ((u8 *)p)[i] = 0;
}

void copy_memory(void *dest, void *source, u64 size)
{
    assert_not_null(dest, "copy_memory: destination is NULL");
    assert_not_null(source, "copy_memory: source is NULL");
    for (int i = 0; i < size; i++)
        ((u8 *)dest)[i] = ((u8 *)source)[i];
}

// MARK: Allocator

typedef struct BlockHeader
{
    u64 signature;
    u64 size;
} BlockHeader;

BlockHeader *_get_block_header(void *p)
{
    return (BlockHeader*)((u8*)p - sizeof(BlockHeader));
}

void *alloc(Allocator a, u64 size)
{
    assert_not_null(a.proc, "alloc: a is NULL");
    return a.proc(a, ALLOCATOR_MSG_ALLOC, size, NULL);
}

void *alloc_zero(Allocator a, u64 size)
{
    assert_not_null(a.proc, "alloc_zero: a is NULL");
    return a.proc(a, ALLOCATOR_MSG_ZERO_ALLOC, size, NULL);
}

void *alloc_realloc(Allocator a, void *p, u64 new_size)
{
    assert_not_null(a.proc, "alloc_realloc: a is NULL");
    return a.proc(a, ALLOCATOR_MSG_REALLOC, new_size, p);
}

void alloc_free(Allocator a, void *p)
{
    assert_not_null(a.proc, "alloc_free: a is NULL");
    a.proc(a, ALLOCATOR_MSG_FREE, 0, p);
}

// Temporary allocator using by default 4MB of max memory. Does not support
// freeing or reallocation as this doesnt really makes sense for a temporary
// allocator.

#define TEMP_BLOCK_SIGNATURE (0xDEADDECAFC0FFEE7ull)

u8 _temp_alloc_buffer[TEMP_ALLOC_BUFSIZE];
u8 *_temp_alloc_head = _temp_alloc_buffer;

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
        u64 actual_size = size + sizeof(BlockHeader);
        if ((_temp_alloc_head - _temp_alloc_buffer) + actual_size > TEMP_ALLOC_BUFSIZE)
            return NULL;

        BlockHeader *block = (BlockHeader*)_temp_alloc_head;
        block->signature = TEMP_BLOCK_SIGNATURE;
        block->size = size;

        _temp_alloc_head += actual_size;
        return (u8*)block + sizeof(BlockHeader);
    }

    case ALLOCATOR_MSG_ZERO_ALLOC:
    {
        void *p = alloc(a, size);
        if (p == NULL)
            return p;

        zero_memory(p, size);
        return p;
    }

    case ALLOCATOR_MSG_REALLOC:
    {
        if (old_ptr == NULL)
            return NULL;

        BlockHeader *block = _get_block_header(old_ptr);
        if (block->signature != TEMP_BLOCK_SIGNATURE)
            return NULL;

        u8 *p = alloc(a, size);
        if (p == NULL)
            return NULL;

        copy_memory(p, old_ptr, block->size);
        return p;
    }

    case ALLOCATOR_MSG_FREE:
        break;
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
    Arena *arena = (Arena*)alloc(a, size + sizeof(Arena));
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

#define ARENA_BLOCK_SIGNATURE (0xBABABEBEF000000Dull)

static void *arena_allocator_proc(Allocator a, AllocatorMessage msg, u64 size, void *old_ptr)
{
    switch (msg)
    {
    case ALLOCATOR_MSG_ALLOC:
    {
        Arena *arena = (Arena*)a.data;
        if (arena == NULL)
            return arena;

        BlockHeader *block = arena_alloc(arena, size + sizeof(BlockHeader));
        if (block == NULL)
            return block;

        block->size = size;
        block->signature = ARENA_BLOCK_SIGNATURE;
        return (u8*)block + sizeof(BlockHeader);
    }

    case ALLOCATOR_MSG_ZERO_ALLOC:
    {
        Arena *arena = (Arena*)a.data;
        if (arena == NULL)
            return arena;

        void *p = alloc(a, size);
        if (p == NULL)
            return p;

        zero_memory(p, size);
        return p;
    }

    case ALLOCATOR_MSG_REALLOC:
    {
        if (old_ptr == NULL)
            return NULL;

        BlockHeader *block = _get_block_header(old_ptr);
        if (block->signature != ARENA_BLOCK_SIGNATURE)
            return NULL;

        u8 *p = alloc(a, size);
        if (p == NULL)
            return NULL;

        copy_memory(p, old_ptr, block->size);
        return p;
    }

    case ALLOCATOR_MSG_FREE:
        break;
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

// MARK: Heap allocator

static void *heap_allocator_proc(Allocator a, AllocatorMessage msg, u64 size, void *old_ptr)
{
    switch (msg)
    {
    case ALLOCATOR_MSG_ALLOC:
        return malloc(size);

    case ALLOCATOR_MSG_ZERO_ALLOC:
        return calloc(size, sizeof(u8));

    case ALLOCATOR_MSG_REALLOC:
        return realloc(old_ptr, size);

    case ALLOCATOR_MSG_FREE:
        free(old_ptr);
        return NULL;
    }

    panic("heap_allocator_proc: got unknown allocator message");
    return NULL;
}

Allocator get_heap_allocator()
{
    return (Allocator) {
        .data = NULL,
        .proc = heap_allocator_proc,
    };
}

void *heap_alloc(u64 size)
{
    return alloc(get_heap_allocator(), size);
}

void *heap_zero_alloc(u64 size)
{
    return alloc_zero(get_heap_allocator(), size);
}

void *heap_realloc(void *old_ptr, u64 size)
{
    return alloc_realloc(get_heap_allocator(), old_ptr, size);
}

void heap_free(void *ptr)
{
    alloc_free(get_heap_allocator(), ptr);
}

// MARK: String

uint cstr_len(const char *s)
{
    assert_not_null(s, "cstr_len: s is NULL");
    uint count = 0;
    const char *p = s;
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

    char *mem = (char*)alloc(a, s.length);
    if (mem == NULL)
        return ERROR_STRING;

    copy_memory(mem, s.s, s.length);
    return (String){
        .s = mem,
        .length = s.length,
        .err = false,
    };
}

String str_alloc_cstr(Allocator a, char *s)
{
    if (s == NULL)
        return ERROR_STRING;

    u64 len = cstr_len(s);
    char *mem = (char*)alloc(a, len);
    if (mem == NULL)
        return ERROR_STRING;

    copy_memory(mem, s, len);
    return (String){
        .s = mem,
        .length = len,
        .err = false,
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

bool cstr_equal(char *a, char *b)
{
    assert_not_null(a, "cstr_equal: a is NULL");
    assert_not_null(b, "cstr_equal: b is NULL");
    return str_equal(_STRING(a), _STRING(b));
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

#define _STRINGBUILDER_START_SIZE 64

StringBuilder str_builder_new(Allocator a)
{
    const u64 size = _STRINGBUILDER_START_SIZE;

    char *mem = (char*)alloc(a, size);
    if (mem == NULL)
        return ERROR_STRING_BUILDER;

    return (StringBuilder){
        .a = a,
        .size = size,
        .length = 0,
        .mem = mem,
        .err = false,
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

        char *new_mem = (char*)alloc_realloc(sb->a, sb->mem, new_size);
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

bool str_builder_append_char(StringBuilder *sb, char c)
{
    char s[2] = {c, 0};
    return str_builder_append_cstr(sb, s);
}

String str_builder_to_string(StringBuilder *sb)
{
    assert_not_null(sb, "str_builder_to_string: sb is NULL");
    return (String){
        .s = sb->mem,
        .length = sb->length,
        .err = false,
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

ByteArray os_read_all_input(Allocator a)
{
    u64 size = 2;

    // Remaining space after the text already read in
    u64 readable_size = size;

    u8 *buffer = alloc(a, size);
    u8 *offset = buffer; // Where to read into

    // [ xxxx---- ]
    //   ^ buffer
    //       ^ offset
    //       ---- readable_size
    //   xxxx---- size

    while (true)
    {
        ByteArray arr = os_read_input(offset, readable_size);
        if (arr.err)
            return arr;

        if (arr.length < readable_size)
            return (ByteArray){
                .err = false,
                .bytes = buffer,
                .length = size,
            };

        u64 new_size = size * 2;
        buffer = alloc_realloc(a, buffer, new_size);
        if (buffer == NULL)
            return ERROR_BYTE_ARRAY;

        offset = buffer + size;
        size = new_size;
        readable_size = size/2;
    }

    panic("os_read_all_input: unreachable");
    return ERROR_BYTE_ARRAY;
}

void _os_flush_output()
{
    // Write nothing to flush
    write(STDERR_FILENO, "", 0);
    write(STDOUT_FILENO, "", 0);
}

void _os_flush_input()
{
    char buffer[64];

    // Set stdin to non-blocking mode
    int flags = fcntl(STDIN_FILENO, F_GETFL);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);

    while (read(STDIN_FILENO, buffer, sizeof(buffer)) > 0);

    // Restore blocking mode
    fcntl(STDIN_FILENO, F_SETFL, flags);
}

void os_exit(u8 status)
{
    _os_flush_output();
    _os_flush_input();
    _exit(status);
}

static String _number_to_string(u64 n, bool sign)
{
    char number[21]; // u64 max is 20 digits
    zero_memory(number, 16);

    int i;
    for (i = 0; n > 0 && i < 20; i++)
    {
        u8 diff = n % 10;
        number[i] = (char)(diff + 48);
        n /= 10;
    }

    if (sign)
        number[i] = '-';

    String result = str_temp(number);
    str_reverse(result);
    return result;
}

String int_to_string(i64 n)
{
    bool sign = n < 0;
    if (n < 0) n *= -1;
    return _number_to_string(n, sign);
}

String uint_to_string(u64 n)
{
    return _number_to_string(n, false);
}

static void _append_specifier(StringBuilder *sb, char *spec, va_list list)
{
    // String
    if (cstr_equal(spec, "s"))
        str_builder_append_cstr(sb, va_arg(list, char*));
    else if (cstr_equal(spec, "S"))
        str_builder_append(sb, va_arg(list, String));

    // Signed int
    else if (cstr_equal(spec, "i8"))
        str_builder_append(sb, int_to_string((i8)va_arg(list, i64)));
    else if (cstr_equal(spec, "i16"))
        str_builder_append(sb, int_to_string((i16)va_arg(list, i64)));
    else if (cstr_equal(spec, "i32"))
        str_builder_append(sb, int_to_string((i32)va_arg(list, i64)));
    else if (cstr_equal(spec, "i64"))
        str_builder_append(sb, int_to_string((i64)va_arg(list, i64)));

    // Unsigned int
    else if (cstr_equal(spec, "u8"))
        str_builder_append(sb, int_to_string((u8)va_arg(list, u64)));
    else if (cstr_equal(spec, "u16"))
        str_builder_append(sb, int_to_string((u16)va_arg(list, u64)));
    else if (cstr_equal(spec, "u32"))
        str_builder_append(sb, int_to_string((u32)va_arg(list, u64)));
    else if (cstr_equal(spec, "u64"))
        str_builder_append(sb, int_to_string((u64)va_arg(list, u64)));

    // Unknown specifier
    else
    {
        str_builder_append_char(sb, '{');
        str_builder_append_cstr(sb, spec);
        str_builder_append_char(sb, '}');
    }
}

#define _MAX_SPECIFIER 8

String _fmt(const char *format, va_list args)
{
    assert_not_null(format, "fmt: format is NULL");

    StringBuilder sb = str_builder_new(get_temporary_allocator());
    String fmt_s = _STRING((char*)format);

    for (int i = 0; i < fmt_s.length; i++)
    {
        char c = fmt_s.s[i];
        if (c != '{')
        {
            str_builder_append_char(&sb, c);
            continue;
        }

        i++; // Skip opening brace

        char spec[_MAX_SPECIFIER];
        zero_memory(spec, _MAX_SPECIFIER);

        // Extract word from {  }
        int j = 0;
        while (i < fmt_s.length && j < _MAX_SPECIFIER && fmt_s.s[i] != '}')
            spec[j++] = fmt_s.s[i++];

        _append_specifier(&sb, spec, args);
    }

    return str_builder_to_string(&sb);
}

String fmt(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    String f = _fmt(format, args);
    va_end(args);
    return f;
}

void out(const char *format, ...)
{
    va_list args;
    va_start(args, format);

    String output = _fmt(format, args);
    os_write_out((u8*)output.s, output.length);
    os_write_out((u8*)"\n", 1);

    va_end(args);
}

void out_no_newline(const char *format, ...)
{
    va_list args;
    va_start(args, format);

    String output = _fmt(format, args);
    os_write_out((u8*)output.s, output.length);

    va_end(args);
}


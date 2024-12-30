#include "buddy.h"

#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>

#include <malloc.h> // Temporary for heap alloc

#if defined(OS_LINUX)
#define PATH_SEP '/'
#elif defined(OS_WINDOWS)
#define PATH_SEP '\\'
#endif

// :utils

#define panic(msg)                         \
    {                                      \
        char *m = "buddy: " msg "\n";      \
        os_write_err((u8*)m, cstr_len(m)); \
        os_exit(1);                        \
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
    for (u64 i = 0; i < size; i++)
        ((u8 *)p)[i] = 0;
}

void copy_memory(void *dest, const void *source, u64 size)
{
    assert_not_null(dest, "copy_memory: destination is NULL");
    assert_not_null(source, "copy_memory: source is NULL");
    for (u64 i = 0; i < size; i++)
        ((u8 *)dest)[i] = ((u8 *)source)[i];
}

// :allocator

typedef struct BlockHeader
{
    u64 signature;
    u64 size;
} BlockHeader;

BlockHeader *_get_block_header(void *p)
{
    return (BlockHeader *)((u8 *)p - sizeof(BlockHeader));
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

void reset_temp_memory(void)
{
    _temp_alloc_head = _temp_alloc_buffer;
}

// Align pointer to next 8 byte interval
u8 *_align_to_8bytes(u8 *ptr)
{
    return ptr + (8 - ((u64)ptr % 8));
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
        if ((u64)(_temp_alloc_head - _temp_alloc_buffer) + actual_size > TEMP_ALLOC_BUFSIZE)
        {
            panic("temporary allocation buffer is full");
            return NULL;
        }

        // Get and align pointer
        u8 *head = _temp_alloc_head;
        head = _align_to_8bytes(head);
        _temp_alloc_head = head + actual_size;

        BlockHeader *block = (BlockHeader *)head;
        block->signature = TEMP_BLOCK_SIGNATURE;
        block->size = size;

        return (u8 *)block + sizeof(BlockHeader);
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
        return NULL;
    }

    panic("temporary_allocator_proc: unknown allocation message");
    return NULL;
}

Allocator get_temporary_allocator(void)
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

u64 temp_mark(void)
{
    return (u64)(_temp_alloc_head - _temp_alloc_buffer);
}

void temp_restore_mark(u64 id)
{
    _temp_alloc_head = _temp_alloc_buffer + id;
}

// :arena

Arena *arena_new(Allocator a, u64 size)
{
    Arena *arena = (Arena *)alloc(a, size + sizeof(Arena));
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
        Arena *arena = (Arena *)a.data;
        if (arena == NULL)
            return arena;

        BlockHeader *block = arena_alloc(arena, size + sizeof(BlockHeader));
        if (block == NULL)
            return block;

        block->size = size;
        block->signature = ARENA_BLOCK_SIGNATURE;
        return (u8 *)block + sizeof(BlockHeader);
    }

    case ALLOCATOR_MSG_ZERO_ALLOC:
    {
        Arena *arena = (Arena *)a.data;
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

void free_arena(Arena *arena, Allocator a)
{
    assert_not_null(arena, "free_arena: arena is null");
    alloc_free(a, arena); // Arena is inline
}

// :heap allocator

static void *heap_allocator_proc(Allocator a, AllocatorMessage msg, u64 size, void *old_ptr)
{
    (void)a; // Unused param

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

Allocator get_heap_allocator(void)
{
    return (Allocator){
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

// :string

Bytes str_to_bytes(String s)
{
    if (s.err)
        return ERROR_BYTES;

    return (Bytes){
        .bytes = (u8 *)s.s,
        .length = s.length,
        .err = false,
    };
}

String bytes_to_str(Bytes b)
{
    if (b.err)
        return ERROR_STRING;

    return (String){
        .s = (char *)b.bytes,
        .length = b.length,
        .err = false,
    };
}

void free_string(String s, Allocator a)
{
    if (!s.err)
        alloc_free(a, s.s);
}

void free_bytes(Bytes b, Allocator a)
{
    if (!b.err)
        alloc_free(a, b.bytes);
}

String str_concat(Allocator a, String s1, String s2)
{
    if (s1.err || s2.err)
        return ERROR_STRING;

    char *ptr = alloc(a, s1.length + s2.length + 1);
    if (ptr == NULL)
        return ERROR_STRING;

    copy_memory(ptr, s1.s, s1.length);
    copy_memory(ptr + s1.length, s2.s, s2.length);

    return (String){
        .err = false,
        .s = ptr,
        .length = s1.length + s2.length,
    };
}

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

String str_view(String s, u64 start, u64 end)
{
    if (s.err || start >= s.length || end > s.length || start > end)
        return ERROR_STRING;

    return (String){
        .err = false,
        .s = s.s + start,
        .length = end - start,
    };
}

String str_temp(const char *s)
{
    return str_alloc_cstr(get_temporary_allocator(), s);
}

String str_copy(String s)
{
    return str_alloc(get_temporary_allocator(), s);
}

String str_copy_alloc(Allocator a, String s)
{
    return str_alloc(a, s);
}

String str_alloc(Allocator a, String s)
{
    if (s.err)
        return ERROR_STRING;

    char *mem = (char *)alloc(a, s.length + 1);
    if (mem == NULL)
        return ERROR_STRING;

    copy_memory(mem, s.s, s.length);
    s.s[s.length] = 0; // Make sure its null termiated

    return (String){
        .s = mem,
        .length = s.length,
        .err = false,
    };
}

String str_alloc_cstr(Allocator a, const char *s)
{
    if (s == NULL)
        return ERROR_STRING;

    u64 len = cstr_len(s);
    char *mem = (char *)alloc(a, len + 1);
    if (mem == NULL)
        return ERROR_STRING;

    copy_memory(mem, (void*)s, len);
    mem[len] = 0; // Make sure is null termiated

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
    for (u64 i = 0; i < s.length; i++)
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
    for (u64 i = 0; i < s.length; i++)
        if (IS_LOWER(s.s[i]))
            s.s[i] -= ASCII_CASE_DIFFERENCE;
    return s;
}

String str_lower(String s)
{
    if (s.err)
        return ERROR_STRING;
    for (u64 i = 0; i < s.length; i++)
        if (IS_UPPER(s.s[i]))
            s.s[i] += ASCII_CASE_DIFFERENCE;
    return s;
}

bool str_equal(String a, String b)
{
    if (a.err || b.err || a.length != b.length)
        return false;
    for (u64 i = 0; i < a.length; i++)
        if (a.s[i] != b.s[i])
            return false;

    return true;
}

bool cstr_equal(const char *a, const char *b)
{
    assert_not_null(a, "cstr_equal: a is NULL");
    assert_not_null(b, "cstr_equal: b is NULL");
    return str_equal(_STRING((char *)a), _STRING((char *)b));
}

String str_replace_char(String s, char old, char new_c)
{
    if (s.err)
        return ERROR_STRING;
    for (u64 i = 0; i < s.length; i++)
        if (s.s[i] == old)
            s.s[i] = new_c;
    return s;
}

String str_replace_str(Allocator a, String s, String old, String new_s)
{
    (void)a;
    (void)s;
    (void)old;
    (void)new_s;

    // TODO: str_replace_str()
    return ERROR_STRING;
}

String str_reverse(String s)
{
    if (s.err)
        return ERROR_STRING;

    u64 l = s.length / 2;
    for (u64 i = 0; i < l; i++)
    {
        char left = s.s[i];
        u64 right_idx = s.length - i - 1;
        s.s[i] = s.s[right_idx];
        s.s[right_idx] = left;
    }

    return s;
}

i64 str_find_char(String s, char c)
{
    if (s.err)
        return -1;

    for (i64 i = 0; (u64)i < s.length; i++)
        if (s.s[i] == c)
            return i;

    return -1;
}

i64 str_find_char_reverse(String s, char c)
{
    if (s.err)
        return -1;

    for (i64 i = (i64)s.length - 1; i >= 0; i--)
        if (s.s[i] == c)
            return i;

    return -1;
}

// :stringbuiler

#define _STRINGBUILDER_START_SIZE 64

StringBuilder str_builder_new(Allocator a)
{
    const u64 size = _STRINGBUILDER_START_SIZE;

    char *mem = (char *)alloc(a, size);
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

void free_string_builder(StringBuilder sb)
{
    alloc_free(sb.a, sb.mem);
}

bool str_builder_append(StringBuilder *sb, String s)
{
    if (s.err)
        return false;
    return str_builder_append_bytes(sb, (u8 *)s.s, s.length);
}

bool str_builder_append_cstr(StringBuilder *sb, const char *s)
{
    assert_not_null(s, "str_builder_append_cstr: s is NULL");
    return str_builder_append(sb, _STRING((char *)s));
}

bool str_builder_append_char(StringBuilder *sb, char c)
{
    u8 b = (u8)c;
    return str_builder_append_bytes(sb, &b, 1);
}

bool str_builder_append_bytes(StringBuilder *sb, const u8 *bytes, u64 length)
{
    assert_not_null(sb, "str_builder_append_bytes: sb is NULL");
    assert_not_null(bytes, "str_builder_append_bytes: bytes is NULL");

    if (sb->err)
        return false;

    // Reallocate internal buffer on overflow
    if (sb->length + length > sb->size)
    {
        u64 new_size = sb->size * 2;
        // Resize until it fits the string
        while (new_size < sb->size + length)
            new_size *= 2;

        char *new_mem = (char *)alloc_realloc(sb->a, sb->mem, new_size);
        if (new_mem == NULL)
            return false;

        sb->mem = new_mem;
        sb->size = new_size;
    }

    copy_memory(sb->mem + sb->length, (void *)bytes, length);
    sb->length += length;
    return true;
}

String str_builder_to_string(StringBuilder *sb)
{
    assert_not_null(sb, "str_builder_to_string: sb is NULL");
    str_builder_append_char(sb, 0); // Null terminator
    return (String){
        .s = sb->mem,
        .length = sb->length - 1, // Exclude null terminator
        .err = false,
    };
}

// :os

void os_write_out(const u8 *bytes, u64 length)
{
    assert_not_null(bytes, "os_write_out: bytes is NULL");
    write(STDOUT_FILENO, bytes, (u32)length);
}

void os_write_err(const u8 *bytes, u64 length)
{
    assert_not_null(bytes, "os_write_err: bytes is NULL");
    write(STDOUT_FILENO, bytes, (u32)length);
}

Bytes os_read_input(u8 *buffer, u64 max_length)
{
    assert_not_null(buffer, "os_read_input: buffer is NULL");
    i64 len = read(STDIN_FILENO, buffer, (u32)max_length);
    if (len < 0)
        return ERROR_BYTES;

    return (Bytes){
        .err = false,
        .bytes = buffer,
        .length = (u64)len,
    };
}

Bytes os_read_all_input(Allocator a)
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
        Bytes b = os_read_input(offset, readable_size);
        if (b.err)
            return b;

        if (b.length < readable_size)
            return (Bytes){
                .err = false,
                .bytes = buffer,
                .length = size,
            };

        u64 new_size = size * 2;
        buffer = alloc_realloc(a, buffer, new_size);
        if (buffer == NULL)
            return ERROR_BYTES;

        offset = buffer + size;
        size = new_size;
        readable_size = size / 2;
    }

    panic("os_read_all_input: unreachable");
    return ERROR_BYTES;
}

void _os_flush_output(void)
{
    // Write nothing to flush
    write(STDERR_FILENO, "", 0);
    write(STDOUT_FILENO, "", 0);
}

void _os_flush_input(void)
{
    // TODO: windows impl flush input
#if defined(OS_LINUX)
    char buffer[64];

    // Set stdin to non-blocking mode
    int flags = fcntl(STDIN_FILENO, F_GETFL);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
    while (read(STDIN_FILENO, buffer, sizeof(buffer)) > 0)
        ;

    // Restore blocking mode
    fcntl(STDIN_FILENO, F_SETFL, flags);
#endif
}

void os_exit(u8 status)
{
    _os_flush_output();
    _os_flush_input();
    _exit(status);
}

static String _number_to_string(u64 n, bool sign)
{
    if (n == 0)
        return str_temp("0");

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
    if (n < 0)
        n *= -1;
    return _number_to_string((u64)n, sign);
}

String uint_to_string(u64 n)
{
    return _number_to_string(n, false);
}

static void _format_file(StringBuilder *sb, File f)
{
    if (f.err)
    {
        str_builder_append_cstr(sb, "(ERROR_FILE)");
        return;
    }

    str_builder_append_cstr(sb, "File {\n");
    str_builder_append(sb, fmt("    .fd = {i32}\n", f.fd));
    str_builder_append(sb, fmt("    .path = {S}\n", f.path));
    str_builder_append(sb, fmt("    .size = {u64}\n", f.info.size));
    str_builder_append(sb, fmt("    .size_on_disk = {u64}\n", f.info.size_on_disk));
    str_builder_append(sb, fmt("    .open = {b}\n", f.open));
    str_builder_append(sb, fmt("    .writeable = {b}\n", f.writeable));
    str_builder_append(sb, fmt("    .readable = {b}\n", f.readable));
    str_builder_append(sb, fmt("    .err = {b}\n", f.err));
    str_builder_append_cstr(sb, "}\n");
}

// :fmt

static void _append_specifier(StringBuilder *sb, char *spec, va_list *list)
{
    // String
    if (cstr_equal(spec, "s"))
    {
        char *s = va_arg(*list, char *);
        if (s == NULL)
            str_builder_append_cstr(sb, "(NULL)");
        else
            str_builder_append_cstr(sb, s);
    }
    else if (cstr_equal(spec, "S"))
    {
        String s = va_arg(*list, String);
        if (s.err)
            str_builder_append_cstr(sb, "(ERROR_STRING)");
        else
            str_builder_append(sb, s);
    }

    // Objects
    else if (cstr_equal(spec, "B"))
    {
        Bytes b = va_arg(*list, Bytes);
        if (b.err)
            str_builder_append_cstr(sb, "(ERROR_BYTES)");
        else
            str_builder_append_bytes(sb, b.bytes, b.length);
    }
    else if (cstr_equal(spec, "F"))
        _format_file(sb, va_arg(*list, File));

    // Bool
    else if (cstr_equal(spec, "b"))
        va_arg(*list, u32) > 0 ? str_builder_append_cstr(sb, "true") : str_builder_append_cstr(sb, "false");

    // Signed int
    else if (cstr_equal(spec, "i8"))
        str_builder_append(sb, int_to_string((i8)va_arg(*list, i32)));
    else if (cstr_equal(spec, "i16"))
        str_builder_append(sb, int_to_string((i16)va_arg(*list, i32)));
    else if (cstr_equal(spec, "i32"))
        str_builder_append(sb, int_to_string((i32)va_arg(*list, i32)));
    else if (cstr_equal(spec, "i64"))
        str_builder_append(sb, int_to_string((i64)va_arg(*list, i64)));

    // Unsigned int
    else if (cstr_equal(spec, "u8"))
        str_builder_append(sb, uint_to_string((u8)va_arg(*list, u64)));
    else if (cstr_equal(spec, "u16"))
        str_builder_append(sb, uint_to_string((u16)va_arg(*list, u64)));
    else if (cstr_equal(spec, "u32"))
        str_builder_append(sb, uint_to_string((u32)va_arg(*list, u64)));
    else if (cstr_equal(spec, "u64"))
        str_builder_append(sb, uint_to_string((u64)va_arg(*list, u64)));

    // Unknown specifier
    else
    {
        str_builder_append_char(sb, '{');
        str_builder_append_cstr(sb, spec);
        str_builder_append_char(sb, '}');
    }
}

#define _MAX_SPECIFIER 8

String _fmt(const char *format, va_list *args)
{
    assert_not_null(format, "_fmt: format is NULL");

    StringBuilder sb = str_builder_new(get_temporary_allocator());
    String fmt_s = _STRING((char *)format);

    for (u64 i = 0; i < fmt_s.length; i++)
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
    if (format == NULL)
        return str_temp("(NULL)");

    va_list args;
    va_start(args, format);
    String f = _fmt(format, &args);
    va_end(args);

    if (f.err)
        return ERROR_STRING;

    return f;
}

void out(const char *format, ...)
{
    if (format == NULL)
    {
        os_write_out((u8 *)"(NULL)\n", 7);
        return;
    }

    va_list args;
    va_start(args, format);

    String output = _fmt(format, &args);
    va_end(args);

    if (output.err)
        return;

    os_write_out((u8 *)output.s, output.length);
    os_write_out((u8 *)"\n", 1);
}

void out_no_newline(const char *format, ...)
{
    if (format == NULL)
    {
        os_write_out((u8 *)"(NULL)", 6);
        return;
    }

    va_list args;
    va_start(args, format);

    String output = _fmt(format, &args);
    va_end(args);

    if (output.err)
        return;

    os_write_out((u8 *)output.s, output.length);
}

// :path

String get_username(void)
{
    // TODO: windows impl get username
#if defined(OS_LINUX)
    char *uname = getlogin();
    if (uname == NULL)
        return ERROR_STRING;
    return str_temp(uname);
#endif

    return ERROR_STRING;
}

String path_root(void)
{
    if (os_is_linux())
        return str_temp("/");
    else if (os_is_windows())
        return str_temp("C:\\");

    panic("path_root: unknown os type");
    return ERROR_STRING;
}

String path_home(void)
{
    if (os_is_linux())
        return fmt("/home/{S}", get_username());
    else if (os_is_windows())
        return fmt("C:\\Users\\{S}", get_username());

    panic("path_home: unknown os type");
    return ERROR_STRING;
}

String path_to_windows(String path)
{
    return str_replace_char(path, '/', '\\');
}

String path_to_unix(String path)
{
    return str_replace_char(path, '\\', '/');
}

String path_get_filename(String path)
{
    i64 pos = str_find_char_reverse(path, '/');
    if (pos == -1)
        pos = str_find_char_reverse(path, '\\');
    if (pos == -1)
        pos = 0;

    return str_view(path, (u64)pos + 1, path.length);
}

String path_get_extension(String path)
{
    String filename = path_get_filename(path);
    i64 pos = str_find_char_reverse(filename, '.');
    if (pos == -1)
        return ERROR_STRING;

    return str_view(filename, (u64)pos + 1, filename.length);
}

String path_back_dir(String path)
{
    i64 pos = str_find_char_reverse(path, '/');
    if (pos == 1)
        pos = str_find_char_reverse(path, '\\');

    if (pos == -1)
        return ERROR_STRING; // Edge case were path is current dir and
                             // function should return ".." not handled.

    if (pos == 0) // Return root
        pos++;

    return str_view(path, 0, (u64)pos);
}

String path_concat(String path, String other)
{
    if (path.err || other.err)
        return ERROR_STRING;

    StringBuilder sb = str_builder_new(get_temporary_allocator());

    bool end_slash = path.s[path.length - 1] == '/' || path.s[path.length - 1] == '\\';
    bool start_slash = other.s[0] == '/' || other.s[0] == '\\';

    if (end_slash && start_slash)
        path.length--;

    str_builder_append(&sb, path);

    if (!end_slash && !start_slash)
        str_builder_append_char(&sb, PATH_SEP);

    str_builder_append(&sb, other);
    return str_builder_to_string(&sb);
}

// :file

bool file_move(const char *path, const char *dest)
{
    if (path == NULL || dest == NULL)
        return false;

    return rename(path, dest) == 0;
}

bool file_move_s(String path, String dest)
{
    if (path.err || dest.err)
        return false;

    return file_move(path.s, dest.s);
}

bool file_copy(const char *path, const char *dest, Allocator a)
{
    return file_copy_s(str_temp(path), str_temp(dest), a);
}

bool file_copy_s(String path, String dest, Allocator a)
{
    Bytes content = file_read_all_s(path, a);
    if (content.err)
        return false;

    return file_write_all_s(dest, content.bytes, content.length);
}

FileInfo file_get_info(const char *path)
{
    return file_get_info_s(str_temp(path));
}

FileInfo file_get_info_s(String path)
{
    if (path.err)
        return ERROR_FILE_INFO;

    struct stat s;
    if (stat(path.s, &s) != 0)
        return ERROR_FILE_INFO;

    return (FileInfo){
        .size = (u64)s.st_size,
        .size_on_disk = (u64)(s.st_blksize * s.st_blocks),
        .last_modified = (u64)s.st_mtim.tv_sec,
    };
}

int _get_linux_file_permissions(void)
{
    mode_t original_umask = umask(0);
    umask(original_umask);
    return 0666 & ~original_umask;
}

File file_open_s(String path, FilePermission perm, bool create_if_absent, bool truncate)
{
    if (path.err)
        return ERROR_FILE;

    // TODO: windows impl open file
#if defined(OS_LINUX)
    int o_flags = 0;

    if (perm == PERM_READ)
        o_flags |= O_RDONLY;
    else if (perm == PERM_WRITE)
        o_flags |= O_WRONLY;
    else if (perm == PERM_READWRITE)
        o_flags |= O_RDWR;
    else if (perm == PERM_APPEND)
    {
        o_flags |= O_RDWR;
        o_flags |= O_APPEND;
    }

    if (create_if_absent)
        o_flags |= O_CREAT;
    if (truncate)
        o_flags |= O_TRUNC;

    int fd = open(path.s, o_flags, _get_linux_file_permissions());
    if (fd < 0)
        return ERROR_FILE;

    FileInfo info = file_get_info_s(path);
    if (info.err)
        return ERROR_FILE;

    return (File){
        .fd = fd,
        .path = path,
        .open = true,
        .info = info,
        .writeable = perm == PERM_WRITE || perm == PERM_APPEND || perm == PERM_READWRITE,
        .readable = perm == PERM_READWRITE || perm == PERM_READ,
        .err = false,
    };
#endif
}

File file_open(const char *path, FilePermission perm, bool create_if_absent, bool truncate)
{
    return file_open_s(str_temp(path), perm, create_if_absent, truncate);
}

void file_close(File *f)
{
    assert_not_null(f, "file_close: f is null");

    if (f->err)
        return;

    close(f->fd);
    f->open = false;
    f->writeable = false;
    f->readable = false;
}

Bytes file_read(File f, Allocator a, u64 size)
{
    if (f.err)
        return ERROR_BYTES;

    u8 *buffer = alloc(a, size);
    if (buffer == NULL)
        return ERROR_BYTES;

    // TODO: This needs to loop to actually read the full requested size as
    // read may return early.
    ssize_t n = read(f.fd, buffer, (u32)size);
    if (n < 0)
        return ERROR_BYTES;

    return (Bytes){
        .bytes = buffer,
        .length = (u64)n,
        .err = false,
    };
}

Bytes file_read_all_s(String path, Allocator a)
{
    if (path.err)
        return ERROR_BYTES;

    return file_read_all(path.s, a);
}

Bytes file_read_all(const char *path, Allocator a)
{
    File f = file_open(path, PERM_READ, false, false);
    Bytes b = file_read(f, a, f.info.size);
    file_close(&f);
    return b;
}

bool file_write(File f, const u8 *bytes, u64 size)
{
    if (f.err || bytes == NULL)
        return false;

    // TODO: same as read, write until full size is written
    ssize_t n = write(f.fd, bytes, (u32)size);
    return n >= 0;
}

bool file_write_arr(File f, Bytes b)
{
    if (f.err || b.err)
        return false;

    return file_write(f, b.bytes, b.length);
}

bool file_write_str(File f, String s)
{
    if (f.err || s.err)
        return false;

    return file_write(f, (u8 *)s.s, s.length);
}

bool file_write_all_s(String path, const u8 *bytes, u64 length)
{
    if (path.err)
        return false;

    return file_write_all(path.s, bytes, length);
}

bool file_write_all(const char *path, const u8 *bytes, u64 length)
{
    File f = file_open(path, PERM_WRITE, true, true);
    bool success = file_write(f, bytes, length);
    file_close(&f);
    return success;
}

bool file_append_all(const char *path, const u8 *bytes, u64 length)
{
    File f = file_open(path, PERM_APPEND, true, false);
    bool success = file_write(f, bytes, length);
    file_close(&f);
    return success;
}

bool file_append_all_s(String path, const u8 *bytes, u64 length)
{
    if (path.err)
        return false;

    return file_append_all(path.s, bytes, length);
}

// :directory

static mode_t _get_linux_dir_permissions(void)
{
    mode_t original_umask = umask(0);
    umask(original_umask);
    return 0777 & ~original_umask;
}

bool dir_new(const char *name)
{
    return dir_new_s(str_temp(name));
}

bool dir_new_s(String name)
{
    // TODO: windows impl new dir
#if defined(OS_LINUX)
    return mkdir(name.s, _get_linux_dir_permissions()) == 0;
#endif
}

Dir dir_read_s(String path, Allocator a)
{
    if (path.err)
        return ERROR_DIR;

    SparseList list = sparse_list_new(sizeof(DirEntry), 16, a);
    if (list.err)
        return ERROR_DIR;

    // TODO: windows impl read dir
#if defined(OS_LINUX)
    DIR *dir = opendir(path.s);
    if (dir == NULL)
        return ERROR_DIR;

    struct dirent *entry = NULL;
    while ((entry = readdir(dir)) != NULL)
    {
        String name = str_alloc_cstr(a, entry->d_name);
        if (name.err)
            return ERROR_DIR;

        DirEntry e = {
            .name = name,
            .is_dir = entry->d_type == DT_DIR,
            .is_file = entry->d_type == DT_REG,
            .is_symlink = entry->d_type == DT_LNK,
        };

        e.is_current_dir = e.is_dir && cstr_equal(e.name.s, ".");
        e.is_parent_dir = e.is_dir && cstr_equal(e.name.s, "..");

        sparse_list_append(&list, &e);
    }

    closedir(dir);
#endif

    if (list.err)
        return ERROR_DIR;

    return (Dir){
        .a = a,
        .entries = list.mem,
        .num_entries = list.size,
        .err = false,
        .path = path,
    };
}

Dir dir_read(const char *path, Allocator a)
{
    return dir_read_s(str_temp(path), a);
}

void free_dir(Dir *dir)
{
    if (dir == NULL || dir->err)
        return;

    for (u64 i = 0; i < dir->num_entries; i++)
        free_string(dir->entries[i].name, dir->a);

    alloc_free(dir->a, dir->entries);
    dir->err = true;
}

// :sparselist

SparseList sparse_list_new(u64 item_size, u64 length, Allocator a)
{
    void *ptr = alloc(a, item_size * length);
    if (ptr == NULL)
        return ERROR_LIST;

    return (SparseList){
        .a = a,
        .cap = length,
        .mem = ptr,
        .size = 0,
        .item_size = item_size,
        .err = false,
    };
}

void sparse_list_append(SparseList *list, const void *item)
{
    assert_not_null(list, "sparse_list_append: list is null");
    assert_not_null(list, "sparse_list_append: item is null");

    if (list->size == list->cap)
    {
        // Reallocate internal array
        void *ptr = alloc_realloc(list->a, list->mem, list->cap * 2 * list->item_size);
        if (ptr == NULL)
            return;

        list->cap *= 2;
        list->mem = ptr;
    }

    u8 *dest = (u8 *)list->mem + (list->size * list->item_size);
    copy_memory(dest, item, list->item_size);
    list->size++;
}

void *sparse_list_get(SparseList *list, u64 index)
{
    assert_not_null(list, "sparse_list_get: list is null");

    if (list->err || index >= list->size)
        return NULL;

    return (u8 *)list->mem + (index * list->item_size);
}

void sparse_list_remove(SparseList *list, u64 index)
{
    assert_not_null(list, "sparse_list_remove: list is null");

    if (list->err || index >= list->size)
        return;

    // Put last element at index and reduce size
    u8 *dest = sparse_list_get(list, index);
    u8 *src = sparse_list_get(list, list->size - 1);
    copy_memory(dest, src, list->item_size);

    list->size--;
}

// :shell

void _cmd(const char *arg1, ...)
{
    assert_not_null(arg1, "_cmd: arg1 is null");

    va_list list;
    va_start(list, arg1);

    StringBuilder sb = str_builder_new(get_temporary_allocator());
    char *s = NULL;

    str_builder_append_cstr(&sb, (char *)arg1);
    str_builder_append_char(&sb, ' ');

    while ((s = va_arg(list, char *)) != NULL)
    {
        str_builder_append_cstr(&sb, s);
        str_builder_append_char(&sb, ' ');
    }

    va_end(list);
    String args = str_builder_to_string(&sb);

    // TODO: windows impl run shell command
#if defined(OS_LINUX)
    if (fork() == 0)
    {
        // As child, execute command
        execl("/bin/sh", "sh", "-c", args.s, NULL);
        os_exit(0);
    }
#endif

    out("cmd: {S}", args);
}

void cmd_fmt(const char *format, ...)
{
    assert_not_null(format, "cmd_fmt: format is null");

    va_list args;
    va_start(args, format);

    String s = _fmt(format, &args);
    va_end(args);

    if (s.err)
        return;

    _cmd(s.s, NULL);
}

// :build

void run_cmd_for_each_file_in_dir(const char *cmd, const char *path, const char *extension)
{
    if (cmd == NULL || path == NULL)
        return;

    run_cmd_for_each_file_in_dir_s(
        str_temp(cmd),
        str_temp(path),
        str_temp(extension));
}

void run_cmd_for_each_file_in_dir_s(String command, String path, String extension)
{
    if (command.err || path.err)
        return;

    Dir dir = dir_read_s(path, get_temporary_allocator());
    if (dir.err)
        return;

    for (u64 i = 0; i < dir.num_entries; i++)
    {
        DirEntry entry = dir.entries[i];

        if (!extension.err)
        {
            String ext = path_get_extension(entry.name);
            if (!str_equal(ext, extension))
                continue;
        }

        if (!entry.is_file)
            continue;

        // Slightly prevent fuck ups with padded nulls
        cmd_fmt(command.s, entry.name, NULL, NULL, NULL);
    }
}


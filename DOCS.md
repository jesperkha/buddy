# Buddy documentation

## Common

```c
void zero_memory(void *p, u64 size)
```

Zeros out memory. Size is in BYTES.  [source](https://github.com/jesperkha/buddy/blob/main/buddy.h#L177)

```c
void copy_memory(void *dest, void *source, u64 size)
```

Copies memory from source to dest. Size is in BYTES.  [source](https://github.com/jesperkha/buddy/blob/main/buddy.h#L185)

## Allocator

```c
void *alloc(Allocator a, u64 size)
```

Allocates memory. Returns NULL if the allocation fails.  [source](https://github.com/jesperkha/buddy/blob/main/buddy.h#L197)

```c
void *alloc_zero(Allocator a, u64 size)
```

Same as `alloc`, but zeroes out memory as wello  [source](https://github.com/jesperkha/buddy/blob/main/buddy.h#L203)

```c
void *alloc_realloc(Allocator a, void *p, u64 new_size)
```

Reallocates memory with new size. Returns NULL if the allocation fails.  [source](https://github.com/jesperkha/buddy/blob/main/buddy.h#L209)

```c
Allocator get_temporary_allocator()
```

The temporary allocator uses predefined global memory with size TEMP_ALLOC_BUFSIZE, which is by default 4MB.  [source](https://github.com/jesperkha/buddy/blob/main/buddy.h#L290)

```c
void reset_temp_memory()
```

Resets the temporary memory back to size 0.  [source](https://github.com/jesperkha/buddy/blob/main/buddy.h#L230)

```c
void *temp_alloc(u64 size)
```

Allocates memory using the temporary allocator. Returns NULL if the allocation fails.  [source](https://github.com/jesperkha/buddy/blob/main/buddy.h#L297)

```c
void *temp_zero_alloc(u64 size)
```

Same as `temp_alloc`, but zeroes out memory as well.  [source](https://github.com/jesperkha/buddy/blob/main/buddy.h#L303)

```c
void *temp_realloc(void *p, u64 size)
```

Reallocates to new size. Does not free the original memory.  [source](https://github.com/jesperkha/buddy/blob/main/buddy.h#L309)

## Arena

```c
Arena *arena_new(Allocator a, u64 size)
```

Allocates an arena with the given max size. Returns NULL on allocation fail.  [source](https://github.com/jesperkha/buddy/blob/main/buddy.h#L317)

```c
void *arena_alloc(Arena *a, u64 size)
```

Allocates a region in the arena. Returns NULL if arena is full.  [source](https://github.com/jesperkha/buddy/blob/main/buddy.h#L329)

```c
void *arena_zero_alloc(Arena *a, u64 size)
```

Same as `arena_alloc`, but zeroes out memory as well.  [source](https://github.com/jesperkha/buddy/blob/main/buddy.h#L340)

```c
Allocator get_arena_allocator(Arena *a)
```

Get an allocator using the given arena.  [source](https://github.com/jesperkha/buddy/blob/main/buddy.h#L381)

## String

```c
uint cstr_len(char *s)
```

Returns the byte length of a NULL-terminated C string.  [source](https://github.com/jesperkha/buddy/blob/main/buddy.h#L391)

```c
String str_temp(char *s)
```

Allocates a new string using the temporary allocator. See `str_alloc` to use a custom allocator. Returns ERROR_STRING if allocation fails or s is NULL.  [source](https://github.com/jesperkha/buddy/blob/main/buddy.h#L415)

```c
String str_alloc(Allocator a, String s)
```

Allocates and returns a copy of the string using allocator. Returns ERROR_STRING if allocation fails or s har an error.  [source](https://github.com/jesperkha/buddy/blob/main/buddy.h#L420)

```c
String str_alloc_cstr(Allocator a, char *s)
```

Allocates and returns a copy of the string using allocator. Returns ERROR_STRING if allocation fails.  [source](https://github.com/jesperkha/buddy/blob/main/buddy.h#L437)

```c
String str_view(String s, uint start, uint end)
```

Returns a string view of the original string. Returns ERROR_STRING if the range is out of bounds or the original string has an error.  [source](https://github.com/jesperkha/buddy/blob/main/buddy.h#L404)

```c
bool str_equal(String a, String b)
```

Returns true if both strings are equal. Returns false if not, or if one has an error.  [source](https://github.com/jesperkha/buddy/blob/main/buddy.h#L490)

```c
uint str_count(String s, char c)
```

Returns count of character c in string s.  [source](https://github.com/jesperkha/buddy/blob/main/buddy.h#L455)

```c
String str_upper(String s)
```

Converts the original string s to uppercase, returns the same string for convenience.  [source](https://github.com/jesperkha/buddy/blob/main/buddy.h#L470)

```c
String str_lower(String s)
```

Converts the original string s to lowercase, returns the same string for convenience.  [source](https://github.com/jesperkha/buddy/blob/main/buddy.h#L480)

```c
String str_replace_char(String s, char old, char new_c)
```

Replaces all occurances of old char with new in the original string. Returns the same string for convenience.  [source](https://github.com/jesperkha/buddy/blob/main/buddy.h#L500)

```c
String str_replace_str(Allocator a, String s, String old, String new_s)
```

Allocates and returns a new copy of s with the old substrings replaced with new. Returns ERROR_STRING if either string has an error or allocation fails.  [source](https://github.com/jesperkha/buddy/blob/main/buddy.h#L510)

```c
String str_reverse(String s)
```

Reverses the original string. Returns same string for convenience. Returns ERROR_STRING if s has an error.  [source](https://github.com/jesperkha/buddy/blob/main/buddy.h#L516)

## StringBuilder

```c
StringBuilder str_builder_new(Allocator a, u64 size)
```

Returns a new allocated string builder with the given max size. Returns ERROR_STRING_BUILDER if allocation fails.  [source](https://github.com/jesperkha/buddy/blob/main/buddy.h#L535)

```c
bool str_builder_append(StringBuilder *sb, String s)
```

Appends string to the builder. Returns true on success. Returns false if s has an error or internal reallocation fails.  [source](https://github.com/jesperkha/buddy/blob/main/buddy.h#L550)

```c
bool str_builder_append_cstr(StringBuilder *sb, char *s)
```

Same as `str_builder_append`.  [source](https://github.com/jesperkha/buddy/blob/main/buddy.h#L577)

```c
String str_builder_to_string(StringBuilder *sb)
```

Returns the string builder as a string.  [source](https://github.com/jesperkha/buddy/blob/main/buddy.h#L583)

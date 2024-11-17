# Buddy Cheatsheet


```c
// Common
void zero_memory(void *p, u64 size);             // Zeros out memory. Size is in BYTES. 
void copy_memory(void *dest, void *source, u64 size); // Copies memory from source to dest. Size is in BYTES. 

// Memory
// --------------------------------------

// Allocators
typedef struct Allocator                         // The allocator is a generic structure with an internal allocation process. Some functions take an allocator as an argument to give the user mote control over how memory gets allocated and used in the program. Most allocators have a `get_x_allocator` function. You can read more about how they work and what they are used for below. 
void *alloc(Allocator a, u64 size);              // Allocates memory. Returns NULL if the allocation fails. 
void *alloc_zero(Allocator a, u64 size);         // Same as `alloc`, but zeroes out memory as well. 
void *alloc_realloc(Allocator a, void *p, u64 new_size); // Reallocates memory with new size. Returns NULL if the allocation fails. 

// Temporary Allocator
Allocator get_temporary_allocator();             // The temporary allocator uses predefined global memory with size TEMP_ALLOC_BUFSIZE, which is by default 4MB. This is used for short term memory with a scoped lifetime. You can reset the allocator to 0 with `reset_temp_memory()`. The temp allocator cannot free memory, but has `mark()` and `restore()` to push and pop chunks off the allocator stack. 
void reset_temp_memory();                        // Resets the temporary memory back to size 0. 
void *temp_alloc(u64 size);                      // Allocates memory using the temporary allocator. Returns NULL if the allocation fails. 
void *temp_zero_alloc(u64 size);                 // Same as `temp_alloc`, but zeroes out memory as well. 
void *temp_realloc(void *p, u64 size);           // Reallocates to new size. Does not free the original memory. 
u64 temp_mark();                                 // Create a mark in the temporary allocator that can be returned to later. This should be used for allocations that dont leave the current scope. Returns the mark ID, restore with `temo_restore_mark()`. 
void temp_restore_mark(u64 id);                  // Restores to mark ID. Any memory allocated after the mark will be discarded. 

// Arena
typedef struct Arena                             // Arenas a chunks of memory you can allocate to and free all at once. They make it easy to handle memory across multiple function calls as you can free everything allocated to the arena all at once. 
Arena *arena_new(Allocator a, u64 size);         // Allocates an arena with the given max size. Returns NULL on allocation fail. 
void *arena_alloc(Arena *a, u64 size);           // Allocates a region in the arena. Returns NULL if arena is full. 
void *arena_zero_alloc(Arena *a, u64 size);      // Same as `arena_alloc`, but zeroes out memory as well. 
Allocator get_arena_allocator(Arena *a);         // Get an allocator using the given arena. 

// Strings
// --------------------------------------

// String
typedef struct String                            // Strings are simply a pointer with a length and an error value. There is no NULL terminator. When using string functions, they may set err=true if something went wrong. This allows future string methods to return gracefully on error, and you just have to check the final string result for an error. 
uint cstr_len(char *s);                          // Returns the byte length of a NULL-terminated C string. 
String str_temp(char *s);                        // Allocates a new string using the temporary allocator. See `str_alloc` to use a custom allocator. Returns ERROR_STRING if allocation fails or s is NULL. 
String str_alloc(Allocator a, String s);         // Allocates and returns a copy of the string using allocator. Returns ERROR_STRING if allocation fails or s har an error. 
String str_alloc_cstr(Allocator a, char *s);     // Allocates and returns a copy of the string using allocator. Returns ERROR_STRING if allocation fails. 
String str_view(String s, uint start, uint end); // Returns a string view of the original string. Returns ERROR_STRING if the range is out of bounds or the original string has an error. 
bool str_equal(String a, String b);              // Returns true if both strings are equal. Returns false if not, or if one has an error. 
uint str_count(String s, char c);                // Returns count of character c in string s. 
String str_upper(String s);                      // Converts the original string s to uppercase, returns the same string for convenience. 
String str_lower(String s);                      // Converts the original string s to lowercase, returns the same string for convenience. 
String str_replace_char(String s, char old, char new_c); // Replaces all occurances of old char with new in the original string. Returns the same string for convenience. 
String str_replace_str(Allocator a, String s, String old, String new_s); // Allocates and returns a new copy of s with the old substrings replaced with new. Returns ERROR_STRING if either string has an error or allocation fails. 
String str_reverse(String s);                    // Reverses the original string. Returns same string for convenience. Returns ERROR_STRING if s has an error. 

// StringBuilder
typedef struct StringBuilder                     // StringBuilder creates a string from multiple others. It automatically reallocates the internal memory with the allocator given on init. 
StringBuilder str_builder_new(Allocator a, u64 size); // Returns a new allocated string builder with the given max size. Returns ERROR_STRING_BUILDER if allocation fails. 
bool str_builder_append(StringBuilder *sb, String s); // Appends string to the builder. Returns true on success. Returns false if s has an error or internal reallocation fails. 
bool str_builder_append_cstr(StringBuilder *sb, char *s); // Same as `str_builder_append`. 
String str_builder_to_string(StringBuilder *sb); // Returns the string builder as a string. 
```
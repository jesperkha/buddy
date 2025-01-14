#pragma once

#if defined(__linux__)

// Linux includes
#include <unistd.h>
#include <dirent.h>
#include <stdarg.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <malloc.h> // Temporary for heap alloc

// Linux defines
#define OS_NAME "linux"
#define os_is_linux() 1
#define os_is_windows() 0
#define OS_LINUX
#define HANDLE void*
#define PATH_SEP '/'

#elif defined(_WIN32) || defined(_WIN64)

// Windows includes
#include <windows.h>
#include <winbase.h>
#include <lmcons.h>

// Windows defines
#define OS_WINDOWS
#define OS_NAME "windows"
#define os_is_linux() 0
#define os_is_windows() 1
#define PATH_SEP '\\'

#else
#error "OS is unknown and therefore not supported by buddy"
#endif

// Universal includes
#include <stdbool.h>

#define KB(n) (n * 1024ull)
#define MB(n) (KB(n) * 1024ull)
#define GB(n) (MB(n) * 1024ull)

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

// Common

// Zeroes out memory. Size is in BYTES.
void zero_memory(void *p, u64 size);
// Copies memory from source to dest. Size is in BYTES.
void copy_memory(void *dest, const void *source, u64 size);

// Allocators

typedef enum AllocatorMessage
{
    ALLOCATOR_MSG_FREE,
    ALLOCATOR_MSG_ALLOC,
    ALLOCATOR_MSG_ZERO_ALLOC,
    ALLOCATOR_MSG_REALLOC,
} AllocatorMessage;

// The allocator is a generic structure with an internal allocation process.
// Some functions take an allocator as an argument to give the user more control
// over how memory gets allocated and used in the program. Most allocators have
// a `get_x_allocator` function.
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

// Heap allocator

// Currently a wrapper for malloc. Will soon change to a custom heap allocator.
Allocator get_heap_allocator(void);
// Allocates memory with heap allocator. Returns NULL if allocation fails.
void *heap_alloc(u64 size);
// Same as `heap_alloc`, but zeroes memory as well.
void *heap_zero_alloc(u64 size);
// Reallocates pointer to new size with heap allocator. Returns NULL on failure.
void *heap_realloc(void *old_ptr, u64 size);
// Frees heap allocated pointer.
void heap_free(void *ptr);

// Temporary allocator

#define TEMP_ALLOC_BUFSIZE MB(8)

// The temporary allocator uses predefined global memory with size
// TEMP_ALLOC_BUFSIZE, which is by default 4MB. This is used for short term
// memory with a scoped lifetime. You can reset the allocator to 0 with
// `reset_temp_memory()`. The temp allocator cannot free memory, but has
// `mark()` and `restore()` to push and pop chunks off the allocator stack.
Allocator get_temporary_allocator(void);
// Resets the temporary memory back to size 0.
void reset_temp_memory(void);
// Allocates memory using the temporary allocator. Returns NULL if the
// allocation fails.
void *temp_alloc(u64 size);
// Same as `temp_alloc`, but zeroes out memory as well.
void *temp_zero_alloc(u64 size);
// Reallocates to new size. Does not free the original memory.
void *temp_realloc(void *p, u64 size);
// Create a mark in the temporary allocator that can be returned to later.
// This should be used for allocations that don't leave the current scope.
// Returns the mark ID, restore with `temo_restore_mark()`.
u64 temp_mark(void);
// Restores to mark ID. Any memory allocated after the mark will be discarded.
void temp_restore_mark(u64 id);

// Arena

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
// Free arena allocated with given allocator.
void free_arena(Arena *arena, Allocator a);
// Allocates a region in the arena. Returns NULL if arena is full.
void *arena_alloc(Arena *a, u64 size);
// Same as `arena_alloc`, but zeroes out memory as well.
void *arena_zero_alloc(Arena *a, u64 size);
// Get an allocator using the given arena.
Allocator get_arena_allocator(Arena *a);

// String

// Strings are simply a pointer with a length and an error value. When using
// string functions, they may set err=true if something went wrong. This allows
// future string methods to return gracefully on error, and you just have to
// check the final string result for an error.
typedef struct String
{
    char *s;
    u64 length;
    bool err;
} String;

#define ERROR_STRING ((String){.s = NULL, .length = 0, .err = true})

// Array of bytes with length.
typedef struct Bytes
{
    u8 *bytes;
    u64 length;
    bool err;
} Bytes;

#define ERROR_BYTES ((Bytes){.err = true, .length = 0, .bytes = NULL})

// Free string allocated with given allocator.
void free_string(String s, Allocator a);
// Free byte array allocated with given allocator.
void free_bytes(Bytes b, Allocator a);

// Convert byte array to string. Keeps original pointer.
String bytes_to_str(Bytes bytes);
// Convert string to byte array. Keeps original pointer.
Bytes str_to_bytes(String s);

// Convert signed integer to string.
String int_to_string(i64 n);
// Convert unsigned integer to string.
String uint_to_string(u64 n);

// Returns the byte length of a NULL-terminated C string.
uint cstr_len(const char *s);
// Allocates a new string using the temporary allocator. See `str_alloc` to use
// a custom allocator. Returns ERROR_STRING if allocation fails or s is NULL.
String str_temp(const char *s);
// Allocates and returns a copy of the string using allocator. Returns
// ERROR_STRING if allocation fails or s har an error.
String str_alloc(Allocator a, String s);
// Allocates and returns a copy of the string using allocator. Returns
// ERROR_STRING if allocation fails.
String str_alloc_cstr(Allocator a, const char *s);
// Returns a copy of the string allocated using temporary allocator.
String str_copy(String s);
// Returns a copy of the string allocated with a.
String str_copy_alloc(Allocator a, String s);
// Returns a string view of the original string. The range is including start
// and excluding end (string slice). Returns ERROR_STRING if the range is out
// of bounds or the original string has an error.
String str_view(String s, u64 start, u64 end);
// Returns true if both strings are equal. Returns false if not, or if one has
// an error.
bool str_equal(String a, String b);
// Returns true if both strings are equal.
bool cstr_equal(const char *a, const char *b);
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
// Returns index of first occurance of c. Returns -1 if not found.
i64 str_find_char(String s, char c);
// Returns index of first occurance of c, searching backwards. Returns -1 if not
// found.
i64 str_find_char_reverse(String s, char c);
// Concatinates s1 and s2, allocating with a. Returns ERROR_STRING if either has an
// error or allocation fails.
String str_concat(Allocator a, String s1, String s2);

// String builder

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

#define ERROR_STRING_BUILDER ((StringBuilder){ \
    .size = 0,                                 \
    .length = 0,                               \
    .mem = NULL,                               \
    .err = true,                               \
})

// Returns a new allocated string builder. Returns ERROR_STRING_BUILDER if
// allocation fails.
StringBuilder str_builder_new(Allocator a);
// Free string builder with the internal allocator.
void free_string_builder(StringBuilder sb);
// Appends string to the builder. Returns true on success. Returns false if s
// has an error or internal reallocation fails.
bool str_builder_append(StringBuilder *sb, String s);
// Appends null-termiated string to string builder.
bool str_builder_append_cstr(StringBuilder *sb, const char *s);
// Appends bytes to string builder.
bool str_builder_append_bytes(StringBuilder *sb, const u8 *bytes, u64 length);
// Appends character to string builder.
bool str_builder_append_char(StringBuilder *sb, char c);
// Returns the string builder as a string.
String str_builder_to_string(StringBuilder *sb);

// Format and print

// Create formatted string. Valid specifiers are:
//
//   Use: "{specifier}"
//
//   s: null terminated string, "(NULL)" if null pointer
//   S: String, if error: "ERROR_STRING"
//   b: boolean, either "true" or "false"
//   B: Bytes as a string, if error: "ERROR_BYTES"
//
//   i8, i16, i32, i64
//   u8, u16, u32, u64
//
// Returns "(NULL)" if format is null.
String fmt(const char *format, ...);
// Print formatted string to standard out. Appends newline.
void out(const char *text, ...);
// Same as out but without the appended newline.
void out_no_newline(const char *format, ...);

// IO

// Returns username of currently logged in user. Returns ERROR_STRING
// if username couldnt be retreived.
String get_username(void);

// Exit program with status. Flushes standard input and output.
void os_exit(u8 status);
// Write bytes to standard output.
void os_write_out(const u8 *bytes, u64 length);
// Write bytes to standard error output.
void os_write_err(const u8 *bytes, u64 length);
// Returns bytes read from standard input, with the bytes pointer pointing to
// the given buffer. Returns ERROR_BYTES on failed read.
Bytes os_read_input(u8 *buffer, u64 max_length);
// Returns bytes read from standard input, allocating a byte array using the
// given allocator. Returns ERROR_BYTES on allocation fail or failed read.
Bytes os_read_all_input(Allocator a);

// Path

// Get the path to root on the system.
String path_root(void);
// Get the path to the home dir of the current user.
String path_home(void);
// Get the file name from the path. Returns ERROR_STRING if path is malformed.
String path_get_filename(String path);
// Get the file extension from the path.
String path_get_extension(String path);
// Go back one directory. Returns *substring* from original path or ERROR_STRING.
String path_back_dir(String path);
// Append other to path. Uses temporary allocator. Returns ERROR_STRING if
// either is a malformed path.
String path_concat(String path, String other);
// Replaces forward slash with backslash in the original string. Returns same
// string for convenience.
String path_to_windows(String path);
// Replaces backslash with forward slash in the original string. Returns same
// string for convenience.
String path_to_unix(String path);

// Files

typedef struct FileInfo
{
    u64 size;
    u64 size_on_disk; // Aligned to disk page size
    u64 last_modified;
    bool err;
} FileInfo;

#define ERROR_FILE_INFO ((FileInfo){.err = true})

typedef enum FilePermission
{
    PERM_WRITE,
    PERM_READ,
    PERM_READWRITE,
    PERM_APPEND,
} FilePermission;

typedef struct File
{
    String path;
    FileInfo info;

    i32 fd;
    HANDLE hfile;

    bool open;
    bool writeable;
    bool readable;

    bool err;
} File;

#define ERROR_FILE ((File){.err = true, .fd = -1, .hfile = NULL, .open = false})

// Open a file with the given permissions. Returns ERROR_FILE on error.
File file_open(const char *path, FilePermission perm, bool create_if_absent, bool truncate);
// Open a file with the given permissions. Returns ERROR_FILE on error.
File file_open_s(String path, FilePermission perm, bool create_if_absent, bool truncate);
// Close file. Sets booleans in file object.
void file_close(File *f);
// Read size bytes from file. Returns byte array with the contents. The length
// may be different to size. Returns ERROR_BYTES on error.
Bytes file_read(File f, Allocator a, u64 size);
// Opens and reads file contents before closing. Returns ERROR_BYTES on
// error.
Bytes file_read_all(const char *path, Allocator a);
// Opens and reads file contents before closing. Returns ERROR_BYTES on
// error.
Bytes file_read_all_s(String path, Allocator a);
// Write bytes to file. Returns true on success.
bool file_write(File f, const u8 *bytes, u64 size);
// Write bytes to file. Returns true on success.
bool file_write_arr(File f, Bytes bytes);
// Write string to file. Returns true on success.
bool file_write_str(File f, String s);
// Opens file and writes bytes before closing. Truncates file and creates new
// if it doesnt already exist. Returns true on success.
bool file_write_all(const char *path, const u8 *bytes, u64 length);
// Opens file and writes bytes before closing. Truncates file and creates new
// if it doesnt already exist. Returns true on success.
bool file_write_all_s(String path, const u8 *bytes, u64 length);
// Opens file and appends bytes to end before closing. Creates new file if it
// doesnt already exist. Returns true on success.
bool file_append_all(const char *path, const u8 *bytes, u64 length);
// Opens file and appends bytes to end before closing. Creates new file if it
// doesnt already exist. Returns true on success.
bool file_append_all_s(String path, const u8 *bytes, u64 length);
// Get file info without opening file.
FileInfo file_get_info(const char *path);
// Get file info without opening file.
FileInfo file_get_info_s(String path);
// Copy file at path to dest. Allocator used to read file contents.
bool file_copy(const char *path, const char *dest, Allocator a);
// Copy file at path to dest. Allocator used to read file contents.
bool file_copy_s(String path, String dest, Allocator a);
// Move file at path to dest.
bool file_move(const char *path, const char *dest);
// Move file at path to dest.
bool file_move_s(String path, String dest);

// Directories

typedef struct DirEntry
{
    String name;

    bool is_file;
    bool is_dir;
    bool is_symlink;

    bool is_current_dir; // Is "."
    bool is_parent_dir;  // Is ".."
} DirEntry;

typedef struct Dir
{
    Allocator a;
    String path;
    u64 num_entries;
    DirEntry *entries;
    bool err;
} Dir;

#define ERROR_DIR ((Dir){.err = true, .path = ERROR_STRING, .num_entries = 0})

// Create new directory with default permissions. Returns false on failure.
bool dir_new(const char *name);
// Create new directory with default permissions. Returns false on failure.
bool dir_new_s(String name);
// Read directory contents. Returns list of entries allocated with given
// allocator. Returns ERROR_DIR on error. Free with free_dir().
Dir dir_read(const char *path, Allocator a);
// Same as dir_read().
Dir dir_read_s(String path, Allocator a);
// Free directory object with internal allocator.
void free_dir(Dir *dir);

// Lists

typedef struct List
{
    Allocator a;
    void *mem;
    u64 size;
    u64 cap;
    u64 item_size;
    bool err;
} List;

typedef List SparseList; // Unordered list with fast remove

// Universal for all list variants
#define ERROR_LIST ((List){.err = true, .mem = NULL, .size = 0, .cap = 0})

// Create new sparse list with allocator and intial size. Item size is in bytes.
// Returns ERROR_LIST if allocation fails.
SparseList sparse_list_new(u64 item_size, u64 length, Allocator a);
// Appends item to end of list. Panics if list or item is null.
void sparse_list_append(SparseList *list, const void *item);
// Get a pointer to the item at index. Returns NULL if index is out of bounds.
// Panics is list is null.
void *sparse_list_get(SparseList *list, u64 index);
// Removes item at index by replacing it with the last item. Fails if index is
// out of bounds. Panics is list is null.
void sparse_list_remove(SparseList *list, u64 index);

// Shell and commands

// Execute shell command.
#define cmd(...) _cmd(__VA_ARGS__, NULL);
// Run shell command with formatted string.
void cmd_fmt(const char *format, ...);

// Use cmd(...) macro instead. Must be NULL terminated list of args.
void _cmd(const char *arg1, ...);

// Build tools

// Runs given command for each file in the path directory. Extension is the
// file type to target, provide NULL to target all files. Command may contain
// *one* {S} specifier which will be replaced with the filename for each file.
void run_cmd_for_each_file_in_dir(const char *command, const char *path, const char *extension);
// Same as run_cmd_for_each_file_in_dir, but replace extension with ERROR_STRING
// to omit the file type filter.
void run_cmd_for_each_file_in_dir_s(String command, String path, String extension);


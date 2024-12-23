#include "buddy.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h> // NULL

// Military grade testing suite

#define log(m) out("TEST: -- " m)
#define title(m) out("TEST: " m)
#define fail(m)                        \
    {                                  \
        title("   >>> FAIL: " m); \
        return false;                  \
    }
#define assert(v, m) \
    if (!(v))        \
        fail(m);
#define run(name) (test_##name)();
#define test(name, body) \
    bool test_##name(void)   \
    {                    \
        title(#name);    \
        body;            \
        title("OK\n");   \
        return true;     \
    }

// ----------------------------------------------------------------------
// -------------------------  TESTING  ----------------------------------
// ----------------------------------------------------------------------

test(Allocation, {
    {
        log("temporary allocator");
        void *a = temp_alloc(100);
        assert(a != NULL, "Expected valid pointer");
        void *b = temp_alloc(GB(1));
        assert(b == NULL, "Expected NULL pointer");
    }
    {
        reset_temp_memory();

        log("temp zero alloc");
        u64 size = TEMP_ALLOC_BUFSIZE - 128; // Size of header
        u8 *p = temp_zero_alloc(size);
        assert(p != NULL, "Expected valid poiner");
        for (u64 i = 0; i < size; i++)
            assert(p[i] == 0, "Expected zeroed memory");

        reset_temp_memory();
    }
    {
        // disable -fsanitize=address
        /*
        log("arena allocator");
        Arena *arena = arena_new(get_temporary_allocator(), 128);
        assert(arena != NULL, "Expected valid arena pointer");
        String s = str_alloc_cstr(get_arena_allocator(arena), "Hello world!");
        String expect = str_temp("Hello world!");
        assert(str_equal(s, expect), "Expected equal");
        String s2 = str_alloc_cstr(get_arena_allocator(arena), "This is a longer string.");
        assert(!s2.err, "Expected valid string");
        void *p = arena_zero_alloc(arena, 100);
        assert(p == NULL, "Expected NULL");
        */
    }
    {
        log("temp memory marking");
        reset_temp_memory();

        u64 mark = temp_mark();
        assert(mark == 0, "Expected 0");
        void *p = temp_alloc(KB(1));
        assert(p != NULL, "Expected valid pointer");
        temp_restore_mark(mark);
        mark = temp_mark();
        void *pp = temp_alloc(KB(1));
        assert(pp != NULL, "Expected valid pointer");
        assert(mark == 0, "Expected 0 after restore");
    }
})

test(String, {
    {
        log("upper and lower case");
        String s = str_temp("Hello World!");
        assert(str_equal(str_upper(s), str_temp("HELLO WORLD!")), "Expected equal");
        assert(str_equal(str_lower(s), str_temp("hello world!")), "Expected equal");
    }
    {
        log("c string to String");
        String s = str_temp("Hello world!");
        assert(s.length == 12, "Expected length 12");
    }
    {
        log("string equal");
        assert(str_equal(str_temp("Hello"), str_temp("Hello")), "Expected equal");
        assert(!str_equal(str_temp("hello"), str_temp("Hello")), "Expected not equal");
        assert(!str_equal(str_temp("he"), str_temp("hello")), "Expected not equal");
    }
    {
        log("string count");
        assert(str_count(str_temp("Hello world!"), 'l') == 3, "Expected 3");
    }
    {
        log("string replace char");
        String replaced = str_replace_char(str_temp("Hello world!"), 'l', '-');
        String expect = str_temp("He--o wor-d!");
        assert(str_equal(replaced, expect), "Expected equal");
    }
    {
        log("string replace string: NOT IMPLEMENTED");
        // String expect = str_temp("Hello there my friends!");
        // String replaced = str_replace_str(
        //     get_temporary_allocator(),
        //     str_temp("Hello world!"),
        //     str_temp("world"),
        //     str_temp("there my friends"));

        // assert(!replaced.err, "Expected non-error");
        // assert(str_equal(replaced, expect), "Expected equal");
    }
    {
        log("string reverse");
        String reversed = str_reverse(str_temp("Hello world!"));
        String expect = str_temp("!dlrow olleH");
        assert(str_equal(reversed, expect), "Expected equal");
    }
})

test(StringBuilder, {
    {
        log("string builder append");
        StringBuilder sb = str_builder_new(get_temporary_allocator());
        assert(!sb.err, "Expected init success");
        assert(str_builder_append_cstr(&sb, "Hello "), "Expected success");
        assert(str_builder_append_cstr(&sb, "world!"), "Expected success");

        String expect = str_temp("Hello world!");
        assert(str_equal(expect, str_builder_to_string(&sb)), "Expected equal");
    }
    {
        log("string builder realloc");
        StringBuilder sb = str_builder_new(get_temporary_allocator());
        assert(!sb.err, "Expected init success");
        assert(str_builder_append_cstr(&sb, "Hello world! "), "Expected success");
        assert(str_builder_append_cstr(&sb, "I have twelve dogs."), "Expected success");

        String expect = str_temp("Hello world! I have twelve dogs.");
        assert(str_equal(expect, str_builder_to_string(&sb)), "Expected equal");
    }
})

test(Fmt, {
    {
        log("number formatting");
        String expect = str_temp("13 -8 255");
        String result = fmt("{u64} {i8} {u8}", 13, -8, 255);
        assert(str_equal(expect, result), "Expected equal");
    }
    {
        log("c-string");
        String expect = str_temp("Hello, world!");
        String result = fmt("{s}, {s}!", "Hello", "world");
        assert(str_equal(expect, result), "Expected equal");
    }
    {
        log("String type");
        String expect = str_temp("Password: 456");
        String pw_full = str_temp("123456789");
        String pw = str_view(pw_full, 3, 6);
        String result = fmt("Password: {S}", pw);
        assert(str_equal(expect, result), "Expected equal");
    }
})

test(Path, {
    {
        log("join path");
        String expect = str_temp("/home/Bob/Documents/divorce.pptx");
        String result = path_concat(str_temp("/home/Bob/"), str_temp("/Documents/divorce.pptx"));
        assert(str_equal(expect, result), "Expected equal");
    }
    {
        log("filename");
        String expect = str_temp("foo.txt");
        String result = path_get_filename(str_temp("/home/user/foo.txt"));
        assert(str_equal(expect, result), "Expected equal");
    }
    {
        log("extension");
        String expect = str_temp("txt");
        String result = path_get_extension(str_temp("/home/user/foo.txt"));
        assert(str_equal(expect, result), "Expected equal");
    }
    {
        log("extension edgecase");
        String expect = str_temp("gitignore");
        String result = path_get_extension(str_temp("/home/user/.gitignore"));
        assert(str_equal(expect, result), "Expected equal");
    }
})

int main(void)
{
    /*
    run(String);
    run(Allocation);
    run(StringBuilder);
    run(Fmt);
    run(Path);
    */

    file_append_all("foo.txt", (u8*)"Hello\n", 6);

    ByteArray bytes = file_read_all("foo.txt", get_temporary_allocator());
    out("{B}", bytes);

    return 0;
}


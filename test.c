#include "buddy.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h> // NULL

// TODO: file and dir IO tests

// Military grade testing suite

#define test(name) for (bool k = true; k; k = false) \
                   for (char *msg = name; k; k = false, out("OK {s}", msg))

#define assert(v, m)         \
    if (!(v))                \
    {                        \
        out(">>> FAIL: {s}: " m, msg); \
        return false;        \
    }                        \

// ----------------------------------------------------------------------
// -------------------------  TESTING  ----------------------------------
// ----------------------------------------------------------------------

bool test_allocation(void)
{
    test("temporary allocator")
    {
        void *a = temp_alloc(MB(2));
        assert(a != NULL, "Expected valid pointer");
    }

    test("temp zero alloc")
    {
        reset_temp_memory();
        u64 size = KB(1);
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

    test("temp memory marking")
    {
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

    return true;
}

bool test_string(void)
{
    test("upper and lower case")
    {
        String s = str_temp("Hello World!");
        assert(str_equal(str_upper(s), str_temp("HELLO WORLD!")), "Expected equal");
        assert(str_equal(str_lower(s), str_temp("hello world!")), "Expected equal");
    }

    test("c string to String")
    {
        String s = str_temp("Hello world!");
        assert(s.length == 12, "Expected length 12");
    }

    test("string equal")
    {
        assert(str_equal(str_temp("Hello"), str_temp("Hello")), "Expected equal");
        assert(!str_equal(str_temp("hello"), str_temp("Hello")), "Expected not equal");
        assert(!str_equal(str_temp("he"), str_temp("hello")), "Expected not equal");
    }

    test("string count")
    {
        assert(str_count(str_temp("Hello world!"), 'l') == 3, "Expected 3");
    }

    test("string replace char")
    {
        String replaced = str_replace_char(str_temp("Hello world!"), 'l', '-');
        String expect = str_temp("He--o wor-d!");
        assert(str_equal(replaced, expect), "Expected equal");
    }

    test("string replace string: NOT IMPLEMENTED")
    {
        // String expect = str_temp("Hello there my friends!");
        // String replaced = str_replace_str(
        //     get_temporary_allocator(),
        //     str_temp("Hello world!"),
        //     str_temp("world"),
        //     str_temp("there my friends"));

        // assert(!replaced.err, "Expected non-error");
        // assert(str_equal(replaced, expect), "Expected equal");
    }

    test("string reverse")
    {
        String reversed = str_reverse(str_temp("Hello world!"));
        String expect = str_temp("!dlrow olleH");
        assert(str_equal(reversed, expect), "Expected equal");
    }

    return true;
}

bool test_string_builder(void)
{
    test("string builder append")
    {
        StringBuilder sb = str_builder_new(get_temporary_allocator());
        assert(!sb.err, "Expected init success");
        assert(str_builder_append_cstr(&sb, "Hello "), "Expected success");
        assert(str_builder_append_cstr(&sb, "world!"), "Expected success");

        String expect = str_temp("Hello world!");
        String result = str_builder_to_string(&sb);
        assert(str_equal(expect, result), "Expected equal");
    }

    test("string builder realloc")
    {
        StringBuilder sb = str_builder_new(get_temporary_allocator());
        assert(!sb.err, "Expected init success");
        assert(str_builder_append_cstr(&sb, "Hello world! "), "Expected success");
        assert(str_builder_append_cstr(&sb, "I have twelve dogs."), "Expected success");

        String expect = str_temp("Hello world! I have twelve dogs.");
        assert(str_equal(expect, str_builder_to_string(&sb)), "Expected equal");
    }

    return true;
}

bool test_fmt(void)
{
    test("number formatting")
    {
        String expect = str_temp("13 -8 255");
        String result = fmt("{u64} {i8} {u8}", 13, -8, 255);
        assert(str_equal(expect, result), "Expected equal");
    }

    test("c-string")
    {
        String expect = str_temp("Hello, world!");
        String result = fmt("{s}, {s}!", "Hello", "world");
        assert(str_equal(expect, result), "Expected equal");
    }

    test("String type")
    {
        String expect = str_temp("Password: 456");
        String pw_full = str_temp("123456789");
        String pw = str_view(pw_full, 3, 6);
        String result = fmt("Password: {S}", pw);
        assert(str_equal(expect, result), "Expected equal");
    }

    return true;
}

bool test_path(void)
{
    test("join path")
    {
        String expect = str_temp("/home/Bob/Documents/divorce.pptx");
        String result = path_concat(str_temp("/home/Bob/"), str_temp("/Documents/divorce.pptx"));
        assert(str_equal(expect, result), "Expected equal");
    }

    test("filename")
    {
        String expect = str_temp("foo.txt");
        String result = path_get_filename(str_temp("/home/user/foo.txt"));
        assert(str_equal(expect, result), "Expected equal");
    }

    test("extension")
    {
        String expect = str_temp("txt");
        String result = path_get_extension(str_temp("/home/user/foo.txt"));
        assert(str_equal(expect, result), "Expected equal");
    }

    test("extension edgecase")
    {
        String expect = str_temp("gitignore");
        String result = path_get_extension(str_temp("/home/user/.gitignore"));
        assert(str_equal(expect, result), "Expected equal");
    }

    return true;
}

bool test_list(void)
{
    test("SparseList")
    {
        SparseList list = sparse_list_new(sizeof(String), 2, get_temporary_allocator());
        assert(!list.err, "Expected no error");

        int count = 10;
        for (int i = 0; i < count; i++)
        {
            String s = fmt("Hello {i32}", i);
            assert(!s.err && s.s != NULL, "String error");
            sparse_list_append(&list, &s);
        }

        for (int i = 0; i < count; i++)
        {
            String *s = sparse_list_get(&list, (u32)i);
            assert(s != NULL, "Expected not null");
            assert(str_equal(*s, fmt("Hello {i32}", i)), "Expected get equal");
        }

        sparse_list_remove(&list, 0);
        sparse_list_remove(&list, 1);
        sparse_list_remove(&list, 2);
        assert(list.size == (u64)count-3, "Expected correct size after remove");

        for (int i = 0; i < 3; i++)
        {
            String *s = sparse_list_get(&list, (u32)i);
            assert(s != NULL, "Expected not null");
            assert(str_equal(*s, fmt("Hello {i32}", count-i-1)), "Expected equal after remove");
        }

        sparse_list_clear(&list);

        for (int i = 0; i < 3; i++)
        {

            String s = fmt("{i32}", i+1);
            sparse_list_append(&list, &s);
        }

        String five = fmt("5");
        sparse_list_put(&list, 1, &five);

        String *got = sparse_list_get(&list, 1);
        assert(str_equal(*got, five), "Expected five");
    }

    return true;
}

void run_tests(void)
{
    test_allocation();
    test_string();
    test_string_builder();
    test_fmt();
    test_list();
    test_path();
}

int main(void)
{
    run_tests();
    return 0;
}


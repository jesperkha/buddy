#include <stdio.h>

#define BUDDY_IMPLEMENTATION
#include "buddy.h"

// Military grade testing suite

#define log(m) printf("TEST: -- " m "\n")
#define title(m) printf("TEST: " m "\n")
#define fail(m)                        \
    {                                  \
        title("   >>> FAIL: " m "\n"); \
        return false;                  \
    }
#define assert(v, m) \
    if (!(v))        \
        fail(m);
#define run(name) (test_##name)();
#define Test(name, body) \
    bool test_##name()   \
    {                    \
        title(#name);    \
        body;            \
        title("OK\n");   \
        return true;     \
    }

// ----------------------------------------------------------------------
// -------------------------  TESTING  ----------------------------------
// ----------------------------------------------------------------------

Test(Allocation, {
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
    u64 size = TEMP_ALLOC_BUFSIZE;
    u8 *p = temp_zero_alloc(size);
    assert(p != NULL, "Expected valid poiner");
    for (int i = 0; i < size; i++)
        assert(p[i] == 0, "Expected zeroed memory");

    reset_temp_memory();
}
});

Test(String, {
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
    log("string replace string");
    String expect = str_temp("Hello there my friends!");
    String replaced = str_replace_str(
        get_temporary_allocator(),
        str_temp("Hello world!"),
        str_temp("world"),
        str_temp("there my friends"));

    assert(!replaced.err, "Expected non-error");
    assert(str_equal(replaced, expect), "Expected equal");
}
{
    log("string reverse");
    String reversed = str_reverse(str_temp("Hello world!"));
    String expect = str_temp("!dlrow olleH");
    assert(str_equal(reversed, expect), "Expected equal");
}
});

Test(StringBuilder, {
{
    log("string builder append");
    StringBuilder sb = str_builder_new(get_temporary_allocator(), 100);
    assert(!sb.err, "Expected init success");
    assert(str_builder_append_cstr(&sb, "Hello "), "Expected success");
    assert(str_builder_append_cstr(&sb, "world!"), "Expected success");

    String expect = str_temp("Hello world!");
    assert(str_equal(expect, str_builder_to_string(&sb)), "Expected equal");
}
{
    log("string builder realloc: NOT IMPLEMENTED");
    // StringBuilder sb = str_builder_new(get_temporary_allocator(), 100);
    // assert(!sb.err, "Expected init success");
    // assert(str_builder_append_cstr(&sb, "abcde"), "Expected success");
    // assert(str_builder_append_cstr(&sb, "fghijklmno"), "Expected success");

    // String expect = str_temp("abcdefghijklmno");
    // assert(str_equal(expect, str_builder_to_string(&sb)), "Expected equal");
}
})

int main()
{
    run(String);
    run(Allocation);
    run(StringBuilder);
    return 0;
}

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
}
});

Test(String, {
{
    log("upper and lower case");
    String s = str_new("Hello World!");
    assert(str_equal(str_upper(s), str_new("HELLO WORLD!")), "Expected equal");
    assert(str_equal(str_lower(s), str_new("hello world!")), "Expected equal");
}
{
    log("c string to String");
    String s = str_new("Hello world!");
    assert(s.length == 12, "Expected length 12");
}
{
    log("string equal");
    assert(str_equal(str_new("Hello"), str_new("Hello")), "Expected equal");
    assert(!str_equal(str_new("hello"), str_new("Hello")), "Expected not equal");
    assert(!str_equal(str_new("he"), str_new("hello")), "Expected not equal");
}
});

int main()
{
    run(String);
    run(Allocation);
    return 0;
}

// Builds the header-only version of buddy
// Outputs in /dist/buddy.h

#include "buddy.h"
#include <stdlib.h>

int main(void)
{
    Arena *arena = arena_new(get_heap_allocator(), MB(1));
    if (arena == NULL)
        panic("arena is null");

    Allocator a = get_arena_allocator(arena);

    Bytes header = file_read_all("buddy.h", a);
    Bytes source = file_read_all("buddy.c", a);
    if (header.err || source.err)
        panic("failed to read source files");

    StringBuilder sb = str_builder_new(a);
    if (sb.err)
        panic("sb is null");

    str_builder_append_bytes(&sb, header.bytes, header.length);
    str_builder_append_cstr(&sb, "\n\n#ifdef BUDDY_IMPLEMENTATION\n\n");
    str_builder_append_bytes(&sb, source.bytes, source.length);
    str_builder_append_cstr(&sb, "\n\n#endif\n\n");

    dir_new("dist");
    if (!file_write_all("dist/buddy.h", (u8 *)sb.mem, sb.length))
        out("failed to write output file");

    free_arena(arena, get_heap_allocator());
    out("done");
    return 0;
}

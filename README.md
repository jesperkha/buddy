# buddy

Single-header modern C utility library

## Use

### Generate header

1. Clone the project
2. Run `make build`
3. Copy the generated header `/dist/buddy.h` into your project

### Use in project

```c
#define BUDDY_IMPLEMENTATION // Only do this is main
#include "buddy.h"

int main(void)
{
    out("Hello world!");
    return 0;
}
```

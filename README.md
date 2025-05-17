# buddy

Single-header modern C utility library

## Use

Copy and run the following command. It will fetch and run the install script which creates the `buddy.h` file in the current directory.

```sh
curl -fsSL https://raw.githubusercontent.com/jesperkha/buddy/refs/heads/main/install.sh | bash
```

Then prepend the implementation definition before including the header in your main file.

```c
#define BUDDY_IMPLEMENTATION // Only do this in main
#include "buddy.h"

int main(void)
{
    out("Hello world!");
    return 0;
}
```

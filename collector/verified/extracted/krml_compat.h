#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef KRML_HOST_MALLOC
#define KRML_HOST_MALLOC malloc
#endif
#ifndef KRML_HOST_FREE
#define KRML_HOST_FREE free
#endif
#ifndef KRML_HOST_EPRINTF
#define KRML_HOST_EPRINTF(...) fprintf(stderr, __VA_ARGS__)
#endif
#ifndef KRML_HOST_EXIT
#define KRML_HOST_EXIT(x) exit(x)
#endif

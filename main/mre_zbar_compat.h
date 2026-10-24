#ifndef MRE_ZBAR_COMPAT_H
#define MRE_ZBAR_COMPAT_H

#include <assert.h>
#include "vmsys.h"
#include "vmio.h"

#endif

#ifdef MRE_ZBAR_ENABLE_ALLOC_MACROS
#define malloc vm_malloc
#define free vm_free
#define calloc(x,y) vm_calloc((x)*(y))
#define realloc vm_realloc

#undef assert
#define assert(x) if(!(x)){vm_exit_app();while(1){};}
#endif

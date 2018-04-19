// Auto-generated, don't edit.
// Copyright (c) 2017-2018 Nguyen Viet Giang. All rights reserved.
#include "import.h"
#include "../utils.h"

aresult_t
aimport_stack_realloc(
    aimport_stack_t *s, u32 capacity)
{
    const u32 new_cap = apowof2_ceil(capacity);
    if (new_cap != 0) {
        void *const items = AREALLOC(
            s->a, s->items, sizeof(aimport_t)*new_cap);
        if (!items) return AR_MEMORY;
        s->items = (aimport_t*)items;
        s->capacity = new_cap;
        s->count = AMIN(s->count, s->capacity);
    }
    return AR_SUCCESS;
}

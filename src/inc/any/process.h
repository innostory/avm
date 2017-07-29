/* Copyright (c) 2017 Nguyen Viet Giang. All rights reserved. */
#pragma once

#include <assert.h>
#include <any/rt_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Initialize as a new process.
ANY_API void aprocess_init(aprocess_t* self, ascheduler_t* owner, apid_t pid);

/// Start process with entry point on top of the stack.
ANY_API void aprocess_start(aprocess_t* self, int32_t stack_sz);

/// Release all internal allocated memory.
ANY_API void aprocess_cleanup(aprocess_t* self);

/// Ensures that there are `more` bytes in the stack.
ANY_API void aprocess_reserve(aprocess_t* self, int32_t more);

/// Throw an error, with description string pushed onto the stack.
ANY_API void any_error(aprocess_t* p, const char* fmt, ...);

/// Get normalized index.
AINLINE int32_t aprocess_absidx(aprocess_t* self, int32_t idx)
{
    int32_t i = idx < 0 ? self->stack_sz + idx : idx - 1;
    if (i < 0 || i >= self->stack_sz) {
        any_error(self, "bad index %d", i);
    }
    return i;
}

/// Push a value onto the stack, should be internal used.
AINLINE void aprocess_push(aprocess_t* self, avalue_t* v)
{
    if (self->stack_sz == self->stack_cap) aprocess_reserve(self, 1);
    *(self->stack + self->stack_sz) = *v;
    ++self->stack_sz;
}

/// Lookup for a module level symbol and push it onto the stack.
ANY_API void any_find(aprocess_t* p, const char* module, const char* name);

/// Call a function on top of the stack.
ANY_API void any_call(aprocess_t* p);

/// Call a function in protected mode.
ANY_API void any_pcall(aprocess_t* p);

/// Suspends the execution flow.
ANY_API void any_yield(aprocess_t* p);

/// Execute in protected mode.
ANY_API int32_t any_try(aprocess_t* p, void(*f)(aprocess_t*, void*), void* ud);

/// Throw an error.
ANY_API void any_throw(aprocess_t* p, int32_t ec);

/// Get the value tag of the value at `idx`.
AINLINE avalue_tag_t any_type(aprocess_t* p, int32_t idx)
{
    return p->stack[aprocess_absidx(p, idx)].tag;
}

// Stack manipulations.
AINLINE void any_push_nil(aprocess_t* self)
{
    avalue_t v;
    v.tag.b = ABT_NIL;
    aprocess_push(self, &v);
}

AINLINE void any_push_bool(aprocess_t* self, int32_t b)
{
    avalue_t v;
    v.tag.b = ABT_BOOL;
    v.v.boolean = b;
    aprocess_push(self, &v);
}

AINLINE void any_push_integer(aprocess_t* self, aint_t i)
{
    avalue_t v;
    v.tag.b = ABT_NUMBER;
    v.tag.variant = AVTN_INTEGER;
    v.v.integer = i;
    aprocess_push(self, &v);
}

AINLINE void any_push_real(aprocess_t* self, areal_t r)
{
    avalue_t v;
    v.tag.b = ABT_NUMBER;
    v.tag.variant = AVTN_REAL;
    v.v.real = r;
    aprocess_push(self, &v);
}

AINLINE int32_t any_to_bool(aprocess_t* self, int32_t idx)
{
    avalue_t* v = self->stack + aprocess_absidx(self, idx);
    assert(v->tag.b == ABT_BOOL);
    return v->v.boolean;
}

AINLINE aint_t any_to_integer(aprocess_t* self, int32_t idx)
{
    avalue_t* v = self->stack + aprocess_absidx(self, idx);
    assert(v->tag.b == ABT_NUMBER);
    assert(v->tag.variant == AVTN_INTEGER);
    return v->v.integer;
}

AINLINE areal_t any_to_real(aprocess_t* self, int32_t idx)
{
    avalue_t* v = self->stack + aprocess_absidx(self, idx);
    assert(v->tag.b == ABT_NUMBER);
    assert(v->tag.variant == AVTN_REAL);
    return v->v.real;
}

AINLINE void any_pop(aprocess_t* self, int32_t n)
{
    self->stack_sz -= n;
    if (self->stack_sz < 0) {
        any_error(self, "stack underflow");
    }
}

/** Returns the index of the top element in the stack.
\note Indices start at 1, so this result is equal to the stack size.
*/
AINLINE int32_t any_top(aprocess_t* self)
{
    return self->stack_sz;
}

#ifdef __cplusplus
} // extern "C"
#endif
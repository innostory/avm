/* Copyright (c) 2017 Nguyen Viet Giang. All rights reserved. */
#ifdef ANY_TOOL

#include <catch.hpp>

#include <stdlib.h>
#include <string.h>
#include <any/asm.h>
#include <any/errno.h>
#include <any/string_table.h>

#define REQUIRE_STR_EQUALS(a, b) REQUIRE(strcmp(a, b) == 0)

static void* myalloc(void*, void* old, int32_t sz)
{
    return realloc(old, sz);
}

static void num_vs_capacity_check(aasm_prototype_t* p)
{
    REQUIRE(p->num_instructions <= p->max_instructions);
    REQUIRE(p->num_constants <= p->max_constants);
    REQUIRE(p->num_imports <= p->max_imports);
    REQUIRE(p->num_nesteds <= p->max_nesteds);
}

typedef struct {
    ainstruction_t pop;
    ainstruction_t ldk;
    ainstruction_t llv;
    ainstruction_t slv;
    ainstruction_t imp;
    ainstruction_t jmp;
    ainstruction_t jin;
    ainstruction_t ivk;

    aasm_constant_t cinteger;
    aasm_constant_t cstring;
    aasm_constant_t creal;
} basic_test_ctx;

static void basic_add(aasm_t* a, basic_test_ctx& t)
{
    t.pop = ai_pop(rand());
    t.ldk = ai_ldk(rand());
    t.llv = ai_llv(rand());
    t.slv = ai_slv(rand());
    t.imp = ai_imp(rand());
    t.jmp = ai_jmp(rand());
    t.jin = ai_jin(rand());
    t.ivk = ai_ivk(rand());

    REQUIRE(0 == any_asm_emit(a, ai_nop()));
    REQUIRE(1 == any_asm_emit(a, t.pop));
    REQUIRE(2 == any_asm_emit(a, t.ldk));
    REQUIRE(3 == any_asm_emit(a, ai_nil()));
    REQUIRE(4 == any_asm_emit(a, ai_ldb(TRUE)));
    REQUIRE(5 == any_asm_emit(a, ai_ldb(FALSE)));
    REQUIRE(6 == any_asm_emit(a, t.llv));
    REQUIRE(7 == any_asm_emit(a, t.slv));
    REQUIRE(8 == any_asm_emit(a, t.imp));
    REQUIRE(9 == any_asm_emit(a, t.jmp));
    REQUIRE(10 == any_asm_emit(a, t.jin));
    REQUIRE(11 == any_asm_emit(a, t.ivk));
    REQUIRE(12 == any_asm_emit(a, ai_ret()));

    t.cinteger = ac_integer(rand());
    t.cstring = ac_string(any_asm_string_to_ref(a, "test_const"));
    t.creal = ac_real((areal_t)rand());

    REQUIRE(0 == any_asm_add_constant(a, t.cinteger));
    REQUIRE(1 == any_asm_add_constant(a, t.cstring));
    REQUIRE(2 == any_asm_add_constant(a, t.creal));

    REQUIRE(0 == any_asm_add_import(a, "tim0", "tin0"));
    REQUIRE(1 == any_asm_add_import(a, "tim1", "tin1"));
    REQUIRE(2 == any_asm_add_import(a, "tim2", "tin2"));
};

static void basic_check(aasm_t* a, basic_test_ctx& t)
{
    auto p = any_asm_prototype(a);
    auto c = any_asm_resolve(a);

    num_vs_capacity_check(p);

    REQUIRE(p->num_instructions == 13);
    REQUIRE(c.instructions[0].b.opcode == AOC_NOP);
    REQUIRE(c.instructions[1].b.opcode == AOC_POP);
    REQUIRE(c.instructions[1].pop.n == t.pop.pop.n);
    REQUIRE(c.instructions[2].b.opcode == AOC_LDK);
    REQUIRE(c.instructions[2].ldk.idx == t.ldk.ldk.idx);
    REQUIRE(c.instructions[3].b.opcode == AOC_NIL);
    REQUIRE(c.instructions[4].b.opcode == AOC_LDB);
    REQUIRE(c.instructions[4].ldb.val == TRUE);
    REQUIRE(c.instructions[5].b.opcode == AOC_LDB);
    REQUIRE(c.instructions[5].ldb.val == FALSE);
    REQUIRE(c.instructions[6].b.opcode == AOC_LLV);
    REQUIRE(c.instructions[6].llv.idx == t.llv.llv.idx);
    REQUIRE(c.instructions[7].b.opcode == AOC_SLV);
    REQUIRE(c.instructions[7].slv.idx == t.slv.slv.idx);
    REQUIRE(c.instructions[8].b.opcode == AOC_IMP);
    REQUIRE(c.instructions[8].imp.idx == t.imp.imp.idx);
    REQUIRE(c.instructions[9].b.opcode == AOC_JMP);
    REQUIRE(c.instructions[9].jmp.displacement == t.jmp.jmp.displacement);
    REQUIRE(c.instructions[10].b.opcode == AOC_JIN);
    REQUIRE(c.instructions[10].jin.displacement == t.jin.jin.displacement);
    REQUIRE(c.instructions[11].b.opcode == AOC_IVK);
    REQUIRE(c.instructions[11].ivk.nargs == t.ivk.ivk.nargs);
    REQUIRE(c.instructions[12].b.opcode == AOC_RET);

    REQUIRE(p->num_constants == 3);
    REQUIRE(c.constants[0].b.type == ACT_INTEGER);
    REQUIRE(c.constants[0].i.val == t.cinteger.i.val);
    REQUIRE(c.constants[1].b.type == ACT_STRING);
    REQUIRE(c.constants[1].s.ref == t.cstring.s.ref);
    REQUIRE(c.constants[2].b.type == ACT_REAL);
    REQUIRE(c.constants[2].r.val == t.creal.r.val);

    REQUIRE(p->num_imports == 3);
    REQUIRE_STR_EQUALS(any_st_to_string(a->st, c.imports[0].module), "tim0");
    REQUIRE_STR_EQUALS(any_st_to_string(a->st, c.imports[0].name), "tin0");
    REQUIRE_STR_EQUALS(any_st_to_string(a->st, c.imports[1].module), "tim1");
    REQUIRE_STR_EQUALS(any_st_to_string(a->st, c.imports[1].name), "tin1");
    REQUIRE_STR_EQUALS(any_st_to_string(a->st, c.imports[2].module), "tim2");
    REQUIRE_STR_EQUALS(any_st_to_string(a->st, c.imports[2].name), "tin2");
}

static void require_equals(aasm_t* a1, aasm_t* a2)
{
    auto p1 = any_asm_prototype(a1);
    auto p2 = any_asm_prototype(a2);
    auto c1 = any_asm_resolve(a1);
    auto c2 = any_asm_resolve(a2);

    REQUIRE_STR_EQUALS(
        any_st_to_string(a1->st, p1->source),
        any_st_to_string(a2->st, p2->source));
    REQUIRE_STR_EQUALS(
        any_st_to_string(a1->st, p1->symbol),
        any_st_to_string(a2->st, p2->symbol));

    REQUIRE(p1->num_upvalues == p2->num_upvalues);
    REQUIRE(p1->num_arguments == p2->num_arguments);
    REQUIRE(p1->num_local_vars == p2->num_local_vars);
    REQUIRE(p1->num_nesteds == p2->num_nesteds);

    REQUIRE(p1->num_instructions == p2->num_instructions);
    REQUIRE(
        memcmp(
            c1.instructions, 
            c2.instructions, 
            p1->num_instructions*sizeof(ainstruction_t)) == 0);

    REQUIRE(p1->num_constants == p2->num_constants);
    for (int32_t i = 0; i < p1->num_constants; ++i) {
        REQUIRE(c1.constants[i].b.type == c2.constants[i].b.type);
        switch (c1.constants[i].b.type) {
        case ACT_INTEGER:
            REQUIRE(c1.constants[i].i.val == c2.constants[i].i.val);
            break;
        case ACT_STRING:
            REQUIRE_STR_EQUALS(
                any_st_to_string(a1->st, c1.constants[i].s.ref),
                any_st_to_string(a2->st, c2.constants[i].s.ref));
            break;
        case ACT_REAL:
            REQUIRE(c1.constants[i].r.val == c2.constants[i].r.val);
            break;
        }
    }

    REQUIRE(p1->num_imports == p2->num_imports);
    for (int32_t i = 0; i < p2->num_imports; ++i) {
        REQUIRE_STR_EQUALS(
            any_st_to_string(a1->st, c1.imports[i].module),
            any_st_to_string(a2->st, c2.imports[i].module));
        REQUIRE_STR_EQUALS(
            any_st_to_string(a1->st, c1.imports[i].name),
            any_st_to_string(a2->st, c2.imports[i].name));
    }
}

TEST_CASE("asm_module")
{
    aasm_t a;
    any_asm_init(&a, &myalloc, NULL);
    REQUIRE(any_asm_load(&a, NULL) == AERR_NONE);
    auto p = any_asm_prototype(&a);

    REQUIRE(p->source == 0);
    REQUIRE(p->symbol == 0);
    REQUIRE(p->num_instructions == 0);
    REQUIRE(p->num_upvalues == 0);
    REQUIRE(p->num_arguments == 0);
    REQUIRE(p->num_local_vars == 0);
    REQUIRE(p->num_constants == 0);
    REQUIRE(p->num_imports == 0);
    REQUIRE(p->num_nesteds == 0);

    num_vs_capacity_check(p);

    any_asm_cleanup(&a);
}

TEST_CASE("asm_basic")
{
    aasm_t a;
    any_asm_init(&a, &myalloc, NULL);
    REQUIRE(any_asm_load(&a, NULL) == AERR_NONE);

    basic_test_ctx t;
    basic_add(&a, t);
    basic_check(&a, t);

    any_asm_cleanup(&a);
}

TEST_CASE("asm_nested")
{
    aasm_t a;
    any_asm_init(&a, &myalloc, NULL);
    REQUIRE(any_asm_load(&a, NULL) == AERR_NONE);

    enum { PUSH_COUNT = 25 };

    for (int32_t i = 0; i < ANY_ASM_MAX_NESTED_LEVEL; ++i) {
        for (int32_t j = 0; j < PUSH_COUNT; ++j) {
            REQUIRE(j == any_asm_push(&a));
            basic_test_ctx t;
            basic_add(&a, t);
            basic_check(&a, t);
            REQUIRE(j == any_asm_pop(&a));
            any_asm_open(&a, j);
            basic_check(&a, t);
            REQUIRE(j == any_asm_pop(&a));
        }
        if (i == 0) {
            REQUIRE(PUSH_COUNT == any_asm_module_push(&a, "symbol"));
            const aasm_prototype_t* p = any_asm_prototype(&a);
            REQUIRE_STR_EQUALS(any_st_to_string(a.st, p->symbol), "symbol");
        } else {
            REQUIRE(PUSH_COUNT == any_asm_push(&a));
        }
    }

    for (int32_t i = ANY_ASM_MAX_NESTED_LEVEL - 1; i >= 0; --i) {
        REQUIRE(PUSH_COUNT == any_asm_pop(&a));
    }

    any_asm_cleanup(&a);
}

TEST_CASE("asm_save_load")
{
    aasm_t a1;
    any_asm_init(&a1, &myalloc, NULL);
    REQUIRE(any_asm_load(&a1, NULL) == AERR_NONE);

    enum { PUSH_COUNT = 5 };

    for (int32_t i = 0; i < ANY_ASM_MAX_NESTED_LEVEL; ++i) {
        for (int32_t j = 0; j < PUSH_COUNT; ++j) {
            REQUIRE(j == any_asm_push(&a1));
            basic_test_ctx t;
            basic_add(&a1, t);
            basic_check(&a1, t);
            REQUIRE(j == any_asm_pop(&a1));
        }
        REQUIRE(PUSH_COUNT == any_asm_push(&a1));
    }

    any_asm_save(&a1);

    for (int32_t i = ANY_ASM_MAX_NESTED_LEVEL - 1; i >= 0; --i) {
        REQUIRE(PUSH_COUNT == any_asm_pop(&a1));
    }

    aasm_t a2;
    any_asm_init(&a2, &myalloc, NULL);
    REQUIRE(any_asm_load(&a2, a1.chunk) == AERR_NONE);

    for (int32_t i = 0; i < ANY_ASM_MAX_NESTED_LEVEL; ++i) {
        for (int32_t j = 0; j < PUSH_COUNT; ++j) {
            any_asm_open(&a1, j);
            any_asm_open(&a2, j);
            require_equals(&a1, &a2);
            REQUIRE(j == any_asm_pop(&a1));
            REQUIRE(j == any_asm_pop(&a2));
        }
        any_asm_open(&a1, PUSH_COUNT);
        any_asm_open(&a2, PUSH_COUNT);
    }

    any_asm_cleanup(&a1);
    any_asm_cleanup(&a2);
}

#endif // ANY_TOOL
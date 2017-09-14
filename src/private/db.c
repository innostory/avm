/* Copyright (c) 2017 Nguyen Viet Giang. All rights reserved. */
#include <any/db.h>

#include <any/scheduler.h>
#include <any/loader.h>

#define REQUEST_BUFF_SZ 2048
#define IO_BUFF_SZ 8192

static const struct wby_header CORS_HEADERS[] = {
    { "Access-Control-Allow-Origin",
      "*"
    },
    { "Access-Control-Allow-Methods",
      "GET, POST, PUT, PATCH, DELETE, OPTIONS"
    },
    { "Access-Control-Allow-Headers",
      "Connection, Content-Type"
    },
    { "Access-Control-Max-Age",
       "600"
    }
};

static const struct wby_header JSON_HEADERS[] = {
    { "Content-Type",
      "application/json"
    },
    { "Cache-Control",
      "no-cache"
    },
    { "Access-Control-Allow-Origin",
      "*"
    },
    { "Access-Control-Allow-Methods",
      "GET, POST, PUT, PATCH, DELETE, OPTIONS"
    },
    { "Access-Control-Allow-Headers",
      "Connection, Content-Type"
    },
    { "Access-Control-Max-Age",
      "600"
    }
};

static AINLINE void*
aalloc(
    adb_t* self, void* old, const aint_t sz)
{
    return self->alloc(self->alloc_ud, old, sz);
}

#define WBY_WRITE_STATIC(con, buf) wby_write(con, buf, sizeof(buf) - 1)

static void
wby_write_fmt(
    struct wby_con* con, const char* fmt, ...)
{
    va_list args;
    char buf[128];
    int sz;
    va_start(args, fmt);
    sz = vsnprintf(buf, sizeof(buf), fmt, args);
    assert(sz >= 0 && sz < sizeof(buf));
    wby_write(con, buf, sz);
    va_end(args);
}

static void
json_response_begin(
    struct wby_con* con, int status, int content_length)
{
    wby_response_begin(
        con, status, content_length,
        JSON_HEADERS, ASTATIC_ARRAY_COUNT(JSON_HEADERS));
}

static int
simple_response(
    struct wby_con* con, int status)
{
    wby_response_begin(
        con, status, 0,
        CORS_HEADERS, ASTATIC_ARRAY_COUNT(CORS_HEADERS));
    wby_response_end(con);
    return 0;
}

static void
encode_imports(
    aimport_t* imports,
    aint_t num, const char* strings, struct wby_con* con)
{
    aint_t i;
    for (i = 0; i < num; ++i) {
        aimport_t* imp = imports + i;
        if (i != 0) WBY_WRITE_STATIC(con, ",");
        wby_write_fmt(con, "{\"module\":\"%s\",\"name\":\"%s\"}",
            strings + imp->module, strings + imp->name);
    }
}

static void
encode_constants(
    aconstant_t* constants,
    aint_t num, const char* strings, struct wby_con* con)
{
    aint_t i;
    for (i = 0; i < num; ++i) {
        aconstant_t* c = constants + i;
        if (i != 0) WBY_WRITE_STATIC(con, ",");
        switch (c->type) {
        case ACT_INTEGER:
            wby_write_fmt(con, "%d", c->integer);
            break;
        case ACT_STRING:
            wby_write_fmt(con, "\"%s\"", strings + c->string);
            break;
        case ACT_REAL:
            wby_write_fmt(con, "%f", c->real);
            break;
        default:
            assert(!"bad constant type");
        }
    }
}

static void
encode_instructions(
    ainstruction_t* instructions, aint_t num, struct wby_con* con)
{
    int32_t i;
    for (i = 0; i < num; ++i) {
        ainstruction_t* ins = instructions + i;
        if (i != 0) WBY_WRITE_STATIC(con, ",");
        WBY_WRITE_STATIC(con, "{\"type\": \"");
        switch (ins->b.opcode) {
        case AOC_NOP:
            WBY_WRITE_STATIC(con, "nop\"}");
            break;
        case AOC_BRK:
            WBY_WRITE_STATIC(con, "brk\"}");
            break;
        case AOC_POP:
            wby_write_fmt(con, "pop\",\"n\":%d}", ins->pop.n);
            break;
        case AOC_LDK:
            wby_write_fmt(con, "ldk\",\"idx\":%d}", ins->ldk.idx);
            break;
        case AOC_NIL:
            WBY_WRITE_STATIC(con, "nil\"}");
            break;
        case AOC_LDB:
            wby_write_fmt(con, "ldb\",\"val\":%s}",
                ins->ldb.val ? "true" : "false");
            break;
        case AOC_LSI:
            wby_write_fmt(con, "lsi\",\"val\":%d}", ins->lsi.val);
            break;
        case AOC_LLV:
            wby_write_fmt(con, "llv\",\"idx\":%d}", ins->llv.idx);
            break;
        case AOC_SLV:
            wby_write_fmt(con, "slv\",\"idx\":%d}", ins->slv.idx);
            break;
        case AOC_IMP:
            wby_write_fmt(con, "imp\",\"idx\":%d}", ins->imp.idx);
            break;
        case AOC_CLS:
            wby_write_fmt(con, "cls\",\"idx\":%d}", ins->cls.idx);
            break;
        case AOC_JMP:
            wby_write_fmt(con, "jmp\",\"displacement\":%d}",
                ins->jmp.displacement);
            break;
        case AOC_JIN:
            wby_write_fmt(con, "jin\",\"displacement\":%d}",
                ins->jin.displacement);
            break;
        case AOC_IVK:
            wby_write_fmt(con, "ivk\",\"nargs\":%d}", ins->ivk.nargs);
            break;
        case AOC_RET:
            WBY_WRITE_STATIC(con, "ret\"}");
            break;
        case AOC_SND:
            WBY_WRITE_STATIC(con, "snd\"}");
            break;
        case AOC_RCV:
            wby_write_fmt(con, "rcv\",\"displacement\":%d}",
                ins->rcv.displacement);
            break;
        case AOC_RMV:
            WBY_WRITE_STATIC(con, "rmv\"}");
            break;
        case AOC_RWD:
            WBY_WRITE_STATIC(con, "rwd\"}");
            break;
        default:
            assert(!"bad instruction type");
        }
    }
}

static void
encode_prototype(
    aprototype_t* p, struct wby_con* con)
{
    int32_t i;
    wby_write_fmt(con, "{\"address\":%zd,", (size_t)p);
    if (p->header->symbol != 0) {
        wby_write_fmt(con, "\"name\":\"%s\",",
            p->strings + p->header->symbol);
    }
    WBY_WRITE_STATIC(con, "\"imports\":[");
    encode_imports(p->imports, p->header->num_imports, p->strings, con);
    WBY_WRITE_STATIC(con, "],\"constants\":[");
    encode_constants(p->constants, p->header->num_constants, p->strings, con);
    WBY_WRITE_STATIC(con, "],\"instructions\":[");
    encode_instructions(p->instructions, p->header->num_instructions, con);
    WBY_WRITE_STATIC(con, "],\"prototypes\":[");
    for (i = 0; i < p->header->num_nesteds; ++i) {
        if (i != 0) WBY_WRITE_STATIC(con, ",");
        encode_prototype(p->nesteds + i, con);
    }
    WBY_WRITE_STATIC(con, "]}");
}

static void
encode_chunk(
    achunk_t* chunk, const char* type, struct wby_con* con)
{
    int32_t i;
    aprototype_t* m = chunk->prototypes;
    wby_write_fmt(con, "{\"address\":%zd,", (size_t)chunk);
    wby_write_fmt(con, "\"type\":\"%s\",", type);
    wby_write_fmt(con, "\"module\":\"%s\",", m->strings + m->header->symbol);
    WBY_WRITE_STATIC(con, "\"prototypes\":[");
    for (i = 0; i < m->header->num_nesteds; ++i) {
        if (i != 0) WBY_WRITE_STATIC(con, ",");
        encode_prototype(m->nesteds + i, con);
    }
    WBY_WRITE_STATIC(con, "]}");
}

static void
encode_chunks(
    alist_t* chunks, const char* type, int32_t* first, struct wby_con* con)
{
    alist_node_t* i = alist_head(chunks);
    while (!alist_is_end(chunks, i)) {
        achunk_t* chunk = ALIST_NODE_CAST(achunk_t, i);
        if (*first == FALSE) wby_write(con, ",", 1);
        encode_chunk(chunk, type, con);
        *first = FALSE;
        i = i->next;
    }
}

static int
handle_modules(
    adb_t* db, struct wby_con* con)
{
    if (strcmp(con->request.method, "GET") == 0) {
        aloader_t* l = &db->target->loader;
        int32_t first = TRUE;
        json_response_begin(con, 200, -1);
        WBY_WRITE_STATIC(con, "[");
        encode_chunks(&l->runnings, "running", &first, con);
        encode_chunks(&l->garbages, "garbage", &first, con);
        WBY_WRITE_STATIC(con, "]");
        wby_response_end(con);
        return 0;
    } else {
        return simple_response(con, 405);
    }
}

static int
dispatch(
    struct wby_con* con, void* ud)
{
    if (strcmp(con->request.method, "OPTIONS") == 0) {
        return simple_response(con, 200);
    } else {
        adb_t* db = (adb_t*)ud;
        const char* uri = con->request.uri;
        if (strcmp(uri, "/modules") == 0) {
            return handle_modules(db, con);
        } else {
            return 1;
        }
    }
}

static int
websocket_connect(
    struct wby_con* con, void* ud)
{
    // TODO
    return 0;
}

static void
websocket_connected(
    struct wby_con* con, void* ud)
{
    // TODO
}

static int
websocket_frame(
    struct wby_con* con, const struct wby_frame* frame, void* ud)
{
    // TODO
    return 0;
}

static void
websocket_closed(
    struct wby_con* con, void* ud)
{
    // TODO
}

static void
on_throw(
    aactor_t* a, void* ud)
{
    // TODO
}

static void
on_spawn(
    aactor_t* a, void* ud)
{
    // TODO
}

static void
on_exit(
    aactor_t* a, void* ud)
{
    // TODO
}

static int32_t
on_step(
    aactor_t* a, void* ud)
{
    // TODO
    return TRUE;
}

static void
on_linked(
    aloader_t* l, void* ud)
{
    // TODO
    ud = NULL;
}

static void
config_callback(
    struct wby_config* cfg)
{
    cfg->dispatch     = &dispatch;
    cfg->ws_connect   = &websocket_connect;
    cfg->ws_connected = &websocket_connected;
    cfg->ws_frame     = &websocket_frame;
    cfg->ws_closed    = &websocket_closed;
}

static void
config(
    struct wby_config* cfg, void* ud,
    const char* address, uint16_t port, aint_t max_connections)
{
    cfg->userdata = ud;
    cfg->address = address;
    cfg->port = port;
    cfg->connection_max = (unsigned int)max_connections;
    cfg->request_buffer_size = REQUEST_BUFF_SZ;
    cfg->io_buffer_size = IO_BUFF_SZ;
    config_callback(cfg);
}

static void
attach(
    ascheduler_t* target, adb_t* db)
{
    aloader_on_linked(&target->loader, &on_linked, db);
    ascheduler_on_throw(target, &on_throw, db);
    ascheduler_on_spawn(target, &on_spawn, db);
    ascheduler_on_exit (target, &on_exit,  db);
    ascheduler_on_step (target, &on_step,  db);
}

static void
detach(
    ascheduler_t* target)
{
    ascheduler_on_throw(target, NULL, NULL);
    ascheduler_on_spawn(target, NULL, NULL);
    ascheduler_on_exit (target, NULL, NULL);
    ascheduler_on_step (target, NULL, NULL);
}

aerror_t
adb_init(
    adb_t* self, aalloc_t alloc, void* alloc_ud, ascheduler_t* target,
    const char* address, uint16_t port, aint_t max_connections)
{
    aerror_t ec;
    wby_size wby_buff_sz;
    struct wby_config cfg;
    memset(&cfg, 0, sizeof(cfg));
    config(&cfg, self, address, port, max_connections);
    memset(self, 0, sizeof(adb_t));
    self->alloc = alloc;
    self->alloc_ud = alloc_ud;
    self->target = target;
    wby_init(&self->wby, &cfg, &wby_buff_sz);
    self->wby_buff = aalloc(self, NULL, (aint_t)wby_buff_sz);
    if (self->wby_buff == NULL) {
        ec = AERR_FULL;
        goto failed;
    }
    if (wby_start(&self->wby, self->wby_buff) != 0) {
        ec = AERR_RUNTIME;
        goto failed;
    }
    attach(target, self);
    return AERR_NONE;
failed:
    if (self->wby_buff) {
        aalloc(self, self->wby_buff, 0);
        self->wby_buff = NULL;
    }
    return ec;
}

void
adb_cleanup(
    adb_t* self)
{
    if (self->wby_buff == NULL) return;
    detach(self->target);
    wby_stop(&self->wby);
    aalloc(self, self->wby_buff, 0);
    self->wby_buff = NULL;
}

void
adb_run_once(
    adb_t* self)
{
    wby_update(&self->wby, FALSE);
}
//Made by rdbo
//https://github.com/rdbo/libmem
//C-compatible version of https://github.com/rdbo/Memory

#include "libmem.h"
#if defined(MEM_COMPATIBLE)

//mem_string_t
struct _mem_string_t mem_string_init()
{
    struct _mem_string_t _string;
    _string.buffer         = (mem_char_t*)MEM_STR("");
    _string.clear          = &mem_string_clear;
    _string.size           = &mem_string_size;
    _string.resize         = &mem_string_resize;
    _string.length         = &mem_string_length;
    _string.begin          = &mem_string_begin;
    _string.end            = &mem_string_end;
    _string.find           = &mem_string_find;
    _string.value          = &mem_string_value;
    _string.c_str          = &mem_string_c_str;
    _string.substr         = &mem_string_substr;
    _string.compare        = &mem_string_compare;
    _string.is_initialized = mem_true;
    return _string;
}

mem_void_t mem_string_clear(struct _mem_string_t* p_string)
{
    if(p_string->is_initialized == mem_false) return;
    memset((void*)p_string->buffer, (int)0x0, (size_t)mem_string_size(p_string));
}

mem_size_t mem_string_size(struct _mem_string_t* p_string)
{
    mem_size_t ret = (mem_size_t)MEM_BAD_RETURN;
    if(p_string->is_initialized)
    {
        ret = (mem_size_t)((mem_uintptr_t)mem_string_end(p_string) - (mem_uintptr_t)mem_string_begin(p_string));
    }

    return ret;
}

mem_void_t mem_string_resize(struct _mem_string_t* p_string, mem_size_t size)
{
    if(p_string->is_initialized != mem_true) return;
    size = size * sizeof(mem_char_t) + 1;
    mem_char_t* _buffer = (mem_char_t*)malloc(size);
    mem_size_t old_size = mem_string_size(p_string);
    memcpy((void*)_buffer, (void*)p_string->buffer, (size_t)(size > old_size ? old_size : size));
    _buffer[size - 1] = MEM_STR('\0');
    p_string->buffer = _buffer;
}

mem_size_t mem_string_length(struct _mem_string_t* p_string)
{
    return (p_string->is_initialized == mem_true ? MEM_STR_LEN(p_string->buffer) : MEM_BAD_RETURN);
}

mem_char_t* mem_string_begin(struct _mem_string_t* p_string)
{
    return (mem_char_t*)p_string->buffer;
}

mem_char_t* mem_string_end(struct _mem_string_t* p_string)
{
    return (mem_char_t*)((mem_uintptr_t)p_string->buffer + mem_string_length(p_string));
}

mem_size_t mem_string_find(struct _mem_string_t* p_string, const mem_char_t* substr)
{
    mem_size_t ret = (mem_size_t)MEM_BAD_RETURN;
    if(p_string->is_initialized != mem_true) return ret;
    mem_size_t str_len    = mem_string_length(p_string);
    mem_size_t substr_len = MEM_STR_LEN(substr);
    for(mem_size_t i = 0; i + substr_len <= str_len + 1; i++)
    {
        if(!MEM_STR_N_CMP(p_string->buffer + i, substr, substr_len))
        {
            ret = i;
            break;
        }
    }

    return ret;
}

mem_void_t mem_string_value(struct _mem_string_t* p_string, const mem_char_t* new_str)
{
    p_string->buffer = (mem_char_t*)new_str;
}

mem_char_t* mem_string_c_str(struct _mem_string_t* p_string)
{
    return p_string->buffer;
}

mem_bool_t mem_string_compare(struct _mem_string_t* p_string, struct _mem_string_t str)
{
    return (mem_bool_t)(MEM_STR_CMP(mem_string_c_str(p_string), mem_string_c_str(&str)) == 0);
}

struct _mem_string_t mem_string_substr(struct _mem_string_t* p_string, mem_size_t start, mem_size_t end)
{
    struct _mem_string_t new_str = mem_string_init();
    mem_size_t size = end - start;
    if(end > start && mem_string_length(p_string) > size)
    {
        mem_size_t buffer_size = size * sizeof(mem_char_t) + 1;
        mem_char_t* _buffer = (mem_char_t*)malloc(buffer_size);
        memcpy((void*)_buffer, (void*)((mem_uintptr_t)p_string->buffer + start), (size_t)size);
        _buffer[buffer_size - 1] = MEM_STR('\0');
        new_str.buffer = _buffer;
    }

    return new_str;
}

//mem_process_t

struct _mem_process_t mem_process_init()
{
    struct _mem_process_t _process;
    _process.name           = mem_string_init();
    _process.pid            = (mem_pid_t)MEM_BAD_RETURN;
    _process.is_valid       = &mem_process_is_valid;
    _process.compare        = &mem_process_compare;
    _process.is_initialized = mem_true;
    return _process;
}

mem_bool_t mem_process_is_valid(struct _mem_process_t* p_process)
{
    mem_string_t str = mem_string_init();
    return (mem_bool_t)(
        p_process->is_initialized == mem_true &&
        MEM_STR_CMP(mem_string_c_str(&p_process->name), MEM_STR("")) &&
        p_process->pid != (mem_pid_t)MEM_BAD_RETURN
    );
}

mem_bool_t mem_process_compare(struct _mem_process_t* p_process, struct _mem_process_t process)
{
    return (mem_bool_t)(
        mem_string_compare(&p_process->name, process.name) == mem_true &&
        p_process->pid == process.pid
    );
}

//mem_module_t

struct _mem_module_t mem_module_init()
{
    struct _mem_module_t _mod;
    _mod.name           = mem_string_init();
    _mod.path           = mem_string_init();
    _mod.base           = (mem_voidptr_t)MEM_BAD_RETURN;
    _mod.size           = (mem_uintptr_t)MEM_BAD_RETURN;
    _mod.end            = (mem_voidptr_t)MEM_BAD_RETURN;
    _mod.is_valid       = &mem_module_is_valid;
    _mod.compare        = &mem_module_compare;
    _mod.is_initialized = mem_true;
    return _mod;
}

mem_bool_t mem_module_is_valid(struct _mem_module_t* p_mod)
{
    return (mem_bool_t)(
        p_mod->is_initialized                         &&
        !MEM_STR_CMP(mem_string_c_str(&p_mod->name), MEM_STR("")) &&
        !MEM_STR_CMP(mem_string_c_str(&p_mod->path), MEM_STR("")) &&
        p_mod->base != (mem_voidptr_t)MEM_BAD_RETURN  &&
        p_mod->size != (mem_size_t)MEM_BAD_RETURN     &&
        p_mod->end  != (mem_voidptr_t)MEM_BAD_RETURN
    );
}

mem_bool_t mem_module_compare(struct _mem_module_t* p_mod, struct _mem_module_t mod)
{
    return (mem_bool_t)(
        mem_string_compare(&p_mod->name, mod.name) &&
        mem_string_compare(&p_mod->path, mod.path) &&
        p_mod->base == mod.base                    &&
        p_mod->size == mod.size                    &&
        p_mod->end  == mod.end                     
    );
}

//mem_alloc_t

struct _mem_alloc_t mem_alloc_init()
{
    struct _mem_alloc_t _alloc;
#   if defined(MEM_WIN)
    _alloc.protection = PAGE_EXECUTE_READWRITE;
    _alloc.type       = MEM_COMMIT | MEM_RESERVE;
#   elif defined(MEM_LINUX)
    _alloc.protection = PROT_EXEC | PROT_READ | PROT_WRITE;
    _alloc.type       = MAP_ANON  | MAP_PRIVATE;
#   endif

    _alloc.is_valid = &mem_alloc_is_valid;
    _alloc.is_initialized = mem_true;
    return _alloc;
}

mem_bool_t mem_alloc_is_valid(struct _mem_alloc_t* p_alloc)
{
    return (mem_bool_t)(
        p_alloc->is_initialized == mem_true &&
        p_alloc->protection     != (mem_prot_t)MEM_BAD_RETURN &&
        p_alloc->type           != (mem_prot_t)MEM_BAD_RETURN
    );
}

//mem_lib_t

struct _mem_lib_t mem_lib_init()
{
    struct _mem_lib_t _lib;
    _lib.path           = mem_string_init();
    _lib.is_valid       = &mem_lib_is_valid;
#   if defined(MEM_WIN)
#   elif defined(MEM_LINUX)
    _lib.mode = (mem_int_t)RTLD_LAZY;
#   endif
    _lib.is_initialized = mem_true;
    return _lib;
}

mem_bool_t mem_lib_is_valid(struct _mem_lib_t* p_lib)
{
    return (mem_bool_t)(
        p_lib->is_initialized &&
        !MEM_STR_CMP(mem_string_c_str(&p_lib->path), MEM_STR(""))
    );
}

#endif //MEM_COMPATIBLE
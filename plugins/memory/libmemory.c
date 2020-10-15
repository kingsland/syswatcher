#include <sys/types.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <plugin_def.h>


#define COST(U) (U == 'G' ? 1024*1024*1024 : U == 'M' ? 1024*1024 : U == 'K' ? 1024 : 1)

struct memory_info
{
    unsigned total;
    unsigned used;
    unsigned free;
    char total_unit;
    char used_unit;
    char free_unit;
};

unsigned __to_unit(unsigned long long size, char* unit)
{
    char _unit[9] = {'B', 'K', 'M', 'G', 'T', 'P', 'E', 'Z', 'Y'};
    unsigned count = 0;
    unsigned _size = 0;

    while (size > 1024) {
        size = size >> 10;
        count ++;
    }

    *unit = _unit[count];
    _size = size;

    return _size;
}

#define LINE_BUF_LENGTH  1024
int mem_info(struct memory_info *mem)
{
    FILE *fp;
    char line[LINE_BUF_LENGTH];
    char cmd[1024];
    size_t pos;
    char *p;

    sprintf(cmd, "cat /proc/meminfo 2>/dev/null");
    if ((fp=popen(cmd, "r")) != NULL) {
        fgets(line, LINE_BUF_LENGTH, fp);
        p = strpbrk(line, "1234567890");            
        mem->total = atoi(p);

        p = strpbrk(p, "KkMmGgBb");            
        mem->total_unit = toupper(*p);

        fgets(line, LINE_BUF_LENGTH, fp);
        p = strpbrk(line, "0123456789");            
        mem->free = atoi(p);
        p = strpbrk(p, "KkMmGgBb");            
        mem->free_unit = toupper(*p);

        if (mem->total_unit == mem->free_unit) {
            mem->used = mem->total - mem->free;
            mem->used_unit = mem->free_unit;
        } else {
            mem->used = mem->total * COST(mem->total_unit) - mem->free * COST(mem->free_unit);
            mem->used = __to_unit(mem->used, &mem->used_unit);
        }

        pclose(fp);
    }

    return 0;
}

static int memory_information(item_t* data)
{
    struct memory_info mem;

    mem_info(&mem);
    sprintf(data->data[0].name, "mem total");
    sprintf(data->data[0].unit, "%c", mem.total_unit);
    data->data[0].t = M_UINT32;
    sprintf(data->data[1].name, "mem used");
    sprintf(data->data[1].unit, "%c", mem.used_unit);
    data->data[1].t = M_UINT32;
    sprintf(data->data[2].name, "mem free");
    sprintf(data->data[2].unit, "%c", mem.free_unit);
    data->data[2].t = M_UINT32;
    sprintf(data->data[3].name, "mem usage");
    sprintf(data->data[3].unit, "%%");
    data->data[3].t = M_FLOAT;

    data->data[0].val.uint32 = mem.total;
    data->data[1].val.uint32 = mem.used;
    data->data[2].val.uint32 = mem.free;
    data->data[3].val.f = (100.0 * mem.used)/mem.total;

    return 0;
}

collect_item_t items[] = {
    {
        .item_name = "memory statistics",
        .item_desc = "memory total used free...",
        .run_once = false,
        .collect_data_func = memory_information,
        .interval = 1,
        .data_count = 4,
    },
};

plugin_info_t pluginfo = {
    .name = "memory",
    .desc = "memory information",
    .item_count = 1,
};




PLUGIN_ENTRY(memory, plugin_info)
{
    PLUGIN_INIT(plugin_info, items, &pluginfo);
    return 0;
}

PLUGIN_EXIT(memory, plugin_info)
{
    PLUGIN_FREE(plugin_info);
    return;
}


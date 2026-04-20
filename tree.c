#include "tree.h"
#include "pes.h"
#include "index.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int object_write(ObjectType type, const void *data, size_t len, ObjectID *id_out);

int tree_from_index(ObjectID *id_out) {
    Index idx;
    if (index_load(&idx) != 0) return -1;

    // Calculate buffer size needed
    size_t buffer_size = (idx.count == 0) ? 1 : idx.count * 256;
    char *buffer = malloc(buffer_size);
    if (!buffer) return -1;

    size_t offset = 0;
    for (int i = 0; i < idx.count; i++) {
        char hex[HASH_HEX_SIZE + 1];
        hash_to_hex(&idx.entries[i].hash, hex);
        offset += sprintf(buffer + offset, "%o %s %s\n",
                          idx.entries[i].mode, hex, idx.entries[i].path);
    }

    int result = object_write(OBJ_TREE, buffer, offset, id_out);
    free(buffer);
    return result;
}

#include "tree.h"
#include "pes.h"
#include "index.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int object_write(ObjectType type, const void *data, size_t len, ObjectID *id_out);

// Helper function to sort tree entries alphabetically by name
int compare_entries(const void *a, const void *b) {
    const TreeEntry *ea = (const TreeEntry *)a;
    const TreeEntry *eb = (const TreeEntry *)b;
    return strcmp(ea->name, eb->name);
}

int tree_serialize(const Tree *tree, void **data_out, size_t *len_out) {
    if (tree->count == 0) {
        *data_out = malloc(1);
        *len_out = 0;
        return 0;
    }

    // We must create a temporary copy of the entries to sort them 
    // because the 'tree' parameter is marked as const!
    TreeEntry *sorted_entries = malloc(tree->count * sizeof(TreeEntry));
    if (!sorted_entries) return -1;
    memcpy(sorted_entries, tree->entries, tree->count * sizeof(TreeEntry));

    // Sort the entries deterministically as required by the test
    qsort(sorted_entries, tree->count, sizeof(TreeEntry), compare_entries);

    size_t size = tree->count * 512;
    char *buffer = malloc(size);
    if (!buffer) {
        free(sorted_entries);
        return -1;
    }

    size_t offset = 0;
    for (int i = 0; i < tree->count; i++) {
        char hex[HASH_HEX_SIZE + 1];
        hash_to_hex(&sorted_entries[i].hash, hex);
        offset += sprintf(buffer + offset, "%o %s %s\n", 
                          sorted_entries[i].mode, hex, sorted_entries[i].name);
    }

    *data_out = buffer;
    *len_out = offset;
    free(sorted_entries);
    return 0;
}

int tree_parse(const void *data, size_t len, Tree *tree_out) {
    tree_out->count = 0;
    if (len == 0 || !data) return 0;
    
    char *ptr = (char *)data;
    char *end = ptr + len;
    
    while (ptr < end) {
        char hex[HASH_HEX_SIZE + 1];
        TreeEntry *e = &tree_out->entries[tree_out->count];
        if (sscanf(ptr, "%o %64s %s", &e->mode, hex, e->name) == 3) {
            hex_to_hash(hex, &e->hash);
            tree_out->count++;
        }
        char *nl = strchr(ptr, '\n');
        if (!nl) break;
        ptr = nl + 1;
    }
    return 0;
}

int tree_from_index(ObjectID *id_out) {
    Index idx;
    if (index_load(&idx) != 0) return -1;

    Tree tree;
    tree.count = idx.count;
    for (int i = 0; i < idx.count; i++) {
        tree.entries[i].mode = idx.entries[i].mode;
        tree.entries[i].hash = idx.entries[i].hash;
        strncpy(tree.entries[i].name, idx.entries[i].path, sizeof(tree.entries[i].name) - 1);
        tree.entries[i].name[sizeof(tree.entries[i].name) - 1] = '\0';
    }

    void *data;
    size_t len;
    // tree_serialize will handle the deterministic sorting for us now!
    if (tree_serialize(&tree, &data, &len) != 0) return -1;

    int result = object_write(OBJ_TREE, data, len, id_out);
    free(data);
    return result;
}

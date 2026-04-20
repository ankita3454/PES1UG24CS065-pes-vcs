#include "commit.h"
#include "tree.h"
#include "pes.h"
#include "index.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

int object_write(ObjectType type, const void *data, size_t len, ObjectID *id_out);
int object_read(const ObjectID *id, ObjectType *type_out, void **data_out, size_t *len_out);

int commit_create(const char *message, ObjectID *id_out) {
    ObjectID tree_id;
    if (tree_from_index(&tree_id) != 0) return -1;

    ObjectID parent_id;
    int has_parent = 0;
    FILE *f_head = fopen(HEAD_FILE, "r");
    if (f_head) {
        char ref_path[256];
        if (fscanf(f_head, "ref: %255s", ref_path) == 1) {
            char full_ref_path[512];
            snprintf(full_ref_path, sizeof(full_ref_path), ".pes/%s", ref_path);
            
            FILE *f_ref = fopen(full_ref_path, "r");
            if (f_ref) {
                char hex[HASH_HEX_SIZE + 1];
                if (fscanf(f_ref, "%64s", hex) == 1 && hex_to_hash(hex, &parent_id) == 0) {
                    has_parent = 1;
                }
                fclose(f_ref);
            }
        }
        fclose(f_head);
    }

    char tree_hex[HASH_HEX_SIZE + 1], parent_hex[HASH_HEX_SIZE + 1];
    hash_to_hex(&tree_id, tree_hex);

    char *buffer = malloc(8192);
    if (!buffer) return -1;

    int len;
    time_t now = time(NULL);
    const char *author = "PES User";

    if (has_parent) {
        hash_to_hex(&parent_id, parent_hex);
        len = sprintf(buffer, "tree %s\nparent %s\nauthor %s\ntimestamp %lu\n\n%s\n",
                      tree_hex, parent_hex, author, (unsigned long)now, message);
    } else {
        len = sprintf(buffer, "tree %s\nauthor %s\ntimestamp %lu\n\n%s\n",
                      tree_hex, author, (unsigned long)now, message);
    }

    if (object_write(OBJ_COMMIT, buffer, len, id_out) != 0) {
        free(buffer); return -1;
    }

    FILE *f_head_read = fopen(HEAD_FILE, "r");
    if (f_head_read) {
        char ref_path[256];
        if (fscanf(f_head_read, "ref: %255s", ref_path) == 1) {
            char full_ref_path[512], commit_hex[HASH_HEX_SIZE + 1];
            snprintf(full_ref_path, sizeof(full_ref_path), ".pes/%s", ref_path);
            hash_to_hex(id_out, commit_hex);
            
            FILE *f_ref_write = fopen(full_ref_path, "w");
            if (f_ref_write) {
                fprintf(f_ref_write, "%s\n", commit_hex);
                fclose(f_ref_write);
            }
        }
        fclose(f_head_read);
    }

    free(buffer);
    return 0;
}

int commit_walk(void (*cb)(const ObjectID *id, const Commit *commit, void *ctx), void *ctx) {
    ObjectID current_id;
    int has_current = 0;
    FILE *f_head = fopen(HEAD_FILE, "r");
    if (!f_head) return -1;

    char ref_path[256];
    if (fscanf(f_head, "ref: %255s", ref_path) == 1) {
        char full_ref[512];
        snprintf(full_ref, sizeof(full_ref), ".pes/%s", ref_path);
        FILE *f_ref = fopen(full_ref, "r");
        if (f_ref) {
            char hex[HASH_HEX_SIZE + 1];
            if (fscanf(f_ref, "%64s", hex) == 1) {
                hex_to_hash(hex, &current_id);
                has_current = 1;
            }
            fclose(f_ref);
        }
    }
    fclose(f_head);

    if (!has_current) return -1;

    while (has_current) {
        ObjectType type;
        void *data;
        size_t len;

        if (object_read(&current_id, &type, &data, &len) != 0) break;
        if (type != OBJ_COMMIT) { free(data); break; }

        Commit commit;
        memset(&commit, 0, sizeof(Commit));
        char tree_hex[65] = {0}, parent_hex[65] = {0};
        char *ptr = (char *)data;
        
        if (sscanf(ptr, "tree %64s", tree_hex) == 1) hex_to_hash(tree_hex, &commit.tree);
        
        char *parent_ptr = strstr(ptr, "parent ");
        if (parent_ptr && sscanf(parent_ptr, "parent %64s", parent_hex) == 1) {
            hex_to_hash(parent_hex, &commit.parent);
            commit.has_parent = 1;
        }

        char *msg_ptr = strstr(ptr, "\n\n");
        if (msg_ptr) strncpy(commit.message, msg_ptr + 2, sizeof(commit.message) - 1);

        cb(&current_id, &commit, ctx);

        if (commit.has_parent) current_id = commit.parent;
        else has_current = 0;
        
        free(data);
    }
    return 0;
}

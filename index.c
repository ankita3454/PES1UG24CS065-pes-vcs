#include "index.h"
#include "pes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>

int object_write(ObjectType type, const void *data, size_t len, ObjectID *id_out);

IndexEntry* index_find(Index *index, const char *path) {
    if (!index || !path) return NULL;
    for (int i = 0; i < index->count; i++) {
        if (strcmp(index->entries[i].path, path) == 0) return &index->entries[i];
    }
    return NULL;
}

int index_status(const Index *index) {
    if (!index) return -1;

    printf("Staged changes:\n");
    for (int i = 0; i < index->count; i++) printf("  staged:     %s\n", index->entries[i].path);
    if (index->count == 0) printf("  (nothing to show)\n");
    printf("\n");

    printf("Unstaged changes:\n");
    int unstaged = 0;
    for (int i = 0; i < index->count; i++) {
        struct stat st;
        if (stat(index->entries[i].path, &st) != 0) {
            printf("  deleted:    %s\n", index->entries[i].path);
            unstaged++;
        } else if ((uint64_t)st.st_mtime != index->entries[i].mtime_sec || (uint32_t)st.st_size != index->entries[i].size) {
            printf("  modified:   %s\n", index->entries[i].path);
            unstaged++;
        }
    }
    if (unstaged == 0) printf("  (nothing to show)\n");
    printf("\n");
    
    printf("Untracked files:\n");
    DIR *dir = opendir(".");
    if (dir) {
        struct dirent *ent;
        int untracked = 0;
        while ((ent = readdir(dir)) != NULL) {
            if (ent->d_name[0] == '.') continue;
            if (strcmp(ent->d_name, "pes") == 0) continue;
            
            if (!index_find((Index*)index, ent->d_name)) {
                struct stat st;
                if (stat(ent->d_name, &st) == 0 && S_ISREG(st.st_mode)) {
                    printf("  untracked:  %s\n", ent->d_name);
                    untracked++;
                }
            }
        }
        closedir(dir);
        if (untracked == 0) printf("  (nothing to show)\n");
    }
    printf("\n");
    return 0;
}

int index_load(Index *index) {
    if (!index) return -1;
    memset(index, 0, sizeof(Index));
    FILE *f = fopen(INDEX_FILE, "r");
    if (!f) return 0; 
    char hex[HASH_HEX_SIZE + 1];
    char line[1024];
    while (index->count < MAX_INDEX_ENTRIES && fgets(line, sizeof(line), f)) {
        IndexEntry *e = &index->entries[index->count];
        if (sscanf(line, "%o %64s %lu %u %[^\n]", &e->mode, hex, &e->mtime_sec, &e->size, e->path) == 5) {
            hex_to_hash(hex, &e->hash);
            index->count++;
        }
    }
    fclose(f);
    return 0;
}

int index_save(const Index *index) {
    if (!index) return -1;
    FILE *f = fopen(INDEX_FILE, "w");
    if (!f) return -1;
    for (int i = 0; i < index->count; i++) {
        char hex[HASH_HEX_SIZE + 1];
        hash_to_hex(&index->entries[i].hash, hex);
        fprintf(f, "%o %s %lu %u %s\n", index->entries[i].mode, hex, (unsigned long)index->entries[i].mtime_sec, index->entries[i].size, index->entries[i].path);
    }
    fclose(f);
    return 0;
}

int index_add(Index *index, const char *path) {
    struct stat st;
    if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) return -1;

    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    
    void *data = malloc(st.st_size > 0 ? st.st_size : 1);
    if (st.st_size > 0) {
        if (fread(data, 1, st.st_size, f) != (size_t)st.st_size) {
            free(data); fclose(f); return -1;
        }
    }
    fclose(f);

    ObjectID id;
    if (object_write(OBJ_BLOB, data, st.st_size, &id) != 0) { free(data); return -1; }
    free(data);

    IndexEntry *e = index_find(index, path);
    if (!e) {
        if (index->count >= MAX_INDEX_ENTRIES) return -1;
        e = &index->entries[index->count++];
        strncpy(e->path, path, sizeof(e->path)-1);
        e->path[sizeof(e->path)-1] = '\0';
    }

    e->mode = (st.st_mode & S_IXUSR) ? 0100755 : 0100644;
    e->hash = id;
    e->mtime_sec = (uint64_t)st.st_mtime;
    e->size = (uint32_t)st.st_size;
    return index_save(index);
}

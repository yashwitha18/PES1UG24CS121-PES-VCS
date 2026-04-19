// object.c — Content-addressable object store

#include "pes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <openssl/sha.h>

// ─── PROVIDED ────────────────────────────────────────────────────────────────

void hash_to_hex(const ObjectID *id, char *hex_out) {
    for (int i = 0; i < HASH_SIZE; i++) {
        sprintf(hex_out + i * 2, "%02x", id->hash[i]);
    }
    hex_out[HASH_HEX_SIZE] = '\0';
}

int hex_to_hash(const char *hex, ObjectID *id_out) {
    if (strlen(hex) < HASH_HEX_SIZE) return -1;
    for (int i = 0; i < HASH_SIZE; i++) {
        unsigned int byte;
        if (sscanf(hex + i * 2, "%2x", &byte) != 1) return -1;
        id_out->hash[i] = (uint8_t)byte;
    }
    return 0;
}

void compute_hash(const void *data, size_t len, ObjectID *id_out) {
    SHA256_CTX ctx;
    SHA256_Init(&ctx);
    SHA256_Update(&ctx, data, len);
    SHA256_Final(id_out->hash, &ctx);
}

void object_path(const ObjectID *id, char *path_out, size_t path_size) {
    char hex[HASH_HEX_SIZE + 1];
    hash_to_hex(id, hex);
    snprintf(path_out, path_size, "%s/%.2s/%s", OBJECTS_DIR, hex, hex + 2);
}

int object_exists(const ObjectID *id) {
    char path[512];
    object_path(id, path, sizeof(path));
    return access(path, F_OK) == 0;
}

// ─── TODO: Implemented ───────────────────────────────────────────────────────

int object_write(ObjectType type, const void *data, size_t len, ObjectID *id_out) {
    // Step 1: Build the header string like "blob 16\0"
    const char *type_str;
    if (type == OBJ_BLOB)        type_str = "blob";
    else if (type == OBJ_TREE)   type_str = "tree";
    else                         type_str = "commit";

    char header[64];
    int header_len = snprintf(header, sizeof(header), "%s %zu", type_str, len) + 1;
    // +1 to include the '\0' at the end of the header

    // Step 2: Combine header + data into one big buffer
    size_t total_len = header_len + len;
    uint8_t *full = malloc(total_len);
    if (!full) return -1;
    memcpy(full, header, header_len);
    memcpy(full + header_len, data, len);

    // Step 3: Compute hash of the full object
    compute_hash(full, total_len, id_out);

    // Step 4: Check if it already exists (deduplication)
    if (object_exists(id_out)) {
        free(full);
        return 0;
    }

    // Step 5: Create shard directory like .pes/objects/2f/
    char hex[HASH_HEX_SIZE + 1];
    hash_to_hex(id_out, hex);

    char shard_dir[256];
    snprintf(shard_dir, sizeof(shard_dir), "%s/%.2s", OBJECTS_DIR, hex);
    mkdir(shard_dir, 0755); // OK if it already exists

    // Step 6: Write to a temp file first
    char tmp_path[512];
    snprintf(tmp_path, sizeof(tmp_path), "%s/%.2s/tmp_XXXXXX", OBJECTS_DIR, hex);
    int fd = mkstemp(tmp_path);
    if (fd < 0) { free(full); return -1; }

    if (write(fd, full, total_len) != (ssize_t)total_len) {
        close(fd); free(full); return -1;
    }

    // Step 7: fsync to make sure data is on disk
    fsync(fd);
    close(fd);
    free(full);

    // Step 8: Rename temp file to final path (atomic)
    char final_path[512];
    object_path(id_out, final_path, sizeof(final_path));
    if (rename(tmp_path, final_path) != 0) return -1;

    // Step 9: fsync the shard directory
    int dir_fd = open(shard_dir, O_RDONLY);
    if (dir_fd >= 0) { fsync(dir_fd); close(dir_fd); }

    return 0;
}

int object_read(const ObjectID *id, ObjectType *type_out, void **data_out, size_t *len_out) {
    // Step 1: Get the file path
    char path[512];
    object_path(id, path, sizeof(path));

    // Step 2: Open and read the whole file
    FILE *f = fopen(path, "rb");
    if (!f) return -1;

    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    uint8_t *buf = malloc(file_size);
    if (!buf) { fclose(f); return -1; }
    if ((long)fread(buf, 1, file_size, f) != file_size) {
        free(buf); fclose(f); return -1;
    }
    fclose(f);

    // Step 3: Verify integrity — recompute hash and compare
    ObjectID computed;
    compute_hash(buf, file_size, &computed);
    if (memcmp(computed.hash, id->hash, HASH_SIZE) != 0) {
        free(buf); return -1; // Corrupted!
    }

    // Step 4: Parse the header — find the '\0' separator
    uint8_t *null_pos = memchr(buf, '\0', file_size);
    if (!null_pos) { free(buf); return -1; }

    // Step 5: Figure out the type from header text
    if      (strncmp((char*)buf, "blob",   4) == 0) *type_out = OBJ_BLOB;
    else if (strncmp((char*)buf, "tree",   4) == 0) *type_out = OBJ_TREE;
    else if (strncmp((char*)buf, "commit", 6) == 0) *type_out = OBJ_COMMIT;
    else { free(buf); return -1; }

    // Step 6: Extract the data portion (everything after the '\0')
    uint8_t *data_start = null_pos + 1;
    *len_out = file_size - (data_start - buf);
    *data_out = malloc(*len_out);
    if (!*data_out) { free(buf); return -1; }
    memcpy(*data_out, data_start, *len_out);

    free(buf);
    return 0;
}

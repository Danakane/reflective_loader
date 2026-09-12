#include "reflective_loader.h"

#ifndef NDEBUG

int main(int argc, char **argv) {
    if (argc != 2) {
        return -1;
    }

    const char *filename = argv[1];

    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        return -1;
    }

    // Move to end to get file size
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    rewind(fp);

    if (size < 0) {
        fclose(fp);
        return -1;
    }

    char *payload = malloc(size);
    if (!payload) {
        fclose(fp);
        return -1;
    }

    size_t read_size = fread(payload, 1, size, fp);
    fclose(fp);

    if (read_size != (size_t)size) {
        free(payload);
        return -1;
    }

    reflective_load(payload, "root", NULL, 1);

    free(payload);

    return 0;
}

#endif

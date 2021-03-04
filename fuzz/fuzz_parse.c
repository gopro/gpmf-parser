#include <stdio.h>
#include <unistd.h>

#include "../GPMF_parser.h"
#include "GPMF_mp4reader.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    char filename[256];
    sprintf(filename, "/tmp/libfuzzer.%d", getpid());

    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        return 0;
    }
    fwrite(data, size, 1, fp);
    fclose(fp);

    OpenMP4Source(filename, MOV_GPMF_TRAK_TYPE, MOV_GPMF_TRAK_SUBTYPE);

    unlink(filename);
    return 0;
}

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc < 4) {
        printf("Usage: trim_output <input_file> <output_file> <start_marker> [end_marker]\n");
        return 1;
    }
    
    FILE *in = fopen(argv[1], "rb");
    if (!in) {
        printf("Failed to open %s\n", argv[1]);
        return 1;
    }
    
    FILE *out = fopen(argv[2], "wb");
    if (!out) {
        printf("Failed to open %s\n", argv[2]);
        fclose(in);
        return 1;
    }
    
    char *start_marker = argv[3];
    char *end_marker = argc > 4 ? argv[4] : NULL;
    
    char line[4096];
    int writing = 0;
    
    while (fgets(line, sizeof(line), in)) {
        if (!writing) {
            if (strstr(line, start_marker)) {
                writing = 1;
                fputs(line, out);
            }
        } else {
            if (end_marker && strstr(line, end_marker)) {
                break;
            }
            fputs(line, out);
        }
    }
    
    fclose(in);
    fclose(out);
    return 0;
}

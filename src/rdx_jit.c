typedef unsigned long long size_t;
typedef void* HANDLE;
#define NULL ((void*)0)

#define PAGE_EXECUTE_READWRITE 0x40
#define MEM_COMMIT  0x1000
#define MEM_RESERVE 0x2000
#define MEM_RELEASE 0x8000
#define STD_OUTPUT_HANDLE ((unsigned int)-11)

void* __stdcall VirtualAlloc(void* lpAddress, size_t dwSize, unsigned int flAllocationType, unsigned int flProtect);
int __stdcall VirtualFree(void* lpAddress, size_t dwSize, unsigned int dwFreeType);
HANDLE __stdcall GetStdHandle(unsigned int nStdHandle);
int __stdcall WriteFile(HANDLE hFile, const void* lpBuffer, unsigned int nNumberOfBytesToWrite, unsigned int* lpNumberOfBytesWritten, void* lpOverlapped);
void __stdcall ExitProcess(unsigned int uExitCode);

int my_strlen(const char* s) {
    int i = 0;
    while(s[i]) i++;
    return i;
}

int my_strncmp(const char* s1, const char* s2, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (s1[i] != s2[i]) return s1[i] - s2[i];
        if (s1[i] == '\0') return 0;
    }
    return 0;
}

void int_to_str(int val, char* buf, int* len) {
    if (val == 0) { buf[0] = '0'; buf[1] = '\0'; *len = 1; return; }
    char temp[32];
    int i = 0;
    while (val > 0) {
        temp[i++] = (val % 10) + '0';
        val /= 10;
    }
    for (int j = 0; j < i; j++) {
        buf[j] = temp[i - 1 - j];
    }
    buf[i] = '\0';
    *len = i;
}

char* find_str(char* s, const char* f) {
    int flen = my_strlen(f);
    while (*s) {
        if (my_strncmp(s, f, flen) == 0) return s;
        s++;
    }
    return NULL;
}

void print_msg(const char* msg) {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    unsigned int written;
    WriteFile(hOut, msg, my_strlen(msg), &written, NULL);
}

int mainCRTStartup() {
    print_msg("Starting JIT compiler...\n");
    
    const char* source = 
        "sys.rdx{\n"
        "// start of program\n"
        "num = 25\n"
        "chr = \"My name\"\n"
        "blo = \"1.2\"\n"
        "call(\"answer : \" [num,chr,blo]);\n"
        "}\n";

    char* p = (char*)source;
    
    print_msg("Parsing source...\n");
    
    // Extract num
    char* num_ptr = find_str(p, "num = ");
    int num_val = 0;
    if (num_ptr) {
        num_ptr += 6;
        while (*num_ptr >= '0' && *num_ptr <= '9') {
            num_val = num_val * 10 + (*num_ptr - '0');
            num_ptr++;
        }
    }
    
    // Extract chr
    char* chr_ptr = find_str(p, "chr = \"");
    char chr_val[64] = {0};
    if (chr_ptr) {
        chr_ptr += 7;
        int i = 0;
        while (*chr_ptr && *chr_ptr != '"') {
            chr_val[i++] = *chr_ptr++;
        }
    }
    
    // Extract blo
    char* blo_ptr = find_str(p, "blo = \"");
    char blo_val[64] = {0};
    if (blo_ptr) {
        blo_ptr += 7;
        int i = 0;
        while (*blo_ptr && *blo_ptr != '"') {
            blo_val[i++] = *blo_ptr++;
        }
    }
    
    // Extract call base string
    char* call_ptr = find_str(p, "call(\"");
    char call_val[64] = {0};
    if (call_ptr) {
        call_ptr += 6;
        int i = 0;
        while (*call_ptr && *call_ptr != '"') {
            call_val[i++] = *call_ptr++;
        }
    }
    
    print_msg("Building final string...\n");
    // Build final string
    char final_string[256];
    int fpos = 0;
    
    for (int i = 0; call_val[i]; i++) final_string[fpos++] = call_val[i];
    
    char num_str[32];
    int num_len;
    int_to_str(num_val, num_str, &num_len);
    for (int i = 0; i < num_len; i++) final_string[fpos++] = num_str[i];
    
    for (int i = 0; chr_val[i]; i++) final_string[fpos++] = chr_val[i];
    for (int i = 0; blo_val[i]; i++) final_string[fpos++] = blo_val[i];
    
    final_string[fpos++] = '\n'; 
    final_string[fpos] = '\0';
    
    print_msg("Final string: ");
    print_msg(final_string);
    
    print_msg("Allocating JIT buffer...\n");
    // Allocate JIT buffer
    unsigned char* jit_mem = (unsigned char*)VirtualAlloc(NULL, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    
    if (!jit_mem) {
        print_msg("VirtualAlloc failed.\n");
        ExitProcess(1);
    }

    print_msg("Emitting opcodes...\n");
    int pos = 0;
    
    // sub rsp, 40h
    jit_mem[pos++] = 0x48; jit_mem[pos++] = 0x83; jit_mem[pos++] = 0xEC; jit_mem[pos++] = 0x40;
    
    // mov ecx, -11
    jit_mem[pos++] = 0xB9; jit_mem[pos++] = 0xF5; jit_mem[pos++] = 0xFF; jit_mem[pos++] = 0xFF; jit_mem[pos++] = 0xFF;
    
    // mov rax, GetStdHandle
    jit_mem[pos++] = 0x48; jit_mem[pos++] = 0xB8;
    void* pGetStdHandle = (void*)GetStdHandle;
    for (int i=0; i<8; i++) { jit_mem[pos++] = ((unsigned char*)&pGetStdHandle)[i]; }
    
    // call rax
    jit_mem[pos++] = 0xFF; jit_mem[pos++] = 0xD0;
    
    // mov rcx, rax
    jit_mem[pos++] = 0x48; jit_mem[pos++] = 0x89; jit_mem[pos++] = 0xC1;
    
    // lea rdx, [rip + 37]
    jit_mem[pos++] = 0x48; jit_mem[pos++] = 0x8D; jit_mem[pos++] = 0x15;
    jit_mem[pos++] = 37; jit_mem[pos++] = 0; jit_mem[pos++] = 0; jit_mem[pos++] = 0;
    
    // mov r8d, string_len
    jit_mem[pos++] = 0x41; jit_mem[pos++] = 0xB8;
    int str_len = my_strlen(final_string);
    for (int i=0; i<4; i++) { jit_mem[pos++] = ((unsigned char*)&str_len)[i]; }
    
    // lea r9, [rsp + 30h]
    jit_mem[pos++] = 0x4C; jit_mem[pos++] = 0x8D; jit_mem[pos++] = 0x4C; jit_mem[pos++] = 0x24; jit_mem[pos++] = 0x30;
    
    // mov qword ptr [rsp + 20h], 0
    jit_mem[pos++] = 0x48; jit_mem[pos++] = 0xC7; jit_mem[pos++] = 0x44; jit_mem[pos++] = 0x24; jit_mem[pos++] = 0x20;
    jit_mem[pos++] = 0; jit_mem[pos++] = 0; jit_mem[pos++] = 0; jit_mem[pos++] = 0;
    
    // mov rax, WriteFile
    jit_mem[pos++] = 0x48; jit_mem[pos++] = 0xB8;
    void* pWriteFile = (void*)WriteFile;
    for (int i=0; i<8; i++) { jit_mem[pos++] = ((unsigned char*)&pWriteFile)[i]; }
    
    // call rax
    jit_mem[pos++] = 0xFF; jit_mem[pos++] = 0xD0;
    
    // add rsp, 40h
    jit_mem[pos++] = 0x48; jit_mem[pos++] = 0x83; jit_mem[pos++] = 0xC4; jit_mem[pos++] = 0x40;
    
    // ret
    jit_mem[pos++] = 0xC3;
    
    print_msg("Appending string...\n");
    // Append string
    for (int i=0; i<str_len; i++) {
        jit_mem[pos++] = final_string[i];
    }
    
    print_msg("Executing JIT buffer...\n");
    // Execute JIT buffer
    void (*run_rdx)(void) = (void (*)(void))jit_mem;
    run_rdx();
    
    print_msg("Done! Freeing memory...\n");
    VirtualFree(jit_mem, 0, MEM_RELEASE);
    ExitProcess(0);
    return 0;
}

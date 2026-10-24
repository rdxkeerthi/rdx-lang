/*
 * ============================================================================
 *  main.c  —  RDX Forensics & Cyber Investigation Language (Standalone Engine)
 * ============================================================================
 *
 *  A syntax-free, human-first standalone programming language designed
 *  for Cybersecurity Investigation, Threat Hunting, Incident Response,
 *  and Network Forensics.
 * ============================================================================
 */

#include "rdx_engine.h"
#include "rdx_cyber.h"
#include "rdx_diag.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RDX_VERSION "v3.0.0"

static void print_banner(void) {
    printf("\033[1;36m");
    printf("  ____  ______   __  ______                         _           \n");
    printf(" |  _ \\|  _ \\ \\ / / |  ____|                       (_)          \n");
    printf(" | |_) | | | \\ V /  | |__ ___  _ __ ___ _ __  ___ _  ___ ___   \n");
    printf(" |  _ <| | | |> <   |  __/ _ \\| '__/ _ \\ '_ \\/ __| |/ __/ __|  \n");
    printf(" | |_) | |_| / . \\  | | | (_) | | |  __/ | | \\__ \\ | (__\\__ \\  \n");
    printf(" |____/|____/_/ \\_\\ |_|  \\___/|_|  \\___|_| |_|___/_|\\___|___/  \n");
    printf("\033[0m");
    printf("  RDX Forensics & Cyber Investigation Language — %s\n\n", RDX_VERSION);
}

static void print_help(void) {
    print_banner();
    printf("Usage:\n");
    printf("  rdx <file.rdx>            Execute an investigation or forensic script\n");
    printf("  rdx run <file.rdx>        Execute an investigation script\n");
    printf("  rdx repl                  Launch the interactive cyber investigation shell\n");
    printf("  rdx --version, -v         Display RDX version and capabilities\n");
    printf("  rdx --help, -h            Show this guide and language documentation\n\n");

    printf("Why RDX?\n");
    printf("  Solves the critical problem in DFIR and SOC triage where analysts are\n");
    printf("  slowed down by verbose Python boilerplate or brittle bash pipelines.\n");
    printf("  RDX provides:\n");
    printf("    * Syntax-Free & Easy: No boilerplate 'main()', optional semicolons.\n");
    printf("    * Human-First Diagnostics: Explains WHERE the error is, WHY it happened,\n");
    printf("      and HOW to FIX it with suggestions.\n");
    printf("    * Native Cyber Primitives: IP/CIDR math, MD5/SHA256, Attack Detection,\n");
    printf("      Network Port Assessment, Incident Alerts & Timeline natively built-in.\n");
    printf("    * Standalone: Single binary, zero external runtime dependencies.\n\n");

    printf("Forensic Modules:\n");
    printf("  ip.is_valid(ip)           Check IPv4 format\n");
    printf("  ip.is_private(ip)         Check RFC1918 private IP\n");
    printf("  ip.is_public(ip)          Check public routable IP\n");
    printf("  ip.in_subnet(ip, cidr)    Match IP in CIDR (e.g. '192.168.1.0/24')\n");
    printf("  ip.octets(ip)             Extract octet list [192, 168, 1, 1]\n");
    printf("  hash.md5(str)             Calculate MD5 hash\n");
    printf("  hash.sha1(str)            Calculate SHA-1 hash\n");
    printf("  hash.sha256(str)          Calculate SHA-256 hash\n");
    printf("  hash.file_sha256(path)    Calculate SHA-256 of file on disk\n");
    printf("  hash.match(h1, h2)        Case-insensitive hash comparison\n");
    printf("  net.port_name(port)       Lookup port service (22->SSH, 443->HTTPS)\n");
    printf("  net.port_risk(port)       Assess risk surface (e.g. 3389->CRITICAL)\n");
    printf("  log.extract_ip(log)       Extract IPv4 address from log line\n");
    printf("  log.extract_url(log)      Extract URL from log line\n");
    printf("  log.extract_cve(log)      Extract CVE number (e.g. CVE-2024-38077)\n");
    printf("  log.detect_attack(log)    Detect SQLi, XSS, Path Traversal, RCE\n");
    printf("  alert(level, msg, detail) Structured security alert with timestamp\n");
    printf("  timeline(event, detail)   Structured chronological event log\n\n");
}

static char *read_entire_file(const char *path, size_t *out_len) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size < 0) { fclose(f); return NULL; }

    char *buf = (char*)malloc(size + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t read_bytes = fread(buf, 1, size, f);
    buf[read_bytes] = '\0';
    fclose(f);
    if (out_len) *out_len = read_bytes;
    return buf;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        rdx_engine_repl();
        return 0;
    }

    if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "help") == 0) {
        print_help();
        return 0;
    }

    if (strcmp(argv[1], "-v") == 0 || strcmp(argv[1], "--version") == 0) {
        printf("RDX Language %s [Standalone Native Runtime]\n", RDX_VERSION);
        return 0;
    }

    if (strcmp(argv[1], "repl") == 0) {
        rdx_engine_repl();
        return 0;
    }

    const char *filepath = argv[1];
    if (strcmp(argv[1], "run") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Error: 'rdx run' requires a script file argument.\n");
            fprintf(stderr, "Usage: rdx run <investigation.rdx>\n");
            return 1;
        }
        filepath = argv[2];
    }

    size_t src_len = 0;
    char *source = read_entire_file(filepath, &src_len);
    if (!source) {
        fprintf(stderr, "\n\033[1;31m[RDX FILE ERROR]\033[0m Unable to open script '%s'.\n", filepath);
        fprintf(stderr, "Please verify the path and file permissions.\n\n");
        return 1;
    }
    fflush(stdout);
    int ret = rdx_engine_run(filepath, source);
    fflush(stdout);
    free(source);
    return ret;
}

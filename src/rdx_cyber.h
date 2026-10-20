/*
 * ============================================================================
 *  rdx_cyber.h  —  RDX Cyber Forensics & Incident Response Primitives
 * ============================================================================
 *
 *  Standalone, zero-dependency cybersecurity engine providing:
 *   - Native IPv4 & CIDR Subnet Bitmask Matching
 *   - Pure C Cryptographic Hashing: MD5, SHA-1, SHA-256 (text & file)
 *   - Network Port Attack-Surface & Vulnerability Classification
 *   - Forensic Log Parsing (IP, URL, CVE extraction)
 *   - Signature-Based Attack Detection (SQLi, XSS, Path Traversal, RCE)
 *   - Structured Incident Alerting & Timeline Auditing
 * ============================================================================
 */

#ifndef RDX_CYBER_H
#define RDX_CYBER_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* ---- IP & CIDR Networking ---- */
bool        rdx_ip_is_valid(const char *ip);
bool        rdx_ip_is_private(const char *ip);
bool        rdx_ip_is_public(const char *ip);
bool        rdx_ip_is_loopback(const char *ip);
bool        rdx_ip_in_subnet(const char *ip, const char *cidr);
bool        rdx_ip_octets(const char *ip, int octets[4]);

/* ---- Cryptographic Hashes (Text & File) ---- */
void        rdx_hash_md5(const char *input, size_t len, char out_hex[33]);
void        rdx_hash_sha1(const char *input, size_t len, char out_hex[41]);
void        rdx_hash_sha256(const char *input, size_t len, char out_hex[65]);
bool        rdx_hash_file_md5(const char *filepath, char out_hex[33]);
bool        rdx_hash_file_sha256(const char *filepath, char out_hex[65]);
bool        rdx_hash_match(const char *h1, const char *h2);

/* ---- Network Port & Surface Assessment ---- */
const char *rdx_net_port_name(int port);
const char *rdx_net_port_risk(int port);

/* ---- Forensic Log & Attack Inspection ---- */
bool        rdx_log_extract_ip(const char *log, char out_ip[64]);
bool        rdx_log_extract_url(const char *log, char out_url[256]);
bool        rdx_log_extract_cve(const char *log, char out_cve[64]);
int         rdx_log_extract_status(const char *log);
const char *rdx_log_detect_attack(const char *log);

/* ---- Incident Alerts & Timelines ---- */
void        rdx_cyber_alert(const char *level, const char *msg, const char *detail);
void        rdx_cyber_timeline(const char *event_name, const char *detail);

#endif /* RDX_CYBER_H */

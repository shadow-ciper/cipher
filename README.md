# cipher
URL Shortener & Unshortener Tool
===========================================

Usage:
 ./cipher2 [option] [url]

Options:
 -s <url> Shorten a long URL using TinyURL API
 -u <url> Unshorten a short URL to reveal its target
 -h Show this help message

Examples:
 ./cipher2 -s https://example.com
 ./cipher2 -u https://tinyurl.com/abc123

Notes:
 * Requires internet connectivity and libcurl.
 * Caller must free() strings returned by -s and -u options.
 * Compile with: gcc -std=c99 -o ./cipher2 ./cipher2.c -lcurl

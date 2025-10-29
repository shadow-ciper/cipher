#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <curl/curl.h>

// Safe strdup replacement for portability
static char *my_strdup(const char *s) {
    size_t len = strlen(s) + 1;
    char *p = malloc(len);
    if (p) memcpy(p, s, len);
    return p;
}

#include <stdio.h> // For printf(), snprintf()
#include <stdlib.h> // For malloc(), free(), realloc()
#include <string.h> // For strcmp(), memcpy(), my_strdup()
#include <stddef.h> // For size_t
#include <curl/curl.h> // For libcurl HTTP operations
// Handle Windows-specific snprintf compatibility
#ifdef _WIN32
#if defined(_MSC_VER) && _MSC_VER < 1900
#define snprintf _snprintf
#endif
#endif
// ============================================================
// STRUCT: Response
// ------------------------------------------------------------
// Used to store data received from HTTP requests.
// libcurl writes data into this struct via a callback.
// The caller is responsible for freeing the data field.
// ============================================================
struct Response {
char *data; // Pointer to dynamically allocated memory buffer
size_t size; // Size (in bytes) of the current data
};
// ============================================================
// CALLBACK FUNCTION: write_callback()
// ------------------------------------------------------------
// libcurl calls this function automatically when it receives
// a block of data from the web server.
// This function appends that data into our Response struct.
// ------------------------------------------------------------
// PARAMETERS:
// contents → Pointer to the received data chunk
// size, nmemb → Used to calculate chunk size
// userp → Pointer to user-defined struct (Response)
// RETURNS:
// Number of bytes handled, or 0 on failure (tells libcurl to stop)
// ============================================================
size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
size_t realsize = size * nmemb; // Actual number of bytes received
struct Response *mem = (struct Response *)userp; // Implicit cast from void is safe
char *ptr = realloc(mem->data, mem->size + realsize + 1);
if (!ptr) { // If memory allocation failed
fprintf(stderr, "Error: Out of memory in write_callback.\n");
return 0; // Returning 0 tells libcurl to stop
}
mem->data = ptr; // Update pointer after realloc
memcpy(&(mem->data[mem->size]), contents, realsize); // Copy new data into buffer
mem->size += realsize; // Update total data size
mem->data[mem->size] = 0; // Null-terminate the buffer
return realsize; // Return the number of bytes handled
}
// ============================================================
// FUNCTION: shorten_url()
// ------------------------------------------------------------
// Sends a long URL to the TinyURL API and retrieves a shortened version.
// ------------------------------------------------------------
// PARAMETER:
// long_url → The full URL (e.g., https://example.com)
// RETURNS:
// Dynamically allocated string containing the shortened URL
// or an error message string. Caller must free the returned string.
// NOTES:
// - Requires internet connectivity and libcurl.
// - URL must not exceed 900 bytes after encoding to avoid truncation.
// ============================================================
char *shorten_url(const char *long_url) {
CURL *curl;
CURLcode res;
struct Response response = { .data = NULL, .size = 0 };
char *encoded_url = NULL;
char api_url[1024];
// Initialize response buffer
response.data = malloc(1);
if (!response.data) {
return my_strdup("Error: Memory allocation failed");
}
response.data[0] = 0;
// Initialize CURL handle
curl = curl_easy_init();
if (!curl) {
free(response.data);
return my_strdup("Error: Could not initialize curl");
}
// URL-encode the input safely using curl handle
encoded_url = curl_easy_escape(curl, long_url, 0);
if (!encoded_url) {
free(response.data);
curl_easy_cleanup(curl);
return my_strdup("Error: URL encoding failed");
}
// Check encoded URL length to prevent truncation
if (strlen(encoded_url) > 900){
free(response.data);
curl_free(encoded_url);
curl_easy_cleanup(curl);
return my_strdup("Error: URL too long for API");
}
// Construct TinyURL API endpoint
snprintf(api_url, sizeof(api_url), "https://tinyurl.com/api-create.php?url=%s", encoded_url);
curl_free(encoded_url); // Free encoded string (no longer needed)
// Configure CURL options
curl_easy_setopt(curl, CURLOPT_URL, api_url);
curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
curl_easy_setopt(curl, CURLOPT_TIMEOUT, 8L); // Timeout for safety
curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L); // Fail faster on no connection
// Execute HTTP request
res = curl_easy_perform(curl);
if (res != CURLE_OK) {
free(response.data);
curl_easy_cleanup(curl);
return my_strdup("Error: Could not shorten URL (network failure)");
}
// Clean up CURL resources
curl_easy_cleanup(curl);
return response.data; // Return final shortened URL or error message
}
// ============================================================
// FUNCTION: unshorten_url()
// ------------------------------------------------------------
// Takes a shortened URL (e.g., https://tinyurl.com/xyz) and follows
// all redirects to find the original destination URL.
// ------------------------------------------------------------
// PARAMETER:
// short_url → The shortened URL
// RETURNS:
// Dynamically allocated string containing the original URL,
// or an error message. Caller must free the returned string.
// NOTES:
// - Requires internet connectivity and libcurl.
// - Uses HEAD requests to minimize data transfer.
// ============================================================
char *unshorten_url(const char *short_url) {
CURL *curl;
CURLcode res;
char *final_url = NULL;
long response_code;
curl = curl_easy_init();
if (!curl) {
return my_strdup("Error: Could not initialize curl");
}
// Configure CURL options
curl_easy_setopt(curl, CURLOPT_URL, short_url);
curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L); // Follow redirects automatically
curl_easy_setopt(curl, CURLOPT_NOBODY, 1L); // HEAD request only (no body)
curl_easy_setopt(curl, CURLOPT_TIMEOUT, 8L); // Timeout limit
curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L); // Connection timeout
// Perform HTTP request
res = curl_easy_perform(curl);
if (res == CURLE_OK) {
// Get final resolved URL (after redirects)
curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &final_url);
curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
// Check response validity and avoid null pointer
if (response_code >= 200 && response_code < 400 && final_url) {
final_url = my_strdup(final_url); // Copy result to heap
} else {
final_url = my_strdup("Error: Invalid or failed redirect response");
}
} else {
final_url = my_strdup("Error: Could not unshorten URL (network issue)");
}
// Cleanup resources
curl_easy_cleanup(curl);
return final_url;
}
// ============================================================
// FUNCTION: show_help()
// ------------------------------------------------------------
// Prints a professional help/usage menu, similar to tools like
// Hydra or Nmap when the "-h" flag is passed.
// ============================================================
void show_help(const char *prog_name) {
printf("\n===========================================\n");
printf(" URL Shortener & Unshortener Tool\n");
printf("===========================================\n\n");
printf("Usage:\n");
printf(" %s [option] [url]\n\n", prog_name);
printf("Options:\n");
printf(" -s <url> Shorten a long URL using TinyURL API\n");
printf(" -u <url> Unshorten a short URL to reveal its target\n");
printf(" -h Show this help message\n\n");
printf("Examples:\n");
printf(" %s -s https://example.com\n", prog_name);
printf(" %s -u https://tinyurl.com/abc123\n\n", prog_name);
printf("Notes:\n");
printf(" * Requires internet connectivity and libcurl.\n");
printf(" * Caller must free() strings returned by -s and -u options.\n");
printf(" * Compile with: gcc -std=c99 -o %s %s.c -lcurl\n\n", prog_name, prog_name);
}
// ============================================================
// FUNCTION: main()
// ------------------------------------------------------------
// Entry point of the program.
// Parses arguments and determines which operation to perform.
// ------------------------------------------------------------
// NOTES:
// - Requires libcurl; link with -lcurl during compilation.
// - Initializes and cleans up global CURL resources.
// ============================================================
int main(int argc, char *argv[]) {
// If no argument provided, show help
if (argc < 2) {
show_help(argv[0]);
return 1;
}
// Initialize libcurl globally (must be done once)
if (curl_global_init(CURL_GLOBAL_ALL) != 0) {
fprintf(stderr, "Error: Failed to initialize libcurl\n");
return 1;
}
// Parse command-line flags
if (strcmp(argv[1], "-h") == 0) {
// Display help message
show_help(argv[0]);
} else if (strcmp(argv[1], "-s") == 0 && argc == 3) {
// Shorten a URL
char *result = shorten_url(argv[2]);
printf("Shortened URL: %s\n", result);
free(result);
} else if (strcmp(argv[1], "-u") == 0 && argc == 3) {
// Unshorten a URL
char *result = unshorten_url(argv[2]);
printf("Original URL: %s\n", result);
free(result);
} else {
// Invalid usage
fprintf(stderr, "Error: Invalid command or missing argument.\n");
show_help(
argv[0]);
}
// Clean up global CURL resources
curl_global_cleanup();
return 0;
}







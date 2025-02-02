#ifndef UTILS_H
#define UTILS_H

#include <cstdlib>
#include <cstdio>
#include <stdint.h>
#include <limits>
#include <cmath>
#include <string>
#include <string_view>
#include <algorithm>
#include <fstream>

#pragma warning(push, 0)
#include <FL/fl_types.h>
#pragma warning(pop)

#if defined(__unix__)
#define __X11__
#endif

#ifdef _WIN32
#define DIR_SEP "\\"
#else
#define DIR_SEP "/"
#endif

#ifdef __APPLE__
#define CONTROL_KEY "\xE2\x8C\x83" // UTF-8 encoding of U+2303 "UP ARROWHEAD"
#define ALT_KEY "\xE2\x8C\xA5" // UTF-8 encoding of U+2325 "OPTION KEY"
#define SHIFT_KEY "\xE2\x87\xA7" // UTF-8 encoding of U+21E7 "UPWARDS WHITE ARROW"

#define COMMAND_KEY_PLUS "\xE2\x8C\x98" // UTF-8 encoding of U+2318 "PLACE OF INTEREST SIGN"
#define ALT_KEY_PLUS ALT_KEY
#define SHIFT_KEY_PLUS SHIFT_KEY
#define COMMAND_SHIFT_KEYS_PLUS SHIFT_KEY_PLUS COMMAND_KEY_PLUS
#define COMMAND_ALT_KEYS_PLUS ALT_KEY_PLUS COMMAND_KEY_PLUS
#else
#define CONTROL_KEY "Ctrl"
#define ALT_KEY "Alt"
#define SHIFT_KEY "Shift"

#define COMMAND_KEY_PLUS CONTROL_KEY "+"
#define ALT_KEY_PLUS ALT_KEY "+"
#define SHIFT_KEY_PLUS SHIFT_KEY "+"
#define COMMAND_SHIFT_KEYS_PLUS COMMAND_KEY_PLUS SHIFT_KEY_PLUS
#define COMMAND_ALT_KEYS_PLUS COMMAND_KEY_PLUS ALT_KEY_PLUS
#endif

#define STRINGIFY(x) _STRINGIFY_HELPER(x)
#define _STRINGIFY_HELPER(x) #x

#ifndef _countof
#define _countof(a) (sizeof(a) / sizeof(a[0]))
#endif

#define RANGE(x) std::begin(x), std::end(x)

#define HI_NYB(n) (uchar)(((n) & 0xF0) >> 4)
#define LO_NYB(n) (uchar)((n) & 0x0F)
#define LE16(n) (uchar)((n) & 0xFFUL), (uchar)(((n) & 0xFF00UL) >> 8)
#define LE32(n) (uchar)((n) & 0xFFUL), (uchar)(((n) & 0xFF00UL) >> 8), (uchar)(((n) & 0xFF0000UL) >> 16), (uchar)(((n) & 0xFF000000UL) >> 24)
#define BE16(n) (uchar)(((n) & 0xFF00UL) >> 8), (uchar)((n) & 0xFFUL)
#define BE32(n) (uchar)(((n) & 0xFF000000UL) >> 24), (uchar)(((n) & 0xFF0000UL) >> 16), (uchar)(((n) & 0xFF00UL) >> 8), (uchar)((n) & 0xFFUL)

typedef uint8_t size8_t;
typedef uint16_t size16_t;
typedef uint32_t size32_t;
typedef uint64_t size64_t;

bool starts_with_ignore_case(std::string_view s, std::string_view p);
bool ends_with_ignore_case(std::string_view s, std::string_view p);
void add_dot_ext(const char *f, const char *ext, char *s);
int text_width(const char *l, int pad = 0);
bool file_exists(const char *f);
size_t file_size(const char *f);
size_t file_size(FILE *f);
void open_ifstream(std::ifstream &ifs, const char *f);
bool check_read(FILE *file, uchar *expected, size_t n);
uint16_t read_uint16(FILE *file);
size_t read_rmp_size(FILE *file);

#endif

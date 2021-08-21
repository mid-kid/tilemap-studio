#include <cstdio>
#include <cstring>
#include <vector>

#pragma warning(push, 0)
#include <FL/fl_utf8.h>
#pragma warning(pop)

#include "tilemap.h"
#include "version.h"

static uchar get_number(FILE *file, int &c) {
	bool hex = false;
	int b = c;
	if (b == '0') {
		b = fgetc(file);
		if (b == 'X' || b == 'x') {
			hex = true;
			b = fgetc(file);
		}
	}
	uchar v = 0;
	if (hex) {
		while (isxdigit(b)) {
			v = v * 16 + (uchar)(b - (b < 'A' ? '0' : b < 'a' ? 'A' - 0xA : 'a' - 0xA));
			b = fgetc(file);
		}
	}
	else {
		while (isdigit(b)) {
			v = v * 10 + (uchar)(b - '0');
			b = fgetc(file);
		}
	}
	c = b;
	return v;
}

static bool import_csv_tiles(FILE *file, std::vector<uchar> &bytes) {
	bool state = false;
	for (int c = fgetc(file); c != EOF;) {
		if (state) {
			if (c == ',' || c == '\n' || c == '\r') {
				state = false;
			}
			else if (!isspace(c)) {
				return false;
			}
		}
		else {
			if (isdigit(c)) {
				uchar v = get_number(file, c);
				bytes.push_back(v);
				state = true;
				continue;
			}
			else if (c == ',') {
				bytes.push_back(0);
			}
			else if (!isspace(c)) {
				return false;
			}
		}
		c = fgetc(file);
	}
	return true;
}

static bool import_c_tiles(FILE *file, std::vector<uchar> &bytes) {
	enum class State { PRELUDE, ARRAY, COMMA, MAYBE_OPEN_COMMENT, MAYBE_CLOSE_COMMENT, BLOCK_COMMENT, LINE_COMMENT };
	State state = State::PRELUDE, state_before_comment = State::PRELUDE;
	for (int c = fgetc(file); c != EOF;) {
		switch (state) {
		case State::PRELUDE:
			if (c == '{') {
				state = State::ARRAY;
			}
			else if (c == '/') {
				state_before_comment = state;
				state = State::MAYBE_OPEN_COMMENT;
			}
			break;
		case State::ARRAY:
			if (isdigit(c)) {
				uchar v = get_number(file, c);
				bytes.push_back(v);
				state = State::COMMA;
				continue;
			}
			goto not_number_or_comma_in_array;
		case State::COMMA:
			if (c == ',') {
				state = State::ARRAY;
			}
			else {
not_number_or_comma_in_array:
				if (c == '}') {
					c = EOF;
					continue;
				}
				else if (c == '/') {
					state_before_comment = state;
					state = State::MAYBE_OPEN_COMMENT;
				}
				else if (!isspace(c)) {
					return false;
				}
			}
			break;
		case State::MAYBE_OPEN_COMMENT:
			if (c == '*') {
				state = State::BLOCK_COMMENT;
			}
			else if (c == '/') {
				state = State::LINE_COMMENT;
			}
			else {
				state = state_before_comment;
				continue;
			}
			break;
		case State::MAYBE_CLOSE_COMMENT:
			if (c == '/') {
				state = state_before_comment;
				break;
			}
			state = State::BLOCK_COMMENT;
			[[fallthrough]];
		case State::BLOCK_COMMENT:
			if (c == '*') {
				state = State::MAYBE_CLOSE_COMMENT;
			}
			break;
		case State::LINE_COMMENT:
			if (c == '\n' || c == '\r') {
				state = state_before_comment;
			}
			break;
		}
		c = fgetc(file);
	}
	return true;
}

static Tilemap::Result import_file_bytes(const char *f, std::vector<uchar> &bytes, bool attrmap) {
	FILE *file = fl_fopen(f, "rb");
	if (!file) { return attrmap ? Tilemap::Result::ATTRMAP_BAD_FILE : Tilemap::Result::TILEMAP_BAD_FILE; }
	bool valid = (ends_with_ignore_case(f, ".csv") ? import_csv_tiles : import_c_tiles)(file, bytes);
	fclose(file);
	if (!valid) { return attrmap ? Tilemap::Result::ATTRMAP_INVALID : Tilemap::Result::TILEMAP_INVALID; }
	return Tilemap::Result::TILEMAP_OK;
}

Tilemap::Result Tilemap::import_tiles(const char *tf, const char *af) {
	std::vector<uchar> tbytes, abytes;
	Result result = import_file_bytes(tf, tbytes, false);
	if (result != Result::TILEMAP_OK) { return (_result = result); }
	if (af && af[0]) {
		result = import_file_bytes(af, abytes, true);
		if (result != Result::TILEMAP_OK) { return (_result = result); }
	}
	_modified = true;
	return (_result = make_tiles(tbytes, abytes));
}

static bool check_read(FILE *file, uchar *expected, size_t n) {
	std::vector<uchar> buffer(n);
	size_t r = fread(buffer.data(), 1, n, file);
	return r == n && (!expected || !memcmp(buffer.data(), expected, n));
}

static uint16_t read_uint16(FILE *file) {
	int lo = fgetc(file);
	int hi = fgetc(file);
	return (uint16_t)(((hi & 0xFF00) >> 8) | (lo & 0xFF));
}

Tilemap::Result Tilemap::import_rmp(const char *f) {
	std::vector<uchar> tbytes, abytes;

	FILE *file = fl_fopen(f, "rb");
	if (!file) { return Tilemap::Result::TILEMAP_BAD_FILE; }

#define CHECK_READ_VALID(expected) do { \
	if (!check_read(file, expected, sizeof(expected))) { \
		fclose(file); \
		return (_result = Tilemap::Result::TILEMAP_INVALID); \
	} \
} while (false)

	// <https://github.com/chadaustin/sphere/blob/master/sphere/docs/internal/map.rmp.txt>

	uchar expected_header[21] = {
		'.', 'r', 'm', 'p', // magic number
		LE16(1),            // version
		0,                  // type (obsolete)
		1,                  // num layers
		0,                  // reserved
		LE16(0),            // num entities
		LE16(0),            // start x
		LE16(0),            // start y
		0,                  // start layer
		0,                  // start direction (north)
		LE16(9),            // num strings
		LE16(0)             // num zones
	};
	CHECK_READ_VALID(expected_header);
	uchar expected_unused_and_strings[235 + 9 * 2] = {0};
	CHECK_READ_VALID(expected_unused_and_strings);

	uint16_t width = read_uint16(file);
	uint16_t height = read_uint16(file);

	uchar expected_layer_header[26] = {
		LE16(0),          // flags
		LE32(0x3F800000), // parallax x (1.0f)
		LE32(0x3F800000), // parallax y (1.0f)
		LE32(0),          // scrolling x (0.0f)
		LE32(0),          // scrolling y (0.0f)
		LE32(0),          // num segments
		0,                // reflective
		0, 0, 0           // reserved
	};
	CHECK_READ_VALID(expected_layer_header);

	uint16_t name_length = read_uint16(file);
	fseek(file, name_length, SEEK_CUR);

	size_t n = width * height * 2;
	tbytes.resize(n);
	if (size_t r = fread(tbytes.data(), 1, n, file); r != n) {
		fclose(file);
		return (_result = Tilemap::Result::TILEMAP_INVALID);
	}

	fclose(file);

#undef CHECK_READ_VALID

	// GBA_4BPP tile IDs must be 0x3FF or below
	Config::format(Tilemap_Format::GBA_4BPP);
	for (size_t i = 1; i < n; i += 2) {
		if (tbytes[i] > 0x03) {
			return (_result = Tilemap::Result::TILEMAP_INVALID);
		}
	}

	_modified = true;
	return (_result = make_tiles(tbytes, abytes));
}
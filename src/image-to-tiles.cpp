#include <string>
#include <vector>
#include <set>
#include <map>
#include <iterator>

#pragma warning(push, 0)
#include <FL/Fl.H>
#include <FL/Fl_PNG_Image.H>
#include <FL/Fl_GIF_Image.H>
#include <FL/Fl_BMP_Image.H>
#include <FL/Fl_Image_Surface.H>
#pragma warning(pop)

#include "utils.h"
#include "config.h"
#include "image.h"
#include "tilemap.h"
#include "tileset.h"
#include "tile.h"
#include "main-window.h"

// Avoid "warning C4458: declaration of 'i' hides class member"
// due to Fl_Window's Fl_X *i
#pragma warning(push)
#pragma warning(disable : 4458)

typedef std::set<Fl_Color> Color_Set;

static bool build_tilemap(const Tile *tiles, size_t n, std::vector<int> tile_palettes,
	std::vector<Tile_Tessera *> &tilemap, std::vector<size_t> &tileset, Tilemap_Format fmt, uint16_t start_id,
	bool use_space, uint16_t space_id, Fl_Color space_color) {
	for (size_t i = 0; i < n; i++) {
		if (use_space && start_id + tileset.size() == space_id) {
			size_t j = 0;
			for (; j < n; j++) {
				if (is_blank_tile(tiles[j], space_color)) { break; }
			}
			tileset.push_back(j);
		}
		const Tile &tile = tiles[i];
		if (use_space && is_blank_tile(tile, space_color)) {
			tilemap.push_back(new Tile_Tessera(0, 0, 0, 0, space_id, false, false, false, false, tile_palettes[i]));
			continue;
		}
		size_t ti = 0, nt = tileset.size();
		bool x_flip = false, y_flip = false;
		for (; ti < nt; ti++) {
			size_t j = tileset[ti];
			if (are_identical_tiles(tile, tiles[j], fmt, x_flip, y_flip)) {
				break;
			}
		}
		if (ti == nt) {
			size_t mn = (size_t)format_tileset_size(fmt);
			if (nt + (size_t)start_id > mn) {
				return false;
			}
			tileset.push_back(i);
		}
		uint16_t id = start_id + (uint16_t)ti;
		Tile_Tessera *tt = new Tile_Tessera(0, 0, 0, 0, id, x_flip, y_flip, false, false, tile_palettes[i]);
		tilemap.push_back(tt);
	}
	return true;
}

static Fl_RGB_Image *print_tileset(const Tile *tiles, const std::vector<size_t> &tileset, const Palettes &palettes,
	const std::vector<int> &tile_palettes, size_t nc, int tw, Fl_Color blank_color, bool indexed) {
	int nt = (int)tileset.size();
	tw = std::min(nt, tw);
	int th = (nt + tw - 1) / tw;

	size_t np = tile_palettes.size();
	std::vector<std::map<Fl_Color, size_t>> reverse_palettes;
	for (const Palette &palette : palettes) {
		std::map<Fl_Color, size_t> reverse_palette;
		for (size_t i = 0; i < nc; i++) {
			reverse_palette.emplace(palette[i], i);
		}
		reverse_palettes.push_back(reverse_palette);
	}

	Fl_Image_Surface *surface = new Fl_Image_Surface(tw * TILE_SIZE, th * TILE_SIZE);
	surface->set_current();

	if (indexed) { nc = MAX_PALETTE_LENGTH; }
	fl_rectf(0, 0, tw * TILE_SIZE, th * TILE_SIZE, indexed ? 0 : blank_color);
	for (int i = 0; i < nt; i++) {
		size_t ti = tileset[i];
		const Tile &tile = tiles[ti];
		int p = ti < np ? tile_palettes[ti] : -1;
		if (p == -1 && indexed) { continue; }
		int x = i % tw, y = i / tw;
		for (int ty = 0; ty < TILE_SIZE; ty++) {
			for (int tx = 0; tx < TILE_SIZE; tx++) {
				Fl_Color c = tile[ty * TILE_SIZE + tx];
				if (p > -1) {
					size_t pi = reverse_palettes[p][c];
					c = Image::get_indexed_grayscale(pi, nc);
				}
				fl_color(c);
				fl_point(x * TILE_SIZE + tx, y * TILE_SIZE + ty);
			}
		}
	}

	Fl_RGB_Image *img = surface->image();
	delete surface;
	Fl_Display_Device::display_device()->set_current();

	return img;
}

static double luminance(Fl_Color c) {
	uchar r, g, b;
	Fl::get_color(c, r, g, b);
	return 0.299 * (double)r + 0.587 * (double)g + 0.114 * (double)b;
}

bool Main_Window::image_to_tiles() {
	// Open the input image

	const char *image_filename = _image_to_tiles_dialog->image_filename();
	const char *image_basename = fl_filename_name(image_filename);

	Fl_RGB_Image *img = NULL;
	if (ends_with_ignore_case(image_basename, ".bmp")) {
		img = new Fl_BMP_Image(image_filename);
	}
	else if (ends_with_ignore_case(image_basename, ".gif")) {
		if (Fl_GIF_Image *gif = new Fl_GIF_Image(image_filename)) {
			img = new Fl_RGB_Image(gif, FL_WHITE);
			delete gif;
		}
	}
	else {
		img = new Fl_PNG_Image(image_filename);
	}
	if (!img || img->fail()) {
		delete img;
		std::string msg = "Could not convert ";
		msg = msg + image_basename + "!\n\nCannot open file.";
		_error_dialog->message(msg);
		_error_dialog->show(this);
		return false;
	}

	// Read the input image tiles

	size_t n = 0, w = 0;
	Tile *tiles = get_image_tiles(img, n, w);
	delete img;
	if (!tiles || !n) {
		delete [] tiles;
		std::string msg = "Could not convert ";
		msg = msg + image_basename + "!\n\nImage dimensions do not fit the "
			STRINGIFY(TILE_SIZE) "x" STRINGIFY(TILE_SIZE) " tile grid.";
		_error_dialog->message(msg);
		_error_dialog->show(this);
		return false;
	}

	// Build the palette

	Tilemap_Format fmt = _image_to_tiles_dialog->format();
	Palette_Format pal_fmt = _image_to_tiles_dialog->palette_format();
	bool make_palette = _image_to_tiles_dialog->palette() && format_can_make_palettes(fmt);

	bool use_color_zero = _image_to_tiles_dialog->color_zero();
	Fl_Color color_zero = use_color_zero ? _image_to_tiles_dialog->fl_color_zero() : 0xFFFFFF00 /* white */;

	Palettes palettes;
	std::vector<int> tile_palettes(n, make_palette ? 0 : -1);
	size_t max_colors = (size_t)format_palette_size(fmt);

	if (make_palette) {
		// Algorithm ported from superfamiconv
		// <https://github.com/Optiroc/SuperFamiconv>

		size_t max_palettes = (size_t)format_palettes_size(fmt);
		uint16_t start_index = _image_to_tiles_dialog->start_index();

		// Get the color set of each tile
		std::vector<Color_Set> cs_tiles;
		size_t qi = 0;
		for (; qi < n; qi++) {
			const Tile &tile = tiles[qi];
			Color_Set s;
			if (use_color_zero) {
				s.insert(color_zero);
			}
			for (int j = 0; j < NUM_TILE_PIXELS; j++) {
				s.insert(tile[j]);
			}
			if (s.size() > max_colors) {
				break;
			}
			cs_tiles.push_back(s);
		}

		// Check that all color sets fit within the color limit
		if (qi < n) {
			size_t qx = qi % w, qy = qi / w;
			delete [] tiles;
			std::string msg = "Could not convert ";
			msg = msg + image_basename + "!\n\nThe tile at (" +
				std::to_string(qx) + ", " + std::to_string(qy) +
				") has more than " + std::to_string(max_colors) + " colors.";
			_error_dialog->message(msg);
			_error_dialog->show(this);
			return false;
		}

		// Remove duplicate color sets
		std::vector<Color_Set> cs_uniq(cs_tiles.size());
		auto cs_uniq_last = std::copy_if(RANGE(cs_tiles), cs_uniq.begin(), [&](const Color_Set &s) {
			return std::find(RANGE(cs_uniq), s) == cs_uniq.end();
		});
		cs_uniq.resize(std::distance(cs_uniq.begin(), cs_uniq_last));

		// Remove color sets that are proper subsets of other color sets
		std::vector<Color_Set> cs_full(cs_uniq.size());
		auto cs_full_last = std::copy_if(RANGE(cs_uniq), cs_full.begin(), [&](const Color_Set &s) {
			return !std::any_of(RANGE(cs_uniq), [&](const Color_Set &c) {
				return s != c && std::includes(RANGE(c), RANGE(s));
			});
		});
		cs_full.resize(std::distance(cs_full.begin(), cs_full_last));

		// Combine color sets as long as they fit within the color limit
		std::vector<Color_Set> cs_opt;
		for (Color_Set &s : cs_full) {
			Color_Set *b = NULL;
			for (Color_Set &c : cs_opt) {
				Color_Set d;
				std::set_difference(RANGE(s), RANGE(c), std::inserter(d, d.begin()));
				if (c.size() + d.size() <= max_colors) {
					b = &c;
				}
			}
			if (b) {
				b->insert(RANGE(s));
			}
			else {
				cs_opt.push_back(s);
			}
		}

		// Sort color sets from most to fewest colors
		std::stable_sort(RANGE(cs_opt), [](const Color_Set &a, const Color_Set &b) {
			return a.size() > b.size();
		});

		// Sort each palette from brightest to darkest color, padded with black, keeping color 0 first
		for (Color_Set &s : cs_opt) {
			Palette palette(RANGE(s));
			std::sort(RANGE(palette), [use_color_zero, color_zero](Fl_Color a, Fl_Color b) {
				if (use_color_zero) {
					if (a == color_zero) { return true; }
					if (b == color_zero) { return false; }
				}
				return luminance(a) > luminance(b);
			});
			if (max_palettes == 1) {
				// Pad the palette to start at the right index
				for (uint16_t i = 0; i < start_index; i++) {
					palette.insert(palette.begin(), i == 0 ? color_zero : FL_BLACK);
				}
			}
			for (size_t i = palette.size(); i < max_colors; i++) {
				palette.push_back(FL_BLACK);
			}
			palettes.push_back(palette);
		}

		// Pad the palettes to start at the right index
		if (max_palettes > 1) {
			for (uint16_t i = 0; i < start_index; i++) {
				Palette palette(max_colors, FL_BLACK);
				palette[0] = color_zero;
				palettes.insert(palettes.begin(), palette);
			}
		}

		// Create the palette file
		const char *palette_filename = _image_to_tiles_dialog->palette_filename();
		const char *palette_basename = fl_filename_name(palette_filename);
		if (!write_palette(palette_filename, palettes, pal_fmt, max_colors)) {
			delete [] tiles;
			std::string msg = "Could not write to ";
			msg = msg + palette_basename + "!";
			_error_dialog->message(msg);
			_error_dialog->show(this);
			return false;
		}

		// Check that the palettes fit within the palette limit
		size_t np = palettes.size();
		if (np > max_palettes) {
			delete [] tiles;
			std::string msg = "Could not convert ";
			msg = msg + image_basename + "!\n\nThe tiles need more than " +
				std::to_string(max_palettes) + " palettes.\n\nAll " +
				std::to_string(np) + " palettes were written to " + palette_basename + ".";
			_error_dialog->message(msg);
			_error_dialog->show(this);
			return false;
		}
		else if (max_palettes == 1 && palettes[0].size() > max_colors) {
			delete [] tiles;
			std::string msg = "Could not convert ";
			msg = msg + image_basename + "!\n\nThe tiles need more than " +
				std::to_string(max_colors) + " colors.\n\nAll " +
				std::to_string(np) + " palettes were written to " + palette_basename + ".";
			_error_dialog->message(msg);
			_error_dialog->show(this);
			return false;
		}

		// Associate tiles with palettes
		for (size_t i = 0; i < n; i++) {
			int pal = 0;
			const Color_Set &s = cs_tiles[i];
			for (size_t j = 0; j < np; j++) {
				const Color_Set &c = cs_opt[j];
				if (std::includes(RANGE(c), RANGE(s))) {
					pal = (int)j;
					break;
				}
			}
			tile_palettes[i] = pal;
		}
	}

	// Build the tilemap and tileset

	std::vector<Tile_Tessera *> tilemap;
	std::vector<size_t> tileset;

	uint16_t start_id = _image_to_tiles_dialog->start_id();
	bool use_space = _image_to_tiles_dialog->use_space();
	uint16_t space_id = _image_to_tiles_dialog->space_id();

	if (!build_tilemap(tiles, n, tile_palettes, tilemap, tileset, fmt, start_id, use_space, space_id, color_zero)) {
		for (Tile_Tessera *tt : tilemap) {
			delete tt;
		}
		delete [] tiles;
		std::string msg = "Could not convert ";
		msg = msg + image_basename + "!\n\nToo many unique tiles.";
		_error_dialog->message(msg);
		_error_dialog->show(this);
		return false;
	}

	// Get the output filenames

	const char *tileset_filename = _image_to_tiles_dialog->tileset_filename();
	const char *tilemap_filename = _image_to_tiles_dialog->tilemap_filename();
	const char *attrmap_filename = _image_to_tiles_dialog->attrmap_filename();
	const char *tileset_basename = fl_filename_name(tileset_filename);
	const char *tilemap_basename = fl_filename_name(tilemap_filename);

	// Create the tilemap file

	if (!Tilemap::write_tiles(tilemap_filename, attrmap_filename, tilemap, fmt)) {
		for (Tile_Tessera *tt : tilemap) {
			delete tt;
		}
		delete [] tiles;
		std::string msg = "Could not write to ";
		msg = msg + tilemap_basename + "!";
		_error_dialog->message(msg);
		_error_dialog->show(this);
		return false;
	}


	for (Tile_Tessera *tt : tilemap) {
		delete tt;
	}

	// Create the tilepal file

	if (make_palette && format_has_per_tile_palettes(fmt)) {
		const char *tilepal_filename = _image_to_tiles_dialog->tilepal_filename();
		const char *tilepal_basename = fl_filename_name(tilepal_filename);
		if (!write_tilepal(tilepal_filename, tileset, tile_palettes)) {
			delete [] tiles;
			std::string msg = "Could not write to ";
			msg = msg + tilepal_basename + "!";
			_error_dialog->message(msg);
			_error_dialog->show(this);
			return false;
		}
	}

	// Create the tileset file

	bool indexed = make_palette && pal_fmt == Palette_Format::INDEXED;
	Fl_RGB_Image *timg = print_tileset(tiles, tileset, palettes, tile_palettes, max_colors, tileset_width(),
		color_zero, indexed);
	Image::Result result = indexed ? Image::write_image(tileset_filename, timg, 0, &palettes, max_colors) :
		Image::write_image(tileset_filename, timg, make_palette ? format_color_depth(fmt) : 0);
	delete timg;
	if (result != Image::Result::IMAGE_OK) {
		delete [] tiles;
		std::string msg = "Could not write to ";
		msg = msg + tileset_basename + "!\n\n" + Image::error_message(result);
		_error_dialog->message(msg);
		_error_dialog->show(this);
		return false;
	}

	delete [] tiles;

	// Alert the completed operation

	std::string msg = "Converted ";
	msg = msg + image_basename + " to\n" + tilemap_basename + " and " + tileset_basename + "!";
	_success_dialog->message(msg);
	_success_dialog->show(this);

	// Open the tilemap and load the tileset

	Config::format(fmt);
	update_active_controls();
	redraw();

	open_tilemap(tilemap_filename, w, 0, true);
	unload_tilesets_cb(NULL, this);
	add_tileset(tileset_filename, start_id);

	return true;
}

#pragma warning(pop)

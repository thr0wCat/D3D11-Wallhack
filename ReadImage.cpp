#include <cassert>
#include <memory.h>
#include "readimage.h"

extern "C" {
#include "libpng/png.h"
#include "libpng/pngconf.h"
}

image::image()
{
    _width = _height = 0;
    _depth = 0;
    _color_bytes = 0;
    _bytes_per_line = 0;
    _data = 0;
    _xdpi = _ydpi = 96;
    _is_alpha_channel_valid = false;
}

bool image::is_valid() const
{
    if(_data) {
        assert((_width != 0) && (_height != 0));
        assert(_color_bytes != 0);
        assert(_bytes_per_line != 0);
        return true;
    }
    return false;
}

bool image::create(image_format fmt, int w, int h)
{
    if(!w || !h)
        return false;
    if(is_valid())
        destroy();
    _width = w;
    _height = h;
    _format = fmt;
    switch(fmt)
    {
    case fmt_gray:
        _depth = 8;
        break;
    case fmt_rgba:
        _depth = 32;
        break;
    default:
        return false;
    }
    _xdpi = _ydpi = 96;
    _bytes_per_line = (int)(((uint)(_depth * w + 31)) >> 5 << 2);
    _color_bytes = _bytes_per_line * h;
    _data = new byte[_color_bytes];
    return true;
}

void image::destroy()
{
    if(_data) {
        delete [] _data;
        _data = 0;
    }
    _width = _height = 0;
    _depth = 0;
    _color_bytes = 0;
    _bytes_per_line = 0;
}

byte* image::get_data(int x, int y) const
{
    assert(is_valid());
    if((x >= _width) || (y >= _height)) {
        assert(!"invalid position.");
        return 0;
    }
    int pos = 0;
    switch(_format)
    {
    case fmt_gray:
        assert(_depth == 8);
        pos = _bytes_per_line * y + x;
        break;
    case fmt_rgba:
        assert(_depth == 32);
        pos = _bytes_per_line * y + x * 4;
        break;
    }
    return _data + pos;
}

void image::copy(const image& img)
{
    if(!img.is_valid())
        return;
    copy(img, 0, 0, img.get_width(), img.get_height(), 0, 0);
}

void image::copy(const image& img, int x, int y, int cx, int cy, int sx, int sy)
{
    if(!img.is_valid() || !cx || !cy || (img.get_format() != get_format()))
        return;
    if(cx < 0 || x + cx > get_width())
        cx = get_width() - x;
    if(cy < 0 || y + cy > get_height())
        cy = get_height() - y;
    if(sx + cx > img.get_width())
        cx = img.get_width() - sx;
    if(sy + cy > img.get_height())
        cy = img.get_height() - sy;
    int sp = 1;
    if(_format == fmt_rgba)
        sp = 4;
    int size = cx * sp;
    for(int j = 0; j < cy; j ++) {
        const byte* src = img.get_data(sx, sy + j);
        byte* dest = get_data(x, y + j);
        assert(src && dest);
        memcpy(dest, src, size);
    }
}

static int convert_dpi_to_dpm(int dpi)
{
    return (int)((float)dpi / 0.0254f + 0.5f);
}

static int convert_dpm_to_dpi(int dpm)
{
    return (int)((float)dpm * 0.0254f + 0.5f);
}

class image_data_stream
{
public:
    image_data_stream(const byte* ptr, int size)
    {
        assert(ptr);
        _ptr = ptr;
        _size = size;
        _pos = 0;
    }
    bool read_byte(byte& data) const
    {
        assert(_ptr);
        if(_pos < _size) {
            data = _ptr[_pos ++];
            return true;
        }
        return false;
    }
    int read_bytes(byte* p, int size) const
    {
        assert(p);
        int left = _size - _pos;
        if(left <= 0)
            return 0;
        if(size > left)
            size = left;
        memcpy(p, _ptr + _pos, size);
        _pos += size;
        return size;
    }
    void seek(int p) { _pos = p; }
    int current_pos() const { return _pos; }
    const byte* get_data() const { return _ptr; }
    int get_size() const { return _size; }

protected:
    const byte*             _ptr;
    int                     _size;
    mutable int             _pos;
};

static void my_png_warning(png_structp, png_const_charp message)
{
}

void my_png_read_fn(png_structp png_ptr, png_bytep data, png_size_t length);
void my_png_setup_image(image& img, png_structp png_ptr, png_infop info_ptr, float gamma);

struct png_reader
{
    enum read_state
    {
        rs_read_header,
        rs_reading_end,
        rs_error,
    };

    float               gamma;
    int                 quality;
    png_struct*         png_ptr;
    png_info*           info_ptr;
    png_info*           end_info;
    png_byte**          row_pointers;
    read_state          state;
    image_data_stream&  device;

public:
    png_reader(image_data_stream& ds):
        device(ds)
    {
        gamma = 0.f;
        quality = 2;
        png_ptr = nullptr;
        info_ptr = nullptr;
        end_info = nullptr;
        row_pointers = nullptr;
    }
    ~png_reader()
    {
        if(png_ptr) {
            png_destroy_read_struct(&png_ptr, 0, 0);
            png_ptr = nullptr;
        }
        if(row_pointers) {
            delete [] row_pointers;
            row_pointers = nullptr;
        }
    }
    bool read_header()
    {
        state = rs_error;
        png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
        if(!png_ptr)
            return false;
        png_set_error_fn(png_ptr, 0, 0, my_png_warning);
        info_ptr = png_create_info_struct(png_ptr);
        if(!info_ptr)
            return false;
        end_info = png_create_info_struct(png_ptr);
        if(!end_info)
            return false;
        if(setjmp(png_jmpbuf(png_ptr)))
            return false;
        png_set_read_fn(png_ptr, this, my_png_read_fn);
        png_read_info(png_ptr, info_ptr);
        state = rs_read_header;
        return true;
    }
    bool read(image& img)
    {
        if(!read_header())
            return false;
        row_pointers = nullptr;
        if(setjmp(png_jmpbuf(png_ptr))) {
            state = rs_error;
            return false;
        }
        my_png_setup_image(img, png_ptr, info_ptr, gamma);
        if(!img.is_valid()) {
            state = rs_error;
            return false;
        }
        png_uint_32 width;
        png_uint_32 height;
        int bit_depth;
        int color_type;
        png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type, 0, 0, 0);
        byte* data = img.get_data(0, 0);
        int bpl = img.get_bytes_per_line();
        row_pointers = new png_bytep[height];
        for(uint y = 0; y < height; y ++)
            row_pointers[y] = data + y * bpl;
        png_read_image(png_ptr, row_pointers);
        img.set_xdpi(convert_dpm_to_dpi(png_get_x_pixels_per_meter(png_ptr, info_ptr)));
        img.set_ydpi(convert_dpm_to_dpi(png_get_y_pixels_per_meter(png_ptr, info_ptr)));
        state = rs_reading_end;
        png_read_end(png_ptr, end_info);
        return true;
    }
};

static void my_png_read_fn(png_structp png_ptr, png_bytep data, png_size_t length)
{
    png_reader* d = (png_reader*)png_get_io_ptr(png_ptr);
    image_data_stream& ds = d->device;
    if((d->state == png_reader::rs_reading_end) && (ds.get_size() - ds.current_pos() < 4) && (length == 4)) {
        /* workaround for certain malformed PNGs that lack the final crc bytes */
        byte endcrc[4] = { 0xae, 0x42, 0x60, 0x82 };
        memcpy(data, endcrc, 4);
        ds.seek(ds.get_size());
        return;
    }
    ds.read_bytes(data, (int)length);
}

static void my_png_setup_image(image& img, png_structp png_ptr, png_infop info_ptr, float gamma)
{
    if(gamma != 0.f && png_get_valid(png_ptr, info_ptr, PNG_INFO_gAMA)) {
        double file_gamma;
        png_get_gAMA(png_ptr, info_ptr, &file_gamma);
        png_set_gamma(png_ptr, gamma, file_gamma);
    }
    png_uint_32 width;
    png_uint_32 height;
    int bit_depth;
    int color_type;
    png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type, 0, 0, 0);
    png_set_interlace_handling(png_ptr);
    image::image_format format;
    bool has_alpha = false;
    if(color_type == PNG_COLOR_TYPE_GRAY) {
        if(bit_depth == 16)
            png_set_strip_16(png_ptr);
        else if(bit_depth < 8)
            png_set_packing(png_ptr);
        png_read_update_info(png_ptr, info_ptr);
        format = image::fmt_gray;
    }
    else {
        if(bit_depth == 16)
            png_set_strip_16(png_ptr);
        png_set_expand(png_ptr);
        if(color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
            png_set_gray_to_rgb(png_ptr);
        format = image::fmt_rgba;
        has_alpha = true;
        if(!(color_type & PNG_COLOR_MASK_ALPHA) && !png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) {
            png_set_filler(png_ptr, 0xff, PNG_FILLER_AFTER);
            has_alpha = false;
        }
        png_read_update_info(png_ptr, info_ptr);
    }
    if(!img.is_valid() || (img.get_width() != width) || (img.get_height() != height) || (img.get_format() != format)) {
        if(img.is_valid())
            img.destroy();
        img.create(format, width, height);
        img.enable_alpha_channel(has_alpha);
    }
}

bool imageio::read_png_image(image& img, const void* ptr, int size)
{
    image_data_stream ds((const byte*)ptr, size);
    png_reader reader(ds);
    return reader.read(img);
}

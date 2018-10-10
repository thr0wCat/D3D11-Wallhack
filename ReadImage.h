#pragma once

#include <stdint.h>

typedef uint8_t byte;
typedef uint32_t uint;

class image
{
public:
    enum image_format
    {
        fmt_gray,                   /* gray8 */
        fmt_rgba,                   /* rgba8888 */
    };

    friend class imageio;

public:
    image();
    ~image() { destroy(); }
    bool is_valid() const;
    image_format get_format() const { return _format; }
    int get_depth() const { return _depth; }
    bool create(image_format fmt, int w, int h);
    void destroy();
    void enable_alpha_channel(bool b) { _is_alpha_channel_valid = b; }
    bool has_alpha() const { return _is_alpha_channel_valid; }
    int get_width() const { return _width; }
    int get_height() const { return _height; }
    int get_bytes_per_line() const { return _bytes_per_line; }
    int get_size() const { return _color_bytes; }
    void set_xdpi(int dpi) { _xdpi = dpi; }
    void set_ydpi(int dpi) { _ydpi = dpi; }
    int get_xdpi() const { return _xdpi; }
    int get_ydpi() const { return _ydpi; }
    byte* get_data(int x, int y) const;
    void copy(const image& img);
    void copy(const image& img, int x, int y, int cx, int cy, int sx, int sy);

protected:
    image_format        _format;
    int                 _width;
    int                 _height;
    int                 _depth;
    int                 _color_bytes;
    int                 _bytes_per_line;
    int                 _xdpi;
    int                 _ydpi;
    byte*               _data;
    bool                _is_alpha_channel_valid;
};

class imageio
{
public:
    static bool read_png_image(image& img, const void* ptr, int size);
};

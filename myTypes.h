#ifndef _MYTYPES_HPP
#define _MYTYPES_HPP

typedef struct {
    gboolean valid;
    GstVideoInfo vinfo;
} CairoOverlayState;

struct customData {
    GstElement *pipeline,
               *source,
               *rtpjpeg,
               *decoder,
               *videoparse1,
               *motion,
               *videoparse2,
               *overlay,
               *videoparse3,
               *sink;
    GstBus *bus;
    guint bus_watch_id;
    GMainLoop *loop;
    GstVideoRectangle *rect;
    CairoOverlayState *overlay_state;
};

enum {
    MOUSE_LEFT_BUTTON     = 1,
    MOUSE_SCROLL_BUTTON   = 2,
    MOUSE_RIGHT_BUTTON    = 3,
    MOUSE_SCROLL_UP       = 4,
    MOUSE_SCROLL_DOWN     = 5,
};

#endif

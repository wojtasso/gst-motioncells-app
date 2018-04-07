#ifndef _MOTION_HPP
#define _MOTION_HPP

#include <gst/gst.h>
#include <gst/video/video.h>
#include <cairo.h>
#include "myTypes.h"

#define UDPSRC_CAPS "application/x-rtp, payload=127"

class Motion
{
    public:
        Motion(int argc, char *argv[]);
        ~Motion();
        void gst_loop();
    private:
        static gboolean handle_keyboard(GIOChannel *source,
                GIOCondition cond,
                gpointer data);
        static void prepare_overlay(GstElement *overlay,
                GstCaps *caps,
                gpointer user_data);
        static void draw_overlay(GstElement *overlay,
                cairo_t *cr,
                guint64 timestamp,
                guint64 duration,
                gpointer user_data);

        static void parse_motion_msg(GstMessage *msg);
        static void parse_xvsink_msg(GstMessage *msg, customData *data);
        static gboolean bus_call(GstBus *bus, GstMessage *msg, customData *data);
        struct customData data;
        GIOChannel *io_stdin;
};

#endif

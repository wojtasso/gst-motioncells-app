#include "Motion.hpp"
#include <iostream>
#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/navigation.h>
#include <cairo.h>
#include <cairo-gobject.h>
#include <glib.h>

Motion::Motion(int argc, char *argv[])
{
    GIOChannel *io_stdin;
    gst_init(&argc, &argv);
    data.loop = g_main_loop_new (NULL, FALSE);

    data.pipeline = gst_pipeline_new ("streaming");
#ifdef UDPSRC
    std::cout << "Read video from UDP" << std::endl;
    data.source = gst_element_factory_make ("udpsrc","videosource");
    data.rtpjpeg = gst_element_factory_make ("rtph264depay", "rtp");
    data.decoder = gst_element_factory_make ("avdec_h264", "jpegdecoder");

    g_object_set(data.source, "port", 5000, NULL);
    gchar *udpsrc_caps_text;
    GstCaps *udpsrc_caps;
    udpsrc_caps_text = g_strdup_printf(UDPSRC_CAPS);
    udpsrc_caps = gst_caps_from_string(udpsrc_caps_text);
    g_object_set(data.source, "caps", udpsrc_caps, NULL);
    gst_caps_unref(udpsrc_caps);
    g_free(udpsrc_caps_text);
#else
    std::cout << "Read video from /dev/video0" << std::endl;
    data.source = gst_element_factory_make ("v4l2src","videosource");
#endif

    data.videoparse1 = gst_element_factory_make("autovideoconvert","videoparse1");
    data.motion = gst_element_factory_make("motioncells", "motion");
    data.videoparse2 = gst_element_factory_make("autovideoconvert", "videoparse2");
    data.overlay = gst_element_factory_make("cairooverlay", "overlay");
    data.videoparse3 = gst_element_factory_make("autovideoconvert", "videoparse3");
    data.sink = gst_element_factory_make ("xvimagesink", "sink");

    g_object_set(data.sink, "sync", false, NULL);

    g_object_set(data.motion, "gridx", 8, NULL);
    g_object_set(data.motion, "gridy", 8, NULL);
    g_object_set(data.motion, "sensitivity", 0.6f, NULL);
    g_object_set(data.motion, "threshold", 0.001, NULL);
    g_object_set(data.motion, "gap", 4, NULL);
    g_object_set(data.motion, "postallmotion", true, NULL);

    // allocate on heap for pedagogical reasons, makes code easier to transfer
    data.overlay_state = g_new0(CairoOverlayState, 1);
    data.rect = g_new0(GstVideoRectangle, 1);

    // Hook up the neccesary signals for cairooverlay
    g_signal_connect(data.overlay, "draw",
            G_CALLBACK(draw_overlay), &data);
    g_signal_connect(data.overlay, "caps-changed",
            G_CALLBACK (prepare_overlay), data.overlay_state);

    if (!data.pipeline ||
            !data.source ||
#ifdef UDPSRC
            !data.rtpjpeg ||
            !data.decoder ||
#endif
            !data.videoparse1 ||
            !data.motion ||
            !data.videoparse2 ||
            !data.overlay ||
            !data.videoparse3 ||
            !data.sink ) {
        g_printerr ("Error");
        exit(EXIT_FAILURE);
    }

    data.bus = gst_pipeline_get_bus(GST_PIPELINE (data.pipeline));
    /* Add a keyboard watch so we get notified of keystrokes */
    io_stdin = g_io_channel_unix_new(fileno (stdin));

    data.bus_watch_id = g_io_add_watch(io_stdin,
            G_IO_IN,
            (GIOFunc)handle_keyboard,
            data.loop);

    GDestroyNotify notify = NULL;
    gst_bus_set_sync_handler(data.bus, (GstBusSyncHandler)bus_call, &data, notify);
    g_print("%d\n", data.bus_watch_id);
    gst_object_unref(data.bus);

    gst_bin_add_many(GST_BIN(data.pipeline),
            data.source,
#ifdef UDPSRC
            data.rtpjpeg,
            data.decoder,
#endif
            data.videoparse1,
            data.motion,
            data.videoparse2,
            data.overlay,
            data.videoparse3,
            data.sink,
            NULL);

    gst_element_link_many(data.source,
#ifdef UDPSRC
            data.rtpjpeg,
            data.decoder,
#endif
            data.videoparse1,
            data.motion,
            data.videoparse2,
            data.overlay,
            data.videoparse3,
            data.sink,
            NULL);
}

Motion::~Motion()
{
    /*Out of the main loop, clean up nicely*/
    g_print("Returned, stopping playback\n");
    gst_element_set_state(data.pipeline, GST_STATE_NULL);
    g_print("Deleting pipeline\n");
    gst_element_set_state(data.pipeline, GST_STATE_NULL);
    gst_object_unref(GST_OBJECT (data.pipeline));
    g_source_remove(data.bus_watch_id);

    g_main_loop_unref(data.loop);
}

void Motion::gst_loop()
{
    g_print("Now playing\n");
    gst_element_set_state(data.pipeline, GST_STATE_PLAYING);

    /*Iterate*/
    g_print("Running...\n");
    g_main_loop_run(data.loop);
}

//Read keys typed to the terminal
gboolean Motion::handle_keyboard(GIOChannel *source,
        GIOCondition cond,
        gpointer data)
{
    gchar *str = NULL;
    GMainLoop *loop = (GMainLoop*)data;

    if (g_io_channel_read_line(source, &str, NULL, NULL, NULL) != G_IO_STATUS_NORMAL)
        return TRUE;

    switch (g_ascii_tolower(str[0])) {
        case 'q':
            g_main_loop_quit(loop);
            break;
        case 0x1b: //Escape
            g_main_loop_quit(loop);
            break;
        default:
            break;
    }

    g_free (str);
    return TRUE;
}

// Store the information from the caps that we are interested in
void Motion::prepare_overlay(GstElement * overlay, GstCaps * caps,
        gpointer user_data)
{
    CairoOverlayState *state = (CairoOverlayState *)user_data;

    state->valid = gst_video_info_from_caps (&state->vinfo, caps);
}

void Motion::draw_overlay(GstElement * overlay,
        cairo_t * cr,
        guint64 timestamp,
        guint64 duration,
        gpointer user_data)
{
    customData *data = (customData *)user_data;

    if (!data->overlay_state->valid)
        return;

    cairo_rectangle(cr,
            data->rect->x,
            data->rect->y,
            data->rect->w,
            data->rect->h);
    cairo_set_source_rgba(cr, 0.9, 0.0, 0.1, 0.25);
    cairo_fill(cr);
}

void Motion::parse_motion_msg(GstMessage *msg)
{
    const GValue *timestamp, *indices;
    const GstStructure * structure = gst_message_get_structure(msg);
    guint numberOfFields = gst_structure_n_fields(structure);

    std::string fieldName = (std::string)gst_structure_nth_field_name(
            structure,
            numberOfFields - 1); //Get the name of the last field in structure

    if (fieldName == "motion_begin") {
        indices = gst_structure_get_value(structure, "motion_cells_indices");
        timestamp = gst_structure_get_value(structure, "motion_begin");
        g_print("Motion begin.\nTimestamp %s, ", g_strdup_value_contents(timestamp));
        g_print("Region %s \n", g_strdup_value_contents(indices));
    } else if (fieldName == "motion") {
        indices = gst_structure_get_value(structure, "motion_cells_indices");
        timestamp = gst_structure_get_value(structure, "motion");
        g_print("Motion.\nTimestamp %s, ", g_strdup_value_contents(timestamp));
        g_print("Region %s \n", g_strdup_value_contents(indices));
    } else if (fieldName == "motion_finished") {
        timestamp = gst_structure_get_value(structure, "motion_finished");
        g_print("Motion finished.\nTimestamp %s \n", g_strdup_value_contents(timestamp));
    }
}

void Motion::parse_xvsink_msg(GstMessage *msg, customData *data)
{
    const gchar *key;
    GstEvent *eve;
    gint button;
    gdouble x, y;
    static bool drawFlag = false;

    if (gst_navigation_message_get_type(msg) != GST_NAVIGATION_MESSAGE_EVENT)
        return;

    if (!gst_navigation_message_parse_event(msg, &eve))
        return;

    switch (gst_navigation_event_get_type(eve)) {
        case GST_NAVIGATION_EVENT_KEY_PRESS:
            if (gst_navigation_event_parse_key_event(eve, &key)) {
                g_printerr("Pressed key: 0x%x \n", g_ascii_tolower(key[0]));
                if (g_ascii_tolower(key[0]) == 'q' ||
                        g_ascii_tolower(key[0]) == 0x65) { //Exit on q or ESC
                    g_main_loop_quit(data->loop);
                }
            }
            break;
        case GST_NAVIGATION_EVENT_KEY_RELEASE:
            if (gst_navigation_event_parse_key_event(eve, &key))
                g_print("Released key: 0x%x \n", g_ascii_tolower(key[0]));
            break;
        case GST_NAVIGATION_EVENT_MOUSE_BUTTON_PRESS:
            if (gst_navigation_event_parse_mouse_button_event(eve, &button, &x, &y) &&
                    button == MOUSE_LEFT_BUTTON) {
                drawFlag = true;
                data->rect->x = x;
                data->rect->y = y;
                g_print("%f %f %d \n", x, y, button);
                g_object_set(data->motion, "calculatemotion", true, NULL);
            }
            //TODO: Motionmaskcoords doesn't work! Fixed in gstreamer-plugins-bad ver. 1.8.3 ?
            if (gst_navigation_event_parse_mouse_button_event(eve, &button, &x, &y) &&
                    button == MOUSE_SCROLL_BUTTON) {
                g_object_set(data->motion, "motionmaskcoords", "0:0:1:1", NULL);
            }

            break;
        case GST_NAVIGATION_EVENT_MOUSE_BUTTON_RELEASE:
            if (gst_navigation_event_parse_mouse_button_event(eve, &button, &x, &y) &&
                    button == MOUSE_LEFT_BUTTON) {
                drawFlag = false;
                data->rect->w = x - data->rect->x;
                data->rect->h = y - data->rect->y;
                g_print("%f %f %d \n", x, y, button);
            }
            break;
        case GST_NAVIGATION_EVENT_MOUSE_MOVE:
            {
                gchar *tmp;
                g_object_get(data->motion, "motionmaskcoords", &tmp, NULL);
                g_print("coords %s \n \n", tmp);

                if (gst_navigation_event_parse_mouse_move_event(eve, &x, &y)) {
                    if (drawFlag) {
                        data->rect->w = x - data->rect->x;
                        data->rect->h = y - data->rect->y;
                    }
                    g_print("%f %f \n", x, y);
                }
                break;
            }
        default:
            break;
    }
}

gboolean Motion::bus_call(GstBus *bus, GstMessage *msg, customData *data)
{
    gchar *debug;
    GError *error;

    const gchar *name = GST_MESSAGE_SRC_NAME(msg);

    switch (GST_MESSAGE_TYPE (msg)) {

        case GST_MESSAGE_ELEMENT:
            if ((std::string)name == "motion")
                parse_motion_msg(msg);
            else if ((std::string)name == "sink")
                parse_xvsink_msg(msg, data);
            break;
        case GST_MESSAGE_EOS:
            g_print ("End of stream\n");
            g_main_loop_quit (data->loop);
            break;
        case GST_MESSAGE_ERROR:
            gst_message_parse_error (msg, &error, &debug);
            g_free (debug);
            g_printerr ("Error: %s\n", error->message);
            g_error_free (error);
            g_main_loop_quit (data->loop);
            break;
        default:
            break;
    }
    return TRUE;
}

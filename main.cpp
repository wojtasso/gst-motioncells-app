#include <iostream>
#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/navigation.h>
#include <cairo.h>
#include <cairo-gobject.h>
#include <glib.h>
#include <string.h>
#include <vector>

struct customData {
    GstElement *pipeline,
               *source,
               *videoparse1,
               *motion,
               *videoparse2,
               *overlay,
               *videoparse3,
               *sink;
    GstBus *bus;
    guint bus_watch_id;

    GMainLoop *loop;
};

typedef struct
{
    gboolean valid;
    GstVideoInfo vinfo;
} CairoOverlayState;

enum {
    MOUSE_LEFT_BUTTON     = 1,
    MOUSE_SCROLL_BUTTON   = 2,
    MOUSE_RIGHT_BUTTON    = 3,
    MOUSE_SCROLL_UP       = 4,
    MOUSE_SCROLL_DOWN     = 5,
};

//Read keys typed to the terminal
static gboolean handle_keyboard(GIOChannel *source, GIOCondition cond, gpointer data)
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
static void prepare_overlay(GstElement * overlay, GstCaps * caps,
        gpointer user_data)
{
    CairoOverlayState *state = (CairoOverlayState *) user_data;

    state->valid = gst_video_info_from_caps (&state->vinfo, caps);
}

static void draw_overlay(GstElement * overlay, cairo_t * cr, guint64 timestamp,
        guint64 duration, gpointer user_data)
{
    CairoOverlayState *s = (CairoOverlayState *)user_data;
    int width, height;

    if (!s->valid)
        return;

    width = GST_VIDEO_INFO_WIDTH(&s->vinfo);
    height = GST_VIDEO_INFO_HEIGHT(&s->vinfo);

    cairo_rectangle(cr, 0, 0, 300, 240);
    cairo_set_source_rgba(cr, 0.9, 0.0, 0.1, 0.25);
    cairo_fill(cr);
}

static gboolean bus_call(GstBus *bus, GstMessage *msg, customData *data)
{
    const gchar *name = GST_MESSAGE_SRC_NAME(msg);

    if ((std::string)name == "motion") {
        g_print("%d \n", GST_MESSAGE_TYPE(msg));
        const GValue *timestamp, *indices;
        const GstStructure * structure = gst_message_get_structure(msg);
        guint numberOfFields = gst_structure_n_fields(structure);

        std::string fieldName = (std::string)gst_structure_nth_field_name(
                structure,
                numberOfFields - 1); //Get the name of the last field in structure

        if (fieldName == "motion_begin") {
            indices = gst_structure_get_value(structure, "motion_cells_indices");
            timestamp = gst_structure_get_value(structure, "motion_begin");
            g_printerr("Motion begin.\nTimestamp %s, ", g_strdup_value_contents(timestamp));
            g_printerr("Region %s \n", g_strdup_value_contents(indices));
        } else if (fieldName == "motion") {
            indices = gst_structure_get_value(structure, "motion_cells_indices");
            timestamp = gst_structure_get_value(structure, "motion");
            //g_printerr("Motion.\nTimestamp %s, ", g_strdup_value_contents(timestamp));
            //g_printerr("Region %s \n", g_strdup_value_contents(indices));
        } else if (fieldName == "motion_finished") {
            timestamp = gst_structure_get_value(structure, "motion_finished");
            //g_printerr("Motion finished.\nTimestamp %s \n", g_strdup_value_contents(timestamp));
        }
    } else if ((std::string)name == "sink" ) {
        GstNavigationMessageType nav = gst_navigation_message_get_type (msg);
        const gchar *key;
        GstEvent *eve;
        gint button;
        gdouble x, y;

        if (nav == GST_NAVIGATION_MESSAGE_EVENT) {
            if (gst_navigation_message_parse_event(msg, &eve)) {
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
                            g_printerr("Released key: 0x%x \n", g_ascii_tolower(key[0]));
                        break;
                    case GST_NAVIGATION_EVENT_MOUSE_BUTTON_PRESS:
                        if (gst_navigation_event_parse_mouse_button_event(eve, &button, &x, &y)) {
                            g_printerr("%f %f %d \n", x, y, button);
                        }
                        break;
                    case GST_NAVIGATION_EVENT_MOUSE_BUTTON_RELEASE:
                        if (gst_navigation_event_parse_mouse_button_event(eve, &button, &x, &y)) {
                            g_printerr("%f %f %d \n", x, y, button);
                        }
                        break;
                    case GST_NAVIGATION_EVENT_MOUSE_MOVE:
                        if (gst_navigation_event_parse_mouse_move_event(eve, &x, &y)) {
                            g_printerr("%f %f \n", x, y);
                        }
                        break;
                    default:
                        break;
                }
            }
        }
    }

    switch (GST_MESSAGE_TYPE (msg)) {
        case GST_MESSAGE_EOS:
            //g_print ("End of stream\n");
            g_main_loop_quit (data->loop);
            break;
        case GST_MESSAGE_ERROR:
            {
                gchar *debug;
                GError *error;
                gst_message_parse_error (msg, &error, &debug);
                g_free (debug);
                g_printerr ("Error: %s\n", error->message);
                g_error_free (error);
                g_main_loop_quit (data->loop);
                break;
            }
        default:
            break;
    }
    return TRUE;
}

int main(int argc, char *argv[])
{
    struct customData data;
    GIOChannel *io_stdin;
    gst_init(&argc, &argv);
    data.loop = g_main_loop_new (NULL, FALSE);

    data.pipeline = gst_pipeline_new ("streaming");
    data.source = gst_element_factory_make ("v4l2src","videosource");
    data.videoparse1 = gst_element_factory_make("autovideoconvert","videoparse1");
    data.motion = gst_element_factory_make("motioncells", "motion");
    data.videoparse2 = gst_element_factory_make("autovideoconvert", "videoparse2");
    data.overlay = gst_element_factory_make("cairooverlay", "overlay");
    data.videoparse3 = gst_element_factory_make("autovideoconvert", "videoparse3");
    data.sink = gst_element_factory_make ("xvimagesink", "sink");

    g_object_set(data.sink, "sync", false, NULL);

    g_object_set(data.motion, "gridx", 8, NULL );
    g_object_set(data.motion, "gridy", 8, NULL );
    g_object_set(data.motion, "sensitivity", 5.0, NULL );
    g_object_set(data.motion, "threshold", 0.01, NULL );
    g_object_set(data.motion, "datafile", "plik" , NULL );
    g_object_set(data.motion, "datafileextension", "rgb" , NULL );
    g_object_set(data.motion, "gap", 1 , NULL);
    g_object_set(data.motion, "postallmotion", false, NULL);
    //g_object_set(data.motion, "motionmaskcoords", "1:1,5:5", NULL);

    // allocate on heap for pedagogical reasons, makes code easier to transfer
    CairoOverlayState *overlay_state = g_new0(CairoOverlayState, 1);

    // Hook up the neccesary signals for cairooverlay
    g_signal_connect(data.overlay, "draw",
            G_CALLBACK(draw_overlay), overlay_state);
    g_signal_connect(data.overlay, "caps-changed",
            G_CALLBACK (prepare_overlay), overlay_state);

    if (!data.pipeline ||
            !data.source ||
            !data.videoparse1 ||
            !data.motion ||
            !data.videoparse2 ||
            !data.overlay ||
            !data.videoparse3 ||
            !data.sink ) {
        g_printerr ("Error");
        return -1;
    }

    data.bus = gst_pipeline_get_bus(GST_PIPELINE (data.pipeline));
    /* Add a keyboard watch so we get notified of keystrokes */
    io_stdin = g_io_channel_unix_new(fileno (stdin));

    data.bus_watch_id = g_io_add_watch(io_stdin, G_IO_IN, (GIOFunc)handle_keyboard, data.loop);
    GDestroyNotify notify = NULL;
    gst_bus_set_sync_handler(data.bus, (GstBusSyncHandler)bus_call, &data, notify);
    g_print("%d\n", data.bus_watch_id);
    gst_object_unref(data.bus);

    gst_bin_add_many(GST_BIN(data.pipeline),
            data.source,
            data.videoparse1,
            data.motion,
            data.videoparse2,
            data.overlay,
            data.videoparse3,
            data.sink,
            NULL);
    gst_element_link_many(data.source,
            data.videoparse1,
            data.motion,
            data.videoparse2,
            data.overlay,
            data.videoparse3,
            data.sink,
            NULL);

    g_print("Now playing\n");
    gst_element_set_state(data.pipeline, GST_STATE_PLAYING);

    /*Iterate*/
    g_print("Running...\n");
    g_main_loop_run(data.loop);

    /*Out of the main loop, clean up nicely*/
    g_print("Returned, stopping playback\n");
    gst_element_set_state(data.pipeline, GST_STATE_NULL);
    g_print("Deleting pipeline\n");
    g_io_channel_unref(io_stdin);
    gst_element_set_state(data.pipeline, GST_STATE_NULL);
    gst_object_unref(GST_OBJECT (data.pipeline));
    g_source_remove(data.bus_watch_id);

    g_main_loop_unref(data.loop);

    return 0;
}

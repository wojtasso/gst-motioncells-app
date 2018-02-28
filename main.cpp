#include <iostream>
#include <gst/gst.h>
#include <glib.h>
#include <stdio.h>
#include <string.h>

using namespace std;

struct customData {
    GstElement *pipeline,
               *source,
               *videoparse1,
               *motion,
               *videoparse2,
               *sink;
    GstBus *bus;
    guint bus_watch_id;

    GMainLoop *loop;
};

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

static gboolean bus_call(GstBus *bus, GstMessage *msg, customData *data)
{
    guint numberOfFields;
    const gchar *name = GST_MESSAGE_SRC_NAME(msg);
    const GstStructure * structure = gst_message_get_structure(msg);
    numberOfFields = gst_structure_n_fields(structure);

    if ((string)name == "motion") {

        const GValue *timestamp, *indices;
        string fieldName = (string)gst_structure_nth_field_name(
                structure,
                numberOfFields - 1);

        if (fieldName == "motion_begin") {
            indices = gst_structure_get_value(structure, "motion_cells_indices");
            timestamp = gst_structure_get_value(structure, "motion_begin");
            g_printerr("Motion begin.\nTimestamp %s, ", g_strdup_value_contents(timestamp));
            g_printerr("Region %s \n", g_strdup_value_contents(indices));
        } else if (fieldName == "motion") {
            indices = gst_structure_get_value(structure, "motion_cells_indices");
            timestamp = gst_structure_get_value(structure, "motion");
            g_printerr("Motion.\nTimestamp %s, ", g_strdup_value_contents(timestamp));
            g_printerr("Region %s \n", g_strdup_value_contents(indices));
        } else if (fieldName == "motion_finished") {
            timestamp = gst_structure_get_value(structure, "motion_finished");
            g_printerr("Motion finished.\nTimestamp %s \n", g_strdup_value_contents(timestamp));
        }
    } else if ((string)name == "sink-actual-sink-xvimage" ) {
        // GstNavigationMessage
        // type
        // event
        const GValue *type, *event;
        type = gst_structure_get_value(structure, "type");
        event = gst_structure_get_value(structure, "event");
        g_printerr("Type %s \n", g_strdup_value_contents(type));
        g_printerr("Event %s \n", g_strdup_value_contents(event));


        //g_print("Got %s \n", GST_MESSAGE_SRC_NAME(msg));
        //g_printerr("Number of structure fields: %d \n", numberOfFields);
        //g_printerr("  %s \n" ,gst_structure_get_name(structure));
        //g_printerr("  %s \n", gst_structure_nth_field_name(structure,0));
        //g_printerr("  %s \n", gst_structure_nth_field_name(structure,1));
    }

        //if (numberOfFields == 2) { //motion begin

        //g_printerr("  %s " ,gst_structure_get_name(structure));
        //g_printerr("  %s \n", gst_structure_nth_field_name(structure,0));
        //g_printerr("  %s \n", gst_structure_nth_field_name(structure,1));

        //typ = gst_structure_get_field_type(structure,"motion_cells_indices");
        //g_printerr("  %s  ",g_type_name(typ));
        //typ = gst_structure_get_field_type(structure,"motion_begin");
        //g_printerr("  %s  ",g_type_name(typ));
        //arg1 = gst_structure_get_value(structure, "motion_cells_indices");
        //arg2 = gst_structure_get_value(structure, "motion_begin");
        //value=g_strdup_value_contents(arg1);
        //g_printerr("%s \n",value);
        //g_printerr("%s \n" ,gst_structure_nth_field_name(structure,1));
        //value=g_strdup_value_contents(arg2);
        //g_printerr("%s \n",value);

    switch (GST_MESSAGE_TYPE (msg)) {
        case GST_MESSAGE_EOS:
            g_print ("End of stream\n");
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
    data.sink = gst_element_factory_make ("autovideosink", "sink");

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

    if (!data.pipeline ||
            !data.source ||
            !data.videoparse1 ||
            !data.motion ||
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
            data.sink,
            NULL);
    gst_element_link_many(data.source,
            data.videoparse1,
            data.motion,
            data.videoparse2,
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

#include <iostream>
#include <gst/gst.h>
#include <glib.h>
#include <stdio.h>
#include <string.h>

#define UDPSRC_CAPS "application/x-rtp, encoding-name=JPEG, payload=26"


using namespace std;

typedef struct __CustomData {
    GstElement *video_queue, *motion_queue, *pipeline,*sink1, *source, *rtpjpeg, *decoder,
               *videoparse1, *videoparse2, *motion, *tee, *sink, *opencvtextoverlay;
    GstBus *bus;
    guint bus_watch_id;


    GMainLoop *loop;
} CustomData;


static gboolean handle_keyboard (GIOChannel *source, GIOCondition cond, gpointer data) {
    gchar *str = NULL;
    GMainLoop *loop = (GMainLoop*) data;
    if (g_io_channel_read_line (source, &str, NULL, NULL, NULL) != G_IO_STATUS_NORMAL) {
        return TRUE;
    }

    switch (g_ascii_tolower (str[0])) {
        case 'q':
            g_main_loop_quit (loop);
            break;
        default:
            break;
    }

    g_free (str);

    return TRUE;
}

static gboolean bus_call (GstBus *bus, GstMessage *msg, CustomData *data){

    GType typ;
    const GValue *arg1,*arg2;
    guint z;
    g_print("Got %s", GST_MESSAGE_SRC_NAME (msg) );
    const gchar *msgtype, *tmp="motion_cells_indices ";
    const gchar *value, *name = GST_MESSAGE_SRC_NAME (msg);
    msgtype = gst_message_type_get_name(GST_MESSAGE_TYPE(msg));
    //g_printerr("  %s " ,msgtype);
    const GstStructure * structure = gst_message_get_structure(msg);
    string x;
    z=gst_structure_n_fields(structure);
    if((string)name=="motion" && z==2){

        //g_printerr("  %s " ,gst_structure_get_name(structure));
        g_printerr("  %s " ,gst_structure_nth_field_name(structure,0));

        //typ = gst_structure_get_field_type(structure,"motion_cells_indices");
        //g_printerr("  %s  ",g_type_name(typ));
        //typ = gst_structure_get_field_type(structure,"motion_begin");
        //g_printerr("  %s  ",g_type_name(typ));
        arg1 = gst_structure_get_value(structure, "motion_cells_indices");
        arg2 = gst_structure_get_value(structure, "motion_begin");
        value=g_strdup_value_contents(arg1);				//Odczyt współrzędnych
        g_printerr("%s \n",value);
        g_printerr("%s " ,gst_structure_nth_field_name(structure,1));
        value=g_strdup_value_contents(arg2);
        g_printerr("%s",value);

        //g_object_get(motion, "motionmaskcoords", x, NULL);
        //g_printerr("  %s", x);
    }else if((string)name=="motion" && z==1){
        g_printerr("  %s " ,gst_structure_nth_field_name(structure,0));
        arg1 = gst_structure_get_value(structure, "motion_finished");
        value=g_strdup_value_contents(arg1);
        g_printerr("%s \n",value);
    }

    g_printerr("\n");


    //motion_cells_indices   motion_begin motion_finished
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
    cout << "Hello !!!  "<< endl;
    CustomData data;
    GIOChannel *io_stdin;

    gst_init(&argc, &argv);

    data.loop = g_main_loop_new (NULL, FALSE);

    data.pipeline = gst_pipeline_new ("streaming");
    data.source = gst_element_factory_make ("udpsrc","videosource");
    data.rtpjpeg = gst_element_factory_make ("rtpjpegdepay", "rtp");
    data.decoder = gst_element_factory_make ("jpegdec", "jpegdecoder");
    data.videoparse1 = gst_element_factory_make("videoparse","videoparse1");
    data.videoparse2 = gst_element_factory_make("videoparse","videoparse2");
    data.motion = gst_element_factory_make("motioncells", "motion");
    data.sink = gst_element_factory_make ("autovideosink", "sink");
    data.sink1 = gst_element_factory_make ("autovideosink", "sink1");
    data.tee = gst_element_factory_make("tee", "tee");
    data.video_queue = gst_element_factory_make ("queue", "video_queue");
    data.motion_queue = gst_element_factory_make ("queue", "motion_queue");
    //data.opencvtextoverlay = gst_element_factory_make ("opencvtextoverlay", "opencvtext");

    //g_object_set(data.opencvtextoverlay, "text", "a", NULL);
    //g_object_set(data.opencvtextoverlay, "xpos", 100, NULL);
    //g_object_set(data.opencvtextoverlay, "ypos", 100, NULL);

    g_object_set(data.tee, "name", "local", NULL);

    g_object_set(data.source, "port", 5000, NULL);
    g_object_set(data.sink, "sync", false, NULL);
    g_object_set(data.sink1, "sync", false, NULL);

    g_object_set(data.videoparse1, "format", 15, NULL );	//GST_VIDEO_FORMAT_RGB
    g_object_set(data.videoparse1, "width", 640, NULL );
    g_object_set(data.videoparse1, "height", 480, NULL );
    //g_object_set(videoparse1, "framesize", 900000, NULL );

    g_object_set(data.videoparse2, "format",  2, NULL ); //GST_VIDEO_FORMAT_I420
    g_object_set(data.videoparse2, "width", 640, NULL );
    g_object_set(data.videoparse2, "height", 480, NULL );


    g_object_set(data.motion, "gridx", 8, NULL );
    g_object_set(data.motion, "gridy", 8, NULL );
    g_object_set(data.motion, "sensitivity", 0.4, NULL );
    g_object_set(data.motion, "threshold", 0.01, NULL );
    g_object_set(data.motion, "datafile", "plik" , NULL );
    g_object_set(data.motion, "datafileextension", "rgb" , NULL );
    g_object_set(data.motion, "gap", 1 , NULL);
    g_object_set(data.motion, "postallmotion", false, NULL);
    //g_object_set(data.motion, "motionmaskcoords", "1:1,5:5", NULL);

    gchar *udpsrc_caps_text;
    GstCaps *udpsrc_caps;
    udpsrc_caps_text = g_strdup_printf(UDPSRC_CAPS);
    udpsrc_caps = gst_caps_from_string(udpsrc_caps_text);
    g_object_set(data.source, "caps", udpsrc_caps, NULL);
    gst_caps_unref(udpsrc_caps);
    g_free(udpsrc_caps_text);


    if(!data.pipeline || !data.source || !data.rtpjpeg || !data.decoder || !data.videoparse1 ||
            !data.tee || !data.videoparse2 ||	!data.motion || !data.sink ){
        g_printerr ("Blad");
        return -1;
    }

    data.bus = gst_pipeline_get_bus (GST_PIPELINE (data.pipeline));
    //bus_watch_id = gst_bus_add_watch (bus, bus_call, loop);
    /* Add a keyboard watch so we get notified of keystrokes */
#ifdef _WIN32
    io_stdin = g_io_channel_win32_new_fd (fileno (stdin));
#else
    io_stdin = g_io_channel_unix_new (fileno (stdin));
#endif

    data.bus_watch_id=g_io_add_watch (io_stdin, G_IO_IN, (GIOFunc)handle_keyboard, data.loop);
    GDestroyNotify notify;
    gst_bus_set_sync_handler(data.bus, (GstBusSyncHandler)bus_call, &data, notify);
    g_print("%d\n", data.bus_watch_id);
    gst_object_unref(data.bus);

    /*gst_bin_add_many(GST_BIN(data.pipeline), data.source, data.rtpjpeg, data.decoder, data.videoparse1,
      data.motion, data.videoparse2, data.sink, NULL);
      gst_element_link_many (data.source, data.rtpjpeg, data.decoder, data.videoparse1, data.motion,
      data.videoparse2, data.sink, NULL);*/
    gst_bin_add_many(GST_BIN(data.pipeline), data.source, data.rtpjpeg, data.decoder, data.motion_queue,
            data.video_queue, data.tee, data.videoparse1, data.motion, data.videoparse2, data.sink,
            data.sink1, NULL);
    gst_element_link_many (data.source, data.rtpjpeg, data.decoder,  data.videoparse1, data.motion, data.tee, NULL);
    gst_element_link_many (data.video_queue, data.sink, NULL);
    gst_element_link_many(  data.motion_queue, data.videoparse2, data.sink1, NULL);

    GstPadTemplate *tee_src_pad_template;
    GstPad *tee_q1_pad, *tee_q2_pad;
    GstPad *q1_pad, *q2_pad;


    if ( !(tee_src_pad_template = gst_element_class_get_pad_template (GST_ELEMENT_GET_CLASS (data.tee), "src_%u"))) {
        gst_object_unref (data.pipeline);
        g_critical ("Unable to get pad template");
        return 0;
    }


    tee_q1_pad = gst_element_request_pad (data.tee, tee_src_pad_template, NULL, NULL);
    g_print ("Obtained request pad %s for q1 branch.\n", gst_pad_get_name (tee_q1_pad));
    q1_pad = gst_element_get_static_pad (data.video_queue, "sink");

    tee_q2_pad = gst_element_request_pad (data.tee, tee_src_pad_template, NULL, NULL);
    g_print ("Obtained request pad %s for q2 branch.\n", gst_pad_get_name (tee_q2_pad));
    q2_pad = gst_element_get_static_pad (data.motion_queue, "sink");


    if (gst_pad_link (tee_q1_pad, q1_pad) != GST_PAD_LINK_OK ){

        g_critical ("Tee for q1 could not be linked.\n");
        gst_object_unref (data.pipeline);
        return 0;
    }


    if (gst_pad_link (tee_q2_pad, q2_pad) != GST_PAD_LINK_OK) {
        g_critical ("Tee for q2 could not be linked.\n");
        gst_object_unref (data.pipeline);
        return 0;
    }

    gst_object_unref (q1_pad);
    gst_object_unref (q2_pad);

    g_print ("Now playing\n");
    gst_element_set_state (data.pipeline, GST_STATE_PLAYING);

    /*Iterate*/
    g_print ("Running...\n");
    g_main_loop_run (data.loop);

    /*Out of the main loop, clean up nicely*/
    gst_element_release_request_pad (data.tee, tee_q1_pad);
    gst_element_release_request_pad (data.tee, tee_q2_pad);
    gst_object_unref (tee_q1_pad);
    gst_object_unref (tee_q2_pad);

    g_print ("Returned, stopping playback\n");
    gst_element_set_state (data.pipeline, GST_STATE_NULL);
    g_print ("Deleting pipeline\n");
    g_io_channel_unref (io_stdin);
    gst_element_set_state (data.pipeline, GST_STATE_NULL);
    gst_object_unref (GST_OBJECT (data.pipeline));
    g_source_remove (data.bus_watch_id);

    g_main_loop_unref (data.loop);



    return 0;

}

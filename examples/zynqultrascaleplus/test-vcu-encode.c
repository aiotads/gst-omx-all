/*
*
* Test application to showcase VCU dynamic features
*
* Copyright (C) 2018 Xilinx
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
* THE SOFTWARE.
*/

#include <gst/gst.h>
#include <gst/video/video.h>

#include <glib.h>
#include <stdlib.h>
#include <stdio.h>
#include "OMX_VideoExt.h"

#define DEFAULT_VIDEO_WIDTH 3840
#define DEFAULT_VIDEO_HEIGHT 2160
#define DEFAULT_ENCODER_FRAM_ERATE 30
#define DEFAULT_ENCODER_GOP_LENGTH 30
#define DEFAULT_ENCODER_CONTROL_RATE 2
#define DEFAULT_ENCODER_TARGET_BITRATE 5000
#define DEFAULT_ENCODER_B_FRAMES 0
#define DEFAULT_ENCODER_TYPE "avc"
#define DEFAULT_LONGTERM_FREQ 0
#define DEFAULT_LONGTERM_REF 0

#define DYNAMIC_BITRATE_STR "BR"
#define DYNAMIC_GOP_LENGTH_STR "GL"
#define DYNAMIC_B_FRAMES_STR "BFrm"
#define DYNAMIC_ROI_STR "ROI"
#define DYNAMIC_KEY_FRAME_STR "KF"
#define DYNAMIC_SCENE_CHANGE_STR "SC"
#define DYNAMIC_INSERT_LONGTERM_STR "IL"
#define DYNAMIC_USE_LONGTERM_STR "UL"

#define DYNAMIC_FEATURE_DELIMIT ","
#define DYNAMIC_PARAM_DELIMIT ":x"


typedef enum
{
  DYNAMIC_BIT_RATE,
  DYNAMIC_GOP_LENGTH,
  DYNAMIC_KEY_FRAME,
  DYNAMIC_B_FRAMES,
  DYNAMIC_ROI,
  DYNAMIC_SCENE_CHANGE,
  DYNAMIC_INSERT_LONGTERM,
  DYNAMIC_USE_LONGTERM,
} DynamicFeatureType;

typedef struct
{
  DynamicFeatureType type;
  guint start_frame;

  union
  {
    struct
    {
      guint x;
      guint y;
      guint width;
      guint height;
      gchar *quality;
    } roi;
    guint value;
  } param;
} DynamicFeature;

typedef struct
{
  guint width;
  guint height;
  guint framerate;
  guint control_rate;
  guint gop_length;
  guint b_frames;
  guint max_bitrate;
  guint target_bitrate;
  guint long_term_freq;
  guint long_term_ref;

  gchar *output_filename;
  gchar *input_filename;
  gchar *dynamic_str;
  gchar *type;
} EncoderSettings;

/* Globals */
EncoderSettings enc;
DynamicFeature *dynamic;

static DynamicFeatureType
get_dynamic_str_enum (gchar * user_string)
{
  if (!g_strcmp0 (user_string, DYNAMIC_BITRATE_STR))
    return DYNAMIC_BIT_RATE;
  else if (!g_strcmp0 (user_string, DYNAMIC_GOP_LENGTH_STR))
    return DYNAMIC_GOP_LENGTH;
  else if (!g_strcmp0 (user_string, DYNAMIC_KEY_FRAME_STR))
    return DYNAMIC_KEY_FRAME;
  else if (!g_strcmp0 (user_string, DYNAMIC_B_FRAMES_STR))
    return DYNAMIC_B_FRAMES;
  else if (!g_strcmp0 (user_string, DYNAMIC_ROI_STR))
    return DYNAMIC_ROI;
  else if (!g_strcmp0 (user_string, DYNAMIC_SCENE_CHANGE_STR))
    return DYNAMIC_SCENE_CHANGE;
  else if (!g_strcmp0 (user_string, DYNAMIC_INSERT_LONGTERM_STR))
    return DYNAMIC_INSERT_LONGTERM;
  else if (!g_strcmp0 (user_string, DYNAMIC_USE_LONGTERM_STR))
    return DYNAMIC_USE_LONGTERM;
  else {
    g_print ("Invalid User string \n");
    return -1;
  }
}

static gboolean
bus_call (GstBus * bus, GstMessage * msg, gpointer data)
{
  GMainLoop *loop = (GMainLoop *) data;

  switch (GST_MESSAGE_TYPE (msg)) {

    case GST_MESSAGE_EOS:
      g_print ("End of stream\n");
      g_main_loop_quit (loop);
      break;

    case GST_MESSAGE_ERROR:{
      gchar *debug;
      GError *error;

      gst_message_parse_error (msg, &error, &debug);
      g_free (debug);

      g_printerr ("Error: %s\n", error->message);
      g_error_free (error);

      g_main_loop_quit (loop);
      break;
    }
    default:
      break;
  }

  return TRUE;
}

static gboolean
check_parameters ()
{
  if (!enc.type)
    enc.type = g_strdup (DEFAULT_ENCODER_TYPE);

  if (!enc.input_filename) {
    g_print
        ("please provide input-filename argument, use --help option for more details\n");
    return FALSE;
  }

  if (!enc.output_filename) {
    g_print
        ("please provide output-filename argument, use --help option for more details\n");
    return FALSE;
  }

  return TRUE;
}

static DynamicFeature *
parse_dynamic_user_string (const char *str, GstElement * encoder)
{
  gchar **token;
  /* Allocate memory for DynamicFeature */
  dynamic = (DynamicFeature *) g_new (DynamicFeature, 1);

  token = g_strsplit_set (str, DYNAMIC_PARAM_DELIMIT, -1);

  dynamic->type = get_dynamic_str_enum (*token);

  switch (dynamic->type) {
    case DYNAMIC_BIT_RATE:
    case DYNAMIC_GOP_LENGTH:
    case DYNAMIC_B_FRAMES:
      dynamic->start_frame = atoi (token[1]);
      dynamic->param.value = atoi (token[2]);
      break;
    case DYNAMIC_KEY_FRAME:
      dynamic->start_frame = atoi (token[1]);
      break;
    case DYNAMIC_ROI:
      dynamic->start_frame = atoi (token[1]);
      dynamic->param.roi.x = atoi (token[2]);
      dynamic->param.roi.y = atoi (token[3]);
      dynamic->param.roi.width = atoi (token[4]);
      dynamic->param.roi.height = atoi (token[5]);
      dynamic->param.roi.quality = g_strdup (token[6]);

      /* set encoder qp-mode to ROI */
      g_object_set (G_OBJECT (encoder), "qp-mode", OMX_ALG_ROI_QP, NULL);
      break;
    case DYNAMIC_SCENE_CHANGE:
      dynamic->start_frame = atoi (token[1]);
      dynamic->param.value = atoi (token[2]);
      break;
    case DYNAMIC_INSERT_LONGTERM:
      dynamic->start_frame = atoi (token[1]);
      break;
    case DYNAMIC_USE_LONGTERM:
      dynamic->start_frame = atoi (token[1]);
      break;
    default:
      g_print ("Invalid DynamicFeatureType \n");
      g_strfreev (token);
      return NULL;
  }
  g_strfreev (token);
  return dynamic;
}

static gboolean
send_downstream_event (GstPad * pad, GstStructure * s)
{
  GstEvent *event;
  GstPad *peer;

  event = gst_event_new_custom (GST_EVENT_CUSTOM_DOWNSTREAM, s);
  peer = gst_pad_get_peer (pad);

  if (!gst_pad_send_event (peer, event))
    g_printerr ("Failed to send custom event\n");

  gst_object_unref (peer);

  return TRUE;
}

static GstPadProbeReturn
videoparser_src_buffer_probe (GstPad * pad, GstPadProbeInfo * info,
    gpointer user_data)
{
  GstBuffer *buffer;
  GstElement *encoder = user_data;
  GstVideoRegionOfInterestMeta *meta;
  GstEvent *event;

  static int framecount = 0;

  buffer = gst_pad_probe_info_get_buffer (info);

  if (framecount == dynamic->start_frame) {

    switch (dynamic->type) {

      case DYNAMIC_BIT_RATE:
        g_print (" Changing video target bitrate to %d kbps at frame %d \n",
            dynamic->param.value, framecount);
        g_object_set (G_OBJECT (encoder), "target-bitrate",
            dynamic->param.value, NULL);
        break;
      case DYNAMIC_GOP_LENGTH:
        g_print (" Changing encoder gop_length value to %d at frame %d \n",
            dynamic->param.value, framecount);
        g_object_set (G_OBJECT (encoder), "gop-length", dynamic->param.value,
            NULL);
        break;
      case DYNAMIC_B_FRAMES:
        g_print (" Changing encoder b_frames count to %d at frame %d \n",
            dynamic->param.value, framecount);
        g_object_set (G_OBJECT (encoder), "b-frames", dynamic->param.value,
            NULL);
        break;
      case DYNAMIC_KEY_FRAME:
        g_print (" Inserting Key Frame at Frame num = %d \n",
            dynamic->start_frame);
        event =
            gst_video_event_new_downstream_force_key_unit (GST_BUFFER_PTS
            (buffer), GST_CLOCK_TIME_NONE, GST_CLOCK_TIME_NONE, FALSE, 1);
        gst_pad_push_event (pad, event);
        break;
      case DYNAMIC_ROI:
        g_print (" Addding ROI at pos = %d X %d, wxh = %dx%d, quality = %s \n",
            dynamic->param.roi.x, dynamic->param.roi.y,
            dynamic->param.roi.width, dynamic->param.roi.height,
            dynamic->param.roi.quality);
        meta =
            gst_buffer_add_video_region_of_interest_meta (buffer,
            "face", dynamic->param.roi.x,
            dynamic->param.roi.y, dynamic->param.roi.width,
            dynamic->param.roi.height);
        g_assert (meta);

        gst_video_region_of_interest_meta_add_param (meta,
            gst_structure_new ("roi/omx-alg",
                "quality", G_TYPE_STRING, dynamic->param.roi.quality, NULL));
        break;
      case DYNAMIC_SCENE_CHANGE:
      {
        GstStructure *s;

        g_print ("Scene change at Frame num = %d in %d frames\n",
            dynamic->start_frame, dynamic->param.value);

        s = gst_structure_new ("omx-alg/scene-change",
            "look-ahead", G_TYPE_UINT, dynamic->param.value, NULL);
        send_downstream_event (pad, s);
      }
        break;
      case DYNAMIC_INSERT_LONGTERM:
      {
        GstStructure *s;

        g_print ("Inserting Longterm picture at Frame num = %d \n",
            dynamic->start_frame);

        s = gst_structure_new_empty ("omx-alg/insert-longterm");
        send_downstream_event (pad, s);
      }
        break;
      case DYNAMIC_USE_LONGTERM:
      {
        GstStructure *s;

        g_print ("Using Longterm reference picture for Frame num = %d \n",
            dynamic->start_frame);

        s = gst_structure_new_empty ("omx-alg/use-longterm");
        send_downstream_event (pad, s);
      }
        break;
      default:
        g_print ("Invalid Dynamic String \n");
    }
  }

  framecount++;
  return GST_PAD_PROBE_OK;
}

int
main (int argc, char *argv[])
{
  GMainLoop *loop;
  GstElement *pipeline, *source, *sink, *encoder, *videoparse, *enc_queue,
      *enc_capsfilter;
  GstCaps *enc_caps;
  GstPad *pad;
  GstBus *bus;
  GError *error = NULL;
  GOptionContext *context;

  static GOptionEntry entries[] = {
    {"width", 'w', 0, G_OPTION_ARG_INT, &enc.width, "width of the Video frame",
        NULL},
    {"height", 'h', 0, G_OPTION_ARG_INT, &enc.height,
          "Height of the Video frame",
        NULL},
    {"framerate", 'f', 0, G_OPTION_ARG_INT, &enc.framerate, "Video Framerate",
        NULL},
    {"control-rate", 'c', 0, G_OPTION_ARG_INT, &enc.control_rate,
        "Rate Control Mode of the Encoder, 1: VBR, 2: CBR", NULL},
    {"b-frames", 'b', 0, G_OPTION_ARG_INT, &enc.b_frames,
        "Num B-frames between consequetive P-frames", NULL},
    {"target-bitrate", 'r', 0, G_OPTION_ARG_INT, &enc.target_bitrate,
        "Bitrate setting in Kbps", NULL},
    {"max-bitrate", 'm', 0, G_OPTION_ARG_INT, &enc.max_bitrate,
        "Max-Bitrate setting in Kbps", NULL},
    {"gop-length", 'g', 0, G_OPTION_ARG_INT, &enc.gop_length,
        "Gop-Length setting of the Encoder", NULL},
    {"output-filename", 'o', 0, G_OPTION_ARG_FILENAME, &enc.output_filename,
        "Output filename", NULL},
    {"input-filename", 'i', 0, G_OPTION_ARG_FILENAME, &enc.input_filename,
        "Input filename", NULL},
    {"encoder-type", 'e', 0, G_OPTION_ARG_STRING, &enc.type,
          "Encoder codec secltion, use -e avc for H264 and -e hevc for H265",
        NULL},
    {"dynamic-str", 'd', 0, G_OPTION_ARG_STRING, &enc.dynamic_str,
          "Dynamic feature string, pattern should be 'Dynamic_feature_str:Frame_number:Value'",
        NULL},
    {"long-term-ref", 'l', 0, G_OPTION_ARG_INT, &enc.long_term_ref,
        "Enable longterm reference pictures", NULL},
    {"long-term-freq", 'u', 0, G_OPTION_ARG_INT, &enc.long_term_freq,
        "Periodicity of longterm ref pictures", NULL},
    {NULL}
  };

  const char *summary =
      "Dynamic Bitrate Ex: ./zynqmp_vcu_encode -w 3840 -h 2160 -e avc -f 30 -c 2 -g 30 -o /run/op.h264 -i /run/input.yuv -d BR:100:1000 \nDynamic Bframes Ex: ./zynqmp_vcu_encode -w 3840 -h 2160 -e hevc -f 30 -c 2 -g 30 -b 4 -o /run/op.h265 -i /run/input.yuv -d BFrm:10:2 \nROI Ex: ./zynqmp_vcu_encode -w 3840 -h 2160 -e avc -f 30 -c 2 -g 30 -o /run/op.h264 -i /run/input.yuv -d ROI:1200x300:200x200:high \n\nDynamic-string pattern should be:\n'BR:frm_num:new_value_in_kbps' -> Dynamic Bitrate\n'BFrm:frame_num:new_value' -> Dynamic Bframes \n'KF:frame_num' -> Key Frame Insertion \n'GL:frame_num:new_value' -> Dynamic GOP length \n'ROI:frame_num:XPOSxYPOS:roi_widthxroi_height:roi_type' -> ROI string \n'IL:frame_num' -> Mark longterm reference picture \n'UL:frame_num -> Use longterm picture ";

  /* Set Encoder defalut parameters */
  enc.width = DEFAULT_VIDEO_WIDTH;
  enc.height = DEFAULT_VIDEO_HEIGHT;
  enc.framerate = DEFAULT_ENCODER_FRAM_ERATE;
  enc.control_rate = DEFAULT_ENCODER_CONTROL_RATE;
  enc.gop_length = DEFAULT_ENCODER_GOP_LENGTH;
  enc.b_frames = DEFAULT_ENCODER_B_FRAMES;
  enc.target_bitrate = DEFAULT_ENCODER_TARGET_BITRATE;
  enc.max_bitrate = DEFAULT_ENCODER_TARGET_BITRATE;
  enc.long_term_ref = DEFAULT_LONGTERM_REF;
  enc.long_term_freq = DEFAULT_LONGTERM_FREQ;

  context = g_option_context_new (" vcu encode test applicaiton");
  g_option_context_set_summary (context, summary);
  g_option_context_add_main_entries (context, entries, NULL);
  g_option_context_add_group (context, gst_init_get_option_group ());
  if (!g_option_context_parse (context, &argc, &argv, &error)) {
    g_print ("option parsing failed: %s\n", error->message);
    g_option_context_free (context);
    g_clear_error (&error);
    return -1;
  }
  g_option_context_free (context);

  if (!check_parameters ())
    return -1;

  /* Initialization */
  gst_init (&argc, &argv);

  loop = g_main_loop_new (NULL, FALSE);

  /* Create Gstreamer elements */
  pipeline = gst_pipeline_new ("video-player");

  source = gst_element_factory_make ("filesrc", "File Source");
  videoparse = gst_element_factory_make ("rawvideoparse", "Video parser");
  if (!g_strcmp0 (enc.type, "avc"))
    encoder = gst_element_factory_make ("omxh264enc", "OMX H264 Encoder");
  else
    encoder = gst_element_factory_make ("omxh265enc", "OMX H265 Encoder");

  enc_capsfilter =
      gst_element_factory_make ("capsfilter", "Encoder output caps");
  enc_queue = gst_element_factory_make ("queue", "Encoder Queue");
  sink = gst_element_factory_make ("filesink", "File Sink");

  if (!pipeline || !source || !sink || !videoparse || !encoder || !enc_queue
      || !enc_capsfilter) {
    g_printerr ("elements could not be created \n");
    return -1;
  }

  /* set element properties */
  g_object_set (G_OBJECT (source), "location", enc.input_filename, NULL);
  g_object_set (G_OBJECT (videoparse), "width", enc.width, "height", enc.height,
      "format", GST_VIDEO_FORMAT_NV12, "framerate", enc.framerate, 1, NULL);
  g_object_set (G_OBJECT (encoder), "target-bitrate", enc.target_bitrate,
      "b-frames", enc.b_frames, "control-rate", enc.control_rate, "gop-length",
      enc.gop_length, "long-term-ref", enc.long_term_ref, "long-term-freq",
      enc.long_term_freq, NULL);

  /* set Encoder src caps */
  if (!g_strcmp0 (enc.type, "avc"))
    enc_caps =
        gst_caps_new_simple ("video/x-h264", "profile", G_TYPE_STRING, "high",
        NULL);
  else
    enc_caps =
        gst_caps_new_simple ("video/x-h265", "profile", G_TYPE_STRING, "main",
        NULL);

  g_object_set (G_OBJECT (enc_capsfilter), "caps", enc_caps, NULL);
  g_object_set (G_OBJECT (sink), "location", enc.output_filename, NULL);

  g_print
      ("Using width = %d height = %d framerate = %d codec = %s target-bitrate = %d control-rate = %d b-frames = %d output-location = %s\n",
      enc.width, enc.height, enc.framerate, enc.type, enc.target_bitrate,
      enc.control_rate, enc.b_frames, enc.output_filename);

  if (enc.control_rate == 1) {
    g_object_set (G_OBJECT (encoder), "max-bitrate", enc.max_bitrate, NULL);
    g_print ("max-bitrate = %d\n", enc.max_bitrate);
  }

  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
  gst_bus_add_watch (bus, bus_call, loop);

  /* Add elements into pipeline */
  gst_bin_add_many (GST_BIN (pipeline), source, videoparse, encoder,
      enc_capsfilter, enc_queue, sink, NULL);

  /* link the elements */
  if (!gst_element_link_many (source, videoparse, encoder, enc_capsfilter,
          enc_queue, sink, NULL)) {
    g_printerr ("Failed to link elements \n");
    return -1;
  }

  if (enc.dynamic_str != NULL) {
    /* Parse Dynamic user string */
    if (!parse_dynamic_user_string (enc.dynamic_str, encoder)) {
      g_printerr ("Error in parsing dynamic user string \n");
      g_free (dynamic);
      return -1;
    }

    pad = gst_element_get_static_pad (videoparse, "src");
    if (!pad)
      g_print ("error in pad creation\n");

    gst_pad_add_probe (pad,
        GST_PAD_PROBE_TYPE_BUFFER, videoparser_src_buffer_probe, encoder, NULL);
    gst_object_unref (pad);
  }

  /* Set the pipeline to "playing" */
  gst_element_set_state (pipeline, GST_STATE_PLAYING);

  /* Iterate */
  g_print ("Running...\n");
  g_main_loop_run (loop);

  /* Out of the main loop, clean up nicely */
  g_print ("Returned, stopping playback\n");
  gst_element_set_state (pipeline, GST_STATE_NULL);

  /* Free the memory */
  if (dynamic)
    g_free (dynamic);
  if (enc.output_filename)
    g_free (enc.output_filename);
  if (enc.input_filename)
    g_free (enc.input_filename);
  if (enc.dynamic_str)
    g_free (enc.dynamic_str);
  if (enc.type)
    g_free (enc.type);

  g_print ("Deleting pipeline\n");
  gst_caps_unref (enc_caps);
  gst_object_unref (pipeline);
  gst_bus_remove_watch (bus);
  gst_object_unref (bus);
  g_main_loop_unref (loop);

  return 0;
}
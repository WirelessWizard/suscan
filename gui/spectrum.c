/*

  Copyright (C) 2017 Gonzalo José Carracedo Carballal

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as
  published by the Free Software Foundation, either version 3 of the
  License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this program.  If not, see
  <http://www.gnu.org/licenses/>

*/

#include "gui.h"

#include <string.h>
#include <sigutils/sampling.h>

#define SUSCAN_GUI_SPECTRUM_ALPHA .01

#define SUSCAN_GUI_HORIZONTAL_DIVS 20
#define SUSCAN_GUI_VERTICAL_DIVS   10

#define SUSCAN_GUI_SPECTRUM_DX (1. / SUSCAN_GUI_HORIZONTAL_DIVS)
#define SUSCAN_GUI_SPECTRUM_DY (1. / SUSCAN_GUI_VERTICAL_DIVS)

#define SUSCAN_GUI_SPECTRUM_SCALE_DELTA .1

#define SUSCAN_GUI_SPECTRUM_LEFT_PADDING 30
#define SUSCAN_GUI_SPECTRUM_TOP_PADDING 5

#define SUSCAN_GUI_SPECTRUM_RIGHT_PADDING 5
#define SUSCAN_GUI_SPECTRUM_BOTTOM_PADDING 30

#define SUSCAN_SPECTRUM_TO_SCR_X(s, x)            \
  (((s)->width                                    \
      - SUSCAN_GUI_SPECTRUM_LEFT_PADDING          \
      - SUSCAN_GUI_SPECTRUM_RIGHT_PADDING)        \
    * (x + .5)                                    \
      + SUSCAN_GUI_SPECTRUM_LEFT_PADDING)

#define SUSCAN_SPECTRUM_FROM_SCR_X(s, x)          \
  ((((x) - SUSCAN_GUI_SPECTRUM_LEFT_PADDING) /    \
  ((s)->width                                     \
      - SUSCAN_GUI_SPECTRUM_LEFT_PADDING          \
      - SUSCAN_GUI_SPECTRUM_RIGHT_PADDING)) - .5) \

#define SUSCAN_SPECTRUM_TO_SCR_Y(s, y)            \
  (((s)->height                                   \
      - SUSCAN_GUI_SPECTRUM_TOP_PADDING           \
      - SUSCAN_GUI_SPECTRUM_BOTTOM_PADDING)       \
    * (y)                                         \
      + SUSCAN_GUI_SPECTRUM_TOP_PADDING)

#define SUSCAN_SPECTRUM_FROM_SCR_Y(s, y)          \
  (((y) - SUSCAN_GUI_SPECTRUM_TOP_PADDING) /      \
  ((s)->height                                    \
      - SUSCAN_GUI_SPECTRUM_TOP_PADDING           \
      - SUSCAN_GUI_SPECTRUM_BOTTOM_PADDING))      \


#define SUSCAN_SPECTRUM_TO_SCR(s, x, y)         \
  SUSCAN_SPECTRUM_TO_SCR_X(s, x), SUSCAN_SPECTRUM_TO_SCR_Y(s, y)

#define SUSCAN_GUI_SPECTRUM_ADJUST_X(s, x)      \
    (((x) - (s)->freq_offset) * (s)->freq_scale)
#define SUSCAN_GUI_SPECTRUM_ADJUST_X_INV(s, x)  \
    ((x) / (s)->freq_scale + (s)->freq_offset)

#define SUSCAN_GUI_SPECTRUM_ADJUST_Y(s, y)      \
    (((y) - (s)->ref_level) / ((s)->dbs_per_div * SUSCAN_GUI_VERTICAL_DIVS))
#define SUSCAN_GUI_SPECTRUM_ADJUST_Y_INV(s, y)  \
    ((y) * (s)->dbs_per_div * SUSCAN_GUI_VERTICAL_DIVS + (s)->ref_level)

void
suscan_gui_spectrum_clear(struct suscan_gui_spectrum *spectrum)
{
  cairo_t *cr;

  cr = cairo_create(spectrum->surface);

  cairo_set_source_rgb(cr, 0, 0, 0);

  cairo_paint(cr);

  cairo_destroy(cr);
}

void
suscan_gui_spectrum_init(struct suscan_gui_spectrum *spectrum)
{
  memset(spectrum, 0, sizeof (struct suscan_gui_spectrum));

  spectrum->show_channels = SU_TRUE;

  spectrum->freq_offset = SUSCAN_GUI_SPECTRUM_FREQ_OFFSET_DEFAULT;
  spectrum->freq_scale  = SUSCAN_GUI_SPECTRUM_FREQ_SCALE_DEFAULT;
  spectrum->ref_level   = SUSCAN_GUI_SPECTRUM_REF_LEVEL_DEFAULT;
  spectrum->dbs_per_div = SUSCAN_GUI_SPECTRUM_DBS_PER_DIV_DEFAULT;
}

void
suscan_spectrum_finalize(struct suscan_gui_spectrum *spectrum)
{
  unsigned int i;

  for (i = 0; i < spectrum->channel_count; ++i)
    free(spectrum->channel_list[i]);

  if (spectrum->channel_list != NULL)
    free(spectrum->channel_list);

  if (spectrum->psd_data != NULL)
    free(spectrum->psd_data);
}

const struct sigutils_channel *
suscan_gui_spectrum_lookup_channel(
    const struct suscan_gui_spectrum *spectrum,
    SUFLOAT fc)
{
  unsigned int i;

  /* Selection has precedence, always */
  if (spectrum->selection.f_lo <= fc && fc <= spectrum->selection.f_hi)
    return &spectrum->selection;

  for (i = 0; i < spectrum->channel_count; ++i)
    if (spectrum->channel_list[i] != NULL)
      if (SU_ABS(spectrum->channel_list[i]->fc - fc)
          < .5 * SU_ABS(spectrum->channel_list[i]->bw))
        return spectrum->channel_list[i];

  return NULL;
}

void
suscan_gui_spectrum_configure(
    struct suscan_gui_spectrum *spectrum,
    GtkWidget *widget)
{
  if (spectrum->surface != NULL)
    cairo_surface_destroy(spectrum->surface);

  spectrum->width = gtk_widget_get_allocated_width(widget);
  spectrum->height = gtk_widget_get_allocated_height(widget);

  spectrum->surface = gdk_window_create_similar_surface(
      gtk_widget_get_window(widget),
      CAIRO_CONTENT_COLOR,
      spectrum->width,
      spectrum->height);

  suscan_gui_spectrum_clear(spectrum);
}

void
suscan_gui_spectrum_apply_delta(
    struct suscan_gui_spectrum *spectrum,
    enum suscan_gui_spectrum_param param,
    SUFLOAT delta)
{
  switch (param) {
    case SUSCAN_GUI_SPECTRUM_PARAM_FREQ_OFFSET:
      /* Multiplied by freq_scale to keep proportion */
      spectrum->ref_level -=
          SUSCAN_GUI_SPECTRUM_SCALE_DELTA * delta * spectrum->freq_scale;
      break;

    case SUSCAN_GUI_SPECTRUM_PARAM_FREQ_SCALE:
      /* Multiplied by freq_scale to keep proportion */
      spectrum->freq_scale -=
          SUSCAN_GUI_SPECTRUM_SCALE_DELTA * delta * spectrum->freq_scale;
      if (spectrum->freq_scale < 1)
        spectrum->freq_scale = 1;
      break;

    case SUSCAN_GUI_SPECTRUM_PARAM_REF_LEVEL:
      /* Multiplied by dbs_per_div to keep proportion */
      spectrum->ref_level -=
          SUSCAN_GUI_SPECTRUM_SCALE_DELTA * delta * spectrum->dbs_per_div;
      break;

    case SUSCAN_GUI_SPECTRUM_PARAM_DBS_PER_DIV:
      /* Multiplied by dbs_per_div to keep proportion */
      spectrum->dbs_per_div -=
          SUSCAN_GUI_SPECTRUM_SCALE_DELTA * delta * spectrum->dbs_per_div;
      break;
  }
}

void
suscan_gui_spectrum_update(
    struct suscan_gui_spectrum *spectrum,
    struct suscan_analyzer_psd_msg *msg)
{
  SUFLOAT *old_data = spectrum->psd_data;
  SUSCOUNT old_size = spectrum->psd_size;

  unsigned int i;

  spectrum->fc        = msg->fc;
  spectrum->psd_data  = suscan_analyzer_psd_msg_take_psd(msg);
  spectrum->psd_size  = msg->psd_size;
  spectrum->samp_rate = msg->samp_rate;
  spectrum->N0        = msg->N0;

  if (old_data != NULL) {
    /* Average against previous update, only if sizes match */
    if (old_size == msg->psd_size)
      for (i = 0; i < old_size; ++i)
        spectrum->psd_data[i] +=
            SUSCAN_GUI_SPECTRUM_ALPHA * (old_data[i] - spectrum->psd_data[i]);
    free(old_data);
  }
}

void
suscan_gui_spectrum_update_channels(
    struct suscan_gui_spectrum *spectrum,
    struct sigutils_channel **channel_list,
    unsigned int channel_count)
{
  unsigned int i;

  for (i = 0; i < spectrum->channel_count; ++i)
    free(spectrum->channel_list[i]);

  if (spectrum->channel_list != NULL)
    free(spectrum->channel_list);

  spectrum->channel_list  = channel_list;
  spectrum->channel_count = channel_count;
}

SUPRIVATE void
suscan_gui_spectrum_draw_channel(
    struct suscan_gui_spectrum *spectrum,
    cairo_t *cr,
    const struct sigutils_channel *channel,
    SUFLOAT red,
    SUFLOAT green,
    SUFLOAT blue)
{
  SUFLOAT x1, xscr1;
  SUFLOAT x2, xscr2;

  SUFLOAT y1, yscr1;
  SUFLOAT y2, yscr2;

  /* Draw channel limits */
  x1 = (channel->f_lo - spectrum->fc) / (SUFLOAT) spectrum->samp_rate;
  x2 = (channel->f_hi - spectrum->fc) / (SUFLOAT) spectrum->samp_rate;

  if (x2 > .5) {
    x1 -= 1;
    x2 -= 1;
  }

  /* Apply frequency scaling */
  x1 = SUSCAN_GUI_SPECTRUM_ADJUST_X(spectrum, x1);
  x2 = SUSCAN_GUI_SPECTRUM_ADJUST_X(spectrum, x2);

  /* Draw channel if and only if it fits */
  if (x1 < .5 && x2 > -.5) {
    xscr1 = SUSCAN_SPECTRUM_TO_SCR_X(spectrum, x1);
    xscr2 = SUSCAN_SPECTRUM_TO_SCR_X(spectrum, x2);

    if (channel->S0 > channel->N0) {
      y1 = SUSCAN_GUI_SPECTRUM_ADJUST_Y(
          spectrum,
          -channel->S0);
      y2 = SUSCAN_GUI_SPECTRUM_ADJUST_Y(
          spectrum,
          -channel->N0);

      yscr1 = SUSCAN_SPECTRUM_TO_SCR_Y(spectrum, y1);
      yscr2 = SUSCAN_SPECTRUM_TO_SCR_Y(spectrum, y2);
    } else {
      yscr1 = SUSCAN_GUI_SPECTRUM_TOP_PADDING;
      yscr2 = spectrum->height - SUSCAN_GUI_SPECTRUM_BOTTOM_PADDING - 1;
    }


    cairo_set_source_rgba(cr, red, green, blue, .25);
    cairo_rectangle(cr, xscr1, yscr1, xscr2 - xscr1, yscr2 - yscr1);
    cairo_stroke_preserve(cr);
    cairo_fill(cr);

    /* Draw detected bandwidth */
    cairo_set_source_rgba(cr, red, green, blue, .5);
    x1 = (channel->fc - channel->bw / 2 - spectrum->fc)
            / (SUFLOAT) spectrum->samp_rate;
    x2 = (channel->fc + channel->bw / 2 - spectrum->fc)
            / (SUFLOAT) spectrum->samp_rate;

    x1 = SUSCAN_GUI_SPECTRUM_ADJUST_X(spectrum, x1);
    x2 = SUSCAN_GUI_SPECTRUM_ADJUST_X(spectrum, x2);

    xscr1 = SUSCAN_SPECTRUM_TO_SCR_X(spectrum, x1);
    xscr2 = SUSCAN_SPECTRUM_TO_SCR_X(spectrum, x2);

    cairo_rectangle(cr, xscr1, yscr1, xscr2 - xscr1, yscr2 - yscr1);
    cairo_stroke_preserve(cr);
    cairo_fill(cr);
  }
}

void
suscan_gui_spectrum_draw_channels(
    struct suscan_gui_spectrum *spectrum,
    cairo_t *cr)
{
  int i;

  if (spectrum->samp_rate > 0) {
    for (i = 0; i < spectrum->channel_count; ++i)
      suscan_gui_spectrum_draw_channel(
          spectrum,
          cr,
          spectrum->channel_list[i],
          .75, /* Red */
          0,   /* Green */
          0);  /* Blue */
  }
}

void
suscan_gui_spectrum_draw_levels(
    struct suscan_gui_spectrum *spectrum,
    cairo_t *cr)
{
  int i;
  int last_end;
  SUFLOAT x, xscr;

  char text[20];
  cairo_text_extents_t extents;

  cairo_select_font_face(
      cr,
      "Inconsolata",
      CAIRO_FONT_SLANT_NORMAL,
      CAIRO_FONT_WEIGHT_NORMAL);

  cairo_set_source_rgba(cr, 1, 1, 1, 1);

  for (i = 1; i < SUSCAN_GUI_VERTICAL_DIVS; ++i) {
    snprintf(
        text,
        sizeof(text),
        "%d",
        -(int) (SUSCAN_GUI_SPECTRUM_ADJUST_Y_INV(
            spectrum,
            (SUFLOAT) i / SUSCAN_GUI_VERTICAL_DIVS)));

    cairo_move_to(
        cr,
        7.5,
        SUSCAN_SPECTRUM_TO_SCR_Y(spectrum, i * SUSCAN_GUI_SPECTRUM_DY));

    cairo_show_text(
        cr,
        text);
  }


  if (spectrum->samp_rate > 0) {
    last_end = 0;

    for (
        i = -SUSCAN_GUI_HORIZONTAL_DIVS / 2 + 1;
        i < SUSCAN_GUI_HORIZONTAL_DIVS / 2;
        ++i) {
      x = i / (SUFLOAT) SUSCAN_GUI_HORIZONTAL_DIVS;
      xscr = SUSCAN_SPECTRUM_TO_SCR_X(spectrum, x);

      snprintf(
          text,
          sizeof(text),
          "%lli",
          (int64_t) (
              spectrum->fc
              + (int) round(SU_NORM2ABS_FREQ(
                  spectrum->samp_rate,
                  2 * SUSCAN_GUI_SPECTRUM_ADJUST_X_INV(spectrum, x)))));

      cairo_text_extents(cr, text, &extents);

      if (xscr - extents.width / 2 > last_end) {
        cairo_move_to(
            cr,
            xscr - extents.width / 2,
            spectrum->height - SUSCAN_GUI_SPECTRUM_BOTTOM_PADDING + 10);

        cairo_show_text(
            cr,
            text);

        last_end = xscr + extents.width / 2;
      }
    }
  }
}

void
suscan_gui_spectrum_redraw(
    struct suscan_gui_spectrum *spectrum,
    cairo_t *cr)
{
  static const double axis_pattern[] = {5.0, 5.0};
  int i;
  int step;
  SUFLOAT x, x_prev;
  SUFLOAT x_adj, x_prev_adj;

  cairo_set_source_surface(cr, spectrum->surface, 0, 0);

  /* Paint in black */
  cairo_paint(cr);

  cairo_set_line_width(cr, 1);

  suscan_gui_spectrum_draw_levels(spectrum, cr);

  /* Draw axes */
  cairo_set_dash(cr, axis_pattern, 2, 0);

  for (
      i = -SUSCAN_GUI_HORIZONTAL_DIVS / 2 + 1;
      i < SUSCAN_GUI_HORIZONTAL_DIVS / 2;
      ++i) {

    if (i == 0)
      cairo_set_source_rgb(cr, 1, 1, 1);
    else
      cairo_set_source_rgb(cr, 0, 0.5, 0);

    cairo_move_to(
        cr,
        SUSCAN_SPECTRUM_TO_SCR(spectrum, i * SUSCAN_GUI_SPECTRUM_DX, 0));
    cairo_line_to(
        cr,
        SUSCAN_SPECTRUM_TO_SCR(spectrum, i * SUSCAN_GUI_SPECTRUM_DX, 1));

    cairo_stroke(cr);
  }

  for (i = 1; i < SUSCAN_GUI_VERTICAL_DIVS; ++i) {
    cairo_move_to(
        cr,
        SUSCAN_SPECTRUM_TO_SCR(spectrum, -.5, i * SUSCAN_GUI_SPECTRUM_DY));
    cairo_line_to(
        cr,
        SUSCAN_SPECTRUM_TO_SCR(spectrum, .5, i * SUSCAN_GUI_SPECTRUM_DY));

    cairo_stroke(cr);
  }

  /* Draw border */
  cairo_set_dash(cr, NULL, 0, 0);
  cairo_move_to(
      cr,
      SUSCAN_SPECTRUM_TO_SCR(spectrum, -.5, 0));
  cairo_line_to(
      cr,
      SUSCAN_SPECTRUM_TO_SCR(spectrum, -.5, 1));
  cairo_stroke(cr);

  cairo_move_to(
      cr,
      SUSCAN_SPECTRUM_TO_SCR(spectrum, -.5, 0));

  cairo_line_to(
      cr,
      SUSCAN_SPECTRUM_TO_SCR(spectrum, .5, 0));
  cairo_stroke(cr);

  cairo_move_to(
      cr,
      SUSCAN_SPECTRUM_TO_SCR(spectrum, .5, 1));
  cairo_line_to(
      cr,
      SUSCAN_SPECTRUM_TO_SCR(spectrum, -.5, 1));
  cairo_stroke(cr);

  cairo_move_to(
      cr,
      SUSCAN_SPECTRUM_TO_SCR(spectrum, .5, 1));

  cairo_line_to(
      cr,
      SUSCAN_SPECTRUM_TO_SCR(spectrum, .5, 0));
  cairo_stroke(cr);

  /* Draw channels, if enabled */
  if (spectrum->show_channels)
    suscan_gui_spectrum_draw_channels(spectrum, cr);

  /* Selected channel is always displayed */
  if (spectrum->samp_rate > 0)
    if (spectrum->selection.bw > 0)
      suscan_gui_spectrum_draw_channel(
          spectrum,
          cr,
          &spectrum->selection,
          0,    /* Red */
          .75,  /* Green */
          .75); /* Blue */

  /* Draw spectrum */
  if (spectrum->psd_data != NULL) {
    x_prev = .5;

    cairo_set_dash(cr, NULL, 0, 0);

    /* Draw noise level (if applicable) */
    if (spectrum->N0 > 0) {
      cairo_set_source_rgb(cr, 0, 1., 1.);
      cairo_move_to(
          cr,
          SUSCAN_SPECTRUM_TO_SCR(
              spectrum,
              -.5,
              SUSCAN_GUI_SPECTRUM_ADJUST_Y(
                  spectrum,
                  -SU_POWER_DB(spectrum->N0))));

      cairo_line_to(
          cr,
          SUSCAN_SPECTRUM_TO_SCR(
              spectrum,
              .5,
              SUSCAN_GUI_SPECTRUM_ADJUST_Y(
                  spectrum,
                  -SU_POWER_DB(spectrum->N0))));

      cairo_stroke(cr);
    }

    cairo_set_source_rgb(cr, 1., 1., 0);

    step = (int) SU_FLOOR(
        spectrum->psd_size / (spectrum->width * spectrum->freq_scale));

    if (step < 1)
      step = 1;

    /* Draw PSD */
    x_prev = 0;
    for (i = step; i < spectrum->psd_size; i += step) {
      if ((x = i / (SUFLOAT) spectrum->psd_size) > .5) {
        x -= 1;

        if (x_prev > x)
          x_prev -= 1;
      }

      x_adj = SUSCAN_GUI_SPECTRUM_ADJUST_X(spectrum, x);

      if (x_adj >= -.5 && x_adj < .5) {
        x_prev_adj = SUSCAN_GUI_SPECTRUM_ADJUST_X(spectrum, x_prev);

        cairo_move_to(
            cr,
            SUSCAN_SPECTRUM_TO_SCR(
                spectrum,
                x_prev_adj,
                SUSCAN_GUI_SPECTRUM_ADJUST_Y(
                    spectrum,
                    -SU_POWER_DB(spectrum->psd_data[i - step]))));
        cairo_line_to(
            cr,
            SUSCAN_SPECTRUM_TO_SCR(
                spectrum,
                x_adj,
                SUSCAN_GUI_SPECTRUM_ADJUST_Y(
                    spectrum,
                    -SU_POWER_DB(spectrum->psd_data[i]))));

        cairo_stroke(cr);
      }

      x_prev = x;
    }
  }
}

void
suscan_gui_spectrum_parse_scroll(
    struct suscan_gui_spectrum *spectrum,
    const GdkEventScroll *ev)
{
  switch (ev->direction) {
    case GDK_SCROLL_SMOOTH:
      if (ev->state & GDK_SHIFT_MASK) {
        suscan_gui_spectrum_apply_delta(
            spectrum,
            SUSCAN_GUI_SPECTRUM_PARAM_DBS_PER_DIV,
            -ev->delta_y);
      } else {
        suscan_gui_spectrum_apply_delta(
            spectrum,
            SUSCAN_GUI_SPECTRUM_PARAM_FREQ_SCALE,
            ev->delta_y);
      }
      break;
  }
}

SUPRIVATE void
suscan_gui_spectrum_parse_dragging(
    struct suscan_gui_spectrum *spectrum,
    const GdkEventMotion *ev)
{
  SUFLOAT  x,  y;
  SUFLOAT lx, ly;

  if (!spectrum->dragging) {
    spectrum->original_ref_level = spectrum->ref_level;
    spectrum->original_freq_offset = spectrum->freq_offset;
    spectrum->dragging = SU_TRUE;
  }

  /* Change reference level */
  y = SUSCAN_GUI_SPECTRUM_ADJUST_Y_INV(
      spectrum,
      SUSCAN_SPECTRUM_FROM_SCR_Y(spectrum, ev->y));

  ly = SUSCAN_GUI_SPECTRUM_ADJUST_Y_INV(
      spectrum,
      SUSCAN_SPECTRUM_FROM_SCR_Y(spectrum, spectrum->last_y));

  spectrum->ref_level = spectrum->original_ref_level + ly - y;

  /* Change frequency offset only if sample rate has been defined */
  if (spectrum->samp_rate != 0) {
    x = SUSCAN_GUI_SPECTRUM_ADJUST_X_INV(
          spectrum,
          SUSCAN_SPECTRUM_FROM_SCR_X(spectrum, ev->x));

    lx = SUSCAN_GUI_SPECTRUM_ADJUST_X_INV(
          spectrum,
          SUSCAN_SPECTRUM_FROM_SCR_X(
              spectrum,
              spectrum->last_x));

    spectrum->freq_offset = spectrum->original_freq_offset + lx - x;
  }
}

SUPRIVATE void
suscan_gui_spectrum_parse_selection(
    struct suscan_gui_spectrum *spectrum,
    const GdkEventMotion *ev)
{
  SUFLOAT  x;
  SUFLOAT lx;

  spectrum->selecting = SU_TRUE;

  /* Change frequency offset only if sample rate has been defined */
  if (spectrum->samp_rate != 0) {
    x = SUSCAN_GUI_SPECTRUM_ADJUST_X_INV(
          spectrum,
          SUSCAN_SPECTRUM_FROM_SCR_X(spectrum, ev->x));

    lx = SUSCAN_GUI_SPECTRUM_ADJUST_X_INV(
          spectrum,
          SUSCAN_SPECTRUM_FROM_SCR_X(
              spectrum,
              spectrum->last_x));

    spectrum->selection.f_lo = spectrum->samp_rate * MIN(x, lx) + spectrum->fc;
    spectrum->selection.f_hi = spectrum->samp_rate * MAX(x, lx) + spectrum->fc;
    spectrum->selection.bw =
        spectrum->selection.f_hi - spectrum->selection.f_lo;
    spectrum->selection.fc =
        .5 * (spectrum->selection.f_lo + spectrum->selection.f_hi);

    spectrum->selection.ft = spectrum->fc;
  }
}

void
suscan_gui_spectrum_parse_motion(
    struct suscan_gui_spectrum *spectrum,
    const GdkEventMotion *ev)
{
  SUBOOL selection_mode;

  if (ev->state & GDK_BUTTON1_MASK) {
    selection_mode = ev->state & GDK_SHIFT_MASK;

    /* Check whether dragging mode is enabled */
    if (!selection_mode)
      suscan_gui_spectrum_parse_dragging(spectrum, ev);
    else
      spectrum->dragging = SU_FALSE;

    /* Check whether selection mode is enabled */
    if (selection_mode)
      suscan_gui_spectrum_parse_selection(spectrum, ev);
    else
      spectrum->selecting = SU_FALSE;

  } else {
    spectrum->dragging  = SU_FALSE;
    spectrum->selecting = SU_FALSE;

    spectrum->last_x = ev->x;
    spectrum->last_y = ev->y;
  }
}

void
suscan_gui_spectrum_reset_selection(struct suscan_gui_spectrum *spectrum)
{
  spectrum->selection.f_hi = spectrum->selection.f_lo = spectrum->selection.bw
      = 0;
}

/******************* These callbacks belong to the GUI API ********************/
gboolean
suscan_spectrum_on_configure_event(
    GtkWidget *widget,
    GdkEventConfigure *event,
    gpointer data)
{
  struct suscan_gui *gui = (struct suscan_gui *) data;

  suscan_gui_spectrum_configure(&gui->main_spectrum, widget);

  return TRUE;
}

gboolean
suscan_spectrum_on_draw(GtkWidget *widget, cairo_t *cr, gpointer data)
{
  struct suscan_gui *gui = (struct suscan_gui *) data;

  char text[32];

  if (gui->current_samp_rate != gui->main_spectrum.samp_rate) {
    gui->current_samp_rate = gui->main_spectrum.samp_rate;
    snprintf(text, sizeof(text), "%li", gui->current_samp_rate);
    gtk_label_set_text(gui->spectrumSampleRate, text);
  }

  gui->main_spectrum.show_channels =
      gtk_toggle_button_get_active(
          GTK_TOGGLE_BUTTON(gui->spectrumShowChannelsCheck));

  suscan_gui_spectrum_redraw(&gui->main_spectrum, cr);

  return FALSE;
}

void
suscan_spectrum_on_scroll(GtkWidget *widget, GdkEventScroll *ev, gpointer data)
{
  struct suscan_gui *gui = (struct suscan_gui *) data;
  char text[32];

  suscan_gui_spectrum_parse_scroll(&gui->main_spectrum, ev);

  snprintf(text, sizeof(text), "%.2lg dB", gui->main_spectrum.dbs_per_div);
  gtk_label_set_text(gui->spectrumDbsPerDivLabel, text);

  snprintf(text, sizeof(text), "%.2lgx", gui->main_spectrum.freq_scale);
  gtk_label_set_text(gui->spectrumFreqScaleLabel, text);
}

void
suscan_spectrum_on_motion(GtkWidget *widget, GdkEventMotion *ev, gpointer data)
{
  struct suscan_gui *gui = (struct suscan_gui *) data;
  char text[32];

  suscan_gui_spectrum_parse_motion(&gui->main_spectrum, ev);

  snprintf(
      text,
      sizeof(text),
      "%.2lg dB",
      gui->main_spectrum.ref_level);
  gtk_label_set_text(gui->spectrumRefLevelLabel, text);

  snprintf(
      text,
      sizeof(text),
      "%.2lg Hz",
      gui->main_spectrum.samp_rate * gui->main_spectrum.freq_offset);
  gtk_label_set_text(gui->spectrumFreqOffsetLabel, text);
}

gboolean
suscan_spectrum_on_button_press(
    GtkWidget *widget,
    GdkEventButton *ev,
    gpointer data)
{
  SUFLOAT x;
  SUFLOAT freq;
  char header[64];
  const struct sigutils_channel *channel;
  struct suscan_gui *gui = (struct suscan_gui *) data;

  if (ev->type == GDK_BUTTON_PRESS) {
    switch (ev->button) {
      case 1:
        /* Reset selection */
        gui->main_spectrum.selection.bw = 0;
        break;

      case 3:
        /* Open context menu */
        x = SUSCAN_GUI_SPECTRUM_ADJUST_X_INV(
            &gui->main_spectrum,
            SUSCAN_SPECTRUM_FROM_SCR_X(&gui->main_spectrum, ev->x));
        freq = x * gui->main_spectrum.samp_rate + gui->main_spectrum.fc;

        /* Lookup channel */
        if ((channel = suscan_gui_spectrum_lookup_channel(
            &gui->main_spectrum,
            freq)) != NULL) {
          gui->selected_channel = *channel;

          snprintf(
              header,
              sizeof(header),
              "Channel @ %lld Hz",
              (uint64_t) round(channel->fc));

          gtk_menu_item_set_label(
              gui->channelHeaderMenuItem,
              header);

          gtk_widget_show_all(GTK_WIDGET(gui->channelMenu));

          gtk_menu_popup_at_pointer(gui->channelMenu, (GdkEvent *) ev);

          return TRUE;
        }
        break;
    }
  }

  return FALSE;
}


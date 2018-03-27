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

#include <glib-object.h>
#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "lcd.h"

G_DEFINE_TYPE(SuGtkLcd, sugtk_lcd, GTK_TYPE_DRAWING_AREA);

static void
sugtk_lcd_line_to_ex(
    cairo_t *cr,
    gfloat x,
    gfloat y,
    gfloat xoff,
    gfloat yoff,
    gboolean flip)
{
  if (flip)
    cairo_line_to(cr, x + yoff, y + xoff);
  else
    cairo_line_to(cr, x + xoff, y + yoff);
}

static void
sugtk_lcd_draw_segment(
    const SuGtkLcd *lcd,
    cairo_surface_t *sf,
    gfloat x,
    gfloat y,
    gboolean vert)
{
  cairo_t *cr;
  gfloat halfthick;

  cr = cairo_create(sf);

  gdk_cairo_set_source_rgba(cr, &lcd->fg_color);

  halfthick = lcd->curr_thickness / 2;

  cairo_set_line_width(cr, .5);

  cairo_line_to(cr, x, y);

  sugtk_lcd_line_to_ex(cr, x, y, halfthick, -halfthick, vert);
  sugtk_lcd_line_to_ex(cr, x, y, lcd->curr_length - halfthick, -halfthick, vert);
  sugtk_lcd_line_to_ex(cr, x, y, lcd->curr_length, 0, vert);
  sugtk_lcd_line_to_ex(cr, x, y, lcd->curr_length - halfthick, halfthick, vert);
  sugtk_lcd_line_to_ex(cr, x, y, halfthick, halfthick, vert);

  cairo_close_path(cr);
  cairo_fill_preserve(cr);

  gdk_cairo_set_source_rgba(cr, &lcd->bg_color);
  cairo_stroke(cr);

  cairo_destroy(cr);
}

static void
sugtk_lcd_draw_glyph(
    const SuGtkLcd *lcd,
    cairo_surface_t *sf,
    gfloat x,
    gfloat y,
    guint segmask)
{
  cairo_t *cr;
  unsigned int i;
  static const struct { gboolean vert; gfloat x; gfloat y; } offsets[] = {
      {FALSE, 0, 0},
      {FALSE, 0, 1},
      {FALSE, 0, 2},
      {TRUE,  0, 0},
      {TRUE,  0, 1},
      {TRUE,  1, 0},
      {TRUE,  1, 1}
  };

  cr = cairo_create(sf);
  gdk_cairo_set_source_rgba(cr, &lcd->bg_color);
  cairo_paint(cr);
  cairo_destroy(cr);

  for (i = 0; i < 7; ++i)
    if (segmask & (1 << i))
      sugtk_lcd_draw_segment(
          lcd,
          sf,
          x + lcd->curr_length * offsets[i].x,
          y + lcd->curr_length * offsets[i].y,
          offsets[i].vert);
}


static void
sugtk_lcd_draw_digit(
    SuGtkLcd *lcd,
    cairo_surface_t *sf,
    gfloat x,
    gfloat y,
    guint digit)
{
  digit = digit % 10;
  static const guint digit_masks[10] = {
      /* 0 */ ~SUGTK_LCD_SEG_MIDDLE,
      /* 1 */ SUGTK_LCD_SEG_TOP_RIGHT | SUGTK_LCD_SEG_BOTTOM_RIGHT,
      /* 2 */ ~SUGTK_LCD_SEG_TOP_LEFT & ~SUGTK_LCD_SEG_BOTTOM_RIGHT,
      /* 3 */ ~SUGTK_LCD_SEG_TOP_LEFT & ~SUGTK_LCD_SEG_BOTTOM_LEFT,
      /* 4 */ SUGTK_LCD_SEG_TOP_RIGHT | SUGTK_LCD_SEG_BOTTOM_RIGHT | SUGTK_LCD_SEG_TOP_LEFT | SUGTK_LCD_SEG_MIDDLE,
      /* 5 */ ~SUGTK_LCD_SEG_TOP_RIGHT & ~SUGTK_LCD_SEG_BOTTOM_LEFT,
      /* 6 */ ~SUGTK_LCD_SEG_TOP_RIGHT,
      /* 7 */ SUGTK_LCD_SEG_TOP_LEFT | SUGTK_LCD_SEG_TOP | SUGTK_LCD_SEG_TOP_RIGHT | SUGTK_LCD_SEG_BOTTOM_RIGHT,
      /* 8 */ SUGTK_LCD_SEG_ALL_H | SUGTK_LCD_SEG_ALL_V,
      /* 9 */ ~SUGTK_LCD_SEG_BOTTOM_LEFT
  };

  sugtk_lcd_draw_glyph(lcd, sf, x, y, digit_masks[digit]);
}

static void
sugtk_lcd_dispose(GObject* object)
{
  SuGtkLcd *lcd = SUGTK_LCD(object);
  unsigned int i;

  if (lcd->sf_display != NULL) {
    cairo_surface_destroy(lcd->sf_display);
    lcd->sf_display = NULL;
  }

  for (i = 0; i < 10; ++i)
    if (lcd->sf_glyphs[i] != NULL) {
      cairo_surface_destroy(lcd->sf_glyphs[i]);
      lcd->sf_glyphs[i] = NULL;
    }

  G_OBJECT_CLASS(sugtk_lcd_parent_class)->dispose(object);
}

static void
sugtk_lcd_class_init(SuGtkLcdClass *class)
{
  GObjectClass  *g_object_class;

  g_object_class = G_OBJECT_CLASS(class);

  g_object_class->dispose = sugtk_lcd_dispose;
}

static void
sugtk_lcd_update_glyphs(SuGtkLcd *lcd)
{
  unsigned int i;
  guint glyph_width  = lcd->glyph_width;
  guint glyph_height = lcd->glyph_height;

  lcd->curr_thickness = glyph_width * lcd->thickness;
  lcd->curr_length = (1 - 2 * lcd->padding) * glyph_width;

  for (i = 0; i < 10; ++i) {
    if (lcd->sf_glyphs[i] != NULL)
      cairo_surface_destroy(lcd->sf_glyphs[i]);

    lcd->sf_glyphs[i] = gdk_window_create_similar_surface(
          gtk_widget_get_window(GTK_WIDGET(lcd)),
          CAIRO_CONTENT_COLOR,
          glyph_width,
          glyph_height);

    sugtk_lcd_draw_digit(
        lcd,
        lcd->sf_glyphs[i],
        (glyph_width - lcd->curr_length) / 2,
        (glyph_height - 2 * lcd->curr_length) / 2,
        i);
  }
}

static void
sugtk_lcd_update_display(SuGtkLcd *lcd)
{
  unsigned int i;
  guint glyph_width  = lcd->glyph_width;
  guint glyph_height = lcd->glyph_height;
  guint p = (lcd->length - 1) * glyph_width;
  guint value = lcd->value;
  cairo_t *cr;

  cr = cairo_create(lcd->sf_display);

  gdk_cairo_set_source_rgba(cr, &lcd->bg_color);
  cairo_paint(cr);

  for (i = 0; i < 10; ++i) {
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    cairo_set_source_surface(cr, lcd->sf_glyphs[value % 10], p, 0);
    cairo_rectangle(cr, p, 0, glyph_width, glyph_height);
    cairo_fill(cr);

    if (i % 3 == 0) {
      gdk_cairo_set_source_rgba(cr, &lcd->fg_color);
      cairo_arc(
          cr,
          p + glyph_width * (1 - .1),
          glyph_height - glyph_width * .3,
          glyph_width * .1,
          0,
          2 * M_PI);
      cairo_fill(cr);
    }
    value /= 10;
    p -= glyph_width;
  }

  cairo_select_font_face(
      cr,
      "Monospace",
      CAIRO_FONT_SLANT_NORMAL,
      CAIRO_FONT_WEIGHT_BOLD);

  cairo_set_font_size(cr, (1 - 2 * lcd->padding) * glyph_height);

  cairo_move_to(cr, 10 * glyph_width, lcd->height - .25 * (1 - 2 * lcd->padding) * glyph_height);
  cairo_show_text(cr, "Hz");
  cairo_destroy(cr);
}

void
sugtk_lcd_set_value(SuGtkLcd *lcd, gulong value)
{
  lcd->value = value;
  sugtk_lcd_update_display(lcd);
  gtk_widget_queue_draw(GTK_WIDGET(lcd));
}

static void
sugtk_lcd_recreate_surfaces(SuGtkLcd *lcd)
{
  if (lcd->sf_display != NULL)
    cairo_surface_destroy(lcd->sf_display);

  lcd->sf_display = gdk_window_create_similar_surface(
      gtk_widget_get_window(GTK_WIDGET(lcd)),
      CAIRO_CONTENT_COLOR,
      lcd->width,
      lcd->height);
}

void
sugtk_lcd_set_fg_color(SuGtkLcd *lcd, GdkRGBA color)
{
  lcd->fg_color = color;
  sugtk_lcd_update_glyphs(lcd);
  sugtk_lcd_update_display(lcd);
  gtk_widget_queue_draw(GTK_WIDGET(lcd));
}

void
sugtk_lcd_set_bg_color(SuGtkLcd *lcd, GdkRGBA color)
{
  lcd->bg_color = color;
  sugtk_lcd_update_glyphs(lcd);
  sugtk_lcd_update_display(lcd);
  gtk_widget_queue_draw(GTK_WIDGET(lcd));
}

static gboolean
sugtk_lcd_on_configure_event(
    GtkWidget *widget,
    GdkEventConfigure *event,
    gpointer data)
{
  SuGtkLcd *lcd = SUGTK_LCD(widget);

  lcd->width  = event->width;
  lcd->height = event->height;

  lcd->glyph_width  = event->height / 2;
  lcd->glyph_height = event->height;

  sugtk_lcd_recreate_surfaces(lcd);
  sugtk_lcd_update_glyphs(lcd);
  sugtk_lcd_update_display(lcd);

  return TRUE;
}

static gboolean
sugtk_lcd_on_draw(GtkWidget *widget, cairo_t *cr, gpointer data)
{
  SuGtkLcd *lcd = SUGTK_LCD(widget);

  if (lcd->sf_display != NULL) {
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_surface(cr, lcd->sf_display, 0, 0);
    cairo_paint(cr);
  }

  return FALSE;
}

static void
sugtk_lcd_init(SuGtkLcd *self)
{
  gtk_widget_set_events(
      GTK_WIDGET(self),
      GDK_EXPOSURE_MASK);

  g_signal_connect(
      self,
      "configure-event",
      (GCallback) sugtk_lcd_on_configure_event,
      NULL);

  g_signal_connect(
      self,
      "draw",
      (GCallback) sugtk_lcd_on_draw,
      NULL);

  self->size = 20;
  self->length = 10;
  self->padding = .2;
  self->thickness = .2;

  self->fg_color.red   = .15;
  self->fg_color.green = .15;
  self->fg_color.blue  = .15;
  self->fg_color.alpha = 1;

  self->bg_color.red   = (gfloat) 0x90 / 0xff;
  self->bg_color.green = (gfloat) 0xb1 / 0xff;
  self->bg_color.blue  = (gfloat) 0x56 / 0xff;
  self->bg_color.alpha = 1;
}

GtkWidget *
sugtk_lcd_new(void)
{
  GtkWidget *widget = (GtkWidget *) g_object_new(SUGTK_TYPE_LCD, NULL);
  SuGtkLcd *lcd = SUGTK_LCD(widget);

  gtk_widget_set_size_request(
      widget,
      lcd->size * (lcd->length + 2), 2 * lcd->size);

  return widget;
}
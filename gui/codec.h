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

#ifndef _GUI_CODEC_H
#define _GUI_CODEC_H

#include <sigutils/sigutils.h>
#include <suscan.h>
#include <codec/codec.h>

#include "symview.h"

struct suscan_gui_inspector;

struct suscan_gui_codec_cfg_ui {
  struct suscan_gui_inspector *inspector;
  const struct suscan_codec_class *desc;
  suscan_config_t *config;
  struct suscan_gui_cfgui *ui;
  GtkWidget *dialog;
};

struct suscan_gui_codec_params {
  struct suscan_gui_inspector *inspector;
  const struct suscan_codec_class *class;
  uint8_t bits_per_symbol;
  suscan_config_t *config;
  unsigned int direction;
  suscan_symbuf_t *source;
  unsigned int start;
  unsigned int end;
  SUBOOL live;
};

#define suscan_gui_codec_params_INITIALIZER \
{                                           \
  NULL, /* inspector */                     \
  NULL, /* class */                         \
  0, /* bits_per_symbol */                  \
  NULL, /* config */                        \
  0, /* direction */                        \
  NULL, /* source */                        \
  0, /* start */                            \
  0, /* end */                              \
  SU_FALSE, /* live */                      \
}

struct suscan_gui_codec_context;

struct suscan_gui_codec_state;

struct suscan_gui_codec {
  struct suscan_gui_codec_params params;
  struct suscan_gui_codec_state *state;

  const char      *desc; /* Borrowed from codec class */
  unsigned int     output_bits;

  int              index;
  GtkBuilder      *builder;

  SUBITS          *input_buffer;
  SUSCOUNT         input_size;

  /* Top level widgets */
  GtkEventBox     *pageLabelEventBox;
  GtkLabel        *pageLabel;
  GtkGrid         *codecGrid;

  /* Toolbar widgets */
  GtkToggleToolButton *autoFitToggleButton;
  GtkSpinButton       *offsetSpinButton;
  GtkSpinButton       *widthSpinButton;

  /* Symbol view widgets */
  SuGtkSymView    *symbolView;

  /* Live decoding */
  SUBOOL          pending_done;
  suscan_symbuf_listener_t *listener;
  grow_buf_t      livebuf;
  suscan_symbuf_t *symbuf;

  /* Decoder contexts, needed to link menus to codec operations */
  PTR_LIST(struct suscan_gui_codec_context, context);
};

struct suscan_gui_codec_context {
  struct suscan_gui_codec *codec;
  struct suscan_gui_codec_cfg_ui *ui;
};

/* Decoder Config UI functions */
void suscan_gui_codec_cfg_ui_destroy(struct suscan_gui_codec_cfg_ui *ui);

SUBOOL suscan_gui_codec_cfg_ui_assert_parent_gui(
    struct suscan_gui_codec_cfg_ui *ui);

SUBOOL suscan_gui_codec_cfg_ui_run(struct suscan_gui_codec_cfg_ui *ui);

struct suscan_gui_codec_cfg_ui *suscan_gui_codec_cfg_ui_new(
    struct suscan_gui_inspector *inspector,
    const struct suscan_codec_class *desc);

/* Codec API */
struct suscan_gui_codec *suscan_gui_codec_new(
    const struct suscan_gui_codec_params *params);

/* Use this if the worker is dead */
void suscan_gui_codec_destroy_hard(struct suscan_gui_codec *codec);

void suscan_gui_codec_destroy(struct suscan_gui_codec *codec);

#endif /* _GUI_CODEC_H */
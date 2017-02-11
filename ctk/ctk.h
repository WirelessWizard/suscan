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

#ifndef _CTK_H
#define _CTK_H

#include <ncurses.h>
#include <menu.h>
#include <panel.h>
#include <ctype.h>
#include <util.h>

#define CTKPRIVATE  static
#define CTKBOOL     int
#define CTK_TRUE    1
#define CTK_FALSE   0

#define CTK_KEY_ESCAPE 033

#define CTK_WIDGET(x) ((ctk_widget_t *) (x))
#define CTK_WIDGET_SUB(x) (CTK_WIDGET(x)->__sub)
#define CTK_WIDGET_SUB_ALLOC_SIZE(x) (sizeof(ctk_widget_t) + sizeof(x))
#define CTK_WIDGET_ASSERT_CLASS(x, t) \
  assert(CTK_WIDGET(x)->class == t)

#define CTK_WIDGET_SHADOW_DX 2
#define CTK_WIDGET_SHADOW_DY 1
#define CTK_WIDGET_SHADOW_CHAR ACS_CKBOARD

#define CTK_CTRL(key) (toupper(key) - '@')

#define CTK_CP_TEXTAREA             1
#define CTK_CP_WIDGET               2
#define CTK_CP_MENU_SELECT          3
#define CTK_CP_ACCEL_HIGHLIGHT      4
#define CTK_CP_MENU_TITLE_HIGHLIGHT 5
#define CTK_CP_DIALOG_NORMAL        6
#define CTK_CP_DIALOG_INFO          7
#define CTK_CP_DIALOG_WARNING       8
#define CTK_CP_DIALOG_ERROR         9
#define CTK_CP_BACKGROUND           10
#define CTK_CP_BACKGROUND_TEXT      11

#define CTK_ITEM_INDEX(item) (item)->__index

struct ctk_item {
  char *name;
  char *desc;
  void *private;

  /* Private members */
  int __index;
};

enum ctk_widget_class {
  CTK_WIDGET_CLASS_NONE,
  CTK_WIDGET_CLASS_WINDOW,
  CTK_WIDGET_CLASS_ENTRY,
  CTK_WIDGET_CLASS_MENU,
  CTK_WIDGET_CLASS_MENUBAR,
  CTK_WIDGET_CLASS_BUTTON,
};

enum ctk_dialog_kind {
  CTK_DIALOG_NORMAL,
  CTK_DIALOG_INFO,
  CTK_DIALOG_WARNING,
  CTK_DIALOG_ERROR
};

struct ctk_widget;

typedef void    (*ctk_kbd_handler_t) (struct ctk_widget *, int);
typedef void    (*ctk_dtor_handler_t) (struct ctk_widget *);
typedef CTKBOOL (*ctk_resize_handler_t) (struct ctk_widget *, unsigned int, unsigned int);
typedef CTKBOOL (*ctk_move_handler_t) (struct ctk_widget *, unsigned int, unsigned int);
typedef void    (*ctk_winch_handler_t) (struct ctk_widget *, unsigned int, unsigned int);
typedef void    (*ctk_submit_handler_t) (struct ctk_widget *, struct ctk_item *);
typedef CTKBOOL (*ctk_attach_handler_t) (struct ctk_widget *, struct ctk_widget *);
typedef void    (*ctk_detach_handler_t) (struct ctk_widget *, struct ctk_widget *);
typedef void    (*ctk_focus_handler_t) (struct ctk_widget *);
typedef void    (*ctk_blur_handler_t) (struct ctk_widget *);
typedef void    (*ctk_redraw_handler_t) (struct ctk_widget *);

struct ctk_widget_handlers {
  ctk_kbd_handler_t    kbd_handler;
  ctk_dtor_handler_t   dtor_handler;
  ctk_resize_handler_t resize_handler;
  ctk_move_handler_t   move_handler;
  ctk_winch_handler_t  winch_handler;
  ctk_submit_handler_t submit_handler;
  ctk_attach_handler_t attach_handler;
  ctk_detach_handler_t detach_handler;
  ctk_focus_handler_t  focus_handler;
  ctk_blur_handler_t   blur_handler;
  ctk_redraw_handler_t redraw_handler;
};

/* Base class for all ctk_widgets */
struct ctk_widget {
  enum ctk_widget_class class;
  struct ctk_widget *root;
  unsigned int x;
  unsigned int y;
  unsigned int width;
  unsigned int height;

  int attrs;
  int accel;

  CTKBOOL has_border;
  CTKBOOL popup;
  CTKBOOL visible;
  CTKBOOL shadow;

  /* Curses objects */
  void *private;
  WINDOW *c_window;
  PANEL  *c_panel;

  WINDOW *c_win_shadow;
  PANEL  *c_pan_shadow;

  /* Event handlers */
  struct ctk_widget_handlers handlers;

  /* First byte of the subclass */
  char __sub[0];
};

typedef struct ctk_widget ctk_widget_t;


/********************* CTK Menu definitions **********************************/
#define CTK_WIDGET_AS_MENU(x) ((ctk_menu_t *) CTK_WIDGET_SUB(x))

struct ctk_menu {
  PTR_LIST(struct ctk_item, item); /* No gaps allowed! */
  char   *title;

  /* Curses objects */
  ITEM  **c_item_list;
  WINDOW *c_sub;
  MENU   *c_menu;
};

typedef struct ctk_menu ctk_menu_t;

/********************** CTK Menu bar definitions *****************************/
#define CTK_WIDGET_AS_MENUBAR(x) ((ctk_menubar_t *) CTK_WIDGET_SUB(x))

struct ctk_menubar {
  PTR_LIST(ctk_widget_t, menu);
  CTKBOOL escape;
  int active; /* Active menu inex */
};

typedef struct ctk_menubar ctk_menubar_t;

/*********************** CTK Window definitions ******************************/
#define CTK_WIDGET_AS_WINDOW(x) ((ctk_window_t *) CTK_WIDGET_SUB(x))

struct ctk_window {
  PTR_LIST(ctk_widget_t, widget);
  char *title;
  int focus; /* Focused element */
};

typedef struct ctk_window ctk_window_t;

/*********************** CTK Button definitions ******************************/
#define CTK_WIDGET_AS_BUTTON(x) ((ctk_button_t *) CTK_WIDGET_SUB(x))

struct ctk_button {
  char *caption;
  CTKBOOL has_focus;
};

typedef struct ctk_button ctk_button_t;

/************************ CTK Entry definitions ******************************/
#define CTK_WIDGET_AS_ENTRY(x) ((ctk_entry_t *) CTK_WIDGET_SUB(x))

struct ctk_entry {
  char *buffer;
  unsigned int allocation;
  unsigned int length; /* Not counting NUL byte */
  unsigned int p; /* Insert position */
  unsigned int pos; /* Display position */
  int cur_attr; /* Cursor attribute */
  CTKBOOL has_focus;

  /*
   * TODO: add constraints
   */
};

typedef struct ctk_entry ctk_entry_t;

CTKBOOL ctk_init(void);
void ctk_update(void);

/***************************** CTK Widget API ********************************/
void ctk_widget_destroy(ctk_widget_t *wid);
ctk_widget_t *ctk_widget_ctor_start(
    ctk_widget_t *root,
    unsigned int x,
    unsigned int y,
    unsigned int width,
    unsigned int height,
    size_t alloc_size);
CTKBOOL ctk_widget_ctor_end(ctk_widget_t *widget);

CTKBOOL ctk_widget_set_border(ctk_widget_t *widget, CTKBOOL val);
CTKBOOL ctk_widget_resize(
    ctk_widget_t *widget,
    unsigned int width,
    unsigned int height);
CTKBOOL ctk_widget_move(ctk_widget_t *widget, unsigned int x, unsigned int y);
CTKBOOL ctk_widget_center(ctk_widget_t *widget);
CTKBOOL ctk_widget_set_shadow(ctk_widget_t *widget, CTKBOOL val);
CTKBOOL ctk_widget_show(ctk_widget_t *widget);
CTKBOOL ctk_widget_hide(ctk_widget_t *widget);
void ctk_widget_focus(ctk_widget_t *widget);
void ctk_widget_blur(ctk_widget_t *widget);
void ctk_widget_refresh(ctk_widget_t *widget);
void ctk_widget_submit(ctk_widget_t *widget, struct ctk_item *item);
void ctk_widget_notify_kbd(ctk_widget_t *widget, int c);
void ctk_widget_set_popup(ctk_widget_t *widget, CTKBOOL val);
void ctk_widget_set_private(ctk_widget_t *widget, void *ptr);
void *ctk_widget_get_private(const ctk_widget_t *widget);
void ctk_widget_set_attrs(ctk_widget_t *widget, int attrs);
void ctk_widget_set_accel(ctk_widget_t *widget, int accel);
int  ctk_widget_get_accel(const ctk_widget_t *widget);
void ctk_widget_redraw(ctk_widget_t *widget);
void ctk_widget_get_handlers(
    ctk_widget_t *widget,
    struct ctk_widget_handlers *h);
void ctk_widget_set_handlers(
    ctk_widget_t *widget,
    const struct ctk_widget_handlers *h);

/***************************** CTK Item API **********************************/
struct ctk_item *ctk_item_new(const char *name, const char *desc, void *private);
struct ctk_item *ctk_item_dup(const struct ctk_item *item);
void ctk_item_destroy(struct ctk_item *item);

/**************************** Misc functions *********************************/
int ctk_widget_lookup_index_by_accel(
    ctk_widget_t * const *list,
    unsigned int count,
    int accel);

/************************* CTK Menu functions ********************************/
ctk_widget_t *ctk_menu_new(unsigned int x, unsigned int y);
CTKBOOL ctk_menu_add_item(
    ctk_widget_t *widget,
    const char *name,
    const char *desc,
    void *private);
CTKBOOL ctk_menu_add_multiple_items(
    ctk_widget_t *widget,
    const struct ctk_item *items,
    unsigned int count);
CTKBOOL ctk_menu_set_title(ctk_widget_t *widget, const char *title);
const char *ctk_menu_get_title(ctk_widget_t *widget);
unsigned int ctk_menu_get_item_count(const ctk_widget_t *widget);
struct ctk_item *ctk_menu_get_first_item(const ctk_widget_t *widget);
unsigned int ctk_menu_get_max_item_name_length(const ctk_widget_t *widget);

/************************** CTK Menubar functions ****************************/
ctk_widget_t *ctk_menubar_new(void);
CTKBOOL ctk_menubar_add_menu(ctk_widget_t *, const char *, ctk_widget_t *);

/************************* CTK Window functions ******************************/
ctk_widget_t *ctk_window_new(const char *title);
void ctk_window_focus_next(ctk_widget_t *widget);

/************************* CTK Button functions ******************************/
ctk_widget_t *ctk_button_new(
    ctk_widget_t *root,
    unsigned int x,
    unsigned int y,
    const char *caption);
CTKBOOL ctk_button_set_caption(ctk_widget_t *widget, const char *caption);
const char *ctk_button_get_caption(ctk_widget_t *widget);

/************************ CTK Selbutton functions ****************************/
ctk_widget_t *ctk_selbutton_new(
    ctk_widget_t *root,
    unsigned int x,
    unsigned int y,
    ctk_widget_t *menu);

/************************** CTK Entry functions ******************************/
const char *ctk_entry_get_text(const ctk_widget_t *widget);
CTKBOOL ctk_entry_set_text(ctk_widget_t *widget, const char *text);
ctk_widget_t *ctk_entry_new(
    ctk_widget_t *root,
    unsigned int x,
    unsigned int y,
    unsigned int width);

/************************* CTK Dialog functions ******************************/
CTKBOOL ctk_msgbox(
    enum ctk_dialog_kind kind,
    const char *title,
    const char *msg);

#endif /* _CTK_H */

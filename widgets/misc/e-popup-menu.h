/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef E_POPUP_MENU_H
#define E_POPUP_MENU_H

#include <gtk/gtkwidget.h>

typedef struct _EPopupMenu EPopupMenu;

struct _EPopupMenu {
	char *name;
	char *pixname;
	void (*fn) (GtkWidget *widget, void *closure);
	EPopupMenu *submenu;
	guint32 disable_mask;
};

GtkMenu *e_popup_menu_create  (EPopupMenu     *menu_list,
			       guint32         disable_mask,
			       guint32         hide_mask,
			       void           *closure);

void     e_popup_menu_run     (EPopupMenu     *menu_list,
			       GdkEventButton *event,
			       guint32         disable_mask,
			       guint32         hide_mask,
			       void           *closure);

#endif /* E_POPUP_MENU_H */

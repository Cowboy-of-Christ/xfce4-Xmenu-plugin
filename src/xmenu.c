/*
 * Copyright (C) 2024-2025 Kamil 'Novik' Nowicki <novik@axisos.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>
#include <libxfce4panel/libxfce4panel.h>
#include <libxfce4ui/libxfce4ui.h>
#include <libxfce4util/libxfce4util.h>
#include <exo/exo.h>
#include <dbus/dbus-glib.h>
#include <unistd.h>
#include <sys/utsname.h>

#include "xmenu.h"

/* Plugin structure */
typedef struct {
    XfcePanelPlugin *plugin;
    GtkWidget       *button;
    GtkWidget       *icon;
    GtkWidget       *menu;
    gboolean         menu_visible;  /* Track menu visibility state */

    /* Configuration */
    gboolean         show_recent_items;
    gint             recent_items_max;
    gboolean         use_xmenu_logo;
    gchar           *custom_icon_name;
    gchar           *app_store_command;
    gchar           *software_update_command;
    gchar           *about_computer_command;
    gchar           *force_quit_command;
    gint             transparency;
} XmenuPlugin;

/* Prototypes */
static void xmenu_construct(XfcePanelPlugin *plugin);
static void xmenu_free_data(XfcePanelPlugin *plugin, XmenuPlugin *xmenu);
static void xmenu_position_menu(GtkMenu *menu, gint *x, gint *y, gboolean *push_in, gpointer data);
static void xmenu_button_clicked(GtkWidget *button, XmenuPlugin *xmenu);
static void xmenu_create_menu(XmenuPlugin *xmenu);
static gboolean xmenu_size_changed(XfcePanelPlugin *plugin, guint size, XmenuPlugin *xmenu);
static void xmenu_orientation_changed(XfcePanelPlugin *plugin, GtkOrientation orientation, XmenuPlugin *xmenu);
static void xmenu_configure_plugin(XfcePanelPlugin *plugin, XmenuPlugin *xmenu);
static void xmenu_save_config(XfcePanelPlugin *plugin, XmenuPlugin *xmenu);
static void xmenu_load_config(XmenuPlugin *xmenu);

/* Menu callbacks */
static void xmenu_about_computer(GtkMenuItem *item G_GNUC_UNUSED, gpointer data);
static void xmenu_system_preferences(GtkMenuItem *item G_GNUC_UNUSED, gpointer data);
static void xmenu_software_update(GtkMenuItem *item G_GNUC_UNUSED, gpointer data);
static void xmenu_app_store(GtkMenuItem *item G_GNUC_UNUSED, gpointer data);
static void xmenu_force_quit(GtkMenuItem *item G_GNUC_UNUSED, gpointer data);
static void xmenu_sleep(GtkMenuItem *item G_GNUC_UNUSED, gpointer data);
static void xmenu_restart(GtkMenuItem *item G_GNUC_UNUSED, gpointer data);
static void xmenu_shutdown(GtkMenuItem *item G_GNUC_UNUSED, gpointer data);
static void xmenu_lock_screen(GtkMenuItem *item G_GNUC_UNUSED, gpointer data);
static void xmenu_logout(GtkMenuItem *item G_GNUC_UNUSED, gpointer data);
static void xmenu_on_menu_show(GtkWidget *menu, gpointer data);
static void xmenu_on_menu_hide(GtkWidget *menu, gpointer data);
static void xmenu_on_menu_deactivate(GtkWidget *menu, gpointer data);

/* Register the plugin */
XFCE_PANEL_PLUGIN_REGISTER(xmenu_construct);

/* Plugin construction */
static void
xmenu_construct(XfcePanelPlugin *plugin)
{
    XmenuPlugin *xmenu;
    GtkWidget *icon;
    GtkCssProvider *button_css;

    /* Allocate plugin structure */
    xmenu = g_slice_new0(XmenuPlugin);
    xmenu->plugin = plugin;

    /* Initialize configuration with defaults */
    xmenu->show_recent_items = TRUE;
    xmenu->recent_items_max = 10;
    xmenu->use_xmenu_logo = TRUE;
    xmenu->custom_icon_name = g_strdup(XMENU_ICON_NAME);
    xmenu->app_store_command = g_strdup(DEFAULT_APP_STORE_COMMAND);
    xmenu->software_update_command = g_strdup(DEFAULT_SOFTWARE_UPDATE_COMMAND);
    xmenu->about_computer_command = g_strdup(DEFAULT_ABOUT_COMPUTER_COMMAND);
    xmenu->force_quit_command = g_strdup(DEFAULT_FORCE_QUIT_COMMAND);
    xmenu->transparency = DEFAULT_TRANSPARENCY;
    xmenu->menu_visible = FALSE;

    /* Create button */
    xmenu->button = xfce_panel_create_button();
    gtk_widget_set_tooltip_text(xmenu->button, _("Xmenu"));
    gtk_button_set_relief(GTK_BUTTON(xmenu->button), GTK_RELIEF_NONE);

    /* Some themes still paint borders/images on flat panel buttons. Apply
     * this provider only to our launcher, preserving themed hover colors
     * and the keyboard focus indicator. */
    gtk_widget_set_name(xmenu->button, "xmenu-button");
    button_css = gtk_css_provider_new();
    gtk_css_provider_load_from_data(button_css,
        "#xmenu-button {"
        " border-style: none;"
        " border-width: 0;"
        " border-image: none;"
        " box-shadow: none;"
        "}", -1, NULL);
    gtk_style_context_add_provider(gtk_widget_get_style_context(xmenu->button),
                                   GTK_STYLE_PROVIDER(button_css),
                                   GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(button_css);

    /* Create icon */
    icon = gtk_image_new_from_icon_name(XMENU_ICON_NAME, GTK_ICON_SIZE_BUTTON);
    if (!gtk_icon_theme_has_icon(gtk_icon_theme_get_for_screen(
                                    gtk_widget_get_screen(xmenu->button)),
                                XMENU_ICON_NAME)) {
        /* Fallback to distributor logo if Command logo not found */
        gtk_image_set_from_icon_name(GTK_IMAGE(icon), XMENU_FALLBACK_ICON, GTK_ICON_SIZE_BUTTON);
    }
    xmenu->icon = icon;
    gtk_widget_set_halign(icon, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(icon, GTK_ALIGN_CENTER);
    gtk_container_add(GTK_CONTAINER(xmenu->button), icon);
    gtk_widget_show(icon);

    /* Connect button signal */
    g_signal_connect(G_OBJECT(xmenu->button), "clicked",
                     G_CALLBACK(xmenu_button_clicked), xmenu);

    /* Add button to plugin */
    gtk_container_add(GTK_CONTAINER(plugin), xmenu->button);
    gtk_widget_show(xmenu->button);

    /* Load configuration */
    xmenu_load_config(xmenu);
    xmenu_size_changed(plugin, xfce_panel_plugin_get_size(plugin), xmenu);

    /* Connect plugin signals */
    g_signal_connect(G_OBJECT(plugin), "free-data",
                     G_CALLBACK(xmenu_free_data), xmenu);
    g_signal_connect(G_OBJECT(plugin), "save",
                     G_CALLBACK(xmenu_save_config), xmenu);
    g_signal_connect(G_OBJECT(plugin), "size-changed",
                     G_CALLBACK(xmenu_size_changed), xmenu);
    g_signal_connect(G_OBJECT(plugin), "orientation-changed",
                     G_CALLBACK(xmenu_orientation_changed), xmenu);
    g_signal_connect(G_OBJECT(plugin), "configure-plugin",
                     G_CALLBACK(xmenu_configure_plugin), xmenu);

    /* Create menu */
    xmenu_create_menu(xmenu);

    /* Show properties dialog on first use */
    xfce_panel_plugin_menu_show_configure(plugin);

    /* Store plugin data */
    g_object_set_data(G_OBJECT(plugin), "xmenu-data", xmenu);
}

/* Free plugin data */
static void
xmenu_free_data(XfcePanelPlugin *plugin G_GNUC_UNUSED, XmenuPlugin *xmenu)
{
    /* Destroy menu */
    if (xmenu->menu)
        gtk_widget_destroy(xmenu->menu);

    /* Free configuration */
    g_free(xmenu->custom_icon_name);
    g_free(xmenu->app_store_command);

    g_free(xmenu->about_computer_command);
    g_free(xmenu->force_quit_command);

    g_free(xmenu->software_update_command);

    /* Free plugin structure */
    g_slice_free(XmenuPlugin, xmenu);
}

/* Position menu */
static void
xmenu_position_menu(GtkMenu *menu, gint *x, gint *y, gboolean *push_in, gpointer data)
{
    XmenuPlugin *xmenu = (XmenuPlugin *)data;
    xfce_panel_plugin_position_menu(menu, x, y, push_in, xmenu->plugin);
}

/* Handle button click */
static void
xmenu_button_clicked(GtkWidget *button G_GNUC_UNUSED, XmenuPlugin *xmenu)
{
    /* Toggle menu visibility */
    if (xmenu->menu_visible) {
        /* Menu is visible, close it */
        gtk_menu_popdown(GTK_MENU(xmenu->menu));
        gtk_widget_hide(GTK_WIDGET(xmenu->menu));
        xmenu->menu_visible = FALSE;
    } else {
        /* Menu is not visible, show it */
        gtk_menu_popup_at_widget(GTK_MENU(xmenu->menu),
                                 xmenu->button,
                                 GDK_GRAVITY_SOUTH_WEST,
                                 GDK_GRAVITY_NORTH_WEST,
                                 NULL);
    }
}

/* Create menu */
static void
xmenu_create_menu(XmenuPlugin *xmenu)
{
    GtkWidget *menu, *item, *image;

    /* Create menu */
    menu = gtk_menu_new();
    xmenu->menu = menu;

    /* Connect menu signals to track visibility and sync with button */
    g_signal_connect(G_OBJECT(menu), "show",
                     G_CALLBACK(xmenu_on_menu_show), xmenu);
    g_signal_connect(G_OBJECT(menu), "hide",
                     G_CALLBACK(xmenu_on_menu_hide), xmenu);
    g_signal_connect(G_OBJECT(menu), "deactivate",
                     G_CALLBACK(xmenu_on_menu_deactivate), xmenu);

    /* About This Computer */
    item = gtk_image_menu_item_new_with_mnemonic(_("_About This Computer"));
    image = gtk_image_new_from_icon_name("computer", GTK_ICON_SIZE_MENU);
    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), image);
    g_signal_connect(G_OBJECT(item), "activate",
                     G_CALLBACK(xmenu_about_computer), xmenu);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    gtk_widget_show(item);

    /* Separator */
    item = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    gtk_widget_show(item);

    /* System Preferences */
    item = gtk_image_menu_item_new_with_mnemonic(_("System _Preferences..."));
    image = gtk_image_new_from_icon_name("preferences-system", GTK_ICON_SIZE_MENU);
    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), image);
    g_signal_connect(G_OBJECT(item), "activate",
                     G_CALLBACK(xmenu_system_preferences), xmenu);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    gtk_widget_show(item);

    /* Software Update */
    item = gtk_image_menu_item_new_with_mnemonic(_("Software _Update..."));
    image = gtk_image_new_from_icon_name("system-software-update", GTK_ICON_SIZE_MENU);
    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), image);
    g_signal_connect(G_OBJECT(item), "activate",
                     G_CALLBACK(xmenu_software_update), xmenu);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    gtk_widget_show(item);

    /* App Store (Pamac) */
    item = gtk_image_menu_item_new_with_mnemonic(_("_App Store..."));
    image = gtk_image_new_from_icon_name("system-software-install", GTK_ICON_SIZE_MENU);
    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), image);
    g_signal_connect(G_OBJECT(item), "activate",
                     G_CALLBACK(xmenu_app_store), xmenu);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    gtk_widget_show(item);

    /* Separator */
    item = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    gtk_widget_show(item);

    /* Recent Items (TODO: Implement submenu) */
    if (xmenu->show_recent_items) {
        item = gtk_image_menu_item_new_with_mnemonic(_("Recent _Items"));
        image = gtk_image_new_from_icon_name("document-open-recent", GTK_ICON_SIZE_MENU);
        gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), image);
        gtk_widget_set_sensitive(item, FALSE); /* Disabled for now */
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
        gtk_widget_show(item);

        /* Separator */
        item = gtk_separator_menu_item_new();
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
        gtk_widget_show(item);
    }

    /* Force Quit */
    item = gtk_image_menu_item_new_with_mnemonic(_("_Force Quit..."));
    image = gtk_image_new_from_icon_name("process-stop", GTK_ICON_SIZE_MENU);
    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), image);
    g_signal_connect(G_OBJECT(item), "activate",
                     G_CALLBACK(xmenu_force_quit), xmenu);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    gtk_widget_show(item);

    /* Separator */
    item = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    gtk_widget_show(item);

    /* Sleep */
    item = gtk_image_menu_item_new_with_mnemonic(_("_Sleep"));
    image = gtk_image_new_from_icon_name("system-suspend", GTK_ICON_SIZE_MENU);
    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), image);
    g_signal_connect(G_OBJECT(item), "activate",
                     G_CALLBACK(xmenu_sleep), xmenu);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    gtk_widget_show(item);

    /* Restart */
    item = gtk_image_menu_item_new_with_mnemonic(_("_Restart..."));
    image = gtk_image_new_from_icon_name("system-reboot", GTK_ICON_SIZE_MENU);
    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), image);
    g_signal_connect(G_OBJECT(item), "activate",
                     G_CALLBACK(xmenu_restart), xmenu);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    gtk_widget_show(item);

    /* Shut Down */
    item = gtk_image_menu_item_new_with_mnemonic(_("Shut _Down..."));
    image = gtk_image_new_from_icon_name("system-shutdown", GTK_ICON_SIZE_MENU);
    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), image);
    g_signal_connect(G_OBJECT(item), "activate",
                     G_CALLBACK(xmenu_shutdown), xmenu);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    gtk_widget_show(item);

    /* Separator */
    item = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    gtk_widget_show(item);

    /* Lock Screen */
    item = gtk_image_menu_item_new_with_mnemonic(_("_Lock Screen"));
    image = gtk_image_new_from_icon_name("system-lock-screen", GTK_ICON_SIZE_MENU);
    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), image);
    g_signal_connect(G_OBJECT(item), "activate",
                     G_CALLBACK(xmenu_lock_screen), xmenu);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    gtk_widget_show(item);

    /* Log Out - Show current username */
    const gchar *username = g_get_user_name();
    gchar *logout_label = g_strdup_printf(_("Log Out %s..."), username);
    item = gtk_image_menu_item_new_with_mnemonic(logout_label);
    g_free(logout_label);
    image = gtk_image_new_from_icon_name("system-log-out", GTK_ICON_SIZE_MENU);
    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), image);
    g_signal_connect(G_OBJECT(item), "activate",
                     G_CALLBACK(xmenu_logout), xmenu);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    gtk_widget_show(item);

    /* Apply transparency if set */
    if (xmenu->transparency < 100) {
        gtk_widget_set_opacity(GTK_WIDGET(menu),
                              xmenu->transparency / 100.0);
    }

    gtk_widget_show_all(menu);
}

/* Size changed callback */
static gboolean
xmenu_size_changed(XfcePanelPlugin *plugin G_GNUC_UNUSED, guint size, XmenuPlugin *xmenu)
{
    gint padding = MAX(2, (gint)size / 8);
    gint icon_size = MAX(1, (gint)size - 2 * padding);

    /* Keep space around the logo as the panel grows or shrinks. Set an
     * explicit pixel size so GTK's standard icon sizes cannot fill the panel.
     * Preserve the selected icon when resizing. */
    gtk_image_set_pixel_size(GTK_IMAGE(xmenu->icon), icon_size);

    /* Set button size */
    gtk_widget_set_size_request(GTK_WIDGET(xmenu->button), size, size);

    return TRUE;
}

/* Orientation changed callback */
static void
xmenu_orientation_changed(XfcePanelPlugin *plugin G_GNUC_UNUSED, GtkOrientation orientation, XmenuPlugin *xmenu)
{
    /* Update button orientation if needed */
    gtk_orientable_set_orientation(GTK_ORIENTABLE(xmenu->button), orientation);
}

/* Menu show callback */
static void
xmenu_on_menu_show(GtkWidget *menu G_GNUC_UNUSED, gpointer data)
{
    XmenuPlugin *xmenu = (XmenuPlugin *)data;
    xmenu->menu_visible = TRUE;
}

/* Menu hide callback */
static void
xmenu_on_menu_hide(GtkWidget *menu G_GNUC_UNUSED, gpointer data)
{
    XmenuPlugin *xmenu = (XmenuPlugin *)data;
    xmenu->menu_visible = FALSE;
    gtk_widget_hide(menu);
}

/* Menu deactivate callback */
static void
xmenu_on_menu_deactivate(GtkWidget *menu G_GNUC_UNUSED, gpointer data)
{
    XmenuPlugin *xmenu = (XmenuPlugin *)data;
    xmenu->menu_visible = FALSE;
    gtk_widget_hide(menu);
}

/* Menu callbacks implementation */
static void
xmenu_about_computer(GtkMenuItem *item G_GNUC_UNUSED, gpointer data)
{
    XmenuPlugin *xmenu = (XmenuPlugin *)data;
    GtkWidget *dialog, *content, *label;
    gchar *text;
    struct utsname un;

    /* An empty command keeps the built-in system information dialog. */
    if (xmenu->about_computer_command[0] != '\0') {
        GError *error = NULL;
        if (!g_spawn_command_line_async(xmenu->about_computer_command, &error)) {
            xfce_dialog_show_error(NULL, error, _("Failed to open About This Computer"));
            g_error_free(error);
        }
        return;
    }

    /* Get system information */
    uname(&un);

    /* Create dialog */
    dialog = gtk_dialog_new_with_buttons(_("About This Computer"),
                                         NULL,
                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                         _("_Close"), GTK_RESPONSE_CLOSE,
                                         NULL);

    gtk_window_set_default_size(GTK_WINDOW(dialog), 400, 300);
    gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);

    /* Create content */
    content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_container_set_border_width(GTK_CONTAINER(content), 10);

    /* System information */
    text = g_strdup_printf("<b>%s</b>\n\n"
                          "<b>Operating System:</b> %s\n"
                          "<b>Kernel:</b> %s %s\n"
                          "<b>Architecture:</b> %s\n"
                          "<b>Hostname:</b> %s\n",
                          _("System Information"),
                          un.sysname, un.release, un.version,
                          un.machine, un.nodename);

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), text);
    gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
    gtk_label_set_selectable(GTK_LABEL(label), TRUE);
    gtk_box_pack_start(GTK_BOX(content), label, FALSE, FALSE, 0);

    g_free(text);

    /* Show dialog */
    gtk_widget_show_all(dialog);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

static void
xmenu_system_preferences(GtkMenuItem *item G_GNUC_UNUSED, gpointer data G_GNUC_UNUSED)
{
    /* Launch XFCE Settings Manager */
    GError *error = NULL;
    if (!g_spawn_command_line_async("xfce4-settings-manager", &error)) {
        xfce_dialog_show_error(NULL, error, _("Failed to open System Preferences"));
        g_error_free(error);
    }
}

static void
xmenu_software_update(GtkMenuItem *item G_GNUC_UNUSED, gpointer data)
{
    XmenuPlugin *xmenu = (XmenuPlugin *)data;
    GError *error = NULL;

    if (xmenu->software_update_command[0] == '\0') {
        xfce_dialog_show_error(NULL, NULL,
                              _("Set a Software Update command in the plugin Properties first."));
        return;
    }

    if (!g_spawn_command_line_async(xmenu->software_update_command, &error)) {
        xfce_dialog_show_error(NULL, error, _("Failed to open Software Update"));
        g_error_free(error);
    }
}

static void
xmenu_app_store(GtkMenuItem *item G_GNUC_UNUSED, gpointer data)
{
    XmenuPlugin *xmenu = (XmenuPlugin *)data;
    GError *error = NULL;

    /* Launch configured app store command */
    if (!g_spawn_command_line_async(xmenu->app_store_command, &error)) {
        gchar *message = g_strdup_printf(_("Failed to open App Store (%s)"), xmenu->app_store_command);
        xfce_dialog_show_error(NULL, error, "%s", message);
        g_free(message);
        g_error_free(error);
    }
}

static void
xmenu_force_quit(GtkMenuItem *item G_GNUC_UNUSED, gpointer data)
{
    XmenuPlugin *xmenu = (XmenuPlugin *)data;
    const gchar *command = xmenu->force_quit_command[0] != '\0'
        ? xmenu->force_quit_command : DEFAULT_FORCE_QUIT_COMMAND;

    /* Launch the configured application, or the default when empty. */
    GError *error = NULL;
    if (!g_spawn_command_line_async(command, &error)) {
        xfce_dialog_show_error(NULL, error, _("Failed to launch Force Quit"));
        g_error_free(error);
    }
}

static void
xmenu_sleep(GtkMenuItem *item G_GNUC_UNUSED, gpointer data G_GNUC_UNUSED)
{
    /* Suspend system */
    GError *error = NULL;
    if (!g_spawn_command_line_async("xfce4-session-logout --suspend", &error)) {
        xfce_dialog_show_error(NULL, error, _("Failed to suspend system"));
        g_error_free(error);
    }
}

static void
xmenu_restart(GtkMenuItem *item G_GNUC_UNUSED, gpointer data G_GNUC_UNUSED)
{
    /* Restart system */
    GError *error = NULL;
    if (!g_spawn_command_line_async("xfce4-session-logout --reboot", &error)) {
        xfce_dialog_show_error(NULL, error, _("Failed to restart system"));
        g_error_free(error);
    }
}

static void
xmenu_shutdown(GtkMenuItem *item G_GNUC_UNUSED, gpointer data G_GNUC_UNUSED)
{
    /* Shutdown system */
    GError *error = NULL;
    if (!g_spawn_command_line_async("xfce4-session-logout --halt", &error)) {
        xfce_dialog_show_error(NULL, error, _("Failed to shutdown system"));
        g_error_free(error);
    }
}

static void
xmenu_lock_screen(GtkMenuItem *item G_GNUC_UNUSED, gpointer data G_GNUC_UNUSED)
{
    /* Lock screen */
    GError *error = NULL;
    if (!g_spawn_command_line_async("xflock4", &error)) {
        xfce_dialog_show_error(NULL, error, _("Failed to lock screen"));
        g_error_free(error);
    }
}

static void
xmenu_logout(GtkMenuItem *item G_GNUC_UNUSED, gpointer data G_GNUC_UNUSED)
{
    /* Log out */
    GError *error = NULL;
    if (!g_spawn_command_line_async("xfce4-session-logout", &error)) {
        xfce_dialog_show_error(NULL, error, _("Failed to log out"));
        g_error_free(error);
    }
}

/* Configuration loading */
static void
xmenu_load_config(XmenuPlugin *xmenu)
{
    gchar *file;
    XfceRc *rc;

    /* Get config file location */
    file = xfce_panel_plugin_save_location(xmenu->plugin, TRUE);
    if (G_UNLIKELY(!file))
        return;

    /* Open config file */
    rc = xfce_rc_simple_open(file, TRUE);
    g_free(file);

    if (G_UNLIKELY(!rc))
        return;

    /* Read settings */
    xmenu->show_recent_items = xfce_rc_read_bool_entry(rc, "show-recent-items", TRUE);
    xmenu->recent_items_max = xfce_rc_read_int_entry(rc, "recent-items-max", 10);

    g_free(xmenu->custom_icon_name);
    xmenu->custom_icon_name = g_strdup(xfce_rc_read_entry(rc, "custom-icon-name", XMENU_ICON_NAME));

    g_free(xmenu->app_store_command);
    xmenu->app_store_command = g_strdup(xfce_rc_read_entry(rc, "app-store-command", DEFAULT_APP_STORE_COMMAND));

    g_free(xmenu->about_computer_command);
    xmenu->about_computer_command = g_strdup(xfce_rc_read_entry(rc, "about-computer-command", DEFAULT_ABOUT_COMPUTER_COMMAND));
    g_free(xmenu->force_quit_command);
    xmenu->force_quit_command = g_strdup(xfce_rc_read_entry(rc, "force-quit-command", DEFAULT_FORCE_QUIT_COMMAND));

    g_free(xmenu->software_update_command);
    xmenu->software_update_command = g_strdup(xfce_rc_read_entry(rc, "software-update-command", DEFAULT_SOFTWARE_UPDATE_COMMAND));

    xmenu->transparency = xfce_rc_read_int_entry(rc, "transparency", DEFAULT_TRANSPARENCY);

    /* Close config file */
    xfce_rc_close(rc);

    /* Update icon */
    gtk_image_set_from_icon_name(GTK_IMAGE(xmenu->icon),
                                 xmenu->custom_icon_name,
                                 GTK_ICON_SIZE_BUTTON);

    /* Apply transparency to both button and menu */
    if (xmenu->transparency < 100) {
        gtk_widget_set_opacity(GTK_WIDGET(xmenu->button),
                              xmenu->transparency / 100.0);
        if (xmenu->menu) {
            gtk_widget_set_opacity(GTK_WIDGET(xmenu->menu),
                                  xmenu->transparency / 100.0);
        }
    }
}

/* Configuration saving */
static void
xmenu_save_config(XfcePanelPlugin *plugin, XmenuPlugin *xmenu)
{
    gchar *file;
    XfceRc *rc;

    /* Get config file location */
    file = xfce_panel_plugin_save_location(plugin, TRUE);
    if (G_UNLIKELY(!file))
        return;

    /* Open config file */
    rc = xfce_rc_simple_open(file, FALSE);
    g_free(file);

    if (G_UNLIKELY(!rc))
        return;

    /* Write settings */
    xfce_rc_write_bool_entry(rc, "show-recent-items", xmenu->show_recent_items);
    xfce_rc_write_int_entry(rc, "recent-items-max", xmenu->recent_items_max);
    xfce_rc_write_entry(rc, "custom-icon-name", xmenu->custom_icon_name);
    xfce_rc_write_entry(rc, "app-store-command", xmenu->app_store_command);
    xfce_rc_write_entry(rc, "about-computer-command", xmenu->about_computer_command);
    xfce_rc_write_entry(rc, "force-quit-command", xmenu->force_quit_command);
    xfce_rc_write_entry(rc, "software-update-command", xmenu->software_update_command);
    xfce_rc_write_int_entry(rc, "transparency", xmenu->transparency);

    /* Close config file */
    xfce_rc_close(rc);
}

/* Configuration dialog response */
static void
xmenu_configure_response(GtkWidget *dialog, gint response, XmenuPlugin *xmenu)
{
    gboolean result;

    if (response == GTK_RESPONSE_HELP) {
        result = g_spawn_command_line_async("exo-open --launch WebBrowser "
                                          "https://docs.xfce.org/xfce/xfce4-panel/start",
                                          NULL);

        if (G_UNLIKELY(result == FALSE))
            g_warning(_("Unable to open the following url: %s"),
                     "https://docs.xfce.org/xfce/xfce4-panel/start");
    } else {
        /* Save configuration on close */
        xmenu_save_config(xmenu->plugin, xmenu);

        /* Recreate menu with new settings */
        if (xmenu->menu) {
            gtk_widget_destroy(xmenu->menu);
            xmenu->menu = NULL;
        }
        xmenu_create_menu(xmenu);

        /* Release the entries and their signal handlers on close. */
        gtk_widget_destroy(dialog);

        /* Unblock panel menu */
        xfce_panel_plugin_unblock_menu(xmenu->plugin);
    }
}

/* Icon chooser callback */
static void
xmenu_icon_chooser_clicked(GtkWidget *button, XmenuPlugin *xmenu)
{
    GtkWidget *chooser;
    gchar *icon;

    chooser = exo_icon_chooser_dialog_new(_("Select An Icon"),
                                         GTK_WINDOW(gtk_widget_get_toplevel(button)),
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
                                         NULL);

    gtk_dialog_set_default_response(GTK_DIALOG(chooser), GTK_RESPONSE_ACCEPT);
    exo_icon_chooser_dialog_set_icon(EXO_ICON_CHOOSER_DIALOG(chooser),
                                     xmenu->custom_icon_name);

    if (gtk_dialog_run(GTK_DIALOG(chooser)) == GTK_RESPONSE_ACCEPT) {
        icon = exo_icon_chooser_dialog_get_icon(EXO_ICON_CHOOSER_DIALOG(chooser));

        g_free(xmenu->custom_icon_name);
        xmenu->custom_icon_name = icon;

        /* Update button icon */
        gtk_image_set_from_icon_name(GTK_IMAGE(xmenu->icon),
                                     icon, GTK_ICON_SIZE_BUTTON);

        /* Update icon chooser button */
        gtk_image_set_from_icon_name(GTK_IMAGE(gtk_button_get_image(GTK_BUTTON(button))),
                                     icon, GTK_ICON_SIZE_DIALOG);
    }

    gtk_widget_destroy(chooser);
}

/* Transparency scale callback */
static void
xmenu_transparency_changed(GtkScale *scale, XmenuPlugin *xmenu)
{
    xmenu->transparency = (gint)gtk_range_get_value(GTK_RANGE(scale));

    /* Apply transparency to both button and menu */
    gtk_widget_set_opacity(GTK_WIDGET(xmenu->button),
                          xmenu->transparency / 100.0);
    if (xmenu->menu) {
        gtk_widget_set_opacity(GTK_WIDGET(xmenu->menu),
                              xmenu->transparency / 100.0);
    }
}

/* Keep command strings owned by the plugin, rather than the entry widget. */
static void
xmenu_command_changed(GtkEntry *entry, gchar **command)
{
    g_free(*command);
    *command = g_strdup(gtk_entry_get_text(entry));
}

static void
xmenu_add_command_entry(GtkGrid *grid, gint row, const gchar *title,
                            const gchar *tooltip, gchar **command)
{
    GtkWidget *label = gtk_label_new_with_mnemonic(title);
    GtkWidget *entry = gtk_entry_new();

    gtk_label_set_xalign(GTK_LABEL(label), 0.0);
    gtk_grid_attach(grid, label, 0, row, 1, 1);
    gtk_entry_set_text(GTK_ENTRY(entry), *command);
    gtk_widget_set_hexpand(entry, TRUE);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry);
    if (tooltip)
        gtk_widget_set_tooltip_text(entry, tooltip);
    g_signal_connect(entry, "changed", G_CALLBACK(xmenu_command_changed), command);
    gtk_grid_attach(grid, entry, 1, row, 1, 1);
}

/* Configuration dialog */
static void
xmenu_configure_plugin(XfcePanelPlugin *plugin, XmenuPlugin *xmenu)
{
    GtkWidget *dialog, *content_area, *grid;
    GtkWidget *label, *button, *scale, *check;
    GtkWidget *icon;
    gint row = 0;

    /* Block plugin context menu */
    xfce_panel_plugin_block_menu(plugin);

    /* Create dialog */
    dialog = xfce_titled_dialog_new_with_buttons(_("Xmenu Settings"),
                                                 GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(plugin))),
                                                 GTK_DIALOG_DESTROY_WITH_PARENT,
                                                 _("_Help"), GTK_RESPONSE_HELP,
                                                 _("_Close"), GTK_RESPONSE_OK,
                                                 NULL);

    /* Center dialog on the screen */
    gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
    gtk_window_set_icon_name(GTK_WINDOW(dialog), "xfce4-settings");

    /* Connect response signal */
    g_signal_connect(G_OBJECT(dialog), "response",
                     G_CALLBACK(xmenu_configure_response), xmenu);

    /* Create grid for layout */
    content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
    gtk_container_set_border_width(GTK_CONTAINER(grid), 12);
    gtk_box_pack_start(GTK_BOX(content_area), grid, TRUE, TRUE, 0);

    /* Icon chooser */
    label = gtk_label_new_with_mnemonic(_("_Icon:"));
    gtk_label_set_xalign(GTK_LABEL(label), 0.0);
    gtk_grid_attach(GTK_GRID(grid), label, 0, row, 1, 1);

    button = gtk_button_new();
    icon = gtk_image_new_from_icon_name(xmenu->custom_icon_name, GTK_ICON_SIZE_DIALOG);
    gtk_button_set_image(GTK_BUTTON(button), icon);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), button);
    g_signal_connect(G_OBJECT(button), "clicked",
                     G_CALLBACK(xmenu_icon_chooser_clicked), xmenu);
    gtk_grid_attach(GTK_GRID(grid), button, 1, row++, 1, 1);

    /* Configurable menu commands */
    xmenu_add_command_entry(GTK_GRID(grid), row++, _("_About This Computer command:"),
                                _("Leave empty to show the built-in system information dialog."),
                                &xmenu->about_computer_command);
    xmenu_add_command_entry(GTK_GRID(grid), row++, _("Software _Update command:"),
                                _("Enter the command for your preferred software updater."),
                                &xmenu->software_update_command);
    xmenu_add_command_entry(GTK_GRID(grid), row++, _("_App Store command:"),
                                NULL, &xmenu->app_store_command);
    xmenu_add_command_entry(GTK_GRID(grid), row++, _("_Force Quit command:"),
                                _("Leave empty to use xfce4-taskmanager."),
                                &xmenu->force_quit_command);

    /* Transparency */
    label = gtk_label_new_with_mnemonic(_("_Transparency:"));
    gtk_label_set_xalign(GTK_LABEL(label), 0.0);
    gtk_grid_attach(GTK_GRID(grid), label, 0, row, 1, 1);

    scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0, 100, 5);
    gtk_range_set_value(GTK_RANGE(scale), xmenu->transparency);
    gtk_scale_set_draw_value(GTK_SCALE(scale), TRUE);
    gtk_scale_set_value_pos(GTK_SCALE(scale), GTK_POS_RIGHT);
    gtk_widget_set_hexpand(scale, TRUE);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), scale);
    g_signal_connect(G_OBJECT(scale), "value-changed",
                     G_CALLBACK(xmenu_transparency_changed), xmenu);
    gtk_grid_attach(GTK_GRID(grid), scale, 1, row++, 1, 1);

    /* Show recent items */
    check = gtk_check_button_new_with_mnemonic(_("Show _recent items"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), xmenu->show_recent_items);
    g_signal_connect(G_OBJECT(check), "toggled",
                     G_CALLBACK(gtk_toggle_button_get_active), &xmenu->show_recent_items);
    gtk_grid_attach(GTK_GRID(grid), check, 0, row++, 2, 1);

    /* Show dialog */
    gtk_widget_show_all(dialog);
}

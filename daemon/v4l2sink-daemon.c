/* vi: set sw=4 ts=4 sts=4 expandtab wrap ai: */
/*
 * Copyright (C) 2020 yetist <yetist@gmail.com>
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
 * */

#include <libkmod.h>
#include "v4l2sink-daemon.h"
#include "dbus-generated.h"

#define V4L2SINK_DBUS_NAME "com.obsproject.v4l2sink"
#define V4L2SINK_DBUS_PATH "/com/obsproject/v4l2sink"
#define MODULE_NAME        "v4l2loopback"
#define VIDEO_NAME         "OBS-Camera"
#define TIMEOUT            30

enum {
    PROP_0,
	PROP_LOOP,
	PROP_NO_TIMEOUT,
    NUM_PROPERTIES
};

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

struct _V4l2sinkDaemon
{
    GObject          object;
    struct kmod_ctx *ctx;
	V4l2sink        *skeleton;
	guint            bus_name_id;
	GMainLoop       *loop;
	gboolean         no_timeout;
    guint            timeout_id;
};

G_DEFINE_TYPE (V4l2sinkDaemon, v4l2sink_daemon, G_TYPE_OBJECT);

gboolean daemon_timeout_cb (V4l2sinkDaemon *daemon)
{
    g_main_loop_quit (daemon->loop);
    return TRUE;
}

static void reset_timeout (V4l2sinkDaemon *daemon)
{
    if (daemon->timeout_id > 0) {
        g_source_remove (daemon->timeout_id);
        daemon->timeout_id = 0;
    }

    if (daemon->no_timeout)
        return;

    daemon->timeout_id = g_timeout_add_seconds (TIMEOUT, (GSourceFunc) daemon_timeout_cb, daemon);
}

static gboolean module_is_inkernel (struct kmod_ctx *ctx, const char *modname, V4l2sinkDaemon *daemon)
{
    struct kmod_module *mod;
    int state;
    gboolean ret;
    reset_timeout (daemon);

    if (kmod_module_new_from_name (ctx, modname, &mod) < 0)
        return FALSE;

    state = kmod_module_get_initstate (mod);

    if (state == KMOD_MODULE_LIVE || state == KMOD_MODULE_BUILTIN)
        ret = TRUE;
    else
        ret = FALSE;

    kmod_module_unref (mod);

    return ret;
}

void print_action (struct kmod_module *m, bool install, const char *options)
{
    const char *path;

    if (install) {
        g_debug ("install %s %s\n", kmod_module_get_install_commands (m), options);
        return;
    }

    path = kmod_module_get_path (m);

    if (path == NULL) {
        if (kmod_module_get_initstate (m) == KMOD_MODULE_BUILTIN)
            g_debug ("builtin %s\n", kmod_module_get_name (m));
    } else
        g_debug ("insmod %s %s\n", kmod_module_get_path(m), options);
}

gboolean v4l2sink_load_module (V4l2sink              *object,
                               GDBusMethodInvocation *invocation,
                               gpointer               user_data)
{
    V4l2sinkDaemon *daemon;
    struct kmod_list *list = NULL;
    int result = 0;
    int ret;

    daemon = V4L2SINK_DAEMON (user_data);

    if (module_is_inkernel (daemon->ctx, MODULE_NAME, daemon))
    {
        v4l2sink_complete_load_module (object, invocation, TRUE);
        reset_timeout (daemon);
        return TRUE;
    }

    ret = kmod_module_new_from_lookup (daemon->ctx, MODULE_NAME, &list);
    if (ret != 0) {
        kmod_module_unref_list (list);
		g_dbus_method_invocation_return_error (invocation, g_quark_from_static_string (V4L2SINK_DBUS_NAME), 1, "ERROR: not found module %s", MODULE_NAME);
        reset_timeout (daemon);
        return FALSE;
    }

    struct kmod_list *l;
    kmod_list_foreach(l, list) {
        struct kmod_module *kmod;
        kmod = kmod_module_get_module (l);
        result += kmod_module_probe_insert_module (kmod,
                KMOD_PROBE_IGNORE_LOADED,
                "card_label=\""VIDEO_NAME"\"",
                NULL,
                NULL,
                print_action);
        g_print("modprobe result: %d", result);
        kmod_module_unref(kmod);
    }

    kmod_module_unref_list (list);
    if (result == 0) {
        v4l2sink_set_module_in_kernel (daemon->skeleton, TRUE);
        v4l2sink_complete_load_module (object, invocation, TRUE);
        reset_timeout (daemon);
        return TRUE;
    } else {
		g_dbus_method_invocation_return_error (invocation, g_quark_from_static_string(V4L2SINK_DBUS_NAME), 1, "ERROR: load module failed: %s", MODULE_NAME);
        reset_timeout (daemon);
        return FALSE;
    }
}

static void bus_acquired_handler_cb (GDBusConnection *connection,
		                             const gchar     *name,
                                     gpointer         user_data)
{
    V4l2sinkDaemon *daemon;

	GError *error = NULL;
	gboolean exported;

    daemon = V4L2SINK_DAEMON (user_data);

	g_signal_connect (daemon->skeleton, "handle-load-module", G_CALLBACK (v4l2sink_load_module), daemon);

	exported = g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (daemon->skeleton),
			connection, V4L2SINK_DBUS_PATH, &error);

	if (!exported)
	{
		g_warning ("Failed to export interface: %s", error->message);
		g_error_free (error);

		g_main_loop_quit (daemon->loop);
	}
}

static void name_lost_handler_cb (GDBusConnection *connection,
                                  const gchar     *name,
                                  gpointer         user_data)
{
    V4l2sinkDaemon *daemon;

	g_debug("bus name lost\n");
    daemon = V4L2SINK_DAEMON (user_data);

	g_main_loop_quit (daemon->loop);
}

static void v4l2sink_daemon_constructed (GObject *object)
{
    V4l2sinkDaemon *daemon;
	GBusNameOwnerFlags flags;

    daemon = V4L2SINK_DAEMON (object);

	G_OBJECT_CLASS (v4l2sink_daemon_parent_class)->constructed (object);

    daemon->ctx = kmod_new (NULL, NULL);
	daemon->bus_name_id = g_bus_own_name (G_BUS_TYPE_SYSTEM,
			                              V4L2SINK_DBUS_NAME,
                                          G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT,
                                          bus_acquired_handler_cb,
                                          NULL,
                                          name_lost_handler_cb,
                                          daemon,
                                          NULL);
}

static void v4l2sink_daemon_dispose (GObject *object)
{
    V4l2sinkDaemon *daemon;

    daemon = V4L2SINK_DAEMON (object);

    if (daemon->skeleton != NULL)
    {
        GDBusInterfaceSkeleton *skeleton;

        skeleton = G_DBUS_INTERFACE_SKELETON (daemon->skeleton);
        g_dbus_interface_skeleton_unexport (skeleton);

        g_clear_object (&daemon->skeleton);
    }

    if (daemon->ctx != NULL )
    {
        kmod_unref (daemon->ctx);
    }

    if (daemon->bus_name_id > 0)
    {
        g_bus_unown_name (daemon->bus_name_id);
        daemon->bus_name_id = 0;
    }

    if (daemon->timeout_id > 0) {
        g_source_remove (daemon->timeout_id);
        daemon->timeout_id = 0;
    }

	G_OBJECT_CLASS (v4l2sink_daemon_parent_class)->dispose (object);
}

static void v4l2sink_daemon_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
    V4l2sinkDaemon *daemon;

    daemon = V4L2SINK_DAEMON (object);

    switch (prop_id)
    {
		case PROP_LOOP:
			daemon->loop = g_value_get_pointer (value);
			break;
		case PROP_NO_TIMEOUT:
			daemon->no_timeout = g_value_get_boolean (value);
			break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void v4l2sink_daemon_class_init (V4l2sinkDaemonClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    gobject_class->set_property = v4l2sink_daemon_set_property;
	gobject_class->constructed = v4l2sink_daemon_constructed;
	gobject_class->dispose = v4l2sink_daemon_dispose;

	properties[PROP_LOOP] =
        g_param_spec_pointer ("loop", "loop", "loop",
				              G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE |
                              G_PARAM_STATIC_STRINGS);
	properties[PROP_NO_TIMEOUT] =
        g_param_spec_boolean ("no-timeout",
                              "no-timeout",
                              "no-timeout",
                              FALSE,
                              G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE);

	g_object_class_install_properties (gobject_class, NUM_PROPERTIES, properties);
}

static void v4l2sink_daemon_init (V4l2sinkDaemon *daemon)
{
    daemon->timeout_id = 0;
	daemon->skeleton = v4l2sink_skeleton_new ();
    v4l2sink_set_module_in_kernel (daemon->skeleton, module_is_inkernel (daemon->ctx, MODULE_NAME, daemon));
}

V4l2sinkDaemon* v4l2sink_daemon_new (GMainLoop *loop, gboolean no_timeout)
{
	return g_object_new (V4L2SINK_TYPE_DAEMON, "loop", loop, "no-timeout", no_timeout, NULL);
}

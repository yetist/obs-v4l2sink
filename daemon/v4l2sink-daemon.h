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

#ifndef __V4L2SINK_DAEMON_H__
#define __V4L2SINK_DAEMON_H__  1

#include <glib-object.h>

G_BEGIN_DECLS

#define V4L2SINK_TYPE_DAEMON  (v4l2sink_daemon_get_type ())

G_DECLARE_FINAL_TYPE (V4l2sinkDaemon, v4l2sink_daemon, V4L2SINK, DAEMON, GObject)

V4l2sinkDaemon* v4l2sink_daemon_new (GMainLoop *loop, gboolean no_timeout);

G_END_DECLS

#endif /* __V4L2SINK_DAEMON_H__ */

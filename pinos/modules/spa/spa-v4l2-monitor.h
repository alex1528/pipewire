/* Pinos
 * Copyright (C) 2015 Wim Taymans <wim.taymans@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef __PINOS_SPA_V4L2_MONITOR_H__
#define __PINOS_SPA_V4L2_MONITOR_H__

#include <pinos/server/core.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _PinosSpaV4l2Monitor PinosSpaV4l2Monitor;

struct _PinosSpaV4l2Monitor {
  SpaMonitor *monitor;
};

PinosSpaV4l2Monitor *      pinos_spa_v4l2_monitor_new      (PinosCore *core);
void                       pinos_spa_v4l2_monitor_destroy  (PinosSpaV4l2Monitor *monitor);

#ifdef __cplusplus
}
#endif

#endif /* __PINOS_SPA_V4L2_MONITOR_H__ */

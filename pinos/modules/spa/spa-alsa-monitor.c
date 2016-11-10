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

#include <dlfcn.h>
#include <errno.h>
#include <poll.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>

#include <spa/include/spa/node.h>
#include <spa/include/spa/monitor.h>
#include <pinos/client/log.h>
#include <pinos/server/node.h>

#include "spa-alsa-monitor.h"

typedef struct
{
  PinosSpaALSAMonitor this;

  PinosObject object;

  PinosCore *core;

  SpaHandle *handle;

  GHashTable *nodes;
} PinosSpaALSAMonitorImpl;

static SpaResult
make_handle (PinosCore *core, SpaHandle **handle, const char *lib, const char *name, const SpaDict *info)
{
  SpaResult res;
  void *hnd, *state = NULL;
  SpaEnumHandleFactoryFunc enum_func;

  if ((hnd = dlopen (lib, RTLD_NOW)) == NULL) {
    g_error ("can't load %s: %s", lib, dlerror());
    return SPA_RESULT_ERROR;
  }
  if ((enum_func = dlsym (hnd, "spa_enum_handle_factory")) == NULL) {
    g_error ("can't find enum function");
    return SPA_RESULT_ERROR;
  }

  while (true) {
    const SpaHandleFactory *factory;

    if ((res = enum_func (&factory, &state)) < 0) {
      if (res != SPA_RESULT_ENUM_END)
        g_error ("can't enumerate factories: %d", res);
      break;
    }
    if (strcmp (factory->name, name))
      continue;

    *handle = g_malloc0 (factory->size);
    if ((res = spa_handle_factory_init (factory, *handle, info, core->support, core->n_support)) < 0) {
      g_error ("can't make factory instance: %d", res);
      return res;
    }
    return SPA_RESULT_OK;
  }
  return SPA_RESULT_ERROR;
}

static void
add_item (PinosSpaALSAMonitor *this, SpaMonitorItem *item)
{
  PinosSpaALSAMonitorImpl *impl = SPA_CONTAINER_OF (this, PinosSpaALSAMonitorImpl, this);
  SpaResult res;
  SpaHandle *handle;
  PinosNode *node;
  void *node_iface, *clock_iface;
  PinosProperties *props = NULL;

  pinos_log_debug ("alsa-monitor %p: add: \"%s\" (%s)", this, item->name, item->id);

  handle = calloc (1, item->factory->size);
  if ((res = spa_handle_factory_init (item->factory,
                                      handle,
                                      item->info,
                                      impl->core->support,
                                      impl->core->n_support)) < 0) {
    g_error ("can't make factory instance: %d", res);
    return;
  }
  if ((res = spa_handle_get_interface (handle, impl->core->registry.uri.spa_node, &node_iface)) < 0) {
    g_error ("can't get NODE interface: %d", res);
    return;
  }
  if ((res = spa_handle_get_interface (handle, impl->core->registry.uri.spa_clock, &clock_iface)) < 0) {
    pinos_log_debug ("can't get CLOCK interface: %d", res);
    clock_iface = NULL;
  }

  if (item->info) {
    unsigned int i;

    props = pinos_properties_new (NULL, NULL);

    for (i = 0; i < item->info->n_items; i++)
      pinos_properties_set (props,
                            item->info->items[i].key,
                            item->info->items[i].value);
  }

  node = pinos_node_new (impl->core,
                         item->factory->name,
                         node_iface,
                         clock_iface,
                         props);

  g_hash_table_insert (impl->nodes, strdup (item->id), node);
}

static void
remove_item (PinosSpaALSAMonitor *this, SpaMonitorItem *item)
{
  PinosSpaALSAMonitorImpl *impl = SPA_CONTAINER_OF (this, PinosSpaALSAMonitorImpl, this);
  PinosNode *node;

  pinos_log_debug ("alsa-monitor %p: remove: \"%s\" (%s)", this, item->name, item->id);

  node = g_hash_table_lookup (impl->nodes, item->id);
  if (node) {
    pinos_node_destroy (node);
    g_hash_table_remove (impl->nodes, item->id);
  }
}

static void
on_monitor_event  (SpaMonitor      *monitor,
                   SpaMonitorEvent *event,
                   void            *user_data)
{
  PinosSpaALSAMonitorImpl *impl = SPA_CONTAINER_OF (monitor, PinosSpaALSAMonitorImpl, this);
  PinosSpaALSAMonitor *this = &impl->this;

  switch (event->type) {
    case SPA_MONITOR_EVENT_TYPE_ADDED:
    {
      SpaMonitorItem *item = (SpaMonitorItem *) event;
      add_item (this, item);
      break;
    }
    case SPA_MONITOR_EVENT_TYPE_REMOVED:
    {
      SpaMonitorItem *item = (SpaMonitorItem *) event;
      remove_item (this, item);
    }
    case SPA_MONITOR_EVENT_TYPE_CHANGED:
    {
      SpaMonitorItem *item = (SpaMonitorItem *) event;
      pinos_log_debug ("alsa-monitor %p: changed: \"%s\"", this, item->name);
      break;
    }
    default:
      break;
  }
}

static void
monitor_destroy (PinosObject * object)
{
  PinosSpaALSAMonitorImpl *impl = SPA_CONTAINER_OF (object, PinosSpaALSAMonitorImpl, object);
  PinosSpaALSAMonitor *this = &impl->this;

  pinos_log_debug ("spa-monitor %p: destroy", this);

  spa_handle_clear (impl->handle);
  free (impl->handle);

  g_hash_table_unref (impl->nodes);
  free (impl);
}

PinosSpaALSAMonitor *
pinos_spa_alsa_monitor_new (PinosCore *core)
{
  PinosSpaALSAMonitorImpl *impl;
  PinosSpaALSAMonitor *this;
  SpaHandle *handle;
  SpaResult res;
  void *iface;
  void *state = NULL;

  if ((res = make_handle (core, &handle,
                          "build/spa/plugins/alsa/libspa-alsa.so",
                          "alsa-monitor",
                          NULL)) < 0) {
    g_error ("can't create alsa-monitor: %d", res);
    return NULL;
  }


  if ((res = spa_handle_get_interface (handle,
                                       core->registry.uri.spa_monitor,
                                       &iface)) < 0) {
    free (handle);
    pinos_log_error ("can't get MONITOR interface: %d", res);
    return NULL;
  }
  impl = calloc (1, sizeof (PinosSpaALSAMonitorImpl));
  impl->core = core;
  this = &impl->this;
  this->monitor = iface;

  pinos_object_init (&impl->object,
                     impl->core->registry.uri.monitor,
                     this,
                     monitor_destroy);

  impl->nodes = g_hash_table_new_full (g_str_hash,
                                       g_str_equal,
                                       free,
                                       NULL);

  while (true) {
    SpaMonitorItem *item;

    if ((res = spa_monitor_enum_items (this->monitor, &item, &state)) < 0) {
      if (res != SPA_RESULT_ENUM_END)
        pinos_log_debug ("spa_monitor_enum_items: got error %d\n", res);
      break;
    }
    add_item (this, item);
  }
  spa_monitor_set_event_callback (this->monitor, on_monitor_event, impl);

  return this;
}

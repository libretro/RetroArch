/**************************************************************

   drm_hook.cpp - Linux DRM/KMS library hook

   ---------------------------------------------------------

   Switchres   Modeline generation engine for emulation

   License     GPL-2.0+
   Copyright   2010-2022 Chris Kennedy, Antonio Giner,
                         Alexandre Wodarczyk, Gil Delescluse

 **************************************************************/

#include <stdio.h>
#include <stdint.h>
#include <dlfcn.h>
#include <cstring>

// DRM headers
#include <xf86drm.h>
#include <xf86drmMode.h>

#define MAX_CONNECTORS 10

bool hook_connector(drmModeConnectorPtr conn);
drmModeConnectorPtr get_connector(uint32_t connector_id);

typedef struct connector_hook
{
	drmModeConnector conn;
	drmModeModeInfo modes[128];
	uint32_t props[128];
	uint64_t prop_values[128];
	uint32_t encoders[128];
} connector_hook;

connector_hook connector[MAX_CONNECTORS];
int m_num_connectors = 0;

//============================================================
//  drmModeGetConnector
//============================================================

drmModeConnectorPtr drmModeGetConnector(int fd, uint32_t connector_id)
{
	static void* (*my_get_connector)(int, uint32_t) = NULL;

	if (!my_get_connector)
		my_get_connector = (void*(*)(int, uint32_t))dlsym(RTLD_NEXT, "drmModeGetConnector");

	// Allow hook detection (original func would return NULL)
	if (fd == -1)
		return &connector[0].conn;

	drmModeConnectorPtr conn = get_connector(connector_id);

	if (conn != NULL)
		// already hooked
		return conn;

	else
	{
		// attempt connector hook
		conn = (drmModeConnectorPtr)my_get_connector(fd, connector_id);
		if (!conn) return NULL;

		if (hook_connector(conn))
		{
			conn = get_connector(connector_id);
			printf("Switchres: returning hooked connector %d\n", conn->connector_id);
		}
	}

	return conn;
}

//============================================================
//  drmModeGetConnectorCurrent
//============================================================

drmModeConnectorPtr drmModeGetConnectorCurrent(int fd, uint32_t connector_id)
{
	static void* (*my_get_connector_current)(int, uint32_t) = NULL;

	if (!my_get_connector_current)
		my_get_connector_current = (void*(*)(int, uint32_t))dlsym(RTLD_NEXT, "drmModeGetConnectorCurrent");

	// Allow hook detection (original func would return NULL)
	if (fd == -1)
		return &connector[0].conn;

	drmModeConnectorPtr conn = get_connector(connector_id);

	if (conn != NULL)
		// already hooked
		return conn;

	else
	{
		// attempt connector hook
		conn = (drmModeConnectorPtr)my_get_connector_current(fd, connector_id);
		if (!conn) return NULL;

		if (hook_connector(conn))
		{
			conn = get_connector(connector_id);
			printf("Switchres: returning hooked connector %d\n", conn->connector_id);
		}
	}

	return conn;
}

//============================================================
//  drmModeFreeConnector
//============================================================

void drmModeFreeConnector(drmModeConnectorPtr ptr)
{
	static void (*my_free_connector)(drmModeConnectorPtr) = NULL;

	if (!my_free_connector)
		my_free_connector = (void (*)(drmModeConnectorPtr)) dlsym(RTLD_NEXT, "drmModeFreeConnector");

	// Skip our hooked connector
	for (int i = 0; i < m_num_connectors; i++)
		if (ptr == &connector[i].conn)
			return;

	my_free_connector(ptr);
}

//============================================================
//  hook_connector
//============================================================

bool hook_connector(drmModeConnectorPtr conn)
{
	if (conn == NULL)
		return false;

	if (m_num_connectors >= MAX_CONNECTORS)
		return false;

	if (conn->count_modes == 0)
		return false;

	connector_hook *conn_hook = &connector[m_num_connectors++];
	drmModeConnectorPtr my_conn = &conn_hook->conn;

	printf("Switchres: hooking connector %d\n", conn->connector_id);
	*my_conn = *conn;

	drmModeModeInfo *my_modes = conn_hook->modes;
	memcpy(my_modes, conn->modes, sizeof(drmModeModeInfo) * conn->count_modes);
	my_conn->modes = my_modes;

	uint32_t *my_encoders = conn_hook->encoders;
	memcpy(my_encoders, conn->encoders, sizeof(uint32_t) * conn->count_encoders);
	my_conn->encoders = my_encoders;

	uint32_t *my_props = conn_hook->props;
	memcpy(my_props, conn->props, sizeof(uint32_t) * conn->count_props);
	my_conn->props = my_props;

	uint64_t *my_prop_values = conn_hook->prop_values;
	memcpy(my_prop_values, conn->prop_values, sizeof(uint64_t) * conn->count_props);
	my_conn->prop_values = my_prop_values;

	drmModeFreeConnector(conn);

	bool found = false;
	drmModeModeInfo *mode = NULL;
	for (int i = 0; i < my_conn->count_modes; i++)
	{
		mode = &my_modes[i];
		if (mode->type & DRM_MODE_TYPE_PREFERRED)
		{
			found = true;
			break;
		}
	}

	// If preferred mode not found, default to first entry
	if (!found)
		mode = &my_modes[0];

	// Add dummy mode to mode list (preferred mode with hdisplay +1, vfresh +1)
	drmModeModeInfo *dummy_mode = &my_modes[my_conn->count_modes];
	*dummy_mode = *mode;
	dummy_mode->vrefresh++;
	dummy_mode->hdisplay++;
	dummy_mode->type |= (1<<7);
	my_conn->count_modes++;

	return true;
}

//============================================================
//  get_connector
//============================================================

drmModeConnectorPtr get_connector(uint32_t connector_id)
{
	for (int i = 0; i < m_num_connectors; i++)
		if (connector[i].conn.connector_id == connector_id)
			return &connector[i].conn;

	return NULL;
}

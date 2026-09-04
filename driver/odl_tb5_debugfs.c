// SPDX-License-Identifier: GPL-2.0
/* Read-only driver state used by `odl_tb5_cli diag`. */
#include <linux/debugfs.h>
#include <linux/err.h>
#include <linux/seq_file.h>

#include "odl_tb5_core.h"

static struct dentry *odl_tb5_debugfs_root;

static const char *odl_tb5_state_name(enum odl_tb5_conn_state state)
{
	switch (state) {
	case ODL_TB5_STATE_DISCONNECTED: return "DISCONNECTED";
	case ODL_TB5_STATE_HANDSHAKE:    return "HANDSHAKE";
	case ODL_TB5_STATE_CONNECTED:    return "CONNECTED";
	case ODL_TB5_STATE_ERROR:        return "ERROR";
	case ODL_TB5_STATE_READY:        return "READY";
	default:                         return "UNKNOWN";
	}
}

static int odl_tb5_status_show(struct seq_file *out, void *unused)
{
	struct odl_tb5_device *dev;
	int count = 0;

	seq_printf(out, "loopback: %d\n", odl_loopback_count);
	mutex_lock(&odl_tb5_devices_lock);
	list_for_each_entry(dev, &odl_tb5_devices_list, list) {
		seq_printf(out,
			   "dev%d: state=%s ring_size=%u login_sent=%d "
			   "login_received=%d login_retries=%d dma_verified=%d "
			   "peer_ping_answered=%d open_count=%d\n",
			   dev->index, odl_tb5_state_name(dev->state),
			   dev->tx.ring_size, dev->login_sent,
			   dev->login_received, dev->login_retries,
			   dev->pong_received, dev->peer_ping_answered,
			   atomic_read(&dev->open_count));
		count++;
	}
	mutex_unlock(&odl_tb5_devices_lock);
	seq_printf(out, "devices: %d\n", count);
	return 0;
}
DEFINE_SHOW_ATTRIBUTE(odl_tb5_status);

void odl_tb5_debugfs_init(void)
{
	odl_tb5_debugfs_root = debugfs_create_dir("odl_tb5", NULL);
	if (IS_ERR_OR_NULL(odl_tb5_debugfs_root)) {
		odl_tb5_debugfs_root = NULL;
		return;
	}
	debugfs_create_file("status", 0444, odl_tb5_debugfs_root, NULL,
			    &odl_tb5_status_fops);
}

void odl_tb5_debugfs_exit(void)
{
	debugfs_remove_recursive(odl_tb5_debugfs_root);
	odl_tb5_debugfs_root = NULL;
}

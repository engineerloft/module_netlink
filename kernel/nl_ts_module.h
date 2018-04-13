#ifndef __NL_TS_MODULE_H__
#define __NL_TS_MODULE_H__

#include "nl_ts_queue.h"

extern int nl_ts_iface_tx_ts_add(int iface_desc, struct nl_ts *ts);
extern int nl_ts_iface_rx_ts_add(int iface_desc, struct nl_ts *ts);

extern int nl_ts_iface_register(const char *iface);
extern int nl_ts_iface_unregister(int iface_desc);

#endif /* __NL_TS_MODULE_H__ */

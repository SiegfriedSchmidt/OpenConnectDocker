#ifndef OC_MAIN_CTL_H
#define OC_MAIN_CTL_H

#include <occtl/ctl.h>
#include <ev.h>

int ctl_handler_init(main_server_st *s);
void ctl_handler_deinit(main_server_st *s);

void ctl_handler_set_fds(main_server_st *s, ev_io *watcher);
void ctl_handler_run_pending(main_server_st *s, ev_io *watcher);
void ctl_handler_notify(main_server_st *s, struct proc_st *proc,
			unsigned int connect);

#endif

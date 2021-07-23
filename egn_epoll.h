#pragma once
#include "egn_core.h"


int egn_epoll_init(egn_t *egn,int n);
void egn_epoll_done(egn_t *egn);

int egn_epoll_add_event(egn_t *egn,egn_event_t *ev,int points,int flags);
int egn_epoll_del_event(egn_t *egn,egn_event_t *ev,int points,int flags);
void egn_epoll_process_event(egn_t *egn,int timeout);
void log_epoll_events(int reevents);

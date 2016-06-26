#ifndef __WATCHPOINT_H__
#define __WATCHPOINT_H__

#include "common.h"

typedef struct watchpoint {
	int NO;
	struct watchpoint *next;
	uint32_t old_value;
	char addr[100];

	/* Add more members if necessary */


} WP;

void init_wp_list();
WP *new_wp();
void free_wp(WP *);
WP *head;

#endif

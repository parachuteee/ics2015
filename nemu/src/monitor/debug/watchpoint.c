#include "monitor/watchpoint.h"
#include "monitor/expr.h"

#define NR_WP 32

static WP wp_list[NR_WP];
WP *head, *free_;

void init_wp_list() {
	int i;
	for(i = 0; i < NR_WP; i ++) {
		wp_list[i].NO = i; 
		wp_list[i].next = &wp_list[i + 1];
		wp_list[i].addr[0] = 0;
	}
	wp_list[NR_WP - 1].next = NULL;

	head = NULL;
	free_ = wp_list;
}

WP *new_wp() {
	WP *temp_f = free_;

	if (temp_f == NULL)
		assert(0);
	WP *New = malloc(sizeof(WP));
	New->NO = temp_f->NO;
	New->next = temp_f->next;
	New->old_value = temp_f->old_value;

	if (head == NULL) {
		New->next = NULL;
		head = New;
	}
	else {
		WP *temp = head;
		while (temp->next != NULL) 
			temp = temp->next;
		New->next = NULL;
		temp->next = New;
	}
	free_ = free_->next;
	return New;	
}

void free_wp(WP *wp) {
	if (wp == NULL) assert(0);

	WP *temp_f = free_;
	WP *New = malloc(sizeof(WP));
	New->NO = wp->NO;
	New->next = NULL;
	New->addr[0] = 0;
	New->old_value = 0;

	if (temp_f == NULL) {
		free_ = New;
	}
	else {
		WP *last_f = free_;
		while (temp_f->next != NULL && temp_f->NO < wp->NO) {
			last_f = temp_f;
			temp_f = temp_f->next;
		}
		if (temp_f == free_) {
			New->next = free_;
			free_ = New;
		}
		else {
			last_f->next = New;
			New->next = temp_f;
		}
	}

	if (wp == head)
		head = head->next;
	else {
		WP *temp_h = head;
		while (temp_h->next != wp)
			temp_h = temp_h->next;
		temp_h->next = wp->next;
	}

}

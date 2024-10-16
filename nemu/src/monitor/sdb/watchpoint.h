#ifndef __WATCHPOINT_H__
#define __WATCHPOINT_H__

#include "common.h"
#include "sdb.h"

#define NR_WP 32
#define EXPR_LEN 256

typedef struct watchpoint {
  int                NO;
  struct watchpoint *next;

  bool used;

  char expr[EXPR_LEN];

  sword_t old_value;
} WP;

static WP  wp_pool[NR_WP] = {};
static WP *head = NULL, *free_ = NULL;

void init_wp_pool();

WP *new_wp();

void free_wp(WP *wp);

void info_watchpoints();

#endif
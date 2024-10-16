/***************************************************************************************
* Copyright (c) 2014-2022 Zihao Yu, Nanjing University
*
* NEMU is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*          http://license.coscl.org.cn/MulanPSL2
*
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
*
* See the Mulan PSL v2 for more details.
***************************************************************************************/

#include "common.h"
#include "sdb.h"

#define NR_WP 32
#define EXPR_LEN 32

typedef struct watchpoint {
  int                NO;
  struct watchpoint *next;

  /* TODO: Add more members if necessary */
  char expr[EXPR_LEN];
  word_t last_value;

} WP;

static WP  wp_pool[NR_WP] = {};
static WP *head = NULL, *free_ = NULL;

void init_wp_pool() {
  int i;
  for (i = 0; i < NR_WP; i++) {
    wp_pool[i].NO   = i;
    wp_pool[i].next = (i == NR_WP - 1 ? NULL : &wp_pool[i + 1]);
  }

  head  = NULL;
  free_ = wp_pool;
}

/* TODO: Implement the functionality of watchpoint */

WP *new_wp() {
  if (free_ == NULL) {
    printf("No enough watchpoints.\n");
    return NULL;
  }

  WP *wp = free_;
  free_ = free_->next;

  wp->next = head;
  head = wp;

  return wp;
}

void free_wp(WP *wp) {
  if (wp == NULL) {
    return;
  }

  if (head == wp) {
  head = wp->next;
  } else {
    WP *prev = head;
    while (prev != NULL && prev->next != wp) {
      prev = prev->next;
    }

    if (prev) {
      prev->next = wp->next;
    }
  }

  wp->next = free_;
  free_ = wp;
}

// void check_watchpoints() {
//   WP *wp = head;
//   while (wp) {
//     bool success = true;
//     sword_t new_value = eval(wp->expr, &success);
//     if (!success) {
//       printf("Failed to evaluate expression: %s\n", wp->expr);
//     } else if (new_value != wp->last_value) {
//       printf("Watchpoint %d triggered: %s\n", wp->NO, wp->expr);
//       printf("Old value = %u, New value = %lu\n", wp->last_value, new_value);
//       wp->last_value = new_value;
//       nemu_state.state = NEMU_STOP;  // 暂停程序
//     }
//     wp = wp->next;
//   }
// }


void info_watchpoints() {
  WP *wp = head;
  if (!wp) {
    printf("No watchpoints.\n");
    return;
  }
  while (wp) {
    printf("Watchpoint %d: %s, value = %u\n", wp->NO, wp->expr, wp->last_value);
    wp = wp->next;
  }
}

void delete_watchpoint(int no) {
  WP *wp = head;
  while (wp) {
    if (wp->NO == no) {
      free_wp(wp);
      printf("Deleted watchpoint %d\n", no);
      return;
    }
    wp = wp->next;
  }
  printf("No watchpoint number %d\n", no);
}

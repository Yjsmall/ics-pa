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

#include "watchpoint.h"
#include "utils.h"
#include <stdio.h>
#include <time.h>

void init_wp_pool() {
  int i;
  for (i = 0; i < NR_WP; i++) {
    wp_pool[i].NO   = i;
    wp_pool[i].next = (i == NR_WP - 1 ? NULL : &wp_pool[i + 1]);
    wp_pool[i].used = false;
  }

  head  = NULL;
  free_ = wp_pool;
}

WP *new_wp() {
  for (WP *p = free_; p->next != NULL; p = p->next) {
    if (p->used == false) {
      p->used = true;
      if (head == NULL) {
        head = p;
      }
      return p;
    }
  }
  printf("No enough watchpoints.\n");
  assert(0);
  return NULL;
}

void free_wp(WP *wp) {
  if (head->NO == wp->NO) {
    head->used = false;
    head       = NULL;
    printf("Delete watchpoint  success.\n");
    return;
  }

  for (WP *p = head; p->next != NULL; p = p->next) {
    //	printf("wp -> no = %d , head -> no = %d, p -> no = %d.\n", wp -> NO, p-> NO, head -> NO);
    if (p->next->NO == wp->NO) {
      p->next       = p->next->next;
      p->next->used = false; // 没有被使用
      printf("free succes.\n");
      return;
    }
  }
}

void check_watchpoints() {
  for (int i = 0; i < NR_WP; i++) {
    if (wp_pool[i].used) {
      bool    success   = false;
      sword_t new_value = expr(wp_pool[i].expr, &success);

      if (!success) {
        printf("The expr of watchpoint %d is error\n", i);
        return;
      }

      if (new_value != wp_pool[i].old_value) {
        nemu_state.state = NEMU_STOP;
        printf("Watchpoint %d: %s\n", i, wp_pool[i].expr);
        printf("Old value = %u\n", wp_pool[i].old_value);
        printf("New value = %u\n", new_value);
        wp_pool[i].old_value = new_value;
      }
    }
  }
}

void info_watchpoints() {
  WP *wp = head;
  if (!wp) {
    printf("No watchpoints.\n");
    return;
  }
  for (int i = 0; i < NR_WP; i++) {
    if (wp_pool[i].used) {
      printf("Watchpoint %d: %s, value = %u\n", i, wp_pool[i].expr, wp_pool[i].old_value);
    }
  }
}

void delete_watchpoint(int no) {
  for (int i = 0; i < NR_WP; i++) {
    if (wp_pool[i].NO == no) {
      free_wp(&wp_pool[i]);
      return;
    }
  }
}

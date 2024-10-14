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

#include "macro.h"
#include <isa.h>

/* We use the POSIX regex functions to process regular expressions.
 * Type 'man regex' for more information about POSIX regex functions.
 */
#include <regex.h>
#include <setjmp.h>
#include <stdio.h>
#include <string.h>

enum {
  TK_NOTYPE = 256,
  TK_EQ,
  TK_NUM,
  TK_HEX,

  /* TODO: Add more token types */

};

static struct rule {
  const char *regex;
  int         token_type;
} rules[] = {

    /* TODO: Add more rules.
   * Pay attention to the precedence level of different rules.
   */

    {" +",     TK_NOTYPE}, // spaces
    {"[0-9]+", TK_NUM   },
    {"\\+",    '+'      }, // plus
    {"\\-",    '-'      },
    {"\\*",    '*'      },
    {"\\/",    '/'      },
    {"\\(",    '('      },
    {"\\)",    ')'      },
    {"==",     TK_EQ    }, // equal
};

#define NR_REGEX ARRLEN(rules)

static regex_t re[NR_REGEX] = {};

/* Rules are used for many times.
 * Therefore we compile them only once before any usage.
 */
void init_regex() {
  int  i;
  char error_msg[128];
  int  ret;

  for (i = 0; i < NR_REGEX; i++) {
    ret = regcomp(&re[i], rules[i].regex, REG_EXTENDED);
    if (ret != 0) {
      regerror(ret, &re[i], error_msg, 128);
      panic("regex compilation failed: %s\n%s", error_msg, rules[i].regex);
    }
  }
}

typedef struct token {
  int  type;
  char str[32];
} Token;

#define INITIAL_CAPACITY 32  // Initial capacity for the tokens array
static int capacity = 0; 
static Token *tokens __attribute__((used)) = NULL;
static int   nr_token __attribute__((used))   = 0;

static bool make_token(char *e) {
  int        position = 0;
  int        i;
  regmatch_t pmatch;

    // Initialize tokens array if it's not already initialized
  if (tokens == NULL) {
    capacity = INITIAL_CAPACITY;
    tokens = malloc(sizeof(Token) * capacity);
    if (tokens == NULL) {
      // Handle memory allocation failure
      return false;
    }
  }

  nr_token = 0;

  while (e[position] != '\0') {
    /* Try all rules one by one. */
    for (i = 0; i < NR_REGEX; i++) {
      if (regexec(&re[i], e + position, 1, &pmatch, 0) == 0 && pmatch.rm_so == 0) {
        char *substr_start = e + position;
        int   substr_len   = pmatch.rm_eo;

        Log("match rules[%d] = \"%s\" at position %d with len %d: %.*s",
            i, rules[i].regex, position, substr_len, substr_len, substr_start);

        position += substr_len;

        /* TODO: Now a new token is recognized with rules[i]. Add codes
         * to record the token in the array `tokens'. For certain types
         * of tokens, some extra actions should be performed.
         */

        if (rules[i].token_type == TK_NOTYPE) {
          break;
        }

        // If we reach the current capacity, resize the tokens array
        if (nr_token >= capacity) {
          capacity *= 2; // Double the capacity
          Token *new_tokens = realloc(tokens, sizeof(Token) * capacity);
          if (new_tokens == NULL) {
            // Handle memory allocation failure
            free(tokens);
            return false;
          }
          tokens = new_tokens;
        }

        switch (rules[i].token_type) {
          case '+': tokens[nr_token++].type = '+'; break;
          case '-': tokens[nr_token++].type = '-'; break;
          case '*': tokens[nr_token++].type = '*'; break;
          case '/': tokens[nr_token++].type = '/'; break;
          case '(': tokens[nr_token++].type = '('; break;
          case ')': tokens[nr_token++].type = ')'; break;
          case TK_NUM:
            strncpy(tokens[nr_token].str, substr_start, substr_len);
            tokens[nr_token].str[substr_len] = '\0';
            tokens[nr_token++].type          = TK_NUM;
            break;
        }

        break;
      }
    }

    if (i == NR_REGEX) {
      printf("no match at position %d\n%s\n%*.s^\n", position, e, position, "");
      return false;
    }
  }

  return true;
}

static bool check_parentheses(int p, int q) {
  if (tokens[p].type != '(' || tokens[q].type != ')') {
    return false;
  }

  int tag = 0;
  for (int i = p; i <= q; ++i) {
    if (tokens[i].type == '(') {
      tag++;
    } else if (tokens[i].type == ')') {
      tag--;
    }

    if (tag == 0 && i < q) {
      return false;
    }
  }

  if (tag != 0) {
    return false;
  }

  return true;
}

int find_major(int p, int q) {
  int ret = -1, par = 0, op_type = 0;
  for (int i = p; i <= q; i++) {
    if (tokens[i].type == TK_NUM) {
      continue;
    }
    if (tokens[i].type == '(') {
      par++;
    } else if (tokens[i].type == ')') {
      if (par == 0) {
        return -1;
      }
      par--;
    } else if (par > 0) {
      continue;
    } else {
      int tmp_type = 0;
      switch (tokens[i].type) {
        case '*':
        case '/': tmp_type = 1; break;
        case '+':
        case '-': tmp_type = 2; break;
        default: assert(0);
      }
      if (tmp_type >= op_type) {
        op_type = tmp_type;
        ret     = i;
      }
    }
  }
  if (par != 0) return -1;
  return ret;
}

word_t eval(int p, int q, bool *ok) {
  *ok = true;
  if (p > q) {
    *ok = false;
    return 0;
  } else if (p == q) {
    if (tokens[p].type != TK_NUM) {
      *ok = false;
      return 0;
    }
    word_t ret = strtol(tokens[p].str, NULL, 10);
    return ret;
  } else if (check_parentheses(p, q)) {
    return eval(p + 1, q - 1, ok);
  } else {
    int major = find_major(p, q);
    if (major < 0) {
      *ok = false;
      return 0;
    }

    word_t val1 = eval(p, major - 1, ok);
    if (!*ok) return 0;
    word_t val2 = eval(major + 1, q, ok);
    if (!*ok) return 0;

    switch (tokens[major].type) {
      case '+': return val1 + val2;
      case '-': return val1 - val2;
      case '*': return val1 * val2;
      case '/':
        if (val2 == 0) {
          *ok = false;
          return 0;
        }
        return (sword_t)val1 / (sword_t)val2; // e.g. -1/2, may not pass the expr test
      default: assert(0);
    }
  }
}

word_t expr(char *e, bool *success) {
  if (!make_token(e)) {
    printf("error make token\n");
    *success = false;
    return 0;
  }

  /* TODO: Insert codes to evaluate the expression. */
  printf("expr is \n");
  for (int i = 0; i < nr_token; i++) {
    if (tokens[i].type == TK_NUM) {
      printf("%s ", tokens[i].str);
    } else {
      printf("%c ", tokens[i].type);
    }
  }
  printf("\n");
  return eval(0, nr_token - 1, success);
}

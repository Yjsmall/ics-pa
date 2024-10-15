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
#include "debug.h"
#include "macro.h"
#include <isa.h>

/* We use the POSIX regex functions to process regular expressions.
 * Type 'man regex' for more information about POSIX regex functions.
 */
#include <regex.h>
#include <setjmp.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

enum {
    TK_NOTYPE = 256,
    TK_EQ,
    TK_NUM,
    TK_HEX,
    TK_NEG,
    TK_REG,
    TK_DEREF,
    TK_AND,
};

static struct rule {
    const char *regex;
    int         token_type;
} rules[] = {

    /* TODO: Add more rules.
   * Pay attention to the precedence level of different rules.
   */

    {" +",                      TK_NOTYPE}, // spaces
    {"0x[0-9a-fA-F]+",          TK_HEX   }, // 十六进制数字
    {"[0-9]+",                  TK_NUM   }, // 十进制数字
    {"\\+",                     '+'      }, // plus
    {"\\-",                     '-'      }, // 减号或负号，后续处理
    {"\\*",                     '*'      }, // 乘号 or deference
    {"\\/",                     '/'      }, // 除号
    {"\\(",                     '('      }, // 左括号
    {"\\)",                     ')'      }, // 右括号
    {"==",                      TK_EQ    }, // equal
    {"\\$[a-zA-Z][0-9a-zA-Z]*", TK_REG   }, // Registers
    {"&&",                      TK_AND   }, // and
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

#define INITIAL_CAPACITY 32 // Initial capacity for the tokens array
static int    tokens_capacity                = 0;
static Token *tokens __attribute__((used))   = NULL;
static int    nr_token __attribute__((used)) = 0;

// 增加对负号的处理
static bool is_negative(int i) {
    // 如果在表达式的开头或者前一个 token 是操作符或者左括号，则认为是负号
    if (i == 0 || tokens[i - 1].type == '+' || tokens[i - 1].type == '-' ||
        tokens[i - 1].type == '*' || tokens[i - 1].type == '/' ||
        tokens[i - 1].type == '(') {
        return true;
    }
    return false;
}

static bool is_deference(int i) {
    if (i == 0 || tokens[i - 1].type == '+' || tokens[i - 1].type == '-' ||
        tokens[i - 1].type == '*' || tokens[i - 1].type == '/' ||
        tokens[i - 1].type == '(') {
        return true;
    }

    return false;
}

static void init_tokens() {
    if (tokens != NULL) {
        return;
    }
    tokens_capacity = INITIAL_CAPACITY;
    tokens          = malloc(sizeof(Token) * tokens_capacity);
    Assert(tokens != NULL, "malloc failed");
}

static bool expand_malloc(word_t cur_size) {
    tokens_capacity *= 2; // Double the capacity
    Token *new_tokens = realloc(tokens, sizeof(Token) * tokens_capacity);
    if (new_tokens == NULL) {
        // Handle memory allocation failure
        free(tokens);
        return false;
    }
    tokens = new_tokens;
    return true;
}

static bool make_token(char *e) {
    int        position = 0;
    int        i;
    regmatch_t pmatch;

    nr_token = 0;
    init_tokens();

    while (e[position] != '\0') {
        /* Try all rules one by one. */
        for (i = 0; i < NR_REGEX; i++) {
            if (regexec(&re[i], e + position, 1, &pmatch, 0) == 0 && pmatch.rm_so == 0) {
                char *substr_start = e + position;
                int   substr_len   = pmatch.rm_eo;

                Log("match rules[%d] = \"%s\" at position %d with len %d: %.*s",
                    i, rules[i].regex, position, substr_len, substr_len, substr_start);

                position += substr_len;

                if (rules[i].token_type == TK_NOTYPE) {
                    break;
                }

                if (nr_token == tokens_capacity) {
                    if (!expand_malloc(tokens_capacity)) {
                        return false;
                    }
                }

                if (rules[i].token_type == '-') {
                    if (is_negative(nr_token)) {
                        tokens[nr_token].type = TK_NEG;
                    } else {
                        tokens[nr_token].type = '-';
                    }
                } 

                if (rules[i].token_type == '*') {
                    if (is_deference(nr_token)) {
                        tokens[nr_token].type = TK_DEREF;
                    } else {
                        tokens[nr_token].type = '*';
                    }
                } 

                tokens[nr_token].type = rules[i].token_type;

                if (rules[i].token_type == TK_NUM || rules[i].token_type == TK_HEX || rules[i].token_type == TK_REG) {
                    strncpy(tokens[nr_token].str, substr_start, substr_len);
                    tokens[nr_token].str[substr_len] = '\0';
                }

                nr_token++;
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
    int ret = -1, par = 0, min_priority = 5; // 优先级最低为0，最高为4

    for (int i = p; i <= q; i++) {
        if (tokens[i].type == TK_NUM || tokens[i].type == TK_HEX || tokens[i].type == TK_REG) {
            continue;
        }

        // 处理括号
        if (tokens[i].type == '(') {
            par++;
        } else if (tokens[i].type == ')') {
            par--;
        } else if (par == 0) {
            int priority = 5; // 初始化为最高优先级

            switch (tokens[i].type) {
                case TK_AND: priority = 0; break; // lowest precedence
                case TK_EQ: priority = 1; break;  // equality check
                case '+':
                case '-': priority = 2; break; // addition and subtraction
                case '*':
                case '/': priority = 3; break; // multiplication and division
                case TK_NEG:
                case TK_DEREF: priority = 4; break; // negation and dereference (highest precedence)
                default: assert(0);
            }

            // 找到优先级最低的操作符
            if (priority <= min_priority) {
                min_priority = priority;
                ret          = i;
            }
        }
    }

    if (par != 0) {
        return -1; // 括号不匹配
    }

    return ret;
}

sword_t eval(int p, int q, bool *ok) {
    *ok = true;
    if (p > q) {
        *ok = false;
        return 0;
    } else if (p == q) {
        if (tokens[p].type == TK_NUM) {
            return strtol(tokens[p].str, NULL, 10);
        } else if (tokens[p].type == TK_HEX) {
            return strtol(tokens[p].str, NULL, 16); 
        } else if (tokens[p].type == TK_REG) {
            // 查找寄存器的值
            return isa_reg_str2val(tokens[p].str, ok);
        }
        *ok = false;
        return 0;
    } else if (check_parentheses(p, q)) {
        return eval(p + 1, q - 1, ok);
    } else {
        int major = find_major(p, q);
        if (major < 0) {
            *ok = false;
            return 0;
        }

        word_t val1 = eval(p, major - 1, ok);
        if (!*ok) {
            return 0;
        }
        word_t val2 = eval(major + 1, q, ok);
        if (!*ok) {
            return 0;
        }

        switch (tokens[major].type) {
            case '+': return val1 + val2;
            case '-': return val1 - val2;
            case '*': return val1 * val2;
            case '/':
                if (val2 == 0) {
                    printf("dive by zero !!!\n");
                    *ok = false;
                    return 0;
                }
                return (sword_t)val1 / (sword_t)val2; // e.g. -1/2, may not pass the expr test
            case TK_EQ: return val1 == val2;
            case TK_AND: return val1 && val2;
            case TK_NEG: return -val2;
            default: assert(0);
        }
    }
}

sword_t expr(char *e, bool *success) {
    if (!make_token(e)) {
        printf("error make token\n");
        *success = false;
        return 0;
    }

    return eval(0, nr_token - 1, success);
}

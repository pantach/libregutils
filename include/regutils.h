/* Copyright 2022 Panayotis Tachtalis
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef REGUTILS_H
#define REGUTILS_H

#include <regex.h>

// Error codes shall start from -100 in order to not collide with backend
// ones (as defined in regex.h)
#define PREG_ERRCODE_START -100

typedef enum Preg_errcode {
	PREG_NOACTION = PREG_ERRCODE_START, // No action is performed
	PREG_MEMFAIL,                       // Memory allocation failure
	PREG_BADMIN,                        // Min should be zero or positive
	PREG_BADLIMIT,                      // Limit should be greater than -2
	PREG_BADBREF,                       // Invalid backreference number
	PREG_ERRCODE_END                    // Shall always be last
} Preg_errcode;

typedef enum Preg_opt {
	PREG_CFLAGS = 0,
	PREG_UFLAGS,
	PREG_MIN,
	PREG_LIMIT
} Preg_opt;

typedef enum Preg_uflags {
	PREG_NOSTRINGS = 1,
} Preg_uflags;

typedef enum Preg_notation {
	PREG_ERE = 0,
	PREG_BRE
} Preg_notation;

typedef struct Preg Preg;

/* Common functions */

Preg* preg_init(void);
void  preg_free(Preg* rm);

void preg_setopt(Preg* rm, Preg_opt opt, int value);
void preg_delopt(Preg* rm, Preg_opt opt, int value);

size_t preg_matc(const Preg* rm);
size_t preg_subc(const Preg* rm);

size_t preg_matchlen(const Preg* rm, int nmatch, int nsub);

regoff_t preg_so(const Preg* rm, int nmatch, int nsub);
regoff_t preg_eo(const Preg* rm, int nmatch, int nsub);

const char* preg_errmsg(const Preg* rm);
int preg_errcode(const Preg* rm);

/* Match functions */

int preg_match(Preg* rm, const char* subject, const char* pattern);
const char* preg_getmatch(const Preg* rm, int nmatch, int nsub);

/* Replace functions */

int preg_replace(Preg* rm, const char* subject, const char* pattern,
                 const char *rep);
size_t preg_replen(const Preg* rm);
const char* preg_getrep(const Preg* rm);

/* Split function */

int preg_split(Preg* rm, const char* subject, const char* pattern);
int preg_splitc(const Preg* rm);
size_t preg_splitlen(const Preg* rm, int nmatch);
const char* preg_getsplit(const Preg* rm, int nmatch);

/* Miscellaneous */

char* preg_escape(const char* str, Preg_notation nota, size_t len);

#endif

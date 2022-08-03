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

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "regutils.h"
#include "vector.h"

#define ERRCODE_POS(x) x -PREG_ERRCODE_START
#define MEM_GROWTH_FACTOR 2

/* The max number with MAX_BREF_DIGITS shall not be greater than INT_MAX, as it
 * is used with atoi(). It shall also not be greater than the number of
 * subexpressions regcomp() supports. */
#define MAX_BREF_DIGITS 1

typedef enum {
	PREG_MATCH = 0,
	PREG_REPLACE,
	PREG_SPLIT
} Preg_mode;

typedef enum {
	PREG_INTERNAL_ERR = 0,
	PREG_EXTERNAL_ERR,
	PREG_INTERDTL_ERR
} Preg_err_end;

typedef struct Preg_err Preg_err;

struct Preg_err {
	Preg_err_end end;
	Preg_errcode errcode;
	char*        errmsg;
} internal_errors[] = {
	{ PREG_INTERNAL_ERR, PREG_NOACTION, "No action is performed" },
	{ PREG_INTERNAL_ERR, PREG_MEMFAIL,  "Failed to allocate memory" },
	{ PREG_INTERNAL_ERR, PREG_BADMIN,   "Min should be zero or positive" },
	{ PREG_INTERNAL_ERR, PREG_BADLIMIT, "Limit should be greater than -2" },
	{ PREG_INTERDTL_ERR, PREG_BADBREF,  "Invalid backreference number" }
};

typedef struct {
	char** sub;
} Preg_sub;

typedef struct {
	Preg_sub* match;
} Preg_match;

typedef struct {
	char*  str;
	size_t len;
} String;

typedef struct {
	String* split;
	size_t size;
} Preg_split;

typedef struct {
	size_t so;              // Backreference's start offset
	int no;                 // Backreference's number
} Bref;

VECTOR_DEF_HEAD(pvoid_vec, void*)
VECTOR_DEF_SRC (pvoid_vec, void*)
VECTOR_DEF_HEAD(bref_vec, Bref)
VECTOR_DEF_SRC (bref_vec, Bref)

struct Preg {
	regex_t comp;           // The compiled regex pattern
	int compd;              // Becomes 1 when comp gets compiled successfully
	regmatch_t** offset;    // Matrix that holds the matched offsets
	size_t offset_size;     // offset's size
	size_t matc;            // Match count
	size_t subc;            // Number of subexpressions in the regex pattern
	int uflags;             // libregutils' flags
	int cflags;             // Regcomp's flags
	int min;                // The number of the minimum match to be returned
	int limit;              // The max number of matches to be returned
	pvoid_vec* mpools;      // Vector of allocated memory pools
	Preg_err err;           // Error
	Preg_mode mode;         // The regex mode
	union {
		Preg_match matches; // Array of regex matches
		String rep;         // Replaced string
		Preg_split splits;  // Split string
	};
};

static void* mem_init(Preg* rm, size_t size);
static void* mem_alloc(void** mem, size_t size);

static int preg_offset(Preg* rm, const char* subject, const char* pattern);
static int preg_offset_alloc(Preg* array);

static int parse_rep(const char* rep, String* nrep, bref_vec* brvec);
static String
assemble(Preg* rm, const char* subject, String* rep, bref_vec* bref);
static int
copy_rep(Preg* rm, int nmatch, String* rep, bref_vec* bref, void* mem);

static void preg_set_mode(Preg* rm, Preg_mode mode);
static int  preg_checkopt(Preg* rm);

static Preg_err_end preg_err_end(int err);
static int  preg_set_error(Preg* rm, int err, ...);
static void preg_set_internal_error(Preg* rm, int err);
static int  preg_set_external_error(Preg* rm, int err);
static int  preg_set_interdtl_error(Preg* rm, int err, va_list args);

static void* mem_init(Preg* rm, size_t size)
{
	void* mem;
	int err;

	mem = malloc(size);
	if (!mem)
		return NULL;

	err = pvoid_vec_append(rm->mpools, mem);
	if (err) {
		free(mem);
		return NULL;
	}

	return mem;
}

static void* mem_alloc(void** mem, size_t size)
{
	void* temp = *mem;
	*mem += size;
	return temp;
}

inline size_t preg_matc(const Preg* rm)
{
	return rm->matc;
}

inline size_t preg_subc(const Preg* rm)
{
	return rm->subc;
}

inline regoff_t preg_so(const Preg* rm, int nmatch, int nsub)
{
	return rm->offset[nmatch][nsub].rm_so;
}

inline regoff_t preg_eo(const Preg* rm, int nmatch, int nsub)
{
	return rm->offset[nmatch][nsub].rm_eo;
}

inline const char* preg_getmatch(const Preg* rm, int nmatch, int nsub)
{
	return rm->matches.match[nmatch].sub[nsub];
}

inline size_t preg_matchlen(const Preg* rm, int nmatch, int nsub)
{
	return rm->offset[nmatch][nsub].rm_eo - rm->offset[nmatch][nsub].rm_so;
}

inline const char* preg_getrep(const Preg* rm)
{
	return rm->rep.str;
}

inline size_t preg_replen(const Preg* rm)
{
	return rm->rep.len;
}

inline int preg_splitc(const Preg* rm)
{
	return rm->splits.size;
}

inline const char* preg_getsplit(const Preg* rm, int nmatch)
{
	return rm->splits.split[nmatch].str;
}

inline size_t preg_splitlen(const Preg* rm, int nmatch)
{
	return rm->splits.split[nmatch].len;
}

inline const char* preg_errmsg(const Preg* rm)
{
	return rm->err.errmsg;
}

inline int preg_errcode(const Preg* rm)
{
	return rm->err.errcode;
}

Preg* preg_init(void)
{
	Preg* rm;

	rm = calloc(1, sizeof(Preg));
	if (rm) {
		// NULL may not be represented as zeroed memory
		rm->offset = NULL;
		rm->cflags = REG_EXTENDED;
		rm->limit  = -1;
		rm->err	   = internal_errors[ERRCODE_POS(PREG_NOACTION)];
		rm->mode   = -1;
		rm->mpools = pvoid_vec_init();
	}

	return rm;
}

void preg_free(Preg* rm)
{
	if (rm) {
		if (rm->compd)
			regfree(&rm->comp);

		pvoid_vec_free(rm->mpools, free);
		free(rm->offset);
		free(rm);
	}
}

void preg_set_mode(Preg* rm, Preg_mode mode)
{
	rm->mode = mode;
	switch (rm->mode) {
	case PREG_MATCH:
		rm->matches.match = NULL;
		break;
	case PREG_REPLACE:
		rm->rep.str = NULL;
		rm->rep.len = 0;
		break;
	case PREG_SPLIT:
		rm->splits.split = NULL;
		rm->splits.size = 0;
	}
}

void preg_setopt(Preg* rm, Preg_opt opt, int value)
{
	switch (opt) {
	case PREG_CFLAGS:
		rm->cflags |= value;
		break;
	case PREG_UFLAGS:
		rm->uflags |= value;
		break;
	case PREG_MIN:
		rm->min = value;
		break;
	case PREG_LIMIT:
		rm->limit = value;
	}
}

void preg_delopt(Preg* rm, Preg_opt opt, int value)
{
	switch (opt) {
	case PREG_CFLAGS:
		rm->cflags &= ~value;
	case PREG_UFLAGS:
		rm->uflags &= ~value;
	default:
		break;
	}
}

int preg_checkopt(Preg* rm)
{
	if (rm->min < 0)
		return PREG_BADMIN;
	else if (rm->limit < -1)
		return PREG_BADLIMIT;
	else
		return 0;
}

Preg_err_end preg_err_end(int err)
{
	if (err >= PREG_ERRCODE_START && err < PREG_ERRCODE_END)
		return internal_errors[ERRCODE_POS(err)].end;
	else
		return PREG_EXTERNAL_ERR;
}

/* In case of PREG_INTERDTL_ERR this function should receive an additional
 * argument: a string describing the error details */
int preg_set_error(Preg* rm, int err, ...)
{
	Preg_err_end errend;

	errend = preg_err_end(err);
	switch (errend) {
		case PREG_INTERNAL_ERR:
			preg_set_internal_error(rm, err);
			break;
		case PREG_EXTERNAL_ERR:
			err = preg_set_external_error(rm, err);
			break;
		case PREG_INTERDTL_ERR:;
			va_list args;
			va_start(args, err);
			err = preg_set_interdtl_error(rm, err, args);
			va_end(args);
	}
	return err;
}

void preg_set_internal_error(Preg* rm, int err)
{
	rm->err = internal_errors[ERRCODE_POS(err)];
}

int preg_set_external_error(Preg* rm, int err)
{
	size_t len;

	len = regerror(err, &rm->comp, NULL, 0);
	rm->err.errmsg = mem_init(rm, len +1);
	if (!rm->err.errmsg) {
		preg_set_internal_error(rm, PREG_MEMFAIL);
		return PREG_MEMFAIL;
	}
	regerror(err, &rm->comp, rm->err.errmsg, len);
	rm->err.errcode = err;
	rm->err.end = PREG_EXTERNAL_ERR;

	return err;
}

int preg_set_interdtl_error(Preg* rm, int err, va_list args)
{
	char* errdet = va_arg(args, char*);  // Error details
	const char* errmsg = preg_errmsg(rm);
	size_t errmsg_len;
	size_t errdet_len;

	rm->err = internal_errors[ERRCODE_POS(err)];

	errmsg_len = strlen(errmsg);
	errdet_len = strlen(errdet);

	rm->err.errmsg = mem_init(rm, errmsg_len +errdet_len +3);
	if (!rm->err.errmsg) {
		preg_set_internal_error(rm, PREG_MEMFAIL);
		return PREG_MEMFAIL;
	}
	memcpy(rm->err.errmsg, errmsg, errmsg_len);
	memcpy(&rm->err.errmsg[errmsg_len], ": ", 2);
	memcpy(&rm->err.errmsg[errmsg_len +2], errdet, errdet_len +1);

	return err;
}

/* If len == -1 preg_escape expects a null terminated string. Else the first len
 * bytes will be escaped */
char* preg_escape(const char* str, Preg_notation notation, size_t len)
{
	unsigned int i;
	const char* bre_specials = "^$.[*\\";
	const char* ere_specials = "^$.[()|*+?{\\";
	const char* special_char;
	char* res;
	size_t index = 0;
	size_t nlen = 0;

	switch (notation) {
		case PREG_BRE:
			special_char = bre_specials;
			break;
		case PREG_ERE:
			special_char = ere_specials;
	}

	if (len == -1)
		len = strlen(str);

	for (i = 0; i < len; ++i) {
		if (strchr(special_char, str[i]))
			++nlen;
		++nlen;
	}

	res = malloc(nlen +1);
	if (!res)
		return NULL;

	for (i = 0; i < len; ++i) {
		if (strchr(special_char, str[i]))
			res[index++] = '\\';
		res[index++] = str[i];
	}
	res[index] = '\0';

	return res;
}

/* Performs a regex match on a given string and stores the results in the
 * "offset" array inside "rm".
 *
 * Parameters:
 * Preg* rm:			An initialized Preg structure
 * const char* subject: The string on which the regex search is performed
 * const char* pattern:	The regex pattern
 *
 * On success it returns 0. Else it returns an error code.
 */
int preg_offset(Preg* rm, const char* subject, const char* pattern)
{
	regmatch_t* match = NULL;
	size_t subject_ro = 0;      // Running offset
	int eflags = 0;
	int err = 0;
	int i, j;

	// Remove REG_NOSUB
	if (REG_NOSUB&rm->cflags)
		rm->cflags &= ~REG_NOSUB;

	err = preg_checkopt(rm);
	if (err)
		goto done;

	err = regcomp(&rm->comp, pattern, rm->cflags);
	if (err)
		goto done;

	rm->compd = 1;

	rm->subc = rm->comp.re_nsub;
	match = malloc((rm->subc +1) * sizeof(*match));
	if (!match) {
		err = PREG_MEMFAIL;
		goto done;
	}

	// Find and discard matches until reaching the minimum accepted match
	for (i = 0; i < rm->min && !(err = regexec(&rm->comp, &subject[subject_ro],
	            rm->subc +1, match, eflags)); ++i) {

		subject_ro += match->rm_eo;

		if (i == 0)
			eflags |= REG_NOTBOL;
	}

	for (i = 0; !(err = regexec(&rm->comp, &subject[subject_ro], rm->subc +1,
	            match, eflags)) && i < (unsigned)rm->limit; ++i) {

		if (i == rm->offset_size) {
			err = preg_offset_alloc(rm);
			if (err)
				goto done;
		}

		for (j = 0; j <= rm->subc; j++) {
			// Regexec returns -1 for subexpressions not matched
			if (match[j].rm_so != -1) {
				rm->offset[i][j].rm_so = match[j].rm_so +subject_ro;
				rm->offset[i][j].rm_eo = match[j].rm_eo +subject_ro;
			}
			else {
				rm->offset[i][j].rm_so = -1;
				rm->offset[i][j].rm_eo = -1;
			}
		}

		subject_ro += match->rm_eo;
		rm->matc++;

		if (i == 0)
			eflags |= REG_NOTBOL;

		/* Sometimes the empty pattern may successfully match zero characters.
		 * But there is nothing more to be done so we break */
		if (*pattern == '\0')
			break;
	}
	if (err == REG_NOMATCH && i > 0)
		err = 0;

done:
	free(match);

	return err;
}

int preg_offset_alloc(Preg* rm)
{
	regmatch_t** offset;
	void* mem;
	size_t old_size;
	size_t new_size;
	size_t offs_elem_size = (rm->subc +1) * sizeof(regmatch_t);
	int i;

	old_size = rm->offset_size;
	new_size = old_size ? old_size * MEM_GROWTH_FACTOR : 1;

    offset = realloc(rm->offset, new_size * sizeof(regmatch_t*));
    if (!offset)
	    return PREG_MEMFAIL;

    mem = mem_init(rm, (new_size -old_size) * offs_elem_size);
    if (!mem)
	    return PREG_MEMFAIL;

	for (i = old_size; i < new_size; ++i)
		offset[i] = mem_alloc(&mem, offs_elem_size);

	rm->offset = offset;
	rm->offset_size = new_size;

    return 0;
}

/* Performs a regex match on a given string and stores the resulting strings.
 *
 * Parameters:
 * Preg* rm:			A Preg structure where the results will be saved
 * const char* subject: The string on which the regex search is performed
 * const char* pattern:	The regex pattern
 *
 * On success it returns 0. Else it returns an error code.
 */
int preg_match(Preg* rm, const char* subject, const char* pattern)
{
	Preg_sub* match = NULL;
	size_t memsize = 0;
	size_t match_size;
	size_t sub_size;
	void* mem;
	int err;
	int i, j;

	preg_set_mode(rm, PREG_MATCH);

	if ((err = preg_offset(rm, subject, pattern)))
		goto end;

	// Does the user want the matched strings?
	if (rm->uflags & PREG_NOSTRINGS)
		goto end;

	// Calculate the size of the relevant structures
	match_size = preg_matc(rm) * sizeof(Preg_sub);
	sub_size  = (preg_subc(rm) +1) * sizeof(char*);

	memsize += match_size;
	memsize += preg_matc(rm) * sub_size;

	// Calculate the total length of the matched strings
	for (i = 0; i < preg_matc(rm); ++i)
		for (j = 0; j <= preg_subc(rm); ++j)
			memsize += preg_matchlen(rm, i, j) +1;

	// One-time allocation
	if ((mem = mem_init(rm, memsize)) == NULL) {
		err = PREG_MEMFAIL;
		goto end;
	}

	match = mem_alloc(&mem, match_size);

	// Copy the matched strings to the appropriate structures
	for (i = 0; i < preg_matc(rm); ++i) {
		match[i].sub = mem_alloc(&mem, sub_size);

		for (j = 0; j <= preg_subc(rm); ++j) {
			size_t len = preg_matchlen(rm, i, j);

			match[i].sub[j] = mem_alloc(&mem, len +1);

			memcpy(match[i].sub[j], &subject[preg_so(rm, i, j)], len);
			match[i].sub[j][len] = '\0';
		}
	}

end:
	rm->matches.match = match;
	err = preg_set_error(rm, err);

	return err;
}

int preg_split(Preg* rm, const char* subject, const char* pattern)
{
	void* mem;
	String* split = NULL;
	size_t split_size = 0;
	size_t prev_eo;
	size_t len;
	size_t len_total = 0;
	int err;
	int i;

	preg_set_mode(rm, PREG_SPLIT);

	if ((err = preg_offset(rm, subject, pattern)))
		goto end;

	// Allocate the max size needed for all the split string segments
	split = mem_init(rm, (preg_matc(rm) +1) * sizeof(String));
	if (!split) {
		err = PREG_MEMFAIL;
		goto end;
	}

	// Calculate the total size needed for all the split segments. Store their
	// lengths and start pointers
	prev_eo = 0;
	for (i = 0; i <= preg_matc(rm); ++i) {
		if (i == preg_matc(rm))
			len = strlen(&subject[prev_eo]);
		else
			len = preg_so(rm, i, 0) -prev_eo;

		if (len) {
			split[split_size].str = (char*)&subject[prev_eo];
			split[split_size].len = len;

			len_total += len +1;

			split_size++;
		}

		if (i != preg_matc(rm))
			prev_eo = preg_eo(rm, i, 0);
	}

	// One time allocation
	if ((mem = mem_init(rm, len_total)) == NULL) {
		err = PREG_MEMFAIL;
		goto end;
	}

	// Iterate a second time over the results and store the resulting strings
	for (i = 0; i < split_size; ++i) {
		void* temp = split[i].str;

		split[i].str = mem_alloc(&mem, split[i].len +1);
		memcpy(split[i].str, temp, split[i].len);
		split[i].str[split[i].len] = '\0';
	}

end:
	rm->splits.split = split;
	rm->splits.size = split_size;

	err = preg_set_error(rm, err);
	return err;
}

int preg_replace(Preg* rm, const char* subject, const char* pattern,
				 const char* rep)
{
	String res;
	String nrep;
	bref_vec bref;
	char errdtls[MAX_BREF_DIGITS +1] = "";
	int err = 0;
	int i;

	bref = bref_vec_init_auto();
	nrep.str = malloc(strlen(rep) +1);
	if (!nrep.str) {
		err = PREG_MEMFAIL;
		goto end;
	}

	if ((err = parse_rep(rep, &nrep, &bref)))
		goto end;

	// If the replacement string includes backreferences
	if (bref.n > 0) {

		// PREG_NOSTRINGS will cause conflicts here
		if (rm->uflags & PREG_NOSTRINGS)
			rm->uflags &= ~PREG_NOSTRINGS;

		// We need the matched strings for applying them to the backreferences
		// in the replacement string
		if ((err = preg_match(rm, subject, pattern)))
			goto end;

		// Check for invalid backreference numbers
		for (i = 0; i < bref.n; i++) {
			if (bref.entry[i].no > preg_subc(rm)) {
				snprintf(errdtls, MAX_BREF_DIGITS +1, "%d", bref.entry[i].no);
				err = PREG_BADBREF;
				goto end;
			}
		}
	}
	else
		if ((err = preg_offset(rm, subject, pattern)))
			goto end;

	res = assemble(rm, subject, &nrep, &bref);
	if (res.str == NULL) {
		err = PREG_MEMFAIL;
		goto end;
	}

	preg_set_mode(rm, PREG_REPLACE);

	rm->rep = res;

end:
	free(nrep.str);
	bref_vec_free_auto(&bref, NULL);

	err = preg_set_error(rm, err, errdtls);

	return err;
}

/* Parses the replacement string "rep", searching for backreferences. The
 * parsed string "nrep", has all "$n" placeholders stripped and the escape
 * sequences applied. "nrep" is expected to point to a memory of at least the
 * length of "rep" plus one. The backreferences found are stored in "brvec".
 * */
static int parse_rep(const char* rep, String* nrep, bref_vec* brvec)
{
	char bref_num[MAX_BREF_DIGITS +1];
	Bref bref;
	int dollars = 0;
	int i = 0;
	int j = 0;
	int k;

	while (rep[i]) {

		if (dollars)
			dollars = 0;

		while (rep[i] == '$') {
			dollars++;
			i++;
		}

		if (dollars) {
			// Dollars followed by a sequence of digits may indicate a
			// backreference
			if (rep[i] >= '0' && rep[i] <= '9') {

				// Remove dollar escapes
				for (k = dollars/2; k > 0; k--)
					nrep->str[j++] = '$';

				// A string with an odd number of dollars includes a
				// backreference
				if (dollars % 2 == 1) {
					for (k = 0; rep[i] >= '0' && rep[i] <= '9' &&
						 k < MAX_BREF_DIGITS; k++)
						bref_num[k] = rep[i++];
					bref_num[k] = '\0';

					bref.so = j;
					bref.no = atoi(bref_num);

					if (bref_vec_append(brvec, bref) == -1)
						return PREG_MEMFAIL;

					continue;
				}
			}
			// Dollars not followed by a sequence of digits are treated
			// literally
			else {
				while (dollars--)
					nrep->str[j++] = '$';

				if (rep[i] == '\0')
					break;
			}
		}

		nrep->str[j++] = rep[i++];
	}
	nrep->str[j] = '\0';
	nrep->len = j;

	return 0;
}

static String
assemble(Preg* rm, const char* subject, String* rep, bref_vec* bref)
{
	String res;
	void*  mem;
	size_t len;
	size_t len_total = 0;
	size_t ro = 0;
	size_t sublen;
	int i, j;

	sublen = strlen(subject);

	len_total += sublen;
	len_total += preg_matc(rm) * rep->len;

	for (i = 0; i < preg_matc(rm); i++) {
		// Add the lengths of all the backreferences
		for (j = 0; j < bref->n; j++)
			len_total += preg_matchlen(rm, i, bref->entry[j].no);

		// Remove the length of the strings that will be replaced
		len_total -= preg_matchlen(rm, i, 0);
	}

	res.str = mem = mem_init(rm, len_total +1);
	if (!mem) {
		res.len = -1;
		return res;
	}
	res.len = len_total;

	for (i = 0; i < preg_matc(rm); i++) {
		memcpy(mem, &subject[ro], preg_so(rm, i, 0) -ro);
		mem += preg_so(rm, i, 0) -ro;
		ro   = preg_eo(rm, i, 0);

		len = copy_rep(rm, i, rep, bref, mem);
		mem += len;
	}
	memcpy(mem, &subject[ro], sublen -ro +1);

	return res;
}

/* Copy the replacement string to "mem" and after applying any specified
 * backreferences to it
 *
 * Return value:
 * The length of the constructed replacement string
 * */
static int
copy_rep(Preg* rm, int nmatch, String* rep, bref_vec* bref, void* mem)
{
	const void* const mem_start = mem;
	size_t ro = 0; // "rep's" reading offset
	size_t len;
	int i;

	// If backreferences were specified, copy the replacement string along with
	// the corresponding parenthesized subexpressions
	if (bref->n) {
		for (i = 0; i < bref->n; i++) {
			memcpy(mem, &rep->str[ro], bref->entry[i].so -ro);
			mem += bref->entry[i].so -ro;
			ro   = bref->entry[i].so;

			len = preg_matchlen(rm, nmatch, bref->entry[i].no);
			memcpy(mem, preg_getmatch(rm, nmatch, bref->entry[i].no), len);
			mem += len;
		}
		memcpy(mem, &rep->str[ro], rep->len -ro);
		mem += rep->len -ro;

		return mem -mem_start;

	}
	// Else copy the replacement string alone
	else {
		memcpy(mem, rep->str, rep->len);

		return rep->len;
	}
}

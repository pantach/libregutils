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

#include <stdio.h>
#include <stdlib.h>
#include <regutils.h>

static void match_demo(const char* subject);
static void replace_demo(const char* subject);
static void split_demo(const char* subject);
static void escape_demo(void);

int main(void)
{
	const char* subject = "There's a _ inside the crate";

	printf("This is a demo of libregutils. Our subject string is: %s\n",
	       subject);

	printf("\n--------------------------------------\n\n"
	       "Perfoming a regex match:\n\n");
	match_demo(subject);
	printf("\n--------------------------------------\n\n"
	       "Perfoming a regex replacement:\n\n");
	replace_demo(subject);
	printf("\n--------------------------------------\n\n"
	       "Perfoming a regex split:\n\n");
	split_demo(subject);
	printf("\n--------------------------------------\n\n"
	       "Perfoming a regex escape:\n\n");
	escape_demo();

	return 0;
}

void match_demo(const char* subject)
{
	Preg* rm;
	int err;
	int i, j;

	/* Before doing anything we need to initialize our handle */
	rm = preg_init();
	if (!rm) {
		printf("Memory allocation failure\n");
		exit(EXIT_FAILURE);
	}

	/* With preg_setopt you can set various options before performing a regex
	 * match. Supported options are PREG_CFLAGS, PREG_MIN and PREG_LIMIT.  */

	/* PREG_CFLAGS are options that will alter the way the regex match is
	 * perfomed. At least the following values are supported:
	 * REG_EXTENDED (activated by default), REG_ICASE and REG_NEWLINE. Whether
	 * extra options are supported depends on you libregex implementation (type
	 * "man regex" for more information). Please note that REG_NOSUB is ignored.
	 * You can specify multiple option values by ORing them together e.g.
	 * REG_ICASE|REG_NEWLINE */
	preg_setopt(rm, PREG_CFLAGS, REG_ICASE);

	/* With PREG_MIN you can specify the minimum match to be returned and
	 * with PREG_LIMIT the maximum number of matches to be returned. With the
	 * following options I instruct regutils to ignore the first match and
	 * return a maximum of ten matches */
	//preg_setopt(rm, PREG_MIN, 2);
	//preg_setopt(rm, PREG_LIMIT, 10);

	/* With preg_delopt you can reset options. For now, only the PREG_CFLAGS
	 * option is supported. For instance if you would like a basic regular
	 * expression (BRE) match instead of the default ERE, you would call the
	 * following: */
	//preg_delopt(rm, PREG_CFLAGS, REG_EXTENDED);

	/* Now that everything is set we can perform the regex match */
	err = preg_match(rm, subject, "c([[:alpha:]]+)e");

	/* You may want to handle a "no match" error differently from other types
	 * of errors */
	if (err == REG_NOMATCH)
		printf("No matches? No problem!\n");

	/* In case of an error, preg_errmsg will return a human readable error
	 * message */
	else if (err) {
		printf("An unexpected error occured: %s\n", preg_errmsg(rm));
		preg_free(rm);
		exit(EXIT_FAILURE);
	}

	/* Since no errors occured we can iterate over out matches and print them.
	 * preg_matc returns the number of successful matches, while preg_subc
	 * returns the number of submatches, which is the same as the number of
	 * parenthesized subexpressions in our regex pattern. Submatches may or may
	 * not have been matched successfully. In case of no match an empty string
	 * is returned */
	else {
		for (i = 0; i < preg_matc(rm); i++)
			// IMPORTANT: Always use '<=' if you are iterating over submatches!
			for (j = 0; j <= preg_subc(rm); j++)
				printf("The term \"%s\" was found with a starting offset of %u,"
				       " an ending offset of %u and a length of %zu\n",
				       preg_getmatch(rm, i, j),
				       preg_so(rm, i, j), preg_eo(rm, i, j),
				       preg_matchlen(rm, i, j));
	}

	/* When finished you shall free the memory */
	preg_free(rm);
}

void replace_demo(const char* subject)
{
	Preg* rm;
	int err;

	rm = preg_init();
	if (!rm) {
		printf("Memory allocation failure\n");
		exit(EXIT_FAILURE);
	}

	/* preg_replace allows you to use backreferences in the replacement string.
	 * Backreferences are denoted by $[0-9], where $0 refers to the whole
	 * matched string, $1 the first parenthesized subexpression and so on. A
	 * literal "$1" can be specified by escaping the backreference with an extra
	 * "$" ("$$1"). Only numbers up to 9 are supported for now */
	err = preg_replace(rm, subject, "_ inside the c([[:alpha:]]+)e",
	                   "$1 inside the crate");
	if (err) {
		printf("An error occured: %s\n", preg_errmsg(rm));
		preg_free(rm);
		exit(EXIT_FAILURE);
	}

	printf("The replaced string is: %s\n", preg_getrep(rm));

	preg_free(rm);
}

void split_demo(const char* subject)
{
	Preg* rm;
	int err;
	int i;

	rm = preg_init();
	if (!rm) {
		printf("Memory allocation failure\n");
		exit(EXIT_FAILURE);
	}

	/* This is pretty straight-forward if you understood the previous examples
	 * */
	err = preg_split(rm, subject, "[_ ]");
	if (err) {
		printf("An error occured: %s\n", preg_errmsg(rm));
		preg_free(rm);
		exit(EXIT_FAILURE);
	}

	printf("The split strings are:\n");

	for (i = 0; i < preg_splitc(rm); i++)
		printf("%s\n", preg_getsplit(rm, i));

	preg_free(rm);
}

void escape_demo()
{
	char* escstr;

	/* Sometimes you may want to search for a string that contains symbols but
	 * you don't want these symbols to be interpeted as special regex
	 * characters. For example searching for "Mr. Smith" in a text, may return
	 * "Mrs Smith" results, as the dot is a special regex character that matches
	 * anything. To avoid this scenario you need to escape your regex pattern.
	 * This is exactly what preg_escape does. */

	/* With the following call, I tell preg_escape to escape the whole string
	 * using the notation of extended regular expressions. "-1" stands for
	 * automatic string length calculation and it's only applicable for
	 * null-terminated strings */
	char* str = "Mr. Smith";
	escstr = preg_escape(str, PREG_ERE, -1);
	printf("Escaping \"%s\" using ERE rules. Result: %s\n", str, escstr);
	free(escstr);

	/* It can also be used with strings that are not null-terminated. In that
	 * case the lenght of the string should be passed as the third argument */
	char str2[] = {'^', '.', '*'};
	escstr = preg_escape(str2, PREG_BRE, 3);
	printf("Escaping \"%s\" using BRE rules. Result: %s\n", str2, escstr);
	free(escstr);
}

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

#ifndef VECTOR_H
#define VECTOR_H

#include <stdlib.h>

#define VECTOR_GROWTH_FACTOR 2

/* The following definition, which replaces a function macro with a similar
 * function macro is done to prevent token pasting of the macro arguments
 * without expanding them first */
#define VECTOR_DEF_HEAD(Vector_t, Entry_t) VECTOR_DEF_HEAD_(Vector_t, Entry_t)
#define VECTOR_DEF_HEAD_(Vector_t, Entry_t)                                    \
	typedef struct {                                                           \
		Entry_t* entry;                                                        \
		size_t n;                                                              \
		size_t size;                                                           \
	} Vector_t;                                                                \
                                                                               \
	Vector_t Vector_t##_init_auto(void);                                       \
	void Vector_t##_free_auto(Vector_t* v, void (*free_entry)(Entry_t));       \
	Vector_t* Vector_t##_init(void);                                           \
	void Vector_t##_free(Vector_t* v, void (*free_entry)(Entry_t));            \
	int Vector_t##_append(Vector_t* v, Entry_t entry);                         \

#define VECTOR_DEF_SRC(Vector_t, Entry_t) VECTOR_DEF_SRC_(Vector_t, Entry_t)
#define VECTOR_DEF_SRC_(Vector_t, Entry_t)                                     \
                                                                               \
static Entry_t* Vector_t##_resize(Vector_t* v, size_t new_size);               \
                                                                               \
Vector_t Vector_t##_init_auto(void)                                            \
{                                                                              \
	Vector_t v = { NULL, 0, 0 };                                               \
	return v;                                                                  \
}                                                                              \
                                                                               \
Vector_t* Vector_t##_init(void)                                                \
{                                                                              \
	Vector_t* v = malloc(sizeof(*v));                                          \
                                                                               \
	if (!v)                                                                    \
		return NULL;                                                           \
                                                                               \
	*v = Vector_t##_init_auto();                                               \
                                                                               \
	return v;                                                                  \
}                                                                              \
                                                                               \
void Vector_t##_free_auto(Vector_t* v, void (*free_entry)(Entry_t))            \
{                                                                              \
	int i;                                                                     \
                                                                               \
	if (v) {                                                                   \
		if (free_entry)                                                        \
			for (i = 0; i < v->n; ++i)                                         \
				free_entry(v->entry[i]);                                       \
                                                                               \
		free(v->entry);                                                        \
	}                                                                          \
}                                                                              \
                                                                               \
void Vector_t##_free(Vector_t* v, void (*free_entry)(Entry_t))                 \
{                                                                              \
	if (v) {                                                                   \
		Vector_t##_free_auto(v, free_entry);                                   \
		free(v);                                                               \
	}                                                                          \
}                                                                              \
                                                                               \
static Entry_t* Vector_t##_resize(Vector_t* v, size_t new_size)                \
{                                                                              \
	void* tmp = realloc(v->entry, new_size * sizeof(Entry_t));                 \
	if (!tmp)                                                                  \
		return NULL;                                                           \
                                                                               \
	v->entry = tmp;                                                            \
	v->size = new_size;                                                        \
                                                                               \
	return v->entry;                                                           \
}                                                                              \
                                                                               \
int Vector_t##_append(Vector_t* v, Entry_t entry)                              \
{                                                                              \
	if (v->n == v->size)                                                       \
		if (!Vector_t##_resize(v, v->n * VECTOR_GROWTH_FACTOR +1))             \
			return -1;                                                         \
                                                                               \
	v->entry[v->n] = entry;                                                    \
	v->n++;                                                                    \
                                                                               \
	return 0;                                                                  \
}

#endif

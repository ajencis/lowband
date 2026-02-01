/**
 * \file z-virt.c
 * \brief Memory management routines
 *
 * Copyright (c) 1997 Ben Harrison.
 *
 * This work is free software; you can redistribute it and/or modify it
 * under the terms of either:
 *
 * a) the GNU General Public License as published by the Free Software
 *    Foundation, version 2, or
 *
 * b) the "Angband licence":
 *    This software may be copied and distributed for educational, research,
 *    and not for profit purposes provided that this copyright and statement
 *    are included in all such copies.  Other copyrights may also apply.
 */
#include "z-virt.h"
#include "z-util.h"

#ifdef MEM_DBG
#include "z-file.h"
extern bool character_generated;

struct alloced_memory {
	struct alloced_memory *next;
	uintptr_t pointer;
	size_t size;
	int line;
	const char *file;
} *alloced_mem_list = NULL;

struct alloced_memory_location {
	struct alloced_memory_location *next;
	size_t amt;
	size_t max;
	int line;
	const char *file;
} *alloced_mem_loc_list = NULL;


static bool can_log(const char *file)
{
	if (!file) return false;

	if (my_stristr(file, "z-")) return false;

	return true;
}

static void log_memory_alloc(void *p, size_t len, int line, const char *file)
{
	struct alloced_memory *new;
	struct alloced_memory_location *source;
	
	if (!can_log(file)) {
		return;
	}
	if (!character_generated) {
		return;
	}

	//dbg_log_fmt("memory", "Allocing %5u bytes for  file %s line %i. (0x%16X)", len, file, line, p);
	
	new = mem_zalloc(sizeof *new);

	new->file = file;
	new->line = line;
	new->size = len;
	new->pointer = (uintptr_t)p;

	new->next = alloced_mem_list;
	alloced_mem_list = new;

	for (source = alloced_mem_loc_list; source; source = source->next) {
		if (streq(file, source->file) && line == source->line) {
			source->amt += len;
			source->max = MAX(source->max, source->amt);
			break;
		}
	}

	if (!source) {
		source = mem_zalloc(sizeof *source);

		source->file = file;
		source->line = line;
		source->amt = len;
		source->max = source->amt;

		source->next = alloced_mem_loc_list;
		alloced_mem_loc_list = source;
	}
}

static bool log_memory_free(void *p)
{
	struct alloced_memory *freed, *prev = NULL;
	struct alloced_memory_location *source;

	for (freed = alloced_mem_list; freed; prev = freed, freed = freed->next) {
		if (freed->pointer == ((uintptr_t)p)) {
			break;
		}
	}

	if (!freed) {
		return false;
	}

	if (can_log(freed->file)) {
		//dbg_log_fmt("memory", "Freeing  %5u bytes from file %s line %i. (0x%16X)", freed->size, freed->file, freed->line, freed->pointer);
	}

	if (prev) {
		prev->next = freed->next;
	}
	else {
		alloced_mem_list = freed->next;
	}

	for (source = alloced_mem_loc_list; source; source = source->next) {
		if (streq(source->file, freed->file) && source->line == freed->line) {
			source->amt -= MIN(source->amt, freed->size);
		}
	}

	mem_free(freed);

	return true;
}

void *mem_alloc_record(size_t len, int line, const char *file)
{
	if (!len) return NULL;

	void *p = malloc(len);

	if (!p) {
		alloced_mem_list_destroy(true);
		quit("Out of memory!");
	}

	log_memory_alloc(p, len, line, file);

	return p;
}

void *mem_zalloc_record(size_t len, int line, const char *file)
{
	void *mem = mem_alloc_record(len, line, file);

	if (len) {
		memset(mem, 0, len);
	}

	return mem;
}

void mem_free_record(void *p, int line, const char *file)
{
	log_memory_free(p);

	free(p);
}

void *mem_realloc_record(void *p, size_t len, int line, const char *file)
{
	/* Note: standard realloc(3) frees if passed a size of 0, so this
	 * wrapper has different behavior. */
	if (!len) {
		return NULL;
	}

	log_memory_free(p);

	p = realloc(p, len);
	if (!p) {
		alloced_mem_list_destroy(true);
		quit("Out of Memory!");
	}

	log_memory_alloc(p, len, line, file);

	return p;
}

void alloced_mem_list_destroy(bool log)
{
	struct alloced_memory *curr, *next;
	struct alloced_memory_location *source, *s_next;

	if (log) {
		dbg_log("memory", "\n\nCurrently allocated memory:");
	}

	for (curr = alloced_mem_list; curr; curr = next) {
		next = curr->next;

		if (log) {
			dbg_log_fmt("memory", "%5u bytes from file %s line %i;", curr->size, curr->file, curr->line);
		}

		mem_free(curr);
	}

	alloced_mem_list = NULL;

	if (log) {
		dbg_log("memory", "\nMemory locations:");
	}

	for (source = alloced_mem_loc_list; source; source = s_next) {
		s_next = source->next;

		if (log) {
			dbg_log_fmt("memory", "%5u bytes total from file %s line %i (maximum was %u)", source->amt, source->file, source->line, source->max);
		}

		mem_free(source);
	}

	alloced_mem_loc_list = NULL;
}

char *string_make_record(const char *str, int line, const char *file)
{
	char *res;
	size_t siz;

	/* Error-checking */
	if (!str) return NULL;

	/* Allocate space for the string (including terminator) */
	siz = strlen(str) + 1;
	res = mem_alloc(siz);

	log_memory_alloc(res, siz, line, file);

	/* Copy the string (with terminator) */
	my_strcpy(res, str, siz);

	return res;
}

void string_free_record(char *str, int line, const char *file)
{
	log_memory_free(str);

	mem_free(str);
}

char *string_append_record(char *s1, const char *s2, int line, const char *file)
{
	size_t len;
	if (!s1 && !s2) {
		return NULL;
	} else if (s1 && !s2) {
		return s1;
	} else if (!s1 && s2) {
		return string_make_record(s2, line, file);
	}
	len = strlen(s1);
	s1 = mem_realloc_record(s1, len + strlen(s2) + 1, line, file);
	my_strcpy(s1 + len, s2, strlen(s2) + 1);
	return s1;
}

#else

/**
 * Allocate `len` bytes of memory.
 *
 * Returns:
 *  - NULL if `len` == 0; or
 *  - a pointer to a block of memory of at least `len` bytes
 *
 * Doesn't return on out of memory.
 */
void *mem_alloc(size_t len)
{
	/* Note: standard malloc(3) returns a non-null pointer if passed
	 * a length of 0. Not quite sure why Angband's wrapper has this
	 * behavior. */
	if (!len)
		return NULL;

	void *p = malloc(len);
	if (!p)
		quit("Out of memory!");
	return p;
}

void *mem_zalloc(size_t len)
{
	void *mem = mem_alloc(len);
	if (len)
		memset(mem, 0, len);
	return mem;
}

void mem_free(void *p)
{
	free(p);
}

void *mem_realloc(void *p, size_t len)
{
	/* Note: standard realloc(3) frees if passed a size of 0, so this
	 * wrapper has different behavior. */
	if (!len)
		return NULL;

	p = realloc(p, len);
	if (!p)
		quit("Out of Memory!");
	return p;
}

/**
 * Duplicates an existing string `str`, allocating as much memory as necessary.
 */
char *string_make(const char *str)
{
	char *res;
	size_t siz;

	/* Error-checking */
	if (!str) return NULL;

	/* Allocate space for the string (including terminator) */
	siz = strlen(str) + 1;
	res = mem_alloc(siz);

	/* Copy the string (with terminator) */
	my_strcpy(res, str, siz);

	return res;
}

void string_free(char *str)
{
	mem_free(str);
}

char *string_append(char *s1, const char *s2)
{
	size_t len;
	if (!s1 && !s2) {
		return NULL;
	} else if (s1 && !s2) {
		return s1;
	} else if (!s1 && s2) {
		return string_make(s2);
	}
	len = strlen(s1);
	s1 = mem_realloc(s1, len + strlen(s2) + 1);
	my_strcpy(s1 + len, s2, strlen(s2) + 1);
	return s1;
}

#endif

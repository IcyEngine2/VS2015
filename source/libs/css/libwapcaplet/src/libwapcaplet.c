/* libwapcaplet.c
 *
 * String internment and management tools.
 *
 * Copyright 2009 The NetSurf Browser Project.
 *		  Daniel Silverstone <dsilvers@netsurf-browser.org>
 */

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "libwapcaplet/libwapcaplet.h"

#ifndef UNUSED
#define UNUSED(x) ((x) = (x))
#endif

static inline lwc_hash
lwc__calculate_hash(const char *str, size_t len)
{
	lwc_hash z = 0x811c9dc5;

	while (len > 0) {
		z *= 0x01000193;
		z ^= *str++;
		len--;
	}

	return z;
}

#define STR_OF(str) ((char *)(str + 1))
#define CSTR_OF(str) ((const char *)(str + 1))

#define NR_BUCKETS_DEFAULT	(4091)

typedef lwc_hash (*lwc_hasher)(const char *, size_t);
typedef int (*lwc_strncmp)(const char *, const char *, size_t);
typedef void * (*lwc_memcpy)(void *, const void *, size_t);

lwc_context* lwc_initialise(lwc_realloc realloc, void* user)
{
	if (!realloc)
		return 0;

	lwc_context* ctx = realloc(0, sizeof(lwc_context), user);
	if (ctx == NULL)
		return 0;

	memset(ctx, 0, sizeof(lwc_context));

	ctx->bucketcount = NR_BUCKETS_DEFAULT;
	ctx->buckets = realloc(0, sizeof(lwc_string *) * ctx->bucketcount, user);
	ctx->realloc = realloc;
	ctx->user = user;
	if (ctx->buckets == NULL) {
		realloc(ctx, 0, user);
		ctx = NULL;
		return 0;
	}

	memset(ctx->buckets, 0, sizeof(lwc_string *) * ctx->bucketcount);
	return ctx;
}
void lwc_shutdown(lwc_context* ctx)
{
	if (ctx)
	{
		ctx->realloc(ctx->buckets, 0, ctx->user);
		ctx->realloc(ctx, 0, ctx->user);
	}
}

static lwc_error
lwc__intern(lwc_context* ctx, const char *s, size_t slen,
	   lwc_string **ret,
	   lwc_hasher hasher,
	   lwc_strncmp compare,
	   lwc_memcpy copy)
{
	lwc_hash h;
	lwc_hash bucket;
	lwc_string *str;
	//lwc_error eret;

	assert((s != NULL) || (slen == 0));
	assert(ret);
	assert(ctx);

	h = hasher(s, slen);
	bucket = h % ctx->bucketcount;
	str = ctx->buckets[bucket];

	while (str != NULL) {
		if ((str->hash == h) && (str->len == slen)) {
			if (compare(CSTR_OF(str), s, slen) == 0) {
				str->refcnt++;
				*ret = str;
				return lwc_error_ok;
			}
		}
		str = str->next;
	}

	/* Add one for the additional NUL. */
	*ret = str = ctx->realloc(0, sizeof(lwc_string) + slen + 1, ctx->user);

	if (str == NULL)
		return lwc_error_oom;

	str->prevptr = &(ctx->buckets[bucket]);
	str->next = ctx->buckets[bucket];
	if (str->next != NULL)
		str->next->prevptr = &(str->next);
	ctx->buckets[bucket] = str;

	str->len = slen;
	str->hash = h;
	str->refcnt = 1;
	str->insensitive = NULL;

	copy(STR_OF(str), s, slen);

	/* Guarantee NUL termination */
	STR_OF(str)[slen] = '\0';

	return lwc_error_ok;
}

lwc_error
lwc_intern_string(lwc_context* ctx, const char *s, size_t slen,
		  lwc_string **ret)
{
	return lwc__intern(ctx, s, slen, ret,
			   lwc__calculate_hash,
			   strncmp, (lwc_memcpy)memcpy);
}

lwc_error
lwc_intern_substring(lwc_context* ctx, lwc_string *str,
		     size_t ssoffset, size_t sslen,
		     lwc_string **ret)
{
	assert(str);
	assert(ret);
	assert(ctx);

	if (ssoffset >= str->len)
		return lwc_error_range;
	if ((ssoffset + sslen) > str->len)
		return lwc_error_range;

	return lwc_intern_string(ctx, CSTR_OF(str) + ssoffset, sslen, ret);
}

lwc_error
lwc_string_tolower(lwc_context* ctx, lwc_string *str, lwc_string **ret)
{
	assert(str);
	assert(ret);
	assert(ctx);
	/* Internally make use of knowledge that insensitive strings
	 * are lower case. */
	if (str->insensitive == NULL) {
		lwc_error error = lwc__intern_caseless_string(ctx, str);
		if (error != lwc_error_ok) {
			return error;
		}
	}

	*ret = lwc_string_ref(str->insensitive);
	return lwc_error_ok;
}

void
lwc_string_destroy(lwc_context* ctx, lwc_string *str)
{
	assert(str);
	assert(ctx);

	*(str->prevptr) = str->next;

	if (str->next != NULL)
		str->next->prevptr = str->prevptr;

	if (str->insensitive != NULL && str->refcnt == 0)
		lwc_string_unref(ctx, str->insensitive);

#ifndef NDEBUG
	memset(str, 0xA5, sizeof(*str) + str->len);
#endif

	ctx->realloc(str, 0, ctx->user);
}

/**** Shonky caseless bits ****/

static inline char
lwc__dolower(const char c)
{
	if (c >= 'A' && c <= 'Z')
		return c + 'a' - 'A';
	return c;
}

static inline lwc_hash
lwc__calculate_lcase_hash(const char *str, size_t len)
{
	lwc_hash z = 0x811c9dc5;

	while (len > 0) {
		z *= 0x01000193;
		z ^= lwc__dolower(*str++);
		len--;
	}

	return z;
}

static int
lwc__lcase_strncmp(const char *s1, const char *s2, size_t n)
{
	while (n--) {
		if (*s1++ != lwc__dolower(*s2++))
			/** @todo Test this somehow? */
			return 1;
	}
	return 0;
}

static void *
lwc__lcase_memcpy(void *_target, const void *_source, size_t n)
{
	char *target = _target;
	const char *source = _source;

	while (n--) {
		*target++ = lwc__dolower(*source++);
	}

	return _target;
}

lwc_error
lwc__intern_caseless_string(lwc_context* ctx, lwc_string *str)
{
	assert(str);
	assert(str->insensitive == NULL);

	return lwc__intern(ctx, CSTR_OF(str),
			   str->len, &(str->insensitive),
			   lwc__calculate_lcase_hash,
			   lwc__lcase_strncmp,
			   lwc__lcase_memcpy);
}

/**** Iteration ****/

void
lwc_iterate_strings(lwc_context* ctx, lwc_iteration_callback_fn cb, void *pw)
{
	lwc_hash n;
	lwc_string *str;
	bool found = false;

	if (ctx == NULL)
		return;

	for (n = 0; n < ctx->bucketcount; ++n) {
		for (str = ctx->buckets[n]; str != NULL; str = str->next) {
			found = true;
			cb(str, pw);
		}
	}
}

/*
 * This file was generated by LibCSS gen_parser 
 * 
 * Generated from:
 *
 * counter_increment:CSS_PROP_COUNTER_INCREMENT IDENT:( INHERIT: NONE:0,COUNTER_INCREMENT_NONE IDENT:) IDENT_LIST:( STRING_OPTNUM:COUNTER_INCREMENT_NAMED 1:COUNTER_INCREMENT_NONE IDENT_LIST:)
 * 
 * Licensed under the MIT License,
 *		  http://www.opensource.org/licenses/mit-license.php
 * Copyright 2010 The NetSurf Browser Project.
 */

#include <assert.h>
#include <string.h>

#include "bytecode/bytecode.h"
#include "bytecode/opcodes.h"
#include "parse/properties/properties.h"
#include "parse/properties/utils.h"

/**
 * Parse counter_increment
 *
 * \param c	  Parsing context
 * \param vector  Vector of tokens to process
 * \param ctx	  Pointer to vector iteration context
 * \param result  resulting style
 * \return CSS_OK on success,
 *	   CSS_NOMEM on memory exhaustion,
 *	   CSS_INVALID if the input is not valid
 *
 * Post condition: \a *ctx is updated with the next token to process
 *		   If the input is invalid, then \a *ctx remains unchanged.
 */
css_error css__parse_counter_increment(css_language *c,
		const parserutils_vector *vector, int *ctx,
		css_style *result)
{
	int orig_ctx = *ctx;
	css_error error;
	const css_token *token;
	bool match;

	token = parserutils_vector_iterate(vector, ctx);
	if ((token == NULL) || ((token->type != CSS_TOKEN_IDENT))) {
		*ctx = orig_ctx;
		return CSS_INVALID;
	}

	if ((lwc_string_caseless_isequal(c->sheet->ctx, token->idata, c->strings[INHERIT], &match) == lwc_error_ok && match)) {
			error = css_stylesheet_style_inherit(result, CSS_PROP_COUNTER_INCREMENT);
	} else if ((lwc_string_caseless_isequal(c->sheet->ctx, token->idata, c->strings[NONE], &match) == lwc_error_ok && match)) {
			error = css__stylesheet_style_appendOPV(result, CSS_PROP_COUNTER_INCREMENT, 0,COUNTER_INCREMENT_NONE);
	} else {
		error = css__stylesheet_style_appendOPV(result, CSS_PROP_COUNTER_INCREMENT, 0, COUNTER_INCREMENT_NAMED);
		if (error != CSS_OK) {
			*ctx = orig_ctx;
			return error;
		}

		while ((token != NULL) && (token->type == CSS_TOKEN_IDENT)) {
			uint32_t snumber;
			css_fixed num;
			int pctx;

			error = css__stylesheet_string_add(c->sheet, lwc_string_ref(token->idata), &snumber);
			if (error != CSS_OK) {
				*ctx = orig_ctx;
				return error;
			}

			error = css__stylesheet_style_append(result, snumber);
			if (error != CSS_OK) {
				*ctx = orig_ctx;
				return error;
			}

			consumeWhitespace(vector, ctx);

			pctx = *ctx;
			token = parserutils_vector_iterate(vector, ctx);
			if ((token != NULL) && (token->type == CSS_TOKEN_NUMBER)) {
				size_t consumed = 0;

				num = css__number_from_lwc_string(token->idata, true, &consumed);
				if (consumed != lwc_string_length(token->idata)) {
					*ctx = orig_ctx;
					return CSS_INVALID;
				}
				consumeWhitespace(vector, ctx);

				pctx = *ctx;
				token = parserutils_vector_iterate(vector, ctx);
			} else {
				num = INTTOFIX(1);
			}

			error = css__stylesheet_style_append(result, num);
			if (error != CSS_OK) {
				*ctx = orig_ctx;
				return error;
			}

			if (token == NULL)
				break;

			if (token->type == CSS_TOKEN_IDENT) {
				error = css__stylesheet_style_append(result, COUNTER_INCREMENT_NAMED);
				if (error != CSS_OK) {
					*ctx = orig_ctx;
					return error;
				}
			} else {
				*ctx = pctx; /* rewind one token back */
			}
		}

		error = css__stylesheet_style_append(result, COUNTER_INCREMENT_NONE);
	}

	if (error != CSS_OK)
		*ctx = orig_ctx;
	
	return error;
}

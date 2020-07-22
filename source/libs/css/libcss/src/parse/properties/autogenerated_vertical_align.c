/*
 * This file was generated by LibCSS gen_parser 
 * 
 * Generated from:
 *
 * vertical_align:CSS_PROP_VERTICAL_ALIGN IDENT:( INHERIT: BASELINE:0,VERTICAL_ALIGN_BASELINE SUB:0,VERTICAL_ALIGN_SUB SUPER:0,VERTICAL_ALIGN_SUPER TOP:0,VERTICAL_ALIGN_TOP TEXT_TOP:0,VERTICAL_ALIGN_TEXT_TOP MIDDLE:0,VERTICAL_ALIGN_MIDDLE BOTTOM:0,VERTICAL_ALIGN_BOTTOM TEXT_BOTTOM:0,VERTICAL_ALIGN_TEXT_BOTTOM IDENT:) LENGTH_UNIT:( UNIT_PX:VERTICAL_ALIGN_SET DISALLOW:unit&UNIT_ANGLE||unit&UNIT_TIME||unit&UNIT_FREQ LENGTH_UNIT:)
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
 * Parse vertical_align
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
css_error css__parse_vertical_align(css_language *c,
		const parserutils_vector *vector, int *ctx,
		css_style *result)
{
	int orig_ctx = *ctx;
	css_error error;
	const css_token *token;
	bool match;

	token = parserutils_vector_iterate(vector, ctx);
	if (token == NULL) {
		*ctx = orig_ctx;
		return CSS_INVALID;
	}

	if ((token->type == CSS_TOKEN_IDENT) && (lwc_string_caseless_isequal(c->sheet->ctx, token->idata, c->strings[INHERIT], &match) == lwc_error_ok && match)) {
			error = css_stylesheet_style_inherit(result, CSS_PROP_VERTICAL_ALIGN);
	} else if ((token->type == CSS_TOKEN_IDENT) && (lwc_string_caseless_isequal(c->sheet->ctx, token->idata, c->strings[BASELINE], &match) == lwc_error_ok && match)) {
			error = css__stylesheet_style_appendOPV(result, CSS_PROP_VERTICAL_ALIGN, 0,VERTICAL_ALIGN_BASELINE);
	} else if ((token->type == CSS_TOKEN_IDENT) && (lwc_string_caseless_isequal(c->sheet->ctx, token->idata, c->strings[SUB], &match) == lwc_error_ok && match)) {
			error = css__stylesheet_style_appendOPV(result, CSS_PROP_VERTICAL_ALIGN, 0,VERTICAL_ALIGN_SUB);
	} else if ((token->type == CSS_TOKEN_IDENT) && (lwc_string_caseless_isequal(c->sheet->ctx, token->idata, c->strings[SUPER], &match) == lwc_error_ok && match)) {
			error = css__stylesheet_style_appendOPV(result, CSS_PROP_VERTICAL_ALIGN, 0,VERTICAL_ALIGN_SUPER);
	} else if ((token->type == CSS_TOKEN_IDENT) && (lwc_string_caseless_isequal(c->sheet->ctx, token->idata, c->strings[TOP], &match) == lwc_error_ok && match)) {
			error = css__stylesheet_style_appendOPV(result, CSS_PROP_VERTICAL_ALIGN, 0,VERTICAL_ALIGN_TOP);
	} else if ((token->type == CSS_TOKEN_IDENT) && (lwc_string_caseless_isequal(c->sheet->ctx, token->idata, c->strings[TEXT_TOP], &match) == lwc_error_ok && match)) {
			error = css__stylesheet_style_appendOPV(result, CSS_PROP_VERTICAL_ALIGN, 0,VERTICAL_ALIGN_TEXT_TOP);
	} else if ((token->type == CSS_TOKEN_IDENT) && (lwc_string_caseless_isequal(c->sheet->ctx, token->idata, c->strings[MIDDLE], &match) == lwc_error_ok && match)) {
			error = css__stylesheet_style_appendOPV(result, CSS_PROP_VERTICAL_ALIGN, 0,VERTICAL_ALIGN_MIDDLE);
	} else if ((token->type == CSS_TOKEN_IDENT) && (lwc_string_caseless_isequal(c->sheet->ctx, token->idata, c->strings[BOTTOM], &match) == lwc_error_ok && match)) {
			error = css__stylesheet_style_appendOPV(result, CSS_PROP_VERTICAL_ALIGN, 0,VERTICAL_ALIGN_BOTTOM);
	} else if ((token->type == CSS_TOKEN_IDENT) && (lwc_string_caseless_isequal(c->sheet->ctx, token->idata, c->strings[TEXT_BOTTOM], &match) == lwc_error_ok && match)) {
			error = css__stylesheet_style_appendOPV(result, CSS_PROP_VERTICAL_ALIGN, 0,VERTICAL_ALIGN_TEXT_BOTTOM);
	} else {
		css_fixed length = 0;
		uint32_t unit = 0;
		*ctx = orig_ctx;

		error = css__parse_unit_specifier(c, vector, ctx, UNIT_PX, &length, &unit);
		if (error != CSS_OK) {
			*ctx = orig_ctx;
			return error;
		}

		if (unit&UNIT_ANGLE||unit&UNIT_TIME||unit&UNIT_FREQ) {
			*ctx = orig_ctx;
			return CSS_INVALID;
		}

		error = css__stylesheet_style_appendOPV(result, CSS_PROP_VERTICAL_ALIGN, 0, VERTICAL_ALIGN_SET);
		if (error != CSS_OK) {
			*ctx = orig_ctx;
			return error;
		}

		error = css__stylesheet_style_vappend(result, 2, length, unit);
	}

	if (error != CSS_OK)
		*ctx = orig_ctx;
	
	return error;
}

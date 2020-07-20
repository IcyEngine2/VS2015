/*
 * This file was generated by LibCSS gen_parser 
 * 
 * Generated from:
 *
 * font_style:CSS_PROP_FONT_STYLE IDENT:( INHERIT: NORMAL:0,FONT_STYLE_NORMAL ITALIC:0,FONT_STYLE_ITALIC OBLIQUE:0,FONT_STYLE_OBLIQUE IDENT:)
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
 * Parse font_style
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
css_error css__parse_font_style(css_language *c,
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
			error = css_stylesheet_style_inherit(result, CSS_PROP_FONT_STYLE);
	} else if ((lwc_string_caseless_isequal(c->sheet->ctx, token->idata, c->strings[NORMAL], &match) == lwc_error_ok && match)) {
			error = css__stylesheet_style_appendOPV(result, CSS_PROP_FONT_STYLE, 0,FONT_STYLE_NORMAL);
	} else if ((lwc_string_caseless_isequal(c->sheet->ctx, token->idata, c->strings[ITALIC], &match) == lwc_error_ok && match)) {
			error = css__stylesheet_style_appendOPV(result, CSS_PROP_FONT_STYLE, 0,FONT_STYLE_ITALIC);
	} else if ((lwc_string_caseless_isequal(c->sheet->ctx, token->idata, c->strings[OBLIQUE], &match) == lwc_error_ok && match)) {
			error = css__stylesheet_style_appendOPV(result, CSS_PROP_FONT_STYLE, 0,FONT_STYLE_OBLIQUE);
	} else {
		error = CSS_INVALID;
	}

	if (error != CSS_OK)
		*ctx = orig_ctx;
	
	return error;
}


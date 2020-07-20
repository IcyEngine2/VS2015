/*
 * This file is part of LibCSS
 * Licensed under the MIT License,
 *		  http://www.opensource.org/licenses/mit-license.php
 * Copyright 2009 John-Mark Bell <jmb@netsurf-browser.org>
 */

#include "bytecode/bytecode.h"
#include "bytecode/opcodes.h"
#include "select/propset.h"
#include "select/propget.h"
#include "utils/utils.h"

#include "select/properties/properties.h"
#include "select/properties/helpers.h"

css_error css__cascade_counter_increment(uint32_t opv, css_style *style,
		css_select_state *state)
{
	return css__cascade_counter_increment_reset(opv, style, state,
			set_counter_increment);
}

css_error css__set_counter_increment_from_hint(const css_hint *hint,
		css_computed_style *style)
{
	css_computed_counter *item;
	css_error error;

	error = set_counter_increment(style, hint->status, hint->data.counter);

	if (hint->status == CSS_COUNTER_INCREMENT_NAMED &&
			hint->data.counter != NULL) {
		for (item = hint->data.counter; item->name != NULL; item++) {
			lwc_string_unref(style->ctx, item->name);
		}
	}

	if (error != CSS_OK && hint->data.counter != NULL)
		style->ctx->realloc(hint->data.counter, 0, style->ctx->user);

	return error;
}

css_error css__initial_counter_increment(css_select_state *state)
{
	return set_counter_increment(state->computed,
			CSS_COUNTER_INCREMENT_NONE, NULL);
}

css_error css__compose_counter_increment(const css_computed_style *parent,
		const css_computed_style *child,
		css_computed_style *result)
{
	css_error error;
	css_computed_counter *copy = NULL;
	const css_computed_counter *items = NULL;
	uint8_t type = get_counter_increment(child, &items);

	if (type == CSS_COUNTER_INCREMENT_INHERIT) {
		type = get_counter_increment(parent, &items);
	}

	if (type == CSS_COUNTER_INCREMENT_NAMED && items != NULL) {
		size_t n_items = 0;
		const css_computed_counter *i;

		for (i = items; i->name != NULL; i++)
			n_items++;

		copy = parent->ctx->realloc(0, (n_items + 1) * sizeof(css_computed_counter), parent->ctx->user);
		if (copy == NULL)
			return CSS_NOMEM;

		memcpy(copy, items, (n_items + 1) *
				sizeof(css_computed_counter));
	}

	error = set_counter_increment(result, type, copy);
	if (error != CSS_OK && copy != NULL)
		parent->ctx->realloc(copy, 0, parent->ctx->user);

	return error;
}

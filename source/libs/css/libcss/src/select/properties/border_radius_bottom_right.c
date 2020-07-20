/*
 * This file is part of LibCSS
 * Licensed under the MIT License,
 *		  http://www.opensource.org/licenses/mit-license.php
 * Copyright 2017 ZhuYaDong <aeix@foxmail.com>
 * 					 2020 Artem Chernyshev (unix4ever)
 */

#include "bytecode/bytecode.h"
#include "bytecode/opcodes.h"
#include "select/propset.h"
#include "select/propget.h"
#include "utils/utils.h"

#include "select/properties/properties.h"
#include "select/properties/helpers.h"

css_error css__cascade_border_radius_bottom_right(uint32_t opv, css_style *style,
                                               css_select_state *state)
{
  return css__cascade_length(opv, style, state, set_border_radius_bottom_right);
}

css_error css__set_border_radius_bottom_right_from_hint(const css_hint *hint,
                                                     css_computed_style *style)
{
  return set_border_radius_bottom_right(style, hint->status,
                                     hint->data.length.value, hint->data.length.unit);
}

css_error css__initial_border_radius_bottom_right(css_select_state *state)
{
  return set_border_radius_bottom_right(state->computed, CSS_BORDER_RADIUS_AUTO, 0, CSS_UNIT_PX);
}

css_error css__compose_border_radius_bottom_right(const css_computed_style *parent,
                                               const css_computed_style *child,
                                               css_computed_style *result)
{
  css_fixed length = 0;
  css_unit unit = CSS_UNIT_PX;
  uint8_t type = get_border_radius_bottom_right(child, &length, &unit);

  if (type == CSS_BORDER_RADIUS_INHERIT) {
    type = get_border_radius_bottom_right(parent, &length, &unit);
  }

  return set_border_radius_bottom_right(result, type, length, unit);
}

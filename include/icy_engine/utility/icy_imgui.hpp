#pragma once

#include <icy_engine/core/icy_core.hpp>
#include <icy_engine/core/icy_event.hpp>
#include <icy_engine/core/icy_array.hpp>
#include <icy_engine/graphics/icy_window.hpp>
#include <icy_engine/graphics/icy_render.hpp>
#include "icy_variant.hpp"

namespace icy 
{
    namespace imgui_flags
    {

        // Flags for ImGui::InputText()
        enum ImGuiInputTextFlags_
        {
            ImGuiInputTextFlags_None                = 0,
            ImGuiInputTextFlags_CharsDecimal        = 1 << 0,   // Allow 0123456789.+-*/
            ImGuiInputTextFlags_CharsHexadecimal    = 1 << 1,   // Allow 0123456789ABCDEFabcdef
            ImGuiInputTextFlags_CharsUppercase      = 1 << 2,   // Turn a..z into A..Z
            ImGuiInputTextFlags_CharsNoBlank        = 1 << 3,   // Filter out spaces, tabs
            ImGuiInputTextFlags_AutoSelectAll       = 1 << 4,   // Select entire text when first taking mouse focus
            ImGuiInputTextFlags_EnterReturnsTrue    = 1 << 5,   // Return 'true' when Enter is pressed (as opposed to every time the value was modified). Consider looking at the IsItemDeactivatedAfterEdit() function.
            ImGuiInputTextFlags_CallbackCompletion  = 1 << 6,   // Callback on pressing TAB (for completion handling)
            ImGuiInputTextFlags_CallbackHistory     = 1 << 7,   // Callback on pressing Up/Down arrows (for history handling)
            ImGuiInputTextFlags_CallbackAlways      = 1 << 8,   // Callback on each iteration. User code may query cursor position, modify text buffer.
            ImGuiInputTextFlags_CallbackCharFilter  = 1 << 9,   // Callback on character inputs to replace or discard them. Modify 'EventChar' to replace or discard, or return 1 in callback to discard.
            ImGuiInputTextFlags_AllowTabInput       = 1 << 10,  // Pressing TAB input a '\t' character into the text field
            ImGuiInputTextFlags_CtrlEnterForNewLine = 1 << 11,  // In multi-line mode, unfocus with Enter, add new line with Ctrl+Enter (default is opposite: unfocus with Ctrl+Enter, add line with Enter).
            ImGuiInputTextFlags_NoHorizontalScroll  = 1 << 12,  // Disable following the cursor horizontally
            ImGuiInputTextFlags_AlwaysOverwrite     = 1 << 13,  // Overwrite mode
            ImGuiInputTextFlags_ReadOnly            = 1 << 14,  // Read-only mode
            ImGuiInputTextFlags_Password            = 1 << 15,  // Password mode, display all characters as '*'
            ImGuiInputTextFlags_NoUndoRedo          = 1 << 16,  // Disable undo/redo. Note that input text owns the text data while active, if you want to provide your own undo/redo stack you need e.g. to call ClearActiveID().
            ImGuiInputTextFlags_CharsScientific     = 1 << 17,  // Allow 0123456789.+-*/eE (Scientific notation input)
            ImGuiInputTextFlags_CallbackResize      = 1 << 18,  // Callback on buffer capacity changes request (beyond 'buf_size' parameter value), allowing the string to grow. Notify when the string wants to be resized (for string types which hold a cache of their Size). You will be provided a new BufSize in the callback and NEED to honor it. (see misc/cpp/imgui_stdlib.h for an example of using this)
            ImGuiInputTextFlags_CallbackEdit        = 1 << 19   // Callback on any edit (note that InputText() already returns true on edit, the callback is useful mainly to manipulate the underlying buffer while focus is active)
        };

        // Flags for ImGui::TreeNodeEx(), ImGui::CollapsingHeader*()
        enum ImGuiTreeNodeFlags_
        {
            ImGuiTreeNodeFlags_None                 = 0,
            ImGuiTreeNodeFlags_Selected             = 1 << 0,   // Draw as selected
            ImGuiTreeNodeFlags_Framed               = 1 << 1,   // Draw frame with background (e.g. for CollapsingHeader)
            ImGuiTreeNodeFlags_AllowItemOverlap     = 1 << 2,   // Hit testing to allow subsequent widgets to overlap this one
            ImGuiTreeNodeFlags_NoTreePushOnOpen     = 1 << 3,   // Don't do a TreePush() when open (e.g. for CollapsingHeader) = no extra indent nor pushing on ID stack
            ImGuiTreeNodeFlags_NoAutoOpenOnLog      = 1 << 4,   // Don't automatically and temporarily open node when Logging is active (by default logging will automatically open tree nodes)
            ImGuiTreeNodeFlags_DefaultOpen          = 1 << 5,   // Default node to be open
            ImGuiTreeNodeFlags_OpenOnDoubleClick    = 1 << 6,   // Need double-click to open node
            ImGuiTreeNodeFlags_OpenOnArrow          = 1 << 7,   // Only open when clicking on the arrow part. If ImGuiTreeNodeFlags_OpenOnDoubleClick is also set, single-click arrow or double-click all box to open.
            ImGuiTreeNodeFlags_Leaf                 = 1 << 8,   // No collapsing, no arrow (use as a convenience for leaf nodes).
            ImGuiTreeNodeFlags_Bullet               = 1 << 9,   // Display a bullet instead of arrow
            ImGuiTreeNodeFlags_FramePadding         = 1 << 10,  // Use FramePadding (even for an unframed text node) to vertically align text baseline to regular widget height. Equivalent to calling AlignTextToFramePadding().
            ImGuiTreeNodeFlags_SpanAvailWidth       = 1 << 11,  // Extend hit box to the right-most edge, even if not framed. This is not the default in order to allow adding other items on the same line. In the future we may refactor the hit system to be front-to-back, allowing natural overlaps and then this can become the default.
            ImGuiTreeNodeFlags_SpanFullWidth        = 1 << 12,  // Extend hit box to the left-most and right-most edges (bypass the indented area).
            ImGuiTreeNodeFlags_NavLeftJumpsBackHere = 1 << 13,  // (WIP) Nav: left direction may move to this TreeNode() from any of its child (items submitted between TreeNode and TreePop)
            //ImGuiTreeNodeFlags_NoScrollOnOpen     = 1 << 14,  // FIXME: TODO: Disable automatic scroll on TreePop() if node got just open and contents is not visible
            ImGuiTreeNodeFlags_CollapsingHeader     = ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_NoAutoOpenOnLog
        };

        // Flags for OpenPopup*(), BeginPopupContext*(), IsPopupOpen() functions.
        // - To be backward compatible with older API which took an 'int mouse_button = 1' argument, we need to treat
        //   small flags values as a mouse button index, so we encode the mouse button in the first few bits of the flags.
        //   It is therefore guaranteed to be legal to pass a mouse button index in ImGuiPopupFlags.
        // - For the same reason, we exceptionally default the ImGuiPopupFlags argument of BeginPopupContextXXX functions to 1 instead of 0.
        //   IMPORTANT: because the default parameter is 1 (==ImGuiPopupFlags_MouseButtonRight), if you rely on the default parameter
        //   and want to another another flag, you need to pass in the ImGuiPopupFlags_MouseButtonRight flag.
        // - Multiple buttons currently cannot be combined/or-ed in those functions (we could allow it later).
        enum ImGuiPopupFlags_
        {
            ImGuiPopupFlags_None                    = 0,
            ImGuiPopupFlags_MouseButtonLeft         = 0,        // For BeginPopupContext*(): open on Left Mouse release. Guaranteed to always be == 0 (same as ImGuiMouseButton_Left)
            ImGuiPopupFlags_MouseButtonRight        = 1,        // For BeginPopupContext*(): open on Right Mouse release. Guaranteed to always be == 1 (same as ImGuiMouseButton_Right)
            ImGuiPopupFlags_MouseButtonMiddle       = 2,        // For BeginPopupContext*(): open on Middle Mouse release. Guaranteed to always be == 2 (same as ImGuiMouseButton_Middle)
            ImGuiPopupFlags_MouseButtonMask_        = 0x1F,
            ImGuiPopupFlags_MouseButtonDefault_     = 1,
            ImGuiPopupFlags_NoOpenOverExistingPopup = 1 << 5,   // For OpenPopup*(), BeginPopupContext*(): don't open if there's already a popup at the same level of the popup stack
            ImGuiPopupFlags_NoOpenOverItems         = 1 << 6,   // For BeginPopupContextWindow(): don't return true when hovering items, only when hovering empty space
            ImGuiPopupFlags_AnyPopupId              = 1 << 7,   // For IsPopupOpen(): ignore the ImGuiID parameter and test for any popup.
            ImGuiPopupFlags_AnyPopupLevel           = 1 << 8,   // For IsPopupOpen(): search/test at any level of the popup stack (default test in the current level)
            ImGuiPopupFlags_AnyPopup                = ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel
        };

        // Flags for ImGui::Selectable()
        enum ImGuiSelectableFlags_
        {
            ImGuiSelectableFlags_None               = 0,
            ImGuiSelectableFlags_DontClosePopups    = 1 << 0,   // Clicking this don't close parent popup window
            ImGuiSelectableFlags_SpanAllColumns     = 1 << 1,   // Selectable frame can span all columns (text will still fit in current column)
            ImGuiSelectableFlags_AllowDoubleClick   = 1 << 2,   // Generate press events on double clicks too
            ImGuiSelectableFlags_Disabled           = 1 << 3,   // Cannot be selected, display grayed out text
            ImGuiSelectableFlags_AllowItemOverlap   = 1 << 4    // (WIP) Hit testing to allow subsequent widgets to overlap this one
        };

        // Flags for ImGui::BeginCombo()
        enum ImGuiComboFlags_
        {
            ImGuiComboFlags_None                    = 0,
            ImGuiComboFlags_PopupAlignLeft          = 1 << 0,   // Align the popup toward the left by default
            ImGuiComboFlags_HeightSmall             = 1 << 1,   // Max ~4 items visible. Tip: If you want your combo popup to be a specific size you can use SetNextWindowSizeConstraints() prior to calling BeginCombo()
            ImGuiComboFlags_HeightRegular           = 1 << 2,   // Max ~8 items visible (default)
            ImGuiComboFlags_HeightLarge             = 1 << 3,   // Max ~20 items visible
            ImGuiComboFlags_HeightLargest           = 1 << 4,   // As many fitting items as possible
            ImGuiComboFlags_NoArrowButton           = 1 << 5,   // Display on the preview box without the square arrow button
            ImGuiComboFlags_NoPreview               = 1 << 6,   // Display only a square arrow button
            ImGuiComboFlags_HeightMask_             = ImGuiComboFlags_HeightSmall | ImGuiComboFlags_HeightRegular | ImGuiComboFlags_HeightLarge | ImGuiComboFlags_HeightLargest
        };

        // Flags for ImGui::BeginTabBar()
        enum ImGuiTabBarFlags_
        {
            ImGuiTabBarFlags_None                           = 0,
            ImGuiTabBarFlags_Reorderable                    = 1 << 0,   // Allow manually dragging tabs to re-order them + New tabs are appended at the end of list
            ImGuiTabBarFlags_AutoSelectNewTabs              = 1 << 1,   // Automatically select new tabs when they appear
            ImGuiTabBarFlags_TabListPopupButton             = 1 << 2,   // Disable buttons to open the tab list popup
            ImGuiTabBarFlags_NoCloseWithMiddleMouseButton   = 1 << 3,   // Disable behavior of closing tabs (that are submitted with p_open != NULL) with middle mouse button. You can still repro this behavior on user's side with if (IsItemHovered() && IsMouseClicked(2)) *p_open = false.
            ImGuiTabBarFlags_NoTabListScrollingButtons      = 1 << 4,   // Disable scrolling buttons (apply when fitting policy is ImGuiTabBarFlags_FittingPolicyScroll)
            ImGuiTabBarFlags_NoTooltip                      = 1 << 5,   // Disable tooltips when hovering a tab
            ImGuiTabBarFlags_FittingPolicyResizeDown        = 1 << 6,   // Resize tabs when they don't fit
            ImGuiTabBarFlags_FittingPolicyScroll            = 1 << 7,   // Add scroll buttons when tabs don't fit
            ImGuiTabBarFlags_FittingPolicyMask_             = ImGuiTabBarFlags_FittingPolicyResizeDown | ImGuiTabBarFlags_FittingPolicyScroll,
            ImGuiTabBarFlags_FittingPolicyDefault_          = ImGuiTabBarFlags_FittingPolicyResizeDown
        };

        // Flags for ImGui::BeginTabItem()
        enum ImGuiTabItemFlags_
        {
            ImGuiTabItemFlags_None                          = 0,
            ImGuiTabItemFlags_UnsavedDocument               = 1 << 0,   // Display a dot next to the title + tab is selected when clicking the X + closure is not assumed (will wait for user to stop submitting the tab). Otherwise closure is assumed when pressing the X, so if you keep submitting the tab may reappear at end of tab bar.
            ImGuiTabItemFlags_SetSelected                   = 1 << 1,   // Trigger flag to programmatically make the tab selected when calling BeginTabItem()
            ImGuiTabItemFlags_NoCloseWithMiddleMouseButton  = 1 << 2,   // Disable behavior of closing tabs (that are submitted with p_open != NULL) with middle mouse button. You can still repro this behavior on user's side with if (IsItemHovered() && IsMouseClicked(2)) *p_open = false.
            ImGuiTabItemFlags_NoPushId                      = 1 << 3,   // Don't call PushID(tab->ID)/PopID() on BeginTabItem()/EndTabItem()
            ImGuiTabItemFlags_NoTooltip                     = 1 << 4,   // Disable tooltip for the given tab
            ImGuiTabItemFlags_NoReorder                     = 1 << 5,   // Disable reordering this tab or having another tab cross over this tab
            ImGuiTabItemFlags_Leading                       = 1 << 6,   // Enforce the tab position to the left of the tab bar (after the tab list popup button)
            ImGuiTabItemFlags_Trailing                      = 1 << 7    // Enforce the tab position to the right of the tab bar (before the scrolling buttons)
        };

        // Flags for ImGui::BeginTable()
        // [BETA API] API may evolve slightly! If you use this, please update to the next version when it comes out!
        // - Important! Sizing policies have complex and subtle side effects, more so than you would expect.
        //   Read comments/demos carefully + experiment with live demos to get acquainted with them.
        // - The DEFAULT sizing policies are:
        //    - Default to ImGuiTableFlags_SizingFixedFit    if ScrollX is on, or if host window has ImGuiWindowFlags_AlwaysAutoResize.
        //    - Default to ImGuiTableFlags_SizingStretchSame if ScrollX is off.
        // - When ScrollX is off:
        //    - Table defaults to ImGuiTableFlags_SizingStretchSame -> all Columns defaults to ImGuiTableColumnFlags_WidthStretch with same weight.
        //    - Columns sizing policy allowed: Stretch (default), Fixed/Auto.
        //    - Fixed Columns will generally obtain their requested width (unless the table cannot fit them all).
        //    - Stretch Columns will share the remaining width.
        //    - Mixed Fixed/Stretch columns is possible but has various side-effects on resizing behaviors.
        //      The typical use of mixing sizing policies is: any number of LEADING Fixed columns, followed by one or two TRAILING Stretch columns.
        //      (this is because the visible order of columns have subtle but necessary effects on how they react to manual resizing).
        // - When ScrollX is on:
        //    - Table defaults to ImGuiTableFlags_SizingFixedFit -> all Columns defaults to ImGuiTableColumnFlags_WidthFixed
        //    - Columns sizing policy allowed: Fixed/Auto mostly.
        //    - Fixed Columns can be enlarged as needed. Table will show an horizontal scrollbar if needed.
        //    - When using auto-resizing (non-resizable) fixed columns, querying the content width to use item right-alignment e.g. SetNextItemWidth(-FLT_MIN) doesn't make sense, would create a feedback loop.
        //    - Using Stretch columns OFTEN DOES NOT MAKE SENSE if ScrollX is on, UNLESS you have specified a value for 'inner_width' in BeginTable().
        //      If you specify a value for 'inner_width' then effectively the scrolling space is known and Stretch or mixed Fixed/Stretch columns become meaningful again.
        // - Read on documentation at the top of imgui_tables.cpp for details.
        enum ImGuiTableFlags_
        {
            // Features
            ImGuiTableFlags_None                       = 0,
            ImGuiTableFlags_Resizable                  = 1 << 0,   // Enable resizing columns.
            ImGuiTableFlags_Reorderable                = 1 << 1,   // Enable reordering columns in header row (need calling TableSetupColumn() + TableHeadersRow() to display headers)
            ImGuiTableFlags_Hideable                   = 1 << 2,   // Enable hiding/disabling columns in context menu.
            ImGuiTableFlags_Sortable                   = 1 << 3,   // Enable sorting. Call TableGetSortSpecs() to obtain sort specs. Also see ImGuiTableFlags_SortMulti and ImGuiTableFlags_SortTristate.
            ImGuiTableFlags_NoSavedSettings            = 1 << 4,   // Disable persisting columns order, width and sort settings in the .ini file.
            ImGuiTableFlags_ContextMenuInBody          = 1 << 5,   // Right-click on columns body/contents will display table context menu. By default it is available in TableHeadersRow().
            // Decorations
            ImGuiTableFlags_RowBg                      = 1 << 6,   // Set each RowBg color with ImGuiCol_TableRowBg or ImGuiCol_TableRowBgAlt (equivalent of calling TableSetBgColor with ImGuiTableBgFlags_RowBg0 on each row manually)
            ImGuiTableFlags_BordersInnerH              = 1 << 7,   // Draw horizontal borders between rows.
            ImGuiTableFlags_BordersOuterH              = 1 << 8,   // Draw horizontal borders at the top and bottom.
            ImGuiTableFlags_BordersInnerV              = 1 << 9,   // Draw vertical borders between columns.
            ImGuiTableFlags_BordersOuterV              = 1 << 10,  // Draw vertical borders on the left and right sides.
            ImGuiTableFlags_BordersH                   = ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_BordersOuterH, // Draw horizontal borders.
            ImGuiTableFlags_BordersV                   = ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_BordersOuterV, // Draw vertical borders.
            ImGuiTableFlags_BordersInner               = ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_BordersInnerH, // Draw inner borders.
            ImGuiTableFlags_BordersOuter               = ImGuiTableFlags_BordersOuterV | ImGuiTableFlags_BordersOuterH, // Draw outer borders.
            ImGuiTableFlags_Borders                    = ImGuiTableFlags_BordersInner | ImGuiTableFlags_BordersOuter,   // Draw all borders.
            ImGuiTableFlags_NoBordersInBody            = 1 << 11,  // [ALPHA] Disable vertical borders in columns Body (borders will always appears in Headers). -> May move to style
            ImGuiTableFlags_NoBordersInBodyUntilResize = 1 << 12,  // [ALPHA] Disable vertical borders in columns Body until hovered for resize (borders will always appears in Headers). -> May move to style
            // Sizing Policy (read above for defaults)
            ImGuiTableFlags_SizingFixedFit             = 1 << 13,  // Columns default to _WidthFixed or _WidthAuto (if resizable or not resizable), matching contents width.
            ImGuiTableFlags_SizingFixedSame            = 2 << 13,  // Columns default to _WidthFixed or _WidthAuto (if resizable or not resizable), matching the maximum contents width of all columns. Implicitly enable ImGuiTableFlags_NoKeepColumnsVisible.
            ImGuiTableFlags_SizingStretchProp          = 3 << 13,  // Columns default to _WidthStretch with default weights proportional to each columns contents widths.
            ImGuiTableFlags_SizingStretchSame          = 4 << 13,  // Columns default to _WidthStretch with default weights all equal, unless overridden by TableSetupColumn().
            // Sizing Extra Options
            ImGuiTableFlags_NoHostExtendX              = 1 << 16,  // Make outer width auto-fit to columns, overriding outer_size.x value. Only available when ScrollX/ScrollY are disabled and Stretch columns are not used.
            ImGuiTableFlags_NoHostExtendY              = 1 << 17,  // Make outer height stop exactly at outer_size.y (prevent auto-extending table past the limit). Only available when ScrollX/ScrollY are disabled. Data below the limit will be clipped and not visible.
            ImGuiTableFlags_NoKeepColumnsVisible       = 1 << 18,  // Disable keeping column always minimally visible when ScrollX is off and table gets too small. Not recommended if columns are resizable.
            ImGuiTableFlags_PreciseWidths              = 1 << 19,  // Disable distributing remainder width to stretched columns (width allocation on a 100-wide table with 3 columns: Without this flag: 33,33,34. With this flag: 33,33,33). With larger number of columns, resizing will appear to be less smooth.
            // Clipping
            ImGuiTableFlags_NoClip                     = 1 << 20,  // Disable clipping rectangle for every individual columns (reduce draw command count, items will be able to overflow into other columns). Generally incompatible with TableSetupScrollFreeze().
            // Padding
            ImGuiTableFlags_PadOuterX                  = 1 << 21,  // Default if BordersOuterV is on. Enable outer-most padding. Generally desirable if you have headers.
            ImGuiTableFlags_NoPadOuterX                = 1 << 22,  // Default if BordersOuterV is off. Disable outer-most padding.
            ImGuiTableFlags_NoPadInnerX                = 1 << 23,  // Disable inner padding between columns (double inner padding if BordersOuterV is on, single inner padding if BordersOuterV is off).
            // Scrolling
            ImGuiTableFlags_ScrollX                    = 1 << 24,  // Enable horizontal scrolling. Require 'outer_size' parameter of BeginTable() to specify the container size. Changes default sizing policy. Because this create a child window, ScrollY is currently generally recommended when using ScrollX.
            ImGuiTableFlags_ScrollY                    = 1 << 25,  // Enable vertical scrolling. Require 'outer_size' parameter of BeginTable() to specify the container size.
            // Sorting
            ImGuiTableFlags_SortMulti                  = 1 << 26,  // Hold shift when clicking headers to sort on multiple column. TableGetSortSpecs() may return specs where (SpecsCount > 1).
            ImGuiTableFlags_SortTristate               = 1 << 27,  // Allow no sorting, disable default sorting. TableGetSortSpecs() may return specs where (SpecsCount == 0).

            // [Internal] Combinations and masks
            ImGuiTableFlags_SizingMask_                = ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_SizingFixedSame | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_SizingStretchSame
        };

        // Flags for ImGui::TableSetupColumn()
        enum ImGuiTableColumnFlags_
        {
            // Input configuration flags
            ImGuiTableColumnFlags_None                  = 0,
            ImGuiTableColumnFlags_Disabled              = 1 << 0,   // Overriding/master disable flag: hide column, won't show in context menu (unlike calling TableSetColumnEnabled() which manipulates the user accessible state)
            ImGuiTableColumnFlags_DefaultHide           = 1 << 1,   // Default as a hidden/disabled column.
            ImGuiTableColumnFlags_DefaultSort           = 1 << 2,   // Default as a sorting column.
            ImGuiTableColumnFlags_WidthStretch          = 1 << 3,   // Column will stretch. Preferable with horizontal scrolling disabled (default if table sizing policy is _SizingStretchSame or _SizingStretchProp).
            ImGuiTableColumnFlags_WidthFixed            = 1 << 4,   // Column will not stretch. Preferable with horizontal scrolling enabled (default if table sizing policy is _SizingFixedFit and table is resizable).
            ImGuiTableColumnFlags_NoResize              = 1 << 5,   // Disable manual resizing.
            ImGuiTableColumnFlags_NoReorder             = 1 << 6,   // Disable manual reordering this column, this will also prevent other columns from crossing over this column.
            ImGuiTableColumnFlags_NoHide                = 1 << 7,   // Disable ability to hide/disable this column.
            ImGuiTableColumnFlags_NoClip                = 1 << 8,   // Disable clipping for this column (all NoClip columns will render in a same draw command).
            ImGuiTableColumnFlags_NoSort                = 1 << 9,   // Disable ability to sort on this field (even if ImGuiTableFlags_Sortable is set on the table).
            ImGuiTableColumnFlags_NoSortAscending       = 1 << 10,  // Disable ability to sort in the ascending direction.
            ImGuiTableColumnFlags_NoSortDescending      = 1 << 11,  // Disable ability to sort in the descending direction.
            ImGuiTableColumnFlags_NoHeaderLabel         = 1 << 12,  // TableHeadersRow() will not submit label for this column. Convenient for some small columns. Name will still appear in context menu.
            ImGuiTableColumnFlags_NoHeaderWidth         = 1 << 13,  // Disable header text width contribution to automatic column width.
            ImGuiTableColumnFlags_PreferSortAscending   = 1 << 14,  // Make the initial sort direction Ascending when first sorting on this column (default).
            ImGuiTableColumnFlags_PreferSortDescending  = 1 << 15,  // Make the initial sort direction Descending when first sorting on this column.
            ImGuiTableColumnFlags_IndentEnable          = 1 << 16,  // Use current Indent value when entering cell (default for column 0).
            ImGuiTableColumnFlags_IndentDisable         = 1 << 17,  // Ignore current Indent value when entering cell (default for columns > 0). Indentation changes _within_ the cell will still be honored.
        };

        // Flags for ImGui::TableNextRow()
        enum ImGuiTableRowFlags_
        {
            ImGuiTableRowFlags_None                         = 0,
            ImGuiTableRowFlags_Headers                      = 1 << 0    // Identify header row (set default background color + width of its contents accounted different for auto column width)
        };

        // Enum for ImGui::TableSetBgColor()
        // Background colors are rendering in 3 layers:
        //  - Layer 0: draw with RowBg0 color if set, otherwise draw with ColumnBg0 if set.
        //  - Layer 1: draw with RowBg1 color if set, otherwise draw with ColumnBg1 if set.
        //  - Layer 2: draw with CellBg color if set.
        // The purpose of the two row/columns layers is to let you decide if a background color changes should override or blend with the existing color.
        // When using ImGuiTableFlags_RowBg on the table, each row has the RowBg0 color automatically set for odd/even rows.
        // If you set the color of RowBg0 target, your color will override the existing RowBg0 color.
        // If you set the color of RowBg1 or ColumnBg1 target, your color will blend over the RowBg0 color.
        enum ImGuiTableBgTarget_
        {
            ImGuiTableBgTarget_None                         = 0,
            ImGuiTableBgTarget_RowBg0                       = 1,        // Set row background color 0 (generally used for background, automatically set when ImGuiTableFlags_RowBg is used)
            ImGuiTableBgTarget_RowBg1                       = 2,        // Set row background color 1 (generally used for selection marking)
            ImGuiTableBgTarget_CellBg                       = 3         // Set cell background color (top-most color)
        };

        // Flags for ImGui::IsWindowFocused()
        enum ImGuiFocusedFlags_
        {
            ImGuiFocusedFlags_None                          = 0,
            ImGuiFocusedFlags_ChildWindows                  = 1 << 0,   // IsWindowFocused(): Return true if any children of the window is focused
            ImGuiFocusedFlags_RootWindow                    = 1 << 1,   // IsWindowFocused(): Test from root window (top most parent of the current hierarchy)
            ImGuiFocusedFlags_AnyWindow                     = 1 << 2,   // IsWindowFocused(): Return true if any window is focused. Important: If you are trying to tell how to dispatch your low-level inputs, do NOT use this. Use 'io.WantCaptureMouse' instead! Please read the FAQ!
            ImGuiFocusedFlags_RootAndChildWindows           = ImGuiFocusedFlags_RootWindow | ImGuiFocusedFlags_ChildWindows
        };

        // Flags for ImGui::IsItemHovered(), ImGui::IsWindowHovered()
        // Note: if you are trying to check whether your mouse should be dispatched to Dear ImGui or to your app, you should use 'io.WantCaptureMouse' instead! Please read the FAQ!
        // Note: windows with the ImGuiWindowFlags_NoInputs flag are ignored by IsWindowHovered() calls.
        enum ImGuiHoveredFlags_
        {
            ImGuiHoveredFlags_None                          = 0,        // Return true if directly over the item/window, not obstructed by another window, not obstructed by an active popup or modal blocking inputs under them.
            ImGuiHoveredFlags_ChildWindows                  = 1 << 0,   // IsWindowHovered() only: Return true if any children of the window is hovered
            ImGuiHoveredFlags_RootWindow                    = 1 << 1,   // IsWindowHovered() only: Test from root window (top most parent of the current hierarchy)
            ImGuiHoveredFlags_AnyWindow                     = 1 << 2,   // IsWindowHovered() only: Return true if any window is hovered
            ImGuiHoveredFlags_AllowWhenBlockedByPopup       = 1 << 3,   // Return true even if a popup window is normally blocking access to this item/window
            //ImGuiHoveredFlags_AllowWhenBlockedByModal     = 1 << 4,   // Return true even if a modal popup window is normally blocking access to this item/window. FIXME-TODO: Unavailable yet.
            ImGuiHoveredFlags_AllowWhenBlockedByActiveItem  = 1 << 5,   // Return true even if an active item is blocking access to this item/window. Useful for Drag and Drop patterns.
            ImGuiHoveredFlags_AllowWhenOverlapped           = 1 << 6,   // Return true even if the position is obstructed or overlapped by another window
            ImGuiHoveredFlags_AllowWhenDisabled             = 1 << 7,   // Return true even if the item is disabled
            ImGuiHoveredFlags_RectOnly                      = ImGuiHoveredFlags_AllowWhenBlockedByPopup | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem | ImGuiHoveredFlags_AllowWhenOverlapped,
            ImGuiHoveredFlags_RootAndChildWindows           = ImGuiHoveredFlags_RootWindow | ImGuiHoveredFlags_ChildWindows
        };

        // Flags for ImGui::BeginDragDropSource(), ImGui::AcceptDragDropPayload()
        enum ImGuiDragDropFlags_
        {
            ImGuiDragDropFlags_None                         = 0,
            // BeginDragDropSource() flags
            ImGuiDragDropFlags_SourceNoPreviewTooltip       = 1 << 0,   // By default, a successful call to BeginDragDropSource opens a tooltip so you can display a preview or description of the source contents. This flag disable this behavior.
            ImGuiDragDropFlags_SourceNoDisableHover         = 1 << 1,   // By default, when dragging we clear data so that IsItemHovered() will return false, to avoid subsequent user code submitting tooltips. This flag disable this behavior so you can still call IsItemHovered() on the source item.
            ImGuiDragDropFlags_SourceNoHoldToOpenOthers     = 1 << 2,   // Disable the behavior that allows to open tree nodes and collapsing header by holding over them while dragging a source item.
            ImGuiDragDropFlags_SourceAllowNullID            = 1 << 3,   // Allow items such as Text(), Image() that have no unique identifier to be used as drag source, by manufacturing a temporary identifier based on their window-relative position. This is extremely unusual within the dear imgui ecosystem and so we made it explicit.
            ImGuiDragDropFlags_SourceExtern                 = 1 << 4,   // External source (from outside of dear imgui), won't attempt to read current item/window info. Will always return true. Only one Extern source can be active simultaneously.
            ImGuiDragDropFlags_SourceAutoExpirePayload      = 1 << 5,   // Automatically expire the payload if the source cease to be submitted (otherwise payloads are persisting while being dragged)
            // AcceptDragDropPayload() flags
            ImGuiDragDropFlags_AcceptBeforeDelivery         = 1 << 10,  // AcceptDragDropPayload() will returns true even before the mouse button is released. You can then call IsDelivery() to test if the payload needs to be delivered.
            ImGuiDragDropFlags_AcceptNoDrawDefaultRect      = 1 << 11,  // Do not draw the default highlight rectangle when hovering over target.
            ImGuiDragDropFlags_AcceptNoPreviewTooltip       = 1 << 12,  // Request hiding the BeginDragDropSource tooltip from the BeginDragDropTarget site.
            ImGuiDragDropFlags_AcceptPeekOnly               = ImGuiDragDropFlags_AcceptBeforeDelivery | ImGuiDragDropFlags_AcceptNoDrawDefaultRect  // For peeking ahead and inspecting the payload before delivery.
        };

        

        // Flags for ColorEdit3() / ColorEdit4() / ColorPicker3() / ColorPicker4() / ColorButton()
        enum ImGuiColorEditFlags_
        {
            ImGuiColorEditFlags_None            = 0,
            ImGuiColorEditFlags_NoAlpha         = 1 << 1,   //              // ColorEdit, ColorPicker, ColorButton: ignore Alpha component (will only read 3 components from the input pointer).
            ImGuiColorEditFlags_NoPicker        = 1 << 2,   //              // ColorEdit: disable picker when clicking on color square.
            ImGuiColorEditFlags_NoOptions       = 1 << 3,   //              // ColorEdit: disable toggling options menu when right-clicking on inputs/small preview.
            ImGuiColorEditFlags_NoSmallPreview  = 1 << 4,   //              // ColorEdit, ColorPicker: disable color square preview next to the inputs. (e.g. to show only the inputs)
            ImGuiColorEditFlags_NoInputs        = 1 << 5,   //              // ColorEdit, ColorPicker: disable inputs sliders/text widgets (e.g. to show only the small preview color square).
            ImGuiColorEditFlags_NoTooltip       = 1 << 6,   //              // ColorEdit, ColorPicker, ColorButton: disable tooltip when hovering the preview.
            ImGuiColorEditFlags_NoLabel         = 1 << 7,   //              // ColorEdit, ColorPicker: disable display of inline text label (the label is still forwarded to the tooltip and picker).
            ImGuiColorEditFlags_NoSidePreview   = 1 << 8,   //              // ColorPicker: disable bigger color preview on right side of the picker, use small color square preview instead.
            ImGuiColorEditFlags_NoDragDrop      = 1 << 9,   //              // ColorEdit: disable drag and drop target. ColorButton: disable drag and drop source.
            ImGuiColorEditFlags_NoBorder        = 1 << 10,  //              // ColorButton: disable border (which is enforced by default)

            // User Options (right-click on widget to change some of them).
            ImGuiColorEditFlags_AlphaBar        = 1 << 16,  //              // ColorEdit, ColorPicker: show vertical alpha bar/gradient in picker.
            ImGuiColorEditFlags_AlphaPreview    = 1 << 17,  //              // ColorEdit, ColorPicker, ColorButton: display preview as a transparent color over a checkerboard, instead of opaque.
            ImGuiColorEditFlags_AlphaPreviewHalf= 1 << 18,  //              // ColorEdit, ColorPicker, ColorButton: display half opaque / half checkerboard, instead of opaque.
            ImGuiColorEditFlags_HDR             = 1 << 19,  //              // (WIP) ColorEdit: Currently only disable 0.0f..1.0f limits in RGBA edition (note: you probably want to use ImGuiColorEditFlags_Float flag as well).
            ImGuiColorEditFlags_DisplayRGB      = 1 << 20,  // [Display]    // ColorEdit: override _display_ type among RGB/HSV/Hex. ColorPicker: select any combination using one or more of RGB/HSV/Hex.
            ImGuiColorEditFlags_DisplayHSV      = 1 << 21,  // [Display]    // "
            ImGuiColorEditFlags_DisplayHex      = 1 << 22,  // [Display]    // "
            ImGuiColorEditFlags_Uint8           = 1 << 23,  // [DataType]   // ColorEdit, ColorPicker, ColorButton: _display_ values formatted as 0..255.
            ImGuiColorEditFlags_Float           = 1 << 24,  // [DataType]   // ColorEdit, ColorPicker, ColorButton: _display_ values formatted as 0.0f..1.0f floats instead of 0..255 integers. No round-trip of value via integers.
            ImGuiColorEditFlags_PickerHueBar    = 1 << 25,  // [Picker]     // ColorPicker: bar for Hue, rectangle for Sat/Value.
            ImGuiColorEditFlags_PickerHueWheel  = 1 << 26,  // [Picker]     // ColorPicker: wheel for Hue, triangle for Sat/Value.
            ImGuiColorEditFlags_InputRGB        = 1 << 27,  // [Input]      // ColorEdit, ColorPicker: input and output data in RGB format.
            ImGuiColorEditFlags_InputHSV        = 1 << 28,  // [Input]      // ColorEdit, ColorPicker: input and output data in HSV format.

            // Defaults Options. You can set application defaults using SetColorEditOptions(). The intent is that you probably don't want to
            // override them in most of your calls. Let the user choose via the option menu and/or call SetColorEditOptions() once during startup.
            ImGuiColorEditFlags_DefaultOptions_ = ImGuiColorEditFlags_Uint8 | ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_InputRGB | ImGuiColorEditFlags_PickerHueBar,
        };

        // Flags for DragFloat(), DragInt(), SliderFloat(), SliderInt() etc.
        // We use the same sets of flags for DragXXX() and SliderXXX() functions as the features are the same and it makes it easier to swap them.
        enum ImGuiSliderFlags_
        {
            ImGuiSliderFlags_None                   = 0,
            ImGuiSliderFlags_AlwaysClamp            = 1 << 4,       // Clamp value to min/max bounds when input manually with CTRL+Click. By default CTRL+Click allows going out of bounds.
            ImGuiSliderFlags_Logarithmic            = 1 << 5,       // Make the widget logarithmic (linear otherwise). Consider using ImGuiSliderFlags_NoRoundToFormat with this if using a format-string with small amount of digits.
            ImGuiSliderFlags_NoRoundToFormat        = 1 << 6,       // Disable rounding underlying value to match precision of the display format string (e.g. %.3f values are rounded to those 3 digits)
            ImGuiSliderFlags_NoInput                = 1 << 7,       // Disable CTRL+Click or Enter key allowing to input text directly into the widget
        };

        // Enumeration for ImGui::SetWindow***(), SetNextWindow***(), SetNextItem***() functions
        // Represent a condition.
        // Important: Treat as a regular enum! Do NOT combine multiple values using binary operators! All the functions above treat 0 as a shortcut to ImGuiCond_Always.
        enum ImGuiCond_
        {
            ImGuiCond_None          = 0,        // No condition (always set the variable), same as _Always
            ImGuiCond_Always        = 1 << 0,   // No condition (always set the variable)
            ImGuiCond_Once          = 1 << 1,   // Set the variable once per runtime session (only the first call will succeed)
            ImGuiCond_FirstUseEver  = 1 << 2,   // Set the variable if the object/window has no persistently saved data (no entry in .ini file)
            ImGuiCond_Appearing     = 1 << 3    // Set the variable if the object/window is appearing after being hidden/inactive (or the first time)
        };
    }
}

namespace icy
{
    enum class imgui_widget_type : uint32_t
    {
        none,
        window,
        popup,
        modal,
        group,
        text,
        tab_bar,
        list_view,
        tree_view,
        combo_view,
        button,
        button_check,
        button_radio,
        button_arrow_min_x,
        button_arrow_max_x,
        button_arrow_min_y,
        button_arrow_max_y,
        menu,
        menu_bar,
        menu_context,
        main_menu_bar,
        drag_value,
        slider_value,
        line_input,
        text_input,
        color_edit,
        color_select,
        menu_item,
        tab_item,
        view_item,
        progress_bar,
        separator,
    };
    struct imgui_widget_flag_enum
    {
        enum : uint32_t
        {
            none            =   0,
            is_hidden       =   1   <<  0x00,
            is_same_line    =   1   <<  0x01,
            is_repeat       =   1   <<  0x02,
            is_bullet       =   1   <<  0x03,
            is_drag_source  =   1   <<  0x04,
            is_drop_target  =   1   <<  0x05,
            is_disabled     =   1   <<  0x06,
            any             =   0xFFFFFFFF,
        };
    };    
    using imgui_widget_flag = decltype(imgui_widget_flag_enum::none);

    struct imgui_window_flag_enum
    {
        enum : uint32_t
        {
            none                =   0,
    
            no_title_bar        =   1   <<  0x00,
            no_resize           =   1   <<  0x01,
            no_move             =   1   <<  0x02,
            no_scroll_bar       =   1   <<  0x03,
            no_scroll_mouse     =   1   <<  0x04,
            no_collapse         =   1   <<  0x05,
            always_auto_resize  =   1   <<  0x06,
            no_background       =   1   <<  0x07,
            no_mouse_inputs     =   1   <<  0x09,
            has_hscroll_bar     =   1   <<  0x0B,
            no_focus_on_appear  =   1   <<  0x0C,
            no_front_on_focus   =   1   <<  0x0D,
            always_vscroll_bar  =   1   <<  0x0E,
            always_hscroll_bar  =   1   <<  0x0F,
            always_use_padding  =   1   <<  0x10,
            no_nav_input        =   1   <<  0x12,
            no_nav_focus        =   1   <<  0x13,
            unsaved_document    =   1   <<  0x14,

            any                 =   0xFFFFFFF,
        };
    };
    using imgui_window_flag = decltype(imgui_window_flag_enum::none);
    inline imgui_window_flag operator|(const imgui_window_flag lhs, const imgui_window_flag rhs) noexcept
    {
        return imgui_window_flag(uint32_t(lhs) | uint32_t(rhs));
    }

    struct imgui_widget_state
    {
        imgui_widget_state(const uint32_t flags = imgui_widget_flag::any) noexcept : widget_flags(flags)
        {

        }
        imgui_widget_type widget_type = imgui_widget_type::none;
        uint32_t widget_flags = 0xFFFF'FFFF;
        uint32_t custom_flags = 0xFFFF'FFFF;
    };

    inline imgui_widget_state imgui_window_state(const imgui_window_flag window_flags, const imgui_widget_flag widget_flags) noexcept
    {
        imgui_widget_state state;
        state.widget_type = imgui_widget_type::window;
        state.widget_flags = widget_flags;
        state.custom_flags = window_flags;
        return state;
    }

    enum class imgui_event_type : uint32_t
    {
        none,
        display_render,
        window_close,
        widget_close,
        widget_edit,
        widget_click,
        drag_drop,
    };
    struct imgui_event
    {
        imgui_event_type type = imgui_event_type::none;
        uint32_t display = 0;
        uint32_t widget = 0;
        uint32_t query = 0;
        render_gui_frame render;
        gui_variant value;
        gui_variant udata;
    };
    struct imgui_system;
    struct imgui_display
    {
        virtual ~imgui_display() noexcept = 0
        {

        }
        virtual uint32_t index() const noexcept = 0;
        virtual shared_ptr<imgui_system> system() const noexcept = 0;
        virtual shared_ptr<icy::window> handle() const noexcept = 0;
        virtual error_type repaint(uint32_t& query) noexcept = 0;
        virtual error_type widget_create(const uint32_t parent, const imgui_widget_type type, uint32_t& widget) noexcept = 0;
        virtual error_type widget_delete(const uint32_t widget) noexcept = 0;
        virtual error_type widget_label(const uint32_t widget, const string_view text) noexcept = 0;
        virtual error_type widget_state(const uint32_t widget, const imgui_widget_state state) noexcept = 0;
        virtual error_type widget_value(const uint32_t widget, const gui_variant& var) noexcept = 0;
        virtual error_type widget_udata(const uint32_t widget, const gui_variant& var) noexcept = 0;
        virtual error_type clear() noexcept = 0;
    };
    struct imgui_system : public event_system
    {
        virtual const icy::thread& thread() const noexcept = 0;
        icy::thread& thread() noexcept
        {
            return const_cast<icy::thread&>(static_cast<const imgui_system*>(this)->thread());
        }
        virtual error_type create_display(shared_ptr<imgui_display>& display, shared_ptr<icy::window> handle) noexcept = 0;
    };
    error_type create_imgui_system(shared_ptr<imgui_system>& system) noexcept;
}
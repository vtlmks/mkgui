// Copyright (c) 2026 Peter Fors
// SPDX-License-Identifier: MIT

#define MKGUI_UNDO_COALESCE_MS 500

// [=]===^=[ input_undo_push ]==================================================================[=]
static void input_undo_push(struct mkgui_input_data *inp) {
	uint32_t now = (uint32_t)(mkgui_now_ns() / 1000000ull);

	if(inp->undo_count > 0 && inp->undo_pos == inp->undo_count && now - inp->undo_last_ms < MKGUI_UNDO_COALESCE_MS) {
		return;

	}

	uint32_t slot = (inp->undo_start + inp->undo_pos) % MKGUI_UNDO_MAX;
	struct mkgui_input_snap *snapshot = &inp->undo[slot];
	memcpy(snapshot->text, inp->text, sizeof(snapshot->text));
	snapshot->cursor = inp->cursor;
	snapshot->sel_start = inp->sel_start;
	snapshot->sel_end = inp->sel_end;
	inp->undo_count = inp->undo_pos;

	if(inp->undo_count == MKGUI_UNDO_MAX) {
		inp->undo_start = (inp->undo_start + 1) % MKGUI_UNDO_MAX;

	} else {
		++inp->undo_count;

	}

	inp->undo_pos = inp->undo_count;
	inp->undo_last_ms = now;

}

// [=]===^=[ input_undo_push_force ]============================================================[=]
static void input_undo_push_force(struct mkgui_input_data *inp) {
	inp->undo_last_ms = (uint32_t)(mkgui_now_ns() / 1000000ull) - MKGUI_UNDO_COALESCE_MS;
	input_undo_push(inp);

}

// [=]===^=[ input_undo_swap ]==================================================================[=]
static void input_undo_swap(struct mkgui_input_data *inp) {
	struct mkgui_input_snap *snapshot = &inp->undo[(inp->undo_start + inp->undo_pos) % MKGUI_UNDO_MAX];
	struct mkgui_input_snap current = {.cursor = inp->cursor, .sel_start = inp->sel_start, .sel_end = inp->sel_end};
	memcpy(current.text, inp->text, sizeof(current.text));
	memcpy(inp->text, snapshot->text, sizeof(inp->text));
	inp->cursor = snapshot->cursor;
	inp->sel_start = snapshot->sel_start;
	inp->sel_end = snapshot->sel_end;
	*snapshot = current;
	inp->undo_last_ms = (uint32_t)(mkgui_now_ns() / 1000000ull) - MKGUI_UNDO_COALESCE_MS;

}

// [=]===^=[ input_undo ]=======================================================================[=]
static uint32_t input_undo(struct mkgui_input_data *inp) {
	if(inp->undo_pos == 0) {
		return 0;

	}

	--inp->undo_pos;
	input_undo_swap(inp);
	return 1;

}

// [=]===^=[ input_redo ]=======================================================================[=]
static uint32_t input_redo(struct mkgui_input_data *inp) {
	if(inp->undo_pos == inp->undo_count) {
		return 0;

	}

	input_undo_swap(inp);
	++inp->undo_pos;
	return 1;

}

// [=]===^=[ textarea_undo_push ]===============================================================[=]
static uint32_t textarea_undo_push(struct mkgui_textarea_data *ta) {
	uint32_t now = (uint32_t)(mkgui_now_ns() / 1000000ull);

	if(ta->undo_count > 0 && ta->undo_pos == ta->undo_count && now - ta->undo_last_ms < MKGUI_UNDO_COALESCE_MS) {
		return 1;

	}

	char *text = malloc((size_t)ta->text_len + 1);

	if(!text) {
		return 0;

	}

	memcpy(text, ta->text, (size_t)ta->text_len + 1);

	for(uint32_t i = ta->undo_pos; i < ta->undo_count; ++i) {
		struct mkgui_textarea_snap *discarded = &ta->undo[(ta->undo_start + i) % MKGUI_UNDO_MAX];
		free(discarded->text);
		discarded->text = NULL;

	}

	struct mkgui_textarea_snap *snapshot = &ta->undo[(ta->undo_start + ta->undo_pos) % MKGUI_UNDO_MAX];
	free(snapshot->text);
	snapshot->text = text;
	snapshot->text_len = ta->text_len;
	snapshot->cursor = ta->cursor;
	snapshot->sel_start = ta->sel_start;
	snapshot->sel_end = ta->sel_end;
	ta->undo_count = ta->undo_pos;

	if(ta->undo_count == MKGUI_UNDO_MAX) {
		ta->undo_start = (ta->undo_start + 1) % MKGUI_UNDO_MAX;

	} else {
		++ta->undo_count;

	}

	ta->undo_pos = ta->undo_count;
	ta->undo_last_ms = now;
	return 1;

}

// [=]===^=[ textarea_undo_push_force ]=========================================================[=]
static uint32_t textarea_undo_push_force(struct mkgui_textarea_data *ta) {
	ta->undo_last_ms = (uint32_t)(mkgui_now_ns() / 1000000ull) - MKGUI_UNDO_COALESCE_MS;
	return textarea_undo_push(ta);

}

// [=]===^=[ textarea_undo_swap ]===============================================================[=]
static void textarea_undo_swap(struct mkgui_textarea_data *ta) {
	struct mkgui_textarea_snap *snapshot = &ta->undo[(ta->undo_start + ta->undo_pos) % MKGUI_UNDO_MAX];
	struct mkgui_textarea_snap current = {ta->text, ta->text_len, ta->cursor, ta->sel_start, ta->sel_end};
	ta->text = snapshot->text;
	ta->text_len = snapshot->text_len;
	ta->text_cap = snapshot->text_len + 1;
	ta->cursor = snapshot->cursor;
	ta->sel_start = snapshot->sel_start;
	ta->sel_end = snapshot->sel_end;
	*snapshot = current;
	ta->undo_last_ms = (uint32_t)(mkgui_now_ns() / 1000000ull) - MKGUI_UNDO_COALESCE_MS;

}

// [=]===^=[ textarea_undo ]====================================================================[=]
static uint32_t textarea_undo(struct mkgui_textarea_data *ta) {
	if(ta->undo_pos == 0) {
		return 0;

	}

	--ta->undo_pos;
	textarea_undo_swap(ta);
	return 1;

}

// [=]===^=[ textarea_redo ]====================================================================[=]
static uint32_t textarea_redo(struct mkgui_textarea_data *ta) {
	if(ta->undo_pos == ta->undo_count) {
		return 0;

	}

	textarea_undo_swap(ta);
	++ta->undo_pos;
	return 1;

}

// [=]===^=[ textarea_undo_free ]===============================================================[=]
static void textarea_undo_free(struct mkgui_textarea_data *ta) {
	for(uint32_t i = 0; i < MKGUI_UNDO_MAX; ++i) {
		free(ta->undo[i].text);
		ta->undo[i].text = NULL;

	}

	ta->undo_pos = 0;
	ta->undo_count = 0;
	ta->undo_start = 0;

}

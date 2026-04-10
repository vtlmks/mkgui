// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT

// [=]===^=[ accel_format_text ]==================================[=]
static void accel_format_text(uint32_t keymod, uint32_t keysym, char *buf, uint32_t buf_size) {
	buf[0] = '\0';
	uint32_t off = 0;
	if(keymod & MKGUI_MOD_CONTROL) {
		off += (uint32_t)snprintf(buf + off, buf_size - off, "Ctrl+");
	}

	if(keymod & MKGUI_MOD_ALT) {
		off += (uint32_t)snprintf(buf + off, buf_size - off, "Alt+");
	}

	if(keymod & MKGUI_MOD_SHIFT) {
		off += (uint32_t)snprintf(buf + off, buf_size - off, "Shift+");
	}

	if(keysym >= 'a' && keysym <= 'z') {
		snprintf(buf + off, buf_size - off, "%c", (char)(keysym - 32));
	} else if(keysym >= 'A' && keysym <= 'Z') {
		snprintf(buf + off, buf_size - off, "%c", (char)keysym);
	} else if(keysym >= '0' && keysym <= '9') {
		snprintf(buf + off, buf_size - off, "%c", (char)keysym);
	} else if(keysym == MKGUI_KEY_F1) {
		snprintf(buf + off, buf_size - off, "F1");
	} else if(keysym >= MKGUI_KEY_F1 && keysym <= MKGUI_KEY_F1 + 11) {
		snprintf(buf + off, buf_size - off, "F%u", keysym - MKGUI_KEY_F1 + 1);
	} else if(keysym == MKGUI_KEY_DELETE) {
		snprintf(buf + off, buf_size - off, "Del");
	} else if(keysym == MKGUI_KEY_HOME) {
		snprintf(buf + off, buf_size - off, "Home");
	} else if(keysym == MKGUI_KEY_END) {
		snprintf(buf + off, buf_size - off, "End");
	} else if(keysym == MKGUI_KEY_PAGE_UP) {
		snprintf(buf + off, buf_size - off, "PgUp");
	} else if(keysym == MKGUI_KEY_PAGE_DOWN) {
		snprintf(buf + off, buf_size - off, "PgDn");
	} else if(keysym == MKGUI_KEY_RETURN) {
		snprintf(buf + off, buf_size - off, "Enter");
	} else if(keysym == MKGUI_KEY_SPACE) {
		snprintf(buf + off, buf_size - off, "Space");
	} else if(keysym == MKGUI_KEY_BACKSPACE) {
		snprintf(buf + off, buf_size - off, "Backspace");
	} else if(keysym == MKGUI_KEY_ESCAPE) {
		snprintf(buf + off, buf_size - off, "Esc");
	} else if(keysym >= 0x20 && keysym <= 0x7e) {
		snprintf(buf + off, buf_size - off, "%c", (char)keysym);
	} else {
		snprintf(buf + off, buf_size - off, "0x%04x", keysym);
	}
}

// [=]===^=[ accel_find_for_widget ]==============================[=]
static struct mkgui_accel *accel_find_for_widget(struct mkgui_ctx *ctx, uint32_t id) {
	for(uint32_t i = 0; i < ctx->accel_count; ++i) {
		if(ctx->accels[i].id == id) {
			return &ctx->accels[i];
		}
	}
	return NULL;
}

// [=]===^=[ accel_dispatch ]=====================================[=]
static uint32_t accel_dispatch(struct mkgui_ctx *ctx, uint32_t keysym, uint32_t keymod, struct mkgui_event *ev) {
	uint32_t mod = keymod & (MKGUI_MOD_CONTROL | MKGUI_MOD_ALT | MKGUI_MOD_SHIFT);
	if(!mod) {
		return 0;
	}
	uint32_t ks_lower = keysym;
	if(ks_lower >= 'A' && ks_lower <= 'Z') {
		ks_lower += 32;
	}
	for(uint32_t i = 0; i < ctx->accel_count; ++i) {
		struct mkgui_accel *a = &ctx->accels[i];
		uint32_t need = a->keymod & (MKGUI_MOD_CONTROL | MKGUI_MOD_ALT | MKGUI_MOD_SHIFT);
		if(ks_lower == a->keysym && mod == need) {
			struct mkgui_widget *w = find_widget(ctx, a->id);
			if(w && w->type == MKGUI_MENUITEM) {
				ev->type = MKGUI_EVENT_MENU;
			} else {
				ev->type = MKGUI_EVENT_ACCEL;
			}
			ev->id = a->id;
			ev->keysym = keysym;
			ev->keymod = keymod;
			return 1;
		}
	}
	return 0;
}

// [=]===^=[ mkgui_accel_add ]====================================[=]
MKGUI_API void mkgui_accel_add(struct mkgui_ctx *ctx, uint32_t id, uint32_t keymod, uint32_t keysym) {
	MKGUI_CHECK(ctx);
	if(ctx->accel_count >= MKGUI_MAX_ACCELS) {
		return;
	}

	if(keysym >= 'A' && keysym <= 'Z') {
		keysym = keysym + 32;
	}
	struct mkgui_accel *a = &ctx->accels[ctx->accel_count++];
	a->keysym = keysym;
	a->keymod = keymod;
	a->id = id;
}

// [=]===^=[ mkgui_accel_remove ]=================================[=]
MKGUI_API void mkgui_accel_remove(struct mkgui_ctx *ctx, uint32_t id) {
	MKGUI_CHECK(ctx);
	for(uint32_t i = 0; i < ctx->accel_count; ) {
		if(ctx->accels[i].id == id) {
			ctx->accels[i] = ctx->accels[--ctx->accel_count];
		} else {
			++i;
		}
	}
}

// [=]===^=[ mkgui_accel_clear ]==================================[=]
MKGUI_API void mkgui_accel_clear(struct mkgui_ctx *ctx) {
	MKGUI_CHECK(ctx);
	ctx->accel_count = 0;
}

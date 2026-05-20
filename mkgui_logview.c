// Copyright (c) 2026, Peter Fors
// SPDX-License-Identifier: MIT

#define MKGUI_LOGVIEW_RUNS_PER_LINE   4
#define MKGUI_LOGVIEW_MAX_PENDING_RUN 256
#define MKGUI_LOGVIEW_DEFAULT_FG      0x00000000u
#define MKGUI_LOGVIEW_DEFAULT_BG      0x00000000u

// Standard "Tango" ANSI palette (0..7 normal, 8..15 bright)
static const uint32_t logview_ansi_palette[16] = {
	0xff000000u, 0xffcc0000u, 0xff4e9a06u, 0xffc4a000u,
	0xff3465a4u, 0xff75507bu, 0xff06989au, 0xffd3d7cfu,
	0xff555753u, 0xffef2929u, 0xff8ae234u, 0xfffce94fu,
	0xff729fcfu, 0xffad7fa8u, 0xff34e2e2u, 0xffeeeeecu,
};

// [=]===^=[ logview_ansi_256 ]==================================[=]
// Translate an xterm 256-color index into a 24-bit RGBA value.
static uint32_t logview_ansi_256(uint32_t idx) {
	if(idx < 16) {
		return logview_ansi_palette[idx];
	}

	if(idx >= 232) {
		uint32_t v = (idx - 232) * 10 + 8;
		return 0xff000000u | (v << 16) | (v << 8) | v;
	}
	idx -= 16;
	uint32_t r = idx / 36;
	uint32_t g = (idx / 6) % 6;
	uint32_t b = idx % 6;
	static const uint32_t steps[6] = { 0, 95, 135, 175, 215, 255 };
	return 0xff000000u | (steps[r] << 16) | (steps[g] << 8) | steps[b];
}

// [=]===^=[ logview_text_at ]====================================[=]
// Return a pointer to a contiguous copy of the source-line text in the
// caller-provided scratch buffer. Handles wrap-around inside the circular
// arena. Returns the scratch pointer if a copy was needed, or a direct arena
// pointer when the line is already contiguous.
static char *logview_text_at(struct mkgui_logview_data *lv, struct mkgui_logview_line *line, char *scratch, uint32_t scratch_size) {
	uint32_t len = line->len;
	if(len + 1 > scratch_size) {
		len = scratch_size - 1;
	}
	uint32_t phys = (uint32_t)(line->arena_off % lv->arena_bytes);
	if(phys + len <= lv->arena_bytes) {
		memcpy(scratch, &lv->text_arena[phys], len);
		scratch[len] = '\0';
		return scratch;
	}
	uint32_t first = lv->arena_bytes - phys;
	memcpy(scratch, &lv->text_arena[phys], first);
	memcpy(&scratch[first], &lv->text_arena[0], len - first);
	scratch[len] = '\0';
	return scratch;
}

// [=]===^=[ logview_get_line ]==================================[=]
// Return a pointer to the line in the ring identified by its sequence number,
// or NULL when the line has been evicted.
static struct mkgui_logview_line *logview_get_line(struct mkgui_logview_data *lv, uint64_t seq) {
	if(seq < lv->line_seq_tail || seq >= lv->line_seq_head) {
		return NULL;
	}
	return &lv->lines[seq % lv->max_lines];
}

// [=]===^=[ logview_get_run ]===================================[=]
static struct mkgui_logview_run *logview_get_run(struct mkgui_logview_data *lv, uint64_t off) {
	if(lv->max_runs == 0) {
		return NULL;
	}
	return &lv->runs[off % lv->max_runs];
}

// [=]===^=[ logview_invalidate_wrap ]===========================[=]
static void logview_invalidate_wrap(struct mkgui_logview_data *lv) {
	lv->cached_width = -1;
	lv->cached_row_h = -1;
	lv->vline_count = 0;
	lv->vline_seq_tail = lv->line_seq_tail;
}

// [=]===^=[ logview_evict_lines_to ]============================[=]
// Advance line_seq_tail until tail >= new_tail, freeing run-ring entries that
// belonged to the dropped lines.
static void logview_evict_lines_to(struct mkgui_logview_data *lv, uint64_t new_tail) {
	while(lv->line_seq_tail < new_tail && lv->line_seq_tail < lv->line_seq_head) {
		struct mkgui_logview_line *line = &lv->lines[lv->line_seq_tail % lv->max_lines];
		// Runs allocated to this line at the tail of the run ring are dropped
		// when we move past them; nothing to free, but if this line owned the
		// oldest runs, advance the run tail conceptually (we use modular math).
		(void)line;
		++lv->line_seq_tail;
	}

	if(lv->vline_seq_tail < lv->line_seq_tail) {
		uint32_t drop = 0;
		while(drop < lv->vline_count && lv->vlines[drop].line_seq < lv->line_seq_tail) {
			++drop;
		}

		if(drop > 0) {
			memmove(&lv->vlines[0], &lv->vlines[drop], (lv->vline_count - drop) * sizeof(struct mkgui_logview_vline));
			lv->vline_count -= drop;
		}
		lv->vline_seq_tail = lv->line_seq_tail;
	}

	if(lv->has_selection) {
		if(lv->sel_anchor_line_seq < lv->line_seq_tail || lv->sel_cursor_line_seq < lv->line_seq_tail) {
			lv->has_selection = 0;
		}
	}
}

// [=]===^=[ logview_make_room ]=================================[=]
// Make space in the circular text arena for `need` bytes by evicting lines
// whose data would be overwritten. Returns 0 if `need` exceeds the arena size.
static uint32_t logview_make_room(struct mkgui_logview_data *lv, uint32_t need) {
	if(need > lv->arena_bytes) {
		return 0;
	}
	uint64_t after = lv->arena_head + need;
	uint64_t cutoff = after > lv->arena_bytes ? (after - lv->arena_bytes) : 0;
	while(lv->line_seq_tail < lv->line_seq_head) {
		struct mkgui_logview_line *line = &lv->lines[lv->line_seq_tail % lv->max_lines];
		if(line->arena_off >= cutoff) {
			break;
		}
		++lv->line_seq_tail;
	}
	// vline cache and selection cleanup via the standard evict path.
	logview_evict_lines_to(lv, lv->line_seq_tail);
	return 1;
}

// [=]===^=[ logview_arena_write ]===============================[=]
// Append `len` bytes to the text arena, wrapping physically if the write
// crosses the arena end. Returns the monotonic offset where the bytes started.
static uint64_t logview_arena_write(struct mkgui_logview_data *lv, const char *src, uint32_t len) {
	uint64_t start = lv->arena_head;
	uint32_t phys = (uint32_t)(start % lv->arena_bytes);
	if(phys + len <= lv->arena_bytes) {
		memcpy(&lv->text_arena[phys], src, len);

	} else {
		uint32_t first = lv->arena_bytes - phys;
		memcpy(&lv->text_arena[phys], src, first);
		memcpy(&lv->text_arena[0], &src[first], len - first);
	}
	lv->arena_head += len;
	return start;
}

// [=]===^=[ logview_pending_push_char ]=========================[=]
static void logview_pending_push_char(struct mkgui_logview_data *lv, char c) {
	if(lv->pending_len + 1 >= lv->pending_cap) {
		uint32_t nc = lv->pending_cap == 0 ? 256 : lv->pending_cap * 2;
		char *nt = (char *)realloc(lv->pending_text, nc);
		if(!nt) {
			return;
		}
		lv->pending_text = nt;
		lv->pending_cap = nc;
	}
	lv->pending_text[lv->pending_len++] = c;
}

// [=]===^=[ logview_pending_push_run ]==========================[=]
// Push a style run for the current pending line if the most recent run uses
// different colors. Bounded by MKGUI_LOGVIEW_MAX_PENDING_RUN.
static void logview_pending_push_run(struct mkgui_logview_data *lv) {
	if(lv->pending_runs_count > 0) {
		struct mkgui_logview_run *prev = &lv->pending_runs[lv->pending_runs_count - 1];
		if(prev->fg == lv->cur_fg && prev->bg == lv->cur_bg && prev->start == lv->pending_len) {
			return;
		}

		if(prev->fg == lv->cur_fg && prev->bg == lv->cur_bg) {
			return;
		}
	}

	if(lv->pending_runs_count >= MKGUI_LOGVIEW_MAX_PENDING_RUN) {
		return;
	}

	if(lv->pending_runs_count >= lv->pending_runs_cap) {
		uint32_t nc = lv->pending_runs_cap == 0 ? 8 : lv->pending_runs_cap * 2;
		if(nc > MKGUI_LOGVIEW_MAX_PENDING_RUN) {
			nc = MKGUI_LOGVIEW_MAX_PENDING_RUN;
		}
		struct mkgui_logview_run *nr = (struct mkgui_logview_run *)realloc(lv->pending_runs, nc * sizeof(struct mkgui_logview_run));
		if(!nr) {
			return;
		}
		lv->pending_runs = nr;
		lv->pending_runs_cap = nc;
	}
	struct mkgui_logview_run *r = &lv->pending_runs[lv->pending_runs_count++];
	r->start = lv->pending_len;
	r->fg = lv->cur_fg;
	r->bg = lv->cur_bg;
}

// [=]===^=[ logview_apply_sgr ]=================================[=]
// Apply one SGR (Select Graphic Rendition) parameter list. Bytes in
// `params`/`plen` are the digits-and-semicolons between ESC[ and `m`.
static void logview_apply_sgr(struct mkgui_logview_data *lv, char *params, uint32_t plen) {
	uint32_t codes[32];
	uint32_t ncodes = 0;
	uint32_t cur = 0;
	uint32_t have_digit = 0;

	for(uint32_t i = 0; i <= plen; ++i) {
		if(i == plen || params[i] == ';') {
			if(ncodes < 32) {
				codes[ncodes++] = have_digit ? cur : 0;
			}
			cur = 0;
			have_digit = 0;

		} else if(params[i] >= '0' && params[i] <= '9') {
			cur = cur * 10 + (uint32_t)(params[i] - '0');
			have_digit = 1;
		}
	}

	if(ncodes == 0) {
		codes[ncodes++] = 0;
	}

	uint32_t i = 0;
	while(i < ncodes) {
		uint32_t c = codes[i];
		if(c == 0) {
			lv->cur_fg = MKGUI_LOGVIEW_DEFAULT_FG;
			lv->cur_bg = MKGUI_LOGVIEW_DEFAULT_BG;

		} else if(c == 1) {
			// Bold: bump fg to bright variant when it's a standard 30-37 color.
			for(uint32_t k = 0; k < 8; ++k) {
				if(lv->cur_fg == logview_ansi_palette[k]) {
					lv->cur_fg = logview_ansi_palette[k + 8];
					break;
				}
			}

		} else if(c == 22) {
			for(uint32_t k = 8; k < 16; ++k) {
				if(lv->cur_fg == logview_ansi_palette[k]) {
					lv->cur_fg = logview_ansi_palette[k - 8];
					break;
				}
			}

		} else if(c >= 30 && c <= 37) {
			lv->cur_fg = logview_ansi_palette[c - 30];

		} else if(c == 38 && i + 1 < ncodes) {
			if(codes[i + 1] == 5 && i + 2 < ncodes) {
				lv->cur_fg = logview_ansi_256(codes[i + 2]);
				i += 2;

			} else if(codes[i + 1] == 2 && i + 4 < ncodes) {
				lv->cur_fg = 0xff000000u | (codes[i + 2] << 16) | (codes[i + 3] << 8) | codes[i + 4];
				i += 4;
			}

		} else if(c == 39) {
			lv->cur_fg = MKGUI_LOGVIEW_DEFAULT_FG;

		} else if(c >= 40 && c <= 47) {
			lv->cur_bg = logview_ansi_palette[c - 40];

		} else if(c == 48 && i + 1 < ncodes) {
			if(codes[i + 1] == 5 && i + 2 < ncodes) {
				lv->cur_bg = logview_ansi_256(codes[i + 2]);
				i += 2;

			} else if(codes[i + 1] == 2 && i + 4 < ncodes) {
				lv->cur_bg = 0xff000000u | (codes[i + 2] << 16) | (codes[i + 3] << 8) | codes[i + 4];
				i += 4;
			}

		} else if(c == 49) {
			lv->cur_bg = MKGUI_LOGVIEW_DEFAULT_BG;

		} else if(c >= 90 && c <= 97) {
			lv->cur_fg = logview_ansi_palette[c - 90 + 8];

		} else if(c >= 100 && c <= 107) {
			lv->cur_bg = logview_ansi_palette[c - 100 + 8];
		}
		++i;
	}
}

// [=]===^=[ logview_commit_line ]===============================[=]
// Move the in-progress pending line into the line ring, copying text to the
// arena and runs to the runs ring. Resets pending state.
static void logview_commit_line(struct mkgui_logview_data *lv) {
	if(lv->max_lines == 0 || lv->arena_bytes == 0) {
		lv->pending_len = 0;
		lv->pending_runs_count = 0;
		return;
	}

	uint32_t len = lv->pending_len;
	if(len > lv->arena_bytes - 1) {
		len = lv->arena_bytes - 1;
	}

	if(!logview_make_room(lv, len)) {
		lv->pending_len = 0;
		lv->pending_runs_count = 0;
		return;
	}

	if(lv->line_seq_head - lv->line_seq_tail >= lv->max_lines) {
		logview_evict_lines_to(lv, lv->line_seq_tail + 1);
	}
	uint64_t arena_off = logview_arena_write(lv, lv->pending_text, len);

	uint32_t runs_count = lv->pending_runs_count;
	if(runs_count > lv->max_runs) {
		runs_count = lv->max_runs;
	}
	uint64_t runs_off = lv->runs_head;
	for(uint32_t i = 0; i < runs_count; ++i) {
		struct mkgui_logview_run *dst = &lv->runs[(lv->runs_head + i) % lv->max_runs];
		*dst = lv->pending_runs[i];
		if(dst->start > len) {
			dst->start = len;
		}
	}
	lv->runs_head += runs_count;

	uint32_t slot = (uint32_t)(lv->line_seq_head % lv->max_lines);
	lv->lines[slot].arena_off = arena_off;
	lv->lines[slot].len = len;
	lv->lines[slot].runs_off = runs_off;
	lv->lines[slot].runs_count = runs_count;
	++lv->line_seq_head;

	lv->pending_len = 0;
	lv->pending_runs_count = 0;
}

// [=]===^=[ logview_feed ]======================================[=]
// Stream `len` raw bytes through the ANSI parser, splitting on '\n' to commit
// finished lines. Carriage returns are dropped to keep \r\n input clean.
static void logview_feed(struct mkgui_logview_data *lv, const char *text, uint32_t len) {
	if(lv->pending_runs_count == 0) {
		logview_pending_push_run(lv);
	}

	for(uint32_t i = 0; i < len; ++i) {
		unsigned char c = (unsigned char)text[i];

		if(lv->esc_state == 1) {
			if(c == '[') {
				lv->esc_state = 2;
				lv->esc_len = 0;

			} else if(c == ']') {
				lv->esc_state = 3;
				lv->esc_len = 0;

			} else {
				lv->esc_state = 0;
			}
			continue;
		}

		if(lv->esc_state == 2) {
			if((c >= 0x40 && c <= 0x7e) || lv->esc_len >= sizeof(lv->esc_buf) - 1) {
				if(c == 'm') {
					lv->esc_buf[lv->esc_len] = '\0';
					logview_apply_sgr(lv, lv->esc_buf, lv->esc_len);
					logview_pending_push_run(lv);
				}
				lv->esc_state = 0;

			} else {
				lv->esc_buf[lv->esc_len++] = (char)c;
			}
			continue;
		}

		if(lv->esc_state == 3) {
			if(c == 0x07 || c == 0x1b) {
				lv->esc_state = 0;
			}
			continue;
		}

		if(c == 0x1b) {
			lv->esc_state = 1;
			continue;
		}

		if(c == '\r') {
			continue;
		}

		if(c == '\n') {
			logview_commit_line(lv);
			logview_pending_push_run(lv);
			continue;
		}

		if(c < 0x20 && c != '\t') {
			continue;
		}
		logview_pending_push_char(lv, (char)c);
	}
}

// [=]===^=[ logview_wrap_width ]================================[=]
// Inner content width available for text after subtracting borders, padding,
// and the vertical scrollbar.
static int32_t logview_wrap_width(struct mkgui_ctx *ctx, int32_t rw) {
	int32_t pad = sc(ctx, 4);
	return rw - 2 - pad * 2 - ctx->scrollbar_w;
}

// [=]===^=[ logview_vline_push ]================================[=]
static void logview_vline_push(struct mkgui_logview_data *lv, uint64_t seq, uint32_t off, uint32_t len) {
	if(lv->vline_count >= lv->vline_cap) {
		uint32_t nc = lv->vline_cap == 0 ? 64 : lv->vline_cap * 2;
		struct mkgui_logview_vline *nv = (struct mkgui_logview_vline *)realloc(lv->vlines, nc * sizeof(struct mkgui_logview_vline));
		if(!nv) {
			return;
		}
		lv->vlines = nv;
		lv->vline_cap = nc;
	}
	struct mkgui_logview_vline *v = &lv->vlines[lv->vline_count++];
	v->line_seq = seq;
	v->src_off = off;
	v->len = len;
}

// [=]===^=[ logview_wrap_line ]=================================[=]
// Append visual lines for one source line. When wrap is disabled, emits a
// single visual line spanning the whole source line.
static void logview_wrap_line(struct mkgui_ctx *ctx, struct mkgui_logview_data *lv, uint64_t seq, int32_t avail_w, uint32_t nowrap) {
	struct mkgui_logview_line *line = logview_get_line(lv, seq);
	if(!line) {
		return;
	}

	if(line->len == 0) {
		logview_vline_push(lv, seq, 0, 0);
		return;
	}

	if(nowrap || avail_w <= 0) {
		logview_vline_push(lv, seq, 0, line->len);
		return;
	}
	char scratch[2048];
	char *text = logview_text_at(lv, line, scratch, sizeof(scratch));
	uint32_t total = (uint32_t)strlen(text);
	uint32_t start = 0;

	while(start < total) {
		uint32_t lo = start;
		uint32_t hi = total;
		uint32_t fit = start;
		// Binary search the largest prefix that fits in avail_w. Linear walk
		// is acceptable too but binary search keeps wide lines cheap.
		while(lo < hi) {
			uint32_t mid = lo + (hi - lo + 1) / 2;
			char save = text[mid];
			text[mid] = '\0';
			int32_t w = text_width(ctx, &text[start]);
			text[mid] = save;
			if(w <= avail_w) {
				fit = mid;
				lo = mid;

			} else {
				hi = mid - 1;
			}
		}

		if(fit == start) {
			fit = start + 1;
		}

		if(fit < total) {
			uint32_t back = fit;
			while(back > start && text[back - 1] != ' ' && text[back - 1] != '\t') {
				--back;
			}

			if(back > start) {
				fit = back;
			}
		}
		logview_vline_push(lv, seq, start, fit - start);
		start = fit;
	}
}

// [=]===^=[ logview_rebuild_wrap ]==============================[=]
// Recompute the visual-line cache when the inner width or row height changed.
// Appended-but-not-yet-cached lines are also covered by this.
static void logview_rebuild_wrap(struct mkgui_ctx *ctx, struct mkgui_logview_data *lv, int32_t rw, uint32_t nowrap) {
	int32_t avail = nowrap ? 0 : logview_wrap_width(ctx, rw);
	if(lv->cached_width != avail || lv->cached_row_h != ctx->row_height || lv->vline_seq_tail != lv->line_seq_tail) {
		lv->vline_count = 0;
		lv->vline_seq_tail = lv->line_seq_tail;
		for(uint64_t s = lv->line_seq_tail; s < lv->line_seq_head; ++s) {
			logview_wrap_line(ctx, lv, s, avail, nowrap);
		}
		lv->cached_width = avail;
		lv->cached_row_h = ctx->row_height;
		return;
	}
	uint64_t last_cached = lv->vline_count > 0 ? lv->vlines[lv->vline_count - 1].line_seq : (lv->line_seq_tail - 1);
	uint64_t start_seq = lv->vline_count > 0 ? last_cached + 1 : lv->line_seq_tail;
	for(uint64_t s = start_seq; s < lv->line_seq_head; ++s) {
		logview_wrap_line(ctx, lv, s, avail, nowrap);
	}
}

// [=]===^=[ logview_total_content_h ]===========================[=]
static int32_t logview_total_content_h(struct mkgui_ctx *ctx, struct mkgui_logview_data *lv) {
	return (int32_t)lv->vline_count * ctx->row_height;
}

// [=]===^=[ logview_clamp_scroll ]==============================[=]
static void logview_clamp_scroll(struct mkgui_logview_data *lv, int32_t content_h, int32_t view_h) {
	int32_t max_scroll = content_h - view_h;
	if(max_scroll < 0) {
		max_scroll = 0;
	}

	if(lv->scroll_y < 0) {
		lv->scroll_y = 0;
	}

	if(lv->scroll_y > max_scroll) {
		lv->scroll_y = max_scroll;
	}
}

// [=]===^=[ logview_apply_stick ]===============================[=]
// Snap scroll to the bottom when the user was already there before an append.
static void logview_apply_stick(struct mkgui_ctx *ctx, struct mkgui_logview_data *lv, int32_t view_h) {
	if(lv->stuck_to_end) {
		int32_t content_h = logview_total_content_h(ctx, lv);
		int32_t max_scroll = content_h - view_h;
		lv->scroll_y = max_scroll < 0 ? 0 : max_scroll;
	}
}

// [=]===^=[ logview_recompute ]=================================[=]
// Wrap cache, follow-bottom snap, scroll clamp. The stick_to_end flag is
// authoritatively maintained by user-scroll handlers, so applying it on every
// recompute is safe: it's a no-op when the flag is 0, and snaps to bottom
// when 1 -- which is exactly what we want both after appends and on resize.
static void logview_recompute(struct mkgui_ctx *ctx, struct mkgui_logview_data *lv, int32_t idx) {
	int32_t rw = ctx->rects[idx].w;
	int32_t rh = ctx->rects[idx].h;
	uint32_t nowrap = (ctx->widgets[idx].style & MKGUI_LOGVIEW_NOWRAP) ? 1 : 0;
	logview_rebuild_wrap(ctx, lv, rw, nowrap);
	int32_t content_h = logview_total_content_h(ctx, lv);
	int32_t view_h = rh - 2;
	logview_apply_stick(ctx, lv, view_h);
	logview_clamp_scroll(lv, content_h, view_h);
}

// [=]===^=[ logview_first_visible_vline ]=======================[=]
static uint32_t logview_first_visible_vline(struct mkgui_logview_data *lv, int32_t row_height) {
	if(lv->scroll_y <= 0 || row_height <= 0) {
		return 0;
	}
	uint32_t first = (uint32_t)(lv->scroll_y / row_height);
	if(first >= lv->vline_count) {
		first = lv->vline_count;
	}
	return first;
}

// [=]===^=[ logview_hit_vline ]=================================[=]
// Translate a (mouse_x, mouse_y) coord to a visual-line index + byte offset
// inside that visual line.
static void logview_hit_vline(struct mkgui_ctx *ctx, struct mkgui_logview_data *lv, int32_t rx, int32_t ry, int32_t my, int32_t mx, uint32_t *out_vline, uint32_t *out_off) {
	*out_vline = 0;
	*out_off = 0;

	if(lv->vline_count == 0) {
		return;
	}
	int32_t local_y = my - (ry + 1) + lv->scroll_y;
	int32_t row_height = ctx->row_height;
	int32_t hit = local_y / row_height;

	if(hit < 0) {
		hit = 0;
	}
	uint32_t v = (uint32_t)hit;
	if(v >= lv->vline_count) {
		v = lv->vline_count - 1;
		*out_vline = v;
		struct mkgui_logview_line *line = logview_get_line(lv, lv->vlines[v].line_seq);
		*out_off = line ? lv->vlines[v].src_off + lv->vlines[v].len : 0;
		return;
	}
	*out_vline = v;
	struct mkgui_logview_vline *vl = &lv->vlines[v];
	struct mkgui_logview_line *line = logview_get_line(lv, vl->line_seq);
	if(!line) {
		return;
	}
	char scratch[2048];
	char *src = logview_text_at(lv, line, scratch, sizeof(scratch));
	int32_t text_pad = sc(ctx, 4);
	int32_t base_x = rx + text_pad - lv->scroll_x;
	uint32_t off = vl->src_off;
	uint32_t end = off + vl->len;

	if(end > line->len) {
		end = line->len;
	}
	uint32_t best = off;
	int32_t best_x = base_x;
	for(uint32_t j = off; j <= end; ++j) {
		char save = src[j];
		src[j] = '\0';
		int32_t w = text_width(ctx, &src[off]);
		src[j] = save;
		int32_t cx = base_x + w;

		if(cx > mx) {
			int32_t prev_w = best_x;
			if(mx - prev_w < cx - mx) {
				*out_off = best;

			} else {
				*out_off = j;
			}
			return;
		}
		best = j;
		best_x = cx;
	}
	*out_off = end;
}

// [=]===^=[ logview_has_scrollbar ]==============================[=]
static uint32_t logview_has_scrollbar(struct mkgui_ctx *ctx, uint32_t idx, struct mkgui_logview_data *lv) {
	int32_t rh = ctx->rects[idx].h;
	int32_t content_h = logview_total_content_h(ctx, lv);
	return content_h > rh - 2;
}

// [=]===^=[ logview_sb_hit ]=====================================[=]
static int32_t logview_sb_hit(struct mkgui_ctx *ctx, uint32_t idx, struct mkgui_logview_data *lv, int32_t mx, int32_t my) {
	int32_t rx = ctx->rects[idx].x;
	int32_t ry = ctx->rects[idx].y;
	int32_t rw = ctx->rects[idx].w;
	int32_t rh = ctx->rects[idx].h;
	int32_t sb_x = rx + rw - ctx->scrollbar_w;
	if(mx < sb_x || mx >= rx + rw) {
		return -1;
	}
	int32_t content_h = logview_total_content_h(ctx, lv);
	int32_t view_h = rh - 2;
	if(content_h <= view_h) {
		return -1;
	}
	int32_t min_thumb = sc(ctx, 20);
	int32_t thumb_h = (int32_t)((int64_t)view_h * view_h / content_h);

	if(thumb_h < min_thumb) {
		thumb_h = min_thumb;
	}
	int32_t max_scroll = content_h - view_h;
	int32_t thumb_y = ry + 1 + (int32_t)((int64_t)lv->scroll_y * (view_h - thumb_h) / max_scroll);
	return my - thumb_y;
}

// [=]===^=[ logview_sb_drag ]====================================[=]
static void logview_sb_drag(struct mkgui_ctx *ctx, uint32_t idx, struct mkgui_logview_data *lv, int32_t my, int32_t offset) {
	int32_t ry = ctx->rects[idx].y;
	int32_t rh = ctx->rects[idx].h;
	int32_t content_h = logview_total_content_h(ctx, lv);
	int32_t view_h = rh - 2;
	if(content_h <= view_h) {
		return;
	}
	int32_t min_thumb = sc(ctx, 20);
	int32_t thumb_h = (int32_t)((int64_t)view_h * view_h / content_h);

	if(thumb_h < min_thumb) {
		thumb_h = min_thumb;
	}
	int32_t max_scroll = content_h - view_h;
	int32_t track = view_h - thumb_h;
	if(track <= 0) {
		return;
	}
	int32_t thumb_pos = my - offset - (ry + 1);
	lv->scroll_y = (int32_t)((int64_t)thumb_pos * max_scroll / track);
	logview_clamp_scroll(lv, content_h, view_h);
	lv->stuck_to_end = (lv->scroll_y >= max_scroll) ? 1 : 0;
	dirty_widget(ctx, idx);
}

// [=]===^=[ logview_sel_range ]==================================[=]
// Sort the selection endpoints into (low, high) order. Returns 0 if there is
// no active selection.
static uint32_t logview_sel_range(struct mkgui_logview_data *lv, uint64_t *lo_seq, uint32_t *lo_off, uint64_t *hi_seq, uint32_t *hi_off) {
	if(!lv->has_selection) {
		return 0;
	}
	uint64_t aseq = lv->sel_anchor_line_seq;
	uint32_t aoff = lv->sel_anchor_off;
	uint64_t cseq = lv->sel_cursor_line_seq;
	uint32_t coff = lv->sel_cursor_off;

	if(aseq < cseq || (aseq == cseq && aoff <= coff)) {
		*lo_seq = aseq;
		*lo_off = aoff;
		*hi_seq = cseq;
		*hi_off = coff;

	} else {
		*lo_seq = cseq;
		*lo_off = coff;
		*hi_seq = aseq;
		*hi_off = aoff;
	}
	return *lo_seq != *hi_seq || *lo_off != *hi_off;
}

// [=]===^=[ logview_vline_to_pos ]==============================[=]
// Convert (vline index, byte offset within that visual line) to a source-line
// (seq, byte offset within source line). Out-of-range vline maps to "after the
// last line", which selection code uses as an open-ended endpoint.
static void logview_vline_to_pos(struct mkgui_logview_data *lv, uint32_t vidx, uint32_t off, uint64_t *out_seq, uint32_t *out_off) {
	if(vidx >= lv->vline_count) {
		if(lv->line_seq_head > 0) {
			struct mkgui_logview_line *last = logview_get_line(lv, lv->line_seq_head - 1);
			*out_seq = lv->line_seq_head - 1;
			*out_off = last ? last->len : 0;

		} else {
			*out_seq = 0;
			*out_off = 0;
		}
		return;
	}
	struct mkgui_logview_vline *vl = &lv->vlines[vidx];
	*out_seq = vl->line_seq;
	*out_off = vl->src_off + off;
}

// [=]===^=[ render_logview ]=====================================[=]
static void render_logview(struct mkgui_ctx *ctx, uint32_t idx) {
	struct mkgui_widget *w = &ctx->widgets[idx];
	int32_t rx = ctx->rects[idx].x;
	int32_t ry = ctx->rects[idx].y;
	int32_t rw = ctx->rects[idx].w;
	int32_t rh = ctx->rects[idx].h;

	uint32_t disabled = (w->flags & MKGUI_DISABLED);
	uint32_t focused = (ctx->focus_id == w->id);
	uint32_t input_bg = disabled_blend(ctx->theme.input_bg, ctx->theme.bg, disabled);
	uint32_t border_color = disabled_blend(focused ? ctx->theme.highlight : ctx->theme.widget_border, ctx->theme.bg, disabled);
	draw_patch(ctx, MKGUI_STYLE_SUNKEN, rx, ry, rw, rh, input_bg, border_color);

	struct mkgui_logview_data *lv = find_logview_data(ctx, w->id);
	if(!lv || lv->max_lines == 0) {
		return;
	}
	logview_recompute(ctx, lv, (int32_t)idx);

	int32_t clip_top = ry + 1;
	int32_t clip_bottom = ry + rh - 1;
	int32_t clip_left = rx + 1;
	int32_t clip_right = rx + rw - 1 - (logview_has_scrollbar(ctx, idx, lv) ? ctx->scrollbar_w : 0);

	uint32_t default_fg = disabled ? ctx->theme.text_disabled : ctx->theme.text;
	int32_t text_pad = sc(ctx, 4);

	uint64_t sel_lo_seq = 0, sel_hi_seq = 0;
	uint32_t sel_lo_off = 0, sel_hi_off = 0;
	uint32_t has_sel = logview_sel_range(lv, &sel_lo_seq, &sel_lo_off, &sel_hi_seq, &sel_hi_off);

	uint32_t first = logview_first_visible_vline(lv, ctx->row_height);
	int32_t y = ry + 1 + (int32_t)first * ctx->row_height - lv->scroll_y;

	for(uint32_t vi = first; vi < lv->vline_count && y < clip_bottom; ++vi) {
		struct mkgui_logview_vline *vl = &lv->vlines[vi];
		struct mkgui_logview_line *line = logview_get_line(lv, vl->line_seq);

		if(!line) {
			y += ctx->row_height;
			continue;
		}
		char scratch[2048];
		char *src = logview_text_at(lv, line, scratch, sizeof(scratch));
		int32_t ty = y + (ctx->row_height - ctx->font_height) / 2;
		int32_t base_x = rx + text_pad - lv->scroll_x;

		uint32_t voff = vl->src_off;
		uint32_t vend = voff + vl->len;
		if(vend > line->len) {
			vend = line->len;
		}

		// Selection rectangle for this visual line. Selection coordinates are
		// in source-line space, intersect them with this visual line slice.
		if(has_sel && vl->line_seq >= sel_lo_seq && vl->line_seq <= sel_hi_seq) {
			uint32_t s0 = voff;
			uint32_t s1 = vend;

			if(vl->line_seq == sel_lo_seq) {
				if(sel_lo_off > s0) {
					s0 = sel_lo_off;
				}
			}

			if(vl->line_seq == sel_hi_seq) {
				if(sel_hi_off < s1) {
					s1 = sel_hi_off;
				}
			}

			if(s0 < s1 || (vl->line_seq < sel_hi_seq && s0 == vend)) {
				char save_a = src[s0];
				src[s0] = '\0';
				int32_t sx1 = base_x + text_width(ctx, &src[voff]);
				src[s0] = save_a;
				int32_t sx2;
				if(vl->line_seq < sel_hi_seq && s1 == vend) {
					sx2 = clip_right;

				} else {
					char save_b = src[s1];
					src[s1] = '\0';
					sx2 = base_x + text_width(ctx, &src[voff]);
					src[s1] = save_b;
				}
				int32_t cx1 = sx1 < clip_left ? clip_left : sx1;
				int32_t cx2 = sx2 > clip_right ? clip_right : sx2;
				if(cx2 > cx1) {
					draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, cx1, y, cx2 - cx1, ctx->row_height, ctx->theme.selection);
				}
			}
		}

		// Background spans first (so text overlays them).
		if(line->runs_count > 0) {
			for(uint32_t r = 0; r < line->runs_count; ++r) {
				struct mkgui_logview_run *run = logview_get_run(lv, line->runs_off + r);
				if(!run || run->bg == 0) {
					continue;
				}
				uint32_t rs = run->start;
				uint32_t re = (r + 1 < line->runs_count) ? logview_get_run(lv, line->runs_off + r + 1)->start : line->len;
				if(re <= voff || rs >= vend) {
					continue;
				}

				if(rs < voff) {
					rs = voff;
				}

				if(re > vend) {
					re = vend;
				}
				char save_a = src[rs];
				src[rs] = '\0';
				int32_t bx1 = base_x + text_width(ctx, &src[voff]);
				src[rs] = save_a;
				char save_b = src[re];
				src[re] = '\0';
				int32_t bx2 = base_x + text_width(ctx, &src[voff]);
				src[re] = save_b;
				int32_t cx1 = bx1 < clip_left ? clip_left : bx1;
				int32_t cx2 = bx2 > clip_right ? clip_right : bx2;
				if(cx2 > cx1) {
					draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, cx1, y, cx2 - cx1, ctx->row_height, run->bg);
				}
			}
		}

		// Foreground text by run, falling back to default color when no runs.
		if(ty >= clip_top && ty + ctx->font_height <= clip_bottom) {
			if(line->runs_count == 0) {
				char tmp[2048];
				uint32_t len = vend - voff;
				if(len >= sizeof(tmp)) {
					len = sizeof(tmp) - 1;
				}
				memcpy(tmp, &src[voff], len);
				tmp[len] = '\0';
				push_text_clip(base_x, ty, tmp, default_fg, clip_left, clip_top, clip_right, clip_bottom);

			} else {
				for(uint32_t r = 0; r < line->runs_count; ++r) {
					struct mkgui_logview_run *run = logview_get_run(lv, line->runs_off + r);
					if(!run) {
						continue;
					}
					uint32_t rs = run->start;
					uint32_t re = (r + 1 < line->runs_count) ? logview_get_run(lv, line->runs_off + r + 1)->start : line->len;

					if(re <= voff || rs >= vend) {
						continue;
					}

					if(rs < voff) {
						rs = voff;
					}

					if(re > vend) {
						re = vend;
					}
					char save_a = src[rs];
					src[rs] = '\0';
					int32_t tx = base_x + text_width(ctx, &src[voff]);
					src[rs] = save_a;
					char tmp[2048];
					uint32_t len = re - rs;

					if(len >= sizeof(tmp)) {
						len = sizeof(tmp) - 1;
					}
					memcpy(tmp, &src[rs], len);
					tmp[len] = '\0';
					uint32_t fg = run->fg == 0 ? default_fg : run->fg;
					push_text_clip(tx, ty, tmp, fg, clip_left, clip_top, clip_right, clip_bottom);
				}
			}
		}
		y += ctx->row_height;
	}

	if(logview_has_scrollbar(ctx, idx, lv)) {
		int32_t sb_x = rx + rw - ctx->scrollbar_w;
		draw_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, sb_x, ry + 1, ctx->scrollbar_w - 1, rh - 2, ctx->theme.scrollbar_bg);
		int32_t content_h = logview_total_content_h(ctx, lv);
		int32_t view_h = rh - 2;
		int32_t min_thumb = sc(ctx, 20);
		int32_t sb_inset = sc(ctx, 2);
		int32_t thumb_h = (int32_t)((int64_t)view_h * view_h / content_h);

		if(thumb_h < min_thumb) {
			thumb_h = min_thumb;
		}
		int32_t thumb_y = ry + 1 + (int32_t)((int64_t)lv->scroll_y * (view_h - thumb_h) / (content_h - view_h));
		draw_rounded_rect_fill(ctx->pixels, ctx->win_w, ctx->win_h, sb_x + sb_inset, thumb_y, ctx->scrollbar_w - sb_inset * 2 - 1, thumb_h, ctx->theme.scrollbar_thumb, ctx->theme.corner_radius);
	}
}

// [=]===^=[ logview_free_buffers ]===============================[=]
// Drop all heap allocations belonging to one logview. Called from clear and
// from reconfigure (setup with different sizes).
static void logview_free_buffers(struct mkgui_logview_data *lv) {
	free(lv->text_arena);
	free(lv->lines);
	free(lv->runs);
	free(lv->pending_text);
	free(lv->pending_runs);
	free(lv->vlines);
	lv->text_arena = NULL;
	lv->lines = NULL;
	lv->runs = NULL;
	lv->pending_text = NULL;
	lv->pending_runs = NULL;
	lv->vlines = NULL;
	lv->pending_cap = 0;
	lv->pending_runs_cap = 0;
	lv->vline_cap = 0;
}

// [=]===^=[ mkgui_logview_setup ]================================[=]
MKGUI_API void mkgui_logview_setup(struct mkgui_ctx *ctx, uint32_t id, uint32_t max_lines, uint32_t arena_bytes) {
	MKGUI_CHECK(ctx);
	struct mkgui_logview_data *lv = find_logview_data(ctx, id);
	if(!lv) {
		return;
	}

	if(max_lines == 0 || arena_bytes == 0) {
		return;
	}
	logview_free_buffers(lv);
	lv->max_lines = max_lines;
	lv->arena_bytes = arena_bytes;
	lv->max_runs = max_lines * MKGUI_LOGVIEW_RUNS_PER_LINE;
	if(lv->max_runs < 64) {
		lv->max_runs = 64;
	}
	lv->text_arena = (char *)calloc(1, arena_bytes);
	lv->lines = (struct mkgui_logview_line *)calloc(max_lines, sizeof(struct mkgui_logview_line));
	lv->runs = (struct mkgui_logview_run *)calloc(lv->max_runs, sizeof(struct mkgui_logview_run));
	lv->arena_head = 0;
	lv->line_seq_head = 0;
	lv->line_seq_tail = 0;
	lv->runs_head = 0;
	lv->cur_fg = MKGUI_LOGVIEW_DEFAULT_FG;
	lv->cur_bg = MKGUI_LOGVIEW_DEFAULT_BG;
	lv->esc_state = 0;
	lv->esc_len = 0;
	lv->pending_len = 0;
	lv->pending_runs_count = 0;
	lv->vline_count = 0;
	lv->vline_seq_tail = 0;
	lv->cached_width = -1;
	lv->cached_row_h = -1;
	lv->scroll_y = 0;
	lv->scroll_x = 0;
	lv->stuck_to_end = 1;
	lv->has_selection = 0;
	dirty_widget_id(ctx, id);
}

// [=]===^=[ mkgui_logview_append_n ]=============================[=]
MKGUI_API void mkgui_logview_append_n(struct mkgui_ctx *ctx, uint32_t id, const char *text, uint32_t len) {
	MKGUI_CHECK(ctx);
	if(!text || len == 0) {
		return;
	}
	struct mkgui_logview_data *lv = find_logview_data(ctx, id);
	if(!lv || lv->max_lines == 0) {
		return;
	}
	logview_feed(lv, text, len);
	logview_invalidate_wrap(lv);
	dirty_widget_id(ctx, id);
}

// [=]===^=[ mkgui_logview_append ]===============================[=]
MKGUI_API void mkgui_logview_append(struct mkgui_ctx *ctx, uint32_t id, const char *text) {
	MKGUI_CHECK(ctx);
	if(!text) {
		return;
	}
	mkgui_logview_append_n(ctx, id, text, (uint32_t)strlen(text));
}

// [=]===^=[ mkgui_logview_clear ]================================[=]
MKGUI_API void mkgui_logview_clear(struct mkgui_ctx *ctx, uint32_t id) {
	MKGUI_CHECK(ctx);
	struct mkgui_logview_data *lv = find_logview_data(ctx, id);
	if(!lv) {
		return;
	}
	lv->line_seq_head = 0;
	lv->line_seq_tail = 0;
	lv->runs_head = 0;
	lv->arena_head = 0;
	lv->pending_len = 0;
	lv->pending_runs_count = 0;
	lv->vline_count = 0;
	lv->vline_seq_tail = 0;
	lv->cached_width = -1;
	lv->cached_row_h = -1;
	lv->scroll_y = 0;
	lv->scroll_x = 0;
	lv->stuck_to_end = 1;
	lv->has_selection = 0;
	lv->esc_state = 0;
	lv->cur_fg = MKGUI_LOGVIEW_DEFAULT_FG;
	lv->cur_bg = MKGUI_LOGVIEW_DEFAULT_BG;
	dirty_widget_id(ctx, id);
}

// [=]===^=[ mkgui_logview_get_line_count ]=======================[=]
MKGUI_API uint32_t mkgui_logview_get_line_count(struct mkgui_ctx *ctx, uint32_t id) {
	MKGUI_CHECK_VAL(ctx, 0);
	struct mkgui_logview_data *lv = find_logview_data(ctx, id);
	if(!lv) {
		return 0;
	}
	return (uint32_t)(lv->line_seq_head - lv->line_seq_tail);
}

// [=]===^=[ mkgui_logview_scroll_to_end ]========================[=]
MKGUI_API void mkgui_logview_scroll_to_end(struct mkgui_ctx *ctx, uint32_t id) {
	MKGUI_CHECK(ctx);
	struct mkgui_logview_data *lv = find_logview_data(ctx, id);
	if(!lv) {
		return;
	}
	lv->stuck_to_end = 1;
	int32_t idx = find_widget_idx(ctx, id);
	if(idx < 0) {
		return;
	}
	logview_recompute(ctx, lv, idx);
	dirty_widget_id(ctx, id);
}

// [=]===^=[ mkgui_logview_is_at_end ]=============================[=]
MKGUI_API uint32_t mkgui_logview_is_at_end(struct mkgui_ctx *ctx, uint32_t id) {
	MKGUI_CHECK_VAL(ctx, 1);
	struct mkgui_logview_data *lv = find_logview_data(ctx, id);
	if(!lv) {
		return 1;
	}
	return lv->stuck_to_end;
}

// [=]===^=[ mkgui_logview_get_selection_text ]====================[=]
MKGUI_API uint32_t mkgui_logview_get_selection_text(struct mkgui_ctx *ctx, uint32_t id, char *out, uint32_t out_size) {
	MKGUI_CHECK_VAL(ctx, 0);
	if(!out || out_size == 0) {
		return 0;
	}
	out[0] = '\0';
	struct mkgui_logview_data *lv = find_logview_data(ctx, id);
	if(!lv) {
		return 0;
	}
	uint64_t lo_seq, hi_seq;
	uint32_t lo_off, hi_off;
	if(!logview_sel_range(lv, &lo_seq, &lo_off, &hi_seq, &hi_off)) {
		return 0;
	}
	uint32_t written = 0;
	for(uint64_t s = lo_seq; s <= hi_seq && written < out_size - 1; ++s) {
		struct mkgui_logview_line *line = logview_get_line(lv, s);
		if(!line) {
			continue;
		}
		char scratch[2048];
		char *src = logview_text_at(lv, line, scratch, sizeof(scratch));
		uint32_t start = (s == lo_seq) ? lo_off : 0;
		uint32_t end = (s == hi_seq) ? hi_off : line->len;

		if(start > line->len) {
			start = line->len;
		}

		if(end > line->len) {
			end = line->len;
		}
		uint32_t want = end - start;
		uint32_t avail = out_size - 1 - written;
		if(want > avail) {
			want = avail;
		}
		memcpy(&out[written], &src[start], want);
		written += want;

		if(s < hi_seq && written < out_size - 1) {
			out[written++] = '\n';
		}
	}
	out[written] = '\0';
	return written;
}

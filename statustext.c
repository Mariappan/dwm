
int
drw_statustext(Drw *drw, int x, int y, unsigned int w, unsigned int h, const char *text, int invert) {
	char buf[1024];
	int tx, ty, th;
	Extnts tex;
	Colormap cmap;
	Visual *vis;
	XftDraw *d = NULL;
	Fnt *curfont, *nextfont;
	size_t i, len;
	int utf8strlen, utf8charlen, render;
	long utf8codepoint = 0;
	const char *utf8str;
	FcCharSet *fccharset;
	FcPattern *fcpattern;
	FcPattern *match;
	XftResult result;
	int charexists = 0;

	if (!(render = x || y || w || h)) {
		w = ~w;
	}

	if (!drw || !drw->scheme) {
		return 0;
	} else if (render) {
		XSetForeground(drw->dpy, drw->gc, invert ? drw->scheme->fg->pix : drw->scheme->bg->pix);
        if (tabtxt == 1 && drw->tabdrawable)
            XFillRectangle(drw->dpy, drw->tabdrawable, drw->gc, x, y, w, h);
        else
            XFillRectangle(drw->dpy, drw->drawable, drw->gc, x, y, w, h);
	}

	if (!text || !drw->fontcount) {
		return 0;
	} else if (render) {
		cmap = DefaultColormap(drw->dpy, drw->screen);
		vis = DefaultVisual(drw->dpy, drw->screen);
        if (tabtxt == 1 && drw->tabdrawable)
            d = XftDrawCreate(drw->dpy, drw->tabdrawable, vis, cmap);
        else
            d = XftDrawCreate(drw->dpy, drw->drawable, vis, cmap);
	}

	curfont = drw->fonts[1];
	while (1) {
		utf8strlen = 0;
		utf8str = text;
		nextfont = NULL;
		while (*text) {
			utf8charlen = utf8decode(text, &utf8codepoint, UTF_SIZ);
			for (i = 1; i < drw->fontcount; i++) {
				charexists = charexists || XftCharExists(drw->dpy, drw->fonts[i]->xfont, utf8codepoint);
				if (charexists) {
					if (drw->fonts[i] == curfont) {
						utf8strlen += utf8charlen;
						text += utf8charlen;
					} else {
						nextfont = drw->fonts[i];
					}
					break;
				}
			}

			if (!charexists || (nextfont && nextfont != curfont)) {
				break;
			} else {
				charexists = 0;
			}
		}

		if (utf8strlen) {
			drw_font_getexts(curfont, utf8str, utf8strlen, &tex);
			/* shorten text if necessary */
			for(len = MIN(utf8strlen, (sizeof buf) - 1); len && (tex.w > w - drw->fonts[1]->h || w < drw->fonts[1]->h); len--)
				drw_font_getexts(curfont, utf8str, len, &tex);

			if (len) {
				memcpy(buf, utf8str, len);
				buf[len] = '\0';
				if(len < utf8strlen)
					for(i = len; i && i > len - 3; buf[--i] = '.');

				if (render) {
					th = curfont->ascent + curfont->descent;
					ty = y + (h / 2) - (th / 2) + curfont->ascent;
					tx = x + (h / 2);
					XftDrawStringUtf8(d, invert ? &drw->scheme->bg->rgb : &drw->scheme->fg->rgb, curfont->xfont, tx, ty, (XftChar8 *)buf, len);
				}

				x += tex.w;
				w -= tex.w;
			}
		}

		if (!*text) {
			break;
		} else if (nextfont) {
			charexists = 0;
			curfont = nextfont;
		} else {
			/* Regardless of whether or not a fallback font is found, the
			 * character must be drawn.
			 */
			charexists = 1;

			if (drw->fontcount >= DRW_FONT_CACHE_SIZE) {
				continue;
			}

			fccharset = FcCharSetCreate();
			FcCharSetAddChar(fccharset, utf8codepoint);

			if (!drw->fonts[1]->pattern) {
				/* Refer to the comment in drw_font_xcreate for more
				 * information.
				 */
				die("The first font in the cache must be loaded from a font string.\n");
			}

			fcpattern = FcPatternDuplicate(drw->fonts[1]->pattern);
			FcPatternAddCharSet(fcpattern, FC_CHARSET, fccharset);
			FcPatternAddBool(fcpattern, FC_SCALABLE, FcTrue);

			FcConfigSubstitute(NULL, fcpattern, FcMatchPattern);
			FcDefaultSubstitute(fcpattern);
			match = XftFontMatch(drw->dpy, drw->screen, fcpattern, &result);

			FcCharSetDestroy(fccharset);
			FcPatternDestroy(fcpattern);

			if (match) {
				curfont = drw_font_xcreate(drw, NULL, match);
				if (curfont && XftCharExists(drw->dpy, curfont->xfont, utf8codepoint)) {
					drw->fonts[drw->fontcount++] = curfont;
				} else {
					if (curfont) {
						drw_font_free(curfont);
					}
					curfont = drw->fonts[1];
				}
			}
		}
	}

	if (render) {
		XftDrawDestroy(d);
	}

	return x;
}


/* sr - screenshot utility
 * Copyright (C) 2022 ArcNyxx
 * see LICENCE file for licensing information */

#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <Imlib2.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/cursorfont.h>
#include <X11/extensions/shape.h>
#include <X11/extensions/Xfixes.h>
#include <X11/extensions/Xinerama.h>

enum select {
	SAll = 1,
	SSel = 2,
	SWin = 4,
	SMon = 8
};

Display *dpy;
Screen  *scr;
Window   root, draw;
Cursor   cursor[5];

static void die(const char *fmt, ...);
static bool pick(int a[4]);

static void
die(const char *fmt, ...)
{
	va_list list;
	va_start(list, fmt);
	vfprintf(stderr, fmt, list);
	va_end(list);

	if (fmt[strlen(fmt) - 1] != '\n')
		perror(NULL);
	XCloseDisplay(dpy);
	exit(1);
}

static bool
pick(int a[4])
{
	const int names[] = { XC_cross, XC_ur_angle, XC_ul_angle,
			XC_lr_angle, XC_ll_angle };
	for (int i = 0; i < 5; ++i)
		cursor[i] = XCreateFontCursor(dpy, names[i]);

	XColor col;
	if (!XAllocNamedColor(dpy, XDefaultColormap(dpy, DefaultScreen(dpy)),
			"grey", &col, &(XColor){ 0 }))
		die("sr: unable to allocate colour\n");

	XSetWindowAttributes attrs = { .background_pixel = col.pixel,
			.override_redirect = true };
	XClassHint hint = { .res_name = "sr", .res_class = "sr" };

	Atom wtyd = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DOCK", false),
			wty = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", false);
	draw = XCreateWindow(dpy, root, 0, 0, scr->width, scr->height, 0,
			CopyFromParent, InputOutput, CopyFromParent,
			CWBackPixel | CWOverrideRedirect, &attrs);
	XChangeProperty(dpy, draw, wty, XA_ATOM, 32, PropModeReplace,
			(unsigned char *)&wtyd, 1);
	XSetClassHint(dpy, draw, &hint);

	int mask = ButtonMotionMask | ButtonPressMask | ButtonReleaseMask, kb;
	if (XGrabPointer(dpy, root, false, mask, GrabModeAsync, GrabModeAsync,
				root, cursor[0], CurrentTime) != GrabSuccess)
		die("sr: unable to grab cursor\n");
	for (int i = 0; i < 20 && (kb = XGrabKeyboard(dpy, root,
			false, GrabModeAsync, GrabModeAsync,
			CurrentTime)) == AlreadyGrabbed; ++i)
		nanosleep(&(struct timespec){ .tv_nsec = 50000000 }, NULL);
	if (kb != GrabSuccess)
		die("sr: unable to grab keyboard\n");

	XEvent  ev;
	bool    press;
	while (!XNextEvent(dpy, &ev)) switch (ev.type) {
	case ButtonPress:
		press = true, a[0] = ev.xbutton.x, a[1] = ev.xbutton.y;
		Window new, target = root;
		int null;
		while (XQueryPointer(dpy, target, &(Window){ 0 }, &new, &a[0],
				&a[1], &null, &null, (unsigned int *)&null))
			target = new;
		break;
	case ButtonRelease:
		goto done;
	case KeyPress:
		KeySym *keysym;
		if ((keysym = XGetKeyboardMapping(dpy, ev.xkey.keycode, 1,
				&(int){ 0 })) == NULL)
			break;
		if (*keysym == XK_Right)
			a[0] = ++a[0] <= scr->width ? a[0] : scr->width;
		else if (*keysym == XK_Left)
			a[0] = --a[0] >= 0 ? a[0] : 0;
		else if (*keysym == XK_Up)
			a[1] = ++a[1] <= scr->width ? a[1] : scr->width;
		else if (*keysym == XK_Down)
			a[1] = --a[1] >= 0 ? a[1] : 0;
		else
			die("sr: key pressed\n");
		XFree(keysym);
		goto next;
	case MotionNotify: /* FALLTHROUGH */
next:
		if (!press)
			break;

		Cursor cur;
		if (a[0] < ev.xbutton.x && a[1] < ev.xbutton.y)
			cur = cursor[3];
		else if (a[0] < ev.xbutton.x)
			cur = cursor[1];
		else if (a[1] < ev.xbutton.y)
			cur = cursor[4];
		else
			cur = cursor[2];
		XChangeActivePointerGrab(dpy, ButtonMotionMask |
				ButtonReleaseMask, cur, CurrentTime);

		if (ev.type == MotionNotify)
			a[2] = ev.xbutton.x - a[0], a[3] = ev.xbutton.y - a[1];
		XRectangle ln[4] = { { a[0], a[1], 1, a[3] }, { a[0] + a[2],
				a[1], 1, a[3] }, { a[0], a[1], a[2], 1 },
				{ a[0], a[1] + a[3], a[2], 1 } };
		XShapeCombineRectangles(dpy, draw, ShapeBounding, 0, 0, ln, 4,
				ShapeSet, 0);
		XMapWindow(dpy, draw);
	}

done:
	XUngrabKeyboard(dpy, CurrentTime);
	XUngrabPointer(dpy, CurrentTime);
	XSelectInput(dpy, draw, StructureNotifyMask);
	XUnmapWindow(dpy, draw);
}

int
main(int argc, char **argv)
{
	int   a[4] = { 0 };
	char *optsel = NULL;
	enum  select act = SSel | SWin;
	bool  optcur = true, optfre = false;

	for (argv = &argv[1]; *argv != NULL; argv = &argv[1]) {
		if ((*argv)[0] != '-')
			die("sr: invalid argument: %s\n", *argv);
		switch ((*argv)[1]) {
		case 'c': optcur = !optcur; break;
		case 'f': optfre = !optfre; break;

		case 'a': act = SAll; break;
		case 'i': optsel = *(argv = &argv[1]); break;

		case 's': act = (act & (SWin | SMon)) | SSel; break;
		case 'm': act = (act & SSel) | SMon; break;
		case 'w': act = (act & SSel) | SWin; break;
		}
	}

	if ((dpy = XOpenDisplay(NULL)) == NULL)
		die("sr: unable to open display\n");
	scr  = ScreenOfDisplay(dpy, DefaultScreen(dpy));
	root = RootWindow(dpy, XScreenNumberOfScreen(scr));

	imlib_context_set_display(dpy);
	imlib_context_set_drawable(root);
	imlib_context_set_visual(
			DefaultVisual(dpy, XScreenNumberOfScreen(scr)));
	imlib_context_set_colormap(
			DefaultColormap(dpy, XScreenNumberOfScreen(scr)));
	imlib_context_set_color_modifier(NULL);
	imlib_context_set_operation(IMLIB_OP_COPY);

	if (optfre)
		XGrabServer(dpy);
	if (act & SAll) {
		a[2] = scr->width, a[3] = scr->height;
		goto noclip;
	} else if (optsel != NULL) {
		char *start = optsel, *end;
		for (int i = 0; i < 4; ++i, start = ++end) {
			for (end = start; isdigit(*end); ++end);
			if ((i != 4 && *end != ',') || (i == 4 && *end !=
					'\0') || !isdigit(start[0]))
				die("sr: invalid option: %s\n", optsel);
			a[i] = atoi(start);
		}
	} else {
		if (act & SSel && pick(a))
			goto clip;

		if (act & SMon) {
			if (act & SSel)
				XQueryPointer(dpy, root, &(Window){ 0 },
						&(Window){ 0 }, &a[0], &a[1],
						&(int){ 0 }, &(int){ 0 },
						&(unsigned int){ 0 });

			int num;
			XineramaScreenInfo *si = XineramaQueryScreens(dpy,
					&num), *sel;
			for (sel = si; sel != NULL; sel = &sel[1])
				if (
					a[0] >= sel->x_org &&
					a[0] <= sel->x_org + sel->width &&
					a[1] >= sel->y_org &&
					a[1] <= sel->y_org + sel->height
				)
					break;
			if (sel == NULL)
				a[0] = a[1] = 0, a[2] = scr->width,
						a[3] = scr->height;
			else
				a[0] = sel->x_org, a[1] = sel->y_org,
						a[2] = sel->width,
						a[3] = sel->height;
			if (si != NULL)
				XFree(si);
		} else if (!optfre) {
			Window target = 0;
			XGetInputFocus(dpy, &target, &(int){ 0 });

			XWindowAttributes attrs;
			if (!XGetWindowAttributes(dpy, target, &attrs) ||
					attrs.map_state != IsViewable)
				die("sr: unable to get window\n");
			a[2] = attrs.width, a[3] = attrs.height;
			XTranslateCoordinates(dpy, target, root, 0, 0,
					&a[0], &a[1], &(Window){ 0 });

#define BW attrs.border_width
			if (BW > 0) a[2] += BW * 2, a[3] += BW * 2,
				a[0] -= BW, a[1] -= BW;
		} else {
			// TODO
		}
	}

clip:
	if (a[0] < 0) a[2] += a[0], a[0] = 0;
	if (a[1] < 0) a[3] += a[1], a[1] = 0;
	a[2] = (a[0] + a[2]) <= scr->width  ? a[2] : scr->width;
	a[3] = (a[1] + a[3]) <= scr->height ? a[3] : scr->height;

noclip:
	Imlib_Image image;
	if ((image = imlib_create_image_from_drawable(0,
			a[0], a[1], a[2], a[3], true)) == NULL)
		die("sr: unable to grab image\n");
	if (optcur) {
		XFixesCursorImage *cur;
		if ((cur = XFixesGetCursorImage(dpy)) == NULL)
			die("sr: unable to get cursor image\n");

		Imlib_Image img;
		DATA32 data[cur->width * cur->height];
		for (int i = 0; i < cur->width * cur->height; ++i)
			data[i] = cur->pixels[i];
		if ((img = imlib_create_image_using_data(cur->width,
				cur->height, data)) == NULL)
			die("sr: unable to create cursor image\n");

		imlib_context_set_image(img);
		imlib_image_set_has_alpha(true);
		imlib_context_set_image(image);
		imlib_blend_image_onto_image(img, 0, 0, 0, cur->width, cur->
				height, cur->x - cur->xhot - a[0], cur->y -
				cur->yhot - a[1], cur->width, cur->height);
		imlib_context_set_image(img);
		imlib_free_image();
		XFree(cur);
	}
	if (optfre)
		XUngrabServer(dpy);

	Imlib_Load_Error ret;
	imlib_context_set_image(image);
	imlib_image_set_format("png");
	imlib_save_image_with_error_return("/dev/stdout", &ret);
	imlib_free_image_and_decache();

	XCloseDisplay(dpy);
	return ret != 0;
}

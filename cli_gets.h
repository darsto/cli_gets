/*-
 * The MIT License
 *
 * Copyright 2019 Darek Stojaczyk
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>

/**
 * Get either next or previous command.
 *
 * This gets called whenever the user presses the up or down arrow key.
 *
 * \param dir -1 for the up arrow key, 1 for the down arrow key
 * \param buf Buffer to fill with the new command. This should not be
 *        modified if there is no previous/next command in the history.
 *        This buffer must be null terminated.
 * \param blen Max size of buf (including the null terminator)
 */
typedef void (*cli_history_cb)(int dir, char *buf, size_t blen);

/**
 * gets() with arrow-navigation, backspace, history, home/end buttons support, etc.
 *
 * \param f_out FILE for printing the user input. Usually stdout or stderr.
 * \param str Any custom string to print before the command prompt.
 *        Must be null-terminated.
 * \param buf Buffer where the user input will be put. It will always be
 *        null-terminated.
 * \param blen Max size of buf (including the null terminator).
 * \param history_cb function to retrieve previous/next user command whenever the
 *        up or down key is pressed. This callback is optional, can be NULL. Then
 *        up/down keys simply won't do anything.
 */
static void
cli_gets(FILE *f_out, char *str, char *buf, size_t blen, cli_history_cb history_cb)
{
	unsigned char b;
	int len = 0, off = 0, pad = 0;
	struct termios oldt, newt;

	/* terminate any previous/junk data */
	buf[0] = 0;
	fprintf(f_out, "\xD%s > ", str);

	tcgetattr( STDIN_FILENO, &oldt);
	newt = oldt;
	cfmakeraw(&newt);

	tcsetattr( STDIN_FILENO, TCSANOW, &newt);
	do {
		b = getchar();
		switch (b) {
		case 0x3: /* ctrl-c */
		case 0x1a: /* ctrl-z */
			tcsetattr( STDIN_FILENO, TCSANOW, &oldt);
			fprintf(f_out, "\n");
			exit(b == 0x3 ? 0 : 1);
			break;
		case 0x1b: /* escaped sequence */ {
			char b2, b3;

			b2 = getchar();
			b3 = getchar();

			if (b2 == 0x5b) {
				if (b3 == 68) { /* left */
					off++;
					if (off > len) off = len;
				} else if (b3 == 67) { /* right */
					off--;
					if (off < 0) off = 0;
				} else if (b3 == 66) { /* down */
					if (history_cb) {
						history_cb(1, buf, blen);
						len = strlen(buf);
						off = 0;
						pad = 0;
					}
				} else if (b3 == 65) { /* up */
					if (history_cb) {
						history_cb(-1, buf, blen);
						len = strlen(buf);
						off = 0;
						pad = 0;
					}
				} else if (b3 == 49) { /* home */
					getchar(); /* dummy */
					off = -len;
				} else if (b3 == 51) { /* delete */
					int i;

					getchar(); /* dummy */
					/* if there is a character at the cursor */
					if (off > 0) {
						/* shift the character at the right side of cursor to the left */
						for (i = len - off; i < len - 1; i++) {
							buf[i] = buf[i + 1];
						}
						/* replace the last char with a space */
						buf[len - 1] = ' ';
						len--;
						pad++;
						off--;
					}
				} else if (b3 == 52) { /* end */
					getchar(); /* dummy */
					off = 0;
				}
			}

			break;
		}
		case 0x7F: /* backspace */ {
			int i;

			/* if there are character behind the cursor */
			if (len - off > 0) {
				/* shift the character at the cursor to the left */
				for (i = len - off - 1; i < len - 1; i++) {
					buf[i] = buf[i + 1];
				}
				/* replace the last char with a space */
				buf[len - 1] = ' ';
				len--;
				pad++;
			}
			break;
		}
		case 0xD: /* carriage return */
			off = 0;
			/* fall-through */
		default:
			if (off != 0) {
				int i;

				/* shift characters at the cursor to the right */
				for (i = len; i > len - off; i--) {
					buf[i] = buf[i - 1];
				}
			}

			buf[len - off] = b;
			buf[len + 1] = 0;
			len++;
			pad = 0;
			break;
		}

		fprintf(f_out, "\xD%s > %s", str, buf);
		if (off + pad > 0) {
			/* move the cursor */
			fprintf(f_out, "\033[%dD", off + pad);
		}
	} while (b != 0xD && len < blen);

	if (b == 0xD) {
		/* exclude the newline (or the padding) */
		len--;
		buf[len] = 0;
	}

	/* the following input won't be saved, but let's
	 * not stop getting user input */
	while (b != 0xD) {
		b = getchar();
	}

	tcsetattr( STDIN_FILENO, TCSANOW, &oldt);
	fprintf(f_out, "\xD%s > %s\n", str, buf);
}


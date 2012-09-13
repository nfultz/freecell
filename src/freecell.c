// Copyright (C) 2007 Linus Akesson <linus@linusakesson.net>
// 
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <ncurses.h>

#include "freecell.h"

#define RELEASE VERSION

struct undo {
	struct undo *next;
	struct column column[8];
	struct card *work[4], *pile[4];
} *history = 0;

char *suitesymbols[] = {"s", "h", "c", "d"};

struct card deck[52];
struct column column[8];
struct card *work[4];
struct card *pile[4];

int nmoves = 0, nundos = 0;

int face = 0;
int arg = 0;

int selected = 0, wselected = 0, selcol, seln;

unsigned int seed;

void newgame() {
	int i;

	for(i = 0; i < 4; i++) {
		work[i] = 0;
		pile[i] = 0;
	}

	for(i = 0; i < 8; i++) {
		column[i].ncard = 0;
	}

	for(i = 0; i < 52; i++) {
		struct column *col;

		col = &column[i % 8];
		col->card[col->ncard++] = &deck[i];
	}
	face = 1;
	selected = 0;
	wselected = 0;
}

void cardstr(struct card *c, int sel) {
	char buf[16];

	snprintf(buf, sizeof(buf), "%2d%s", c->value, suitesymbols[c->kind]);
	if(c->kind & 1) {
		if(sel) {
			attrset(COLOR_PAIR(3));
		} else {
			attrset(COLOR_PAIR(1));
		}
	} else {
		if(sel) {
			attrset(COLOR_PAIR(2));
		}
	}
	addstr(buf);
	attrset(A_NORMAL);
}

void render() {
	int i, height, c;
	char buf[256];

	erase();
	mvaddstr(0, 0, "space                                  enter");
	snprintf(buf, sizeof(buf), "#%d", seed);
	mvaddstr(0, 22 - strlen(buf) / 2, buf);
	mvaddstr(1, 0, "[   ][   ][   ][   ]    [   ][   ][   ][   ]");
	move(1, 21);
	if(arg) {
		char buf[16];

		snprintf(buf, sizeof(buf), "%d", arg);
		addstr(buf);
	} else {
		attrset(A_BOLD | COLOR_PAIR(4));
		addstr(face? "=)" : "(=");
		attrset(A_NORMAL);
	}
	for(i = 0; i < 4; i++) {
		move(1, 1 + 5 * i);
		if(work[i]) {
			int sel = 0;

			if(wselected && selcol == i) {
				sel = 1;
			}
			cardstr(work[i], sel);
			mvaddch(2, 2 + 5 * i, 'w' + i);
		}
	}
	for(i = 0; i < 4; i++) {
		move(1, 25 + 5 * i);
		if(pile[i]) {
			cardstr(pile[i], 0);
		}
	}
	height = 0;
	for(c = 0; c < 8; c++) {
		struct column *col = &column[c];

		for(i = 0; i < col->ncard; i++) {
			int sel;

			move(4 + i, 3 + 5 * c);
			sel = 0;
			if(selected && selcol == c && i >= col->ncard - seln) {
				sel = 1;
			}
			cardstr(col->card[i], sel);
		}
		if(height < col->ncard) height = col->ncard;
	}
	mvaddstr(5 + height, 0, "    a    b    c    d    e    f    g    h");
	snprintf(buf, sizeof(buf), "%d move%s, %d undo%s", nmoves, (nmoves == 1)? "" : "s", nundos, (nundos == 1)? "" : "s");
	mvaddstr(6 + height, 44 - strlen(buf), buf);
	mvaddstr(6 + height, 0, "quit undo ?=help");
	attrset(COLOR_PAIR(1));
	mvaddch(6 + height, 0, 'q');
	mvaddch(6 + height, 5, 'u');
	mvaddch(6 + height, 10, '?');
	attrset(A_NORMAL);
	move(5 + height, 43);
	refresh();
}

int mayautomove(struct card *c) {
	int v, ov1, ov2, sv;

	if(!c) return 0;
	if(pile[c->kind]) {
		if(c->value != pile[c->kind]->value + 1) return 0;
	} else {
		if(c->value != 1) return 0;
	}

	// we know that the card may legally be moved to the foundation.

	v = c->value;
	ov1 = pile[c->kind ^ 1]? pile[c->kind ^ 1]->value : 0;
	ov2 = pile[c->kind ^ 3]? pile[c->kind ^ 3]->value : 0;
	sv = pile[c->kind ^ 2]? pile[c->kind ^ 2]->value : 0;

	// a. if the values of the foundations of the different colours are at least v - 1
	
	if(ov1 >= v - 1 && ov2 >= v - 1) return 1;

	// b. if the values of the foundations of the different colours are at
	// least v - 2, and the value of the foundation of similar colour is at
	// least v - 3.

	if(ov1 >= v - 2 && ov2 >= v - 2 && sv >= v - 3) return 1;

	return 0;
}

int automove() {
	int i;
	struct card *card;

	for(i = 0; i < 4; i++) {
		card = work[i];
		if(mayautomove(card)) {
			pile[card->kind] = card;
			work[i] = 0;
			return 1;
		}
	}
	for(i = 0; i < 8; i++) {
		if(column[i].ncard) {
			card = column[i].card[column[i].ncard - 1];
			if(mayautomove(card)) {
				pile[card->kind] = card;
				column[i].ncard--;
				return 1;
			}
		}
	}
	return 0;
}

int gameover() {
	int i;

	for(i = 0; i < 4; i++) {
		if(!pile[i]) return 0;
		if(pile[i]->value != 13) return 0;
	}
	return 1;
}

void pushundo() {
	struct undo *u = malloc(sizeof(struct undo));

	u->next = history;
	memcpy(u->column, column, sizeof(column));
	memcpy(u->work, work, sizeof(work));
	memcpy(u->pile, pile, sizeof(pile));
	history = u;
	nmoves++;
}

void popundo() {
	struct undo *u = history;

	if(u) {
		history = u->next;
		memcpy(column, u->column, sizeof(column));
		memcpy(work, u->work, sizeof(work));
		memcpy(pile, u->pile, sizeof(pile));
		free(u);
		nmoves--;
		nundos++;
	}
	
	selected = 0;
	wselected = 0;
	arg = 0;
}

void helpscreen() {
	erase();
	mvaddstr(0, 0, "freecell " RELEASE);
	mvaddstr(0, 24, "www.linusakesson.net");
	mvaddstr(2, 0, " The aim of the game is to move all cards to");
	mvaddstr(3, 0, "the foundations in the upper right corner.");
	mvaddstr(4, 0, " You may only move one card at a time.   The");
	mvaddstr(5, 0, "foundations accept cards of increasing value");
	mvaddstr(6, 0, "within each suite   (you may place 2; on top");
	mvaddstr(7, 0, "of 1;).  The columns accept cards of falling");
	mvaddstr(8, 0, "value, different colour (you may place 2; on");
	mvaddstr(9, 0, "either 3. or 3:). The four free cells in the");
	mvaddstr(10, 0, "upper left corner will accept any cards, but");
	mvaddstr(11, 0, "at most one card per cell.");
	mvaddstr(13, 0, "Type any character to continue.    Page 1(4)");
	attrset(COLOR_PAIR(1));
	mvaddstr(6, 35, "2");
	mvaddstr(6, 36, suitesymbols[3]);
	mvaddstr(7, 3, "1");
	mvaddstr(7, 4, suitesymbols[3]);
	mvaddstr(8, 39, "2");
	mvaddstr(8, 40, suitesymbols[3]);
	attrset(A_BOLD);
	mvaddstr(9, 7, "3");
	mvaddstr(9, 8, suitesymbols[0]);
	mvaddstr(9, 13, "3");
	mvaddstr(9, 14, suitesymbols[2]);
	attrset(A_NORMAL);
	move(12, 43);
	refresh();
	getch();

	erase();
	mvaddstr(0, 0, "freecell " RELEASE);
	mvaddstr(0, 24, "www.linusakesson.net");
	mvaddstr(2, 0, "To move a card,  type the name of the column");
	mvaddstr(3, 0, "(a-h) or cell (w-z) which contains the card,");
	mvaddstr(4, 0, "followed by the name of the destination cell");
	mvaddstr(5, 0, "or column. Press the enter key for the dest-");
	mvaddstr(6, 0, "ination in order to  move the card to one of");
	mvaddstr(7, 0, "the foundation piles.  As a convenience, you");
	mvaddstr(8, 0, "may also move a card to an unspecified  free");
	mvaddstr(9, 0, "cell,  by substituting the space bar for the");
	mvaddstr(10, 0, "destination.");
	mvaddstr(13, 0, "Type any character to continue.    Page 2(4)");
	attrset(COLOR_PAIR(4));
	mvaddstr(3, 1, "a");
	mvaddstr(3, 3, "h");
	mvaddstr(3, 15, "w");
	mvaddstr(3, 17, "z");
	mvaddstr(5, 21, "enter");
	mvaddstr(9, 27, "space");
	attrset(A_NORMAL);
	move(12, 43);
	refresh();
	getch();

	erase();
	mvaddstr(0, 0, "freecell " RELEASE);
	mvaddstr(0, 24, "www.linusakesson.net");
	mvaddstr(2, 0, "While you may only move one card at a time,");
	mvaddstr(3, 0, "you can use free cells and empty columns as");
	mvaddstr(4, 0, "temporary buffers. That way, it is possible");
	mvaddstr(5, 0, "to move a range of cards from one column to");
	mvaddstr(6, 0, "another,  as long as they are already in an");
	mvaddstr(7, 0, "acceptable order.   The program can do this");
	mvaddstr(8, 0, "automatically for you:  Prefix your command");
	mvaddstr(9, 0, "with the number of cards to move,  e.g. 3ab");
	mvaddstr(10, 0, "will move 3 cards from column a to column b");
	mvaddstr(11, 0, "and requires 2 free cells or empty columns.");
	mvaddstr(13, 0, "Type any character to continue.    Page 3(4)");
	attrset(COLOR_PAIR(4));
	mvaddstr(9, 40, "3ab");
	attrset(A_NORMAL);
	move(12, 43);
	refresh();
	getch();

	erase();
	mvaddstr(0, 0, "freecell " RELEASE);
	mvaddstr(0, 24, "www.linusakesson.net");
	mvaddstr(2, 0, "When it is deemed safe to do so,  cards will");
	mvaddstr(3, 0, "automatically  be  moved  to  the foundation");
	mvaddstr(4, 0, "piles.");
	mvaddstr(6, 0, "Modern freecell was invented by Paul Alfille");
	mvaddstr(7, 0, "in 1978 - http://wikipedia.org/wiki/Freecell");
	mvaddstr(8, 0, "Almost every game is solvable, but the level");
	mvaddstr(9, 0, "of difficulty can vary a lot.");
	attrset(COLOR_PAIR(4));
	mvaddstr(11, 0, "   Good luck, and don't get too addicted!");
	attrset(A_NORMAL);
	mvaddstr(13, 0, "Type any character to continue.    Page 4(4)");
	move(12, 43);
	refresh();
	getch();
}

usage() {
	printf("freecell " RELEASE " by Linus Akesson\n");
	printf("http://www.linusakesson.net\n");
	printf("\n");
	printf("Usage: freecell [options] [game#]\n");
	printf("\n");
	printf("-sABCD   --suites ABCD  Configures four characters as suite symbols.\n");
	printf("\n");
	printf("-h       --help         Displays this information.\n");
	printf("-V       --version      Displays brief version information.\n");
	exit(0);
}

int main(int argc, char **argv) {
	struct option longopts[] = {
		{"help", 0, 0, 'h'},
		{"version", 0, 0, 'V'},
		{"suites", 1, 0, 's'},
		{0, 0, 0, 0}
	};
	int running = 1;
	int opt;
	int i;

	do {
		opt = getopt_long(argc, argv, "hVs:", longopts, 0);
		switch(opt) {
			case 0:
			case 'h':
				usage();
				break;
			case 'V':
				printf("freecell " RELEASE " by Linus Akesson\n");
				exit(0);
				break;
			case 's':
				if(strlen(optarg) != 4) usage();
				for(i = 0; i < 4; i++) {
					char buf[32];

					snprintf(buf, sizeof(buf), "%c", optarg[i]);
					suitesymbols[i] = strdup(buf);
				}
				break;
		}
	} while(opt >= 0);

	argc -= optind;
	argv += optind;

	if(argc == 1) {
		seed = atoi(argv[0]);
	} else if(argc == 0) {
		srand(time(0));
		seed = rand() & 0xffffffff;
	} else usage();

	newgame();
	dealgame(seed);

	initscr();
	noecho();
	curs_set(0);
	start_color();
	keypad(stdscr, TRUE);
	init_pair(1, COLOR_CYAN, COLOR_BLACK);
	init_pair(2, COLOR_WHITE, COLOR_BLUE);
	init_pair(3, COLOR_CYAN, COLOR_BLUE);
	init_pair(4, COLOR_YELLOW, COLOR_BLACK);
	while(running) {
		int c;

		for(;;) {
			render();
			if(automove()) {
				usleep(50000);
			} else {
				break;
			}
		}

		if(gameover()) {
			int i;
			char *str = "WELL DONE!";

			attrset(A_BOLD | COLOR_PAIR(4));
			mvaddstr(3, 17, str);
			move(5, 43);
			refresh();
			usleep(50000);
			for(i = 0; i < strlen(str); i++) {
				attrset(A_BOLD | COLOR_PAIR(4));
				if(i) mvaddch(3, 17 + i - 1, str[i - 1]);
				attrset(A_BOLD);
				mvaddch(3, 17 + i, str[i]);
				move(5, 43);
				refresh();
				usleep(100000);
			}
			attrset(A_BOLD | COLOR_PAIR(4));
			mvaddstr(3, 17, str);
			move(5, 43);
			refresh();
			break;
		}

		c = getch();
		if(c >= '0' && c <= '9') {
			if(arg < 10 && !selected && !wselected) {
				arg = arg * 10 + c - '0';
			}
		} else {
			if(c == 32) {
				if(selected || wselected) {
					int i;

					for(i = 0; i < 4; i++) {
						if(!work[i]) {
							break;
						}
					}
					if(i < 4) {
						c = 'w' + i;
					} else {
						c = 27;
						face = 0;
					}
				}
			}
			if(c == 'q') {
				running = 0;
			} else if(c == 27) {
				selected = 0;
				wselected = 0;
			} else if(c == 'u') {
				popundo();
			} else if(c == '?') {
				helpscreen();
			} else if(c == 10 || c == 13 || c == KEY_ENTER) {
				struct card *card = 0;
				int may = 0;

				if(selected) {
					struct column *col = &column[selcol];

					if(seln == 1 && col->ncard) {
						card = col->card[col->ncard - 1];
						if(pile[card->kind]) {
							if(card->value == pile[card->kind]->value + 1) {
								may = 1;
							}
						} else {
							if(card->value == 1) {
								may = 1;
							}
						}
						if(may) {
							pushundo();
							pile[card->kind] = card;
							col->ncard--;
						}
					}
					selected = 0;
				} else if(wselected) {
					if(work[selcol]) {
						card = work[selcol];
						if(pile[card->kind]) {
							if(card->value == pile[card->kind]->value + 1) {
								may = 1;
							}
						} else {
							if(card->value == 1) {
								may = 1;
							}
						}
						if(may) {
							pushundo();
							pile[card->kind] = card;
							work[selcol] = 0;
						}
					}
					wselected = 0;
				}
				face = 1;
			} else if(c >= 'a' && c <= 'h') {
				struct column *col = &column[c - 'a'];
				int may = 0;

				if(selected) {
					int nfree = 0, i;

					for(i = 0; i < 4; i++) {
						if(!work[i]) nfree++;
					}
					for(i = 0; i < 8; i++) {
						if(!column[i].ncard) nfree++;
					}
					if(nfree >= seln - 1 + !col->ncard) {
						int first = column[selcol].ncard - seln;
						struct card *card = column[selcol].card[first];

						may = 1;
						if(col->ncard
						&& ((card->kind & 1) == (col->card[col->ncard - 1]->kind & 1))) may = 0;
						if(col->ncard
						&& (card->value + 1 != col->card[col->ncard - 1]->value)) may = 0;
						if(may) {
							pushundo();
							for(i = 0; i < seln; i++) {
								col->card[col->ncard++] = column[selcol].card[first + i];
							}
							column[selcol].ncard -= seln;
						}
					}
					selected = 0;
				} else if(wselected) {
					if(col->ncard) {
						if((col->card[col->ncard - 1]->kind & 1) != (work[selcol]->kind & 1)
						&& (col->card[col->ncard - 1]->value == work[selcol]->value + 1)) {
							may = 1;
						}
					} else {
						may = 1;
					}
					if(may) {
						pushundo();
						col->card[col->ncard++] = work[selcol];
						work[selcol] = 0;
					}
					wselected = 0;
				} else {
					int maxn, i;

					selcol = c - 'a';
					if(column[selcol].ncard) {
						selected = 1;
						seln = arg? arg : 1;
						maxn = 1;
						for(i = column[selcol].ncard - 1; i > 0; i--) {
							if(((column[selcol].card[i]->kind & 1) != (column[selcol].card[i - 1]->kind & 1))
							&& (column[selcol].card[i]->value + 1 == column[selcol].card[i - 1]->value)) {
								maxn++;
							} else {
								break;
							}
						}
						if(seln > maxn) seln = maxn;
					}
				}
				face = c >= 'e';
			} else if(c >= 'w' && c <= 'z') {
				int w = c - 'w';

				if(selected) {
					struct column *col = &column[selcol];

					if(seln == 1 && !work[w] && col->ncard) {
						pushundo();
						work[w] = col->card[col->ncard - 1];
						col->ncard--;
					}
					selected = 0;
				} else if(wselected) {
					if(!work[w]) {
						pushundo();
						work[w] = work[selcol];
						work[selcol] = 0;
					}
					wselected = 0;
				} else {
					if(work[w]) {
						wselected = 1;
						selcol = w;
					}
				}
				face = 0;
			}
			arg = 0;
		}
	}
	endwin();
	return 0;
}

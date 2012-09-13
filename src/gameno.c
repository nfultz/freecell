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

#include "freecell.h"

// This file contains Microsoft compatible freecell game number handling.

extern struct column column[8];

void dealgame(int seed) {
	unsigned long holdseed;
	int i;
	int deck[52];
	int cardsleft = 52;
	int c;
	struct card *card;
	int suitemap[4] = {2, 3, 1, 0};

	holdseed = (unsigned long) seed;

	for(i = 0; i < 52; i++) {
		deck[i] = i;
	}

	for(i = 0; i < 52; i++) {
		// Pick a card, any card.
		holdseed = (holdseed * 214013 + 2531011) & 0xffffffff;
		c = ((holdseed >> 16) & 0x7fff) % cardsleft;
		// Place it on the table.
		card = column[i % 8].card[i / 8];
		card->value = deck[c] / 4 + 1;
		card->kind = suitemap[deck[c] % 4];
		// Move the last card in the deck into the vacant position.
		deck[c] = deck[cardsleft - 1];
		cardsleft--;
	}
}

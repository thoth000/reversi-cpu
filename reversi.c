#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define BOARD_SIZE 0x80
#define INIT_BLACK 0x0000000810000000
#define INIT_WHITE 0x0000001008000000

#define WIDTH 8
#define HEIGHT 8

#define L_MASK 0xfefefefefefefefe
#define R_MASK 0x7f7f7f7f7f7f7f7f
#define T_MASK 0xffffffffffffff00
#define B_MASK 0x00ffffffffffffff

typedef unsigned long long uint64;

int directions[8] = {
	1, // R
	-WIDTH+1, // TR
	-WIDTH, // T
	-WIDTH-1, // TL
	-1, // L
	WIDTH-1, // BL
	WIDTH, // B
	WIDTH+1, // BR
};

uint64 masks[8] = {
	R_MASK,
	T_MASK & R_MASK,
	T_MASK,
	T_MASK & L_MASK,
	L_MASK,
	B_MASK & L_MASK,
	B_MASK,
	B_MASK & R_MASK
};

int scores[7] = {
	-80,
	-40,
	-1,
	1,
	5,
	20,
	100
};

uint64 score_masks[7] = {
	0x0042000000004200, // -80
	0x4281000000008142, // - 40
	0x003c424242423c00, // -1
	0x0000182424180000, // 1
	0x1800248181240018, // 5
	0x2400810000810024, // 20
	0x8100000000000081, // 100
};

uint64 improved_mask[8] = {
	0x0040000000000000, // TL-80
	0x4080000000000000, // TL-40
	0x0002000000000000, // TR-80
	0x0201000000000000, // TR-40
	0x0000000000004000, // BL-80
	0x0000000000008040, // BL-40
	0x0000000000000200, // BR-80
	0x0000000000000102, // BR-40
};

uint64 get_legal(uint64 m_board, uint64 e_board);
int bitcount(uint64 board);
uint64 trans(uint64 p, int dir);
uint64 get_rev(uint64 m_board, uint64 e_board, uint64 p);
uint64 input2bit(int x, int y);
uint64 get_input(uint64 legal, uint64 m_board, uint64 e_board);
int evaluate(uint64 legal, uint64 m_board, uint64 e_board);
uint64 definite_board(uint64 m_board);
uint64 get_negamax_input(uint64 legal, uint64 m_board, uint64 e_board);
int negamax(uint64 m_board, uint64 e_board, int dep, int a, int b);
void disp_board(uint64 white, uint64 black);
void disp_input(uint64 input);
int game(uint64 black, uint64 white, int cpu);
void disp_uint64(uint64 bit);
uint64 bit_shift(uint64 bit, int shift);

uint64 bit_shift(uint64 bit, int shift) {
	if (shift >= 0) {
		return bit >> shift;
	}
	else {
		return bit << (-shift);
	}
};

uint64 get_legal(uint64 m_board, uint64 e_board) {
	uint64 blank = ~(m_board | e_board);
	uint64 legal = 0;
	uint64 tmp;
	int dir, i;
	for (dir = 0; dir < 8; dir++) {
		tmp = bit_shift(m_board, directions[dir]) & masks[dir] & e_board;
		for (i = 0; i < 5; i++) {
			tmp |= bit_shift(tmp, directions[dir]) & masks[dir] & e_board;
		}
		legal |= bit_shift(tmp, directions[dir]) & masks[dir] & blank;
	}
	return legal;
};

int bitcount(uint64 board) {
	int count = 0, i;
	for (i = 0; i < 64; i++) {
		count += bit_shift(board, i) & 1;
	}
	return count;
};

uint64 trans(uint64 p, int dir) {
	return bit_shift(p, directions[dir]) & masks[dir];
};

uint64 get_rev(uint64 m_board, uint64 e_board, uint64 p) {
	uint64 t_rev, rev = 0, mask;
	int dir;
	if (((m_board | e_board) & p) != 0) {
		return rev;
	}
	for (dir = 0; dir < 8; dir++) {
		t_rev = 0;
		mask = trans(p, dir);
		while ((mask != 0) && ((mask & e_board) != 0)) {
			t_rev |= mask;
			mask = trans(mask, dir);
		}
		if ((mask & m_board) != 0) {
			rev |= t_rev;
		}
	}
	return rev;
};

uint64 input2bit(int x, int y) {
	if ((x < 0 || x > WIDTH - 1) || (y < 0 || y > HEIGHT - 1)) {
		return 0;
	}
	return bit_shift(1llu,  -(y * WIDTH + x));
};

uint64 get_input(uint64 legal, uint64 m_board, uint64 e_board) {
	uint64 input = 0;
	char x;
	int y, check=0;

	do {
		printf("a 0のように置きたい場所を選んでください．: ");
		check = scanf("%[abcdefgh] %d", &x, &y);
		if(check<2){
			printf("入力が不適です．\n");
			continue;
		}
		printf("%d, %d\n", x-'a', y);
		input = input2bit(7 - (x - 'a'), 7 - y);
	} while ((input & legal) == 0);
	return input;
};

int evaluate(uint64 legal, uint64 m_board, uint64 e_board) {
	int bp=0, cn, fs, i;
	if (bitcount(~(m_board | e_board)) < 10) {
		return 2 * bitcount(m_board) - bitcount(e_board);
	}
	for (i = 0; i < 7; i++) {
		bp += scores[i] * (bitcount(m_board & score_masks[i]) - bitcount(e_board & score_masks[i]));
	}
	cn = bitcount(legal);
	fs = bitcount(definite_board(m_board)) - bitcount(definite_board(e_board));
	return 2 * bp + 5 * fs + cn;
}

void improve_mask(uint64 rev) {
	if (rev == 1e63) { // TL
		score_masks[0] ^= improved_mask[0];
		score_masks[1] ^= improved_mask[1];
		score_masks[3] ^= improved_mask[0] | improved_mask[1];
	}else if(rev== 1e56){ // TR
		score_masks[0] ^= improved_mask[2];
		score_masks[1] ^= improved_mask[3];
		score_masks[3] ^= improved_mask[2] | improved_mask[3];
	}else if(rev== 1e7){ // BL
		score_masks[0] ^= improved_mask[4];
		score_masks[1] ^= improved_mask[5];
		score_masks[3] ^= improved_mask[4] | improved_mask[5];
	}else if(rev==1){ // BR
		score_masks[0] ^= improved_mask[6];
		score_masks[1] ^= improved_mask[7];
		score_masks[3] ^= improved_mask[6] | improved_mask[7];
	}
}

uint64 definite_board(uint64 m_board) {
	uint64 d_board = 0;
	int w, i ,j;
	//TL
	w = -1;
	for (i = 7; i > -1; i--) {
		for (j = 7; j > w; j--) {
			if (bit_shift(m_board, i * WIDTH + j) & 1) {
				d_board |= input2bit(j,i);
			}
			else {
				w = j;
			}
		}
	}
	//TR
	w = 8;
	for (i = 7; i > -1; i--) {
		for (j = 0; j < w; j++) {
			if (bit_shift(m_board, i * WIDTH + j) & 1) {
				d_board |= input2bit(j,i);
			}
			else {
				w = j;
			}
		}
	}
	//BL
	w = -1;
	for (i = 0; i < 8; i++) {
		for (j = 7; j > w; j--) {
			if (bit_shift(m_board, i * WIDTH + j) & 1) {
				d_board |= input2bit(j,i);
			}
			else {
				w = j;
			}
		}
	}
	//BR
	w = 8;
	for (i = 0; i < 8; i++) {
		for (j = 0; j < w; j++) {
			if (bit_shift(m_board, i * WIDTH + j) & 1) {
				d_board |= input2bit(j,i);
			}
			else {
				w = j;
			}
		}
	}
	return d_board;
};

int negamax(uint64 player, uint64 opponent, int depth, int alpha, int beta) {
	// beta = ?????????alpha????p?]???l
	int x, y, score;
	uint64 p, rev, legal;

	legal = get_legal(player, opponent);

	if (!legal) {
		if (bitcount(~(player | opponent)) == 0) {
			return evaluate(legal, player, opponent);
		}
		else {
			return -2000;
		}
	}

	if (!depth) {
		return evaluate(legal, player, opponent);
	}

	for (y = 7; y > -1 && alpha < beta; y--) {
		for (x = 7; x > -1 && alpha < beta; x--) {
			p = input2bit(x, y);
			if (legal & p) {
				rev = get_rev(player, opponent, p);
				score = -negamax(opponent ^ rev, player ^ (p | rev), depth - 1, -beta, -alpha);
				if (score > alpha) { //??????X?R?A(?????X?R?A??t)????????????
					alpha = score;
				}
			}
		}
	}

	return alpha; //?]?????l????
}

uint64 get_negamax_input(uint64 legal, uint64 m_board, uint64 e_board) {
	int x, y, mx = -1e8, score;
	uint64 p, rev, best_p = 0;

	for (y = 7;y > -1; y--) {
		for (x = 7; x > -1; x--) {
			p = input2bit(x, y);
			if (legal & p) {
				rev = get_rev(m_board, e_board, p);
				score = -negamax(e_board ^ rev, m_board ^ (p | rev), 5, -1e8, 1e8);
				if (score > mx) { //??????????L?????????
					mx = score;
					best_p = p;
				}
			}
		}
	}
	return best_p;
}

void disp_board(uint64 white, uint64 black) {
	int x, y;

    puts("   ┃ a ┃ b ┃ c ┃ d ┃ e ┃ f ┃ g ┃ h ┃");
    puts("━━━╋━━━╋━━━╋━━━╋━━━╋━━━╋━━━╋━━━╋━━━┃");
    for (y = 7; y > 0; y--) {
        printf(" %d ┃",7 - y);
        for (x = 7; x > -1; x--) {
            if (bit_shift(white, y * WIDTH + x) & 1) printf(" ●┃");
			else if (bit_shift(black, y * WIDTH + x) & 1) printf(" 〇┃");
			else printf("   ┃");
        }
        printf("\n");
        puts("━━━╋━━━╋━━━╋━━━╋━━━╋━━━╋━━━╋━━━╋━━━┃");
    }
	printf(" 7 ┃");
	for (x = 7; x > -1; x--) {
        if (bit_shift(white, x) & 1) printf(" ●┃");
		else if (bit_shift(black, x) & 1) printf(" 〇┃");
		else printf("   ┃");
    }
    printf("\n");
	puts("━━━┻━━━┻━━━┻━━━┻━━━┻━━━┻━━━┻━━━┻━━━┛");
}

void disp_input(uint64 input) {
	int x, y, yi;
	char xi;
	for (y = 7; y > -1; y--) {
		for (x = 7; x > -1; x--) {
			if (bit_shift(input, y * WIDTH + x) & 1) {
				yi = 7 - y;
				xi = 'a' + (7 - x);
				printf("cpu Input : %c %d\n", xi, yi);
				return ;
			}
		}
	}
}

void disp_uint64(uint64 bit) {
	int x, y;
	for (y = 7; y > -1; y--) {
		for (x = 7; x > -1; x--) {
			printf("%lld ", bit_shift(bit, y * WIDTH + x) & 1);
		}
		puts("");
	}
}

int game(uint64 black, uint64 white, int cpu) {
	int pass_count = 0, turn = 0, b_count, w_count, value;
	uint64 legal, rev, input;

	while (pass_count <= 2) {
		disp_board(white, black);

		if (turn) {
			legal = get_legal(white, black);
			printf("白のターンです．\n");
		}
		else {
			legal = get_legal(black, white);
			printf("黒のターンです．\n");
		}

		if (legal == 0) {
			pass_count++;
			turn ^= 1;
			printf("置けないので，パスします．\n");
			continue;
		}
		else {
			pass_count = 0;
		}

		if (turn) {
			value = evaluate(get_legal(white, black), white, black);
			if (value < 0) {
				printf("黒が良いです． %d\n", -value);
			}
			else {
				printf("白が良いです． %d\n", value);
			}
		}
		else {
			value = evaluate(get_legal(black, white), black, white);
			if (value > 0) {
				printf("黒が良いです． %d\n", value);
			}
			else {
				printf("白が良いです． %d\n", -value);
			}
		}

		if (turn) {
			if (cpu == turn) {
				input = get_negamax_input(legal, white, black);
				disp_input(input);
			}
			else {
				input = get_input(legal, white, black);
			}
			rev = get_rev(white, black, input);
			white ^= (input | rev);
			black ^= rev;
		}
		else {
			if (cpu == turn) {
				input = get_negamax_input(legal, black, white);
				disp_input(input);
			}
			else {
				input = get_input(legal, black, white);
			}
			rev = get_rev(black, white, input);
			black ^= (input | rev);
			white ^= rev;
		}
		turn ^= 1;
	}

	disp_board(white, black);

	puts("ゲーム終了です．");

	b_count = bitcount(black);
	w_count = bitcount(white);
	if (b_count - w_count > 0) {
		printf("%d石差で黒の勝ちです.\n", b_count - w_count);
	}
	else if (b_count - w_count < 0) {
		printf("%d石差で白の勝ちです．\n", w_count - b_count);
	}
	else {
		puts("引き分けです．");
	}
	return EXIT_SUCCESS;
};

int main(int argc, char** argv) {
	uint64 black = INIT_BLACK, white = INIT_WHITE;
	int cpu;

	printf("CPUの石を選んでください．->黒: 0，白: 1\n>>");
	scanf("%d", &cpu);

	puts("ゲームを始めます．");
	game(black, white, cpu);
}

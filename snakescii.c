/*
  	Snakescii - a simple ascii snake-like using ncurses
 
  	Copyright (C) 2019 Liam McDermott
  
	This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <curses.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>

#define TIMEDELAY 80000

int x, y;
int running, grow, score, paused;
WINDOW *mainwin;
WINDOW *scorewin;

typedef struct Snake {
	char direction;
	int x, y;
	struct Snake *next;
} SNAKE;

typedef struct Food {
	int x, y;
	struct Food *next;
} FOOD;

static SNAKE *head;
FOOD *food;

pthread_mutex_t lock;

/* Populate game with food bits */
void createFood() {

	int foodX, foodY;
	int found = 0;
	while (!found) {
		foodX = rand() % (x-1+1-2) + 2;
		foodY = rand() % (y-3+1-2) + 2;
		found = 1;
	
		/* See if random position is on snake */
		SNAKE *p = head;
		while (p != NULL) {
			if (p->x == foodX && p->y == foodY) {
				found = 0;
				break;
			}
			p = p->next;
		}
	}
	
	FOOD *f = food;
	if (f == NULL) { /* Create first bit */
		food = malloc(sizeof(FOOD));
		food->x = foodX;
		food->y = foodY;
		food->next = NULL;	
	}
	else{
		while (f->next != NULL) {
			f = f->next;
		}
		f->next = malloc(sizeof(FOOD));
		f->next->x = foodX;
		f->next->y = foodY;
		f->next->next = NULL;
	}
}

void drawScore() {

	werase(scorewin);
	wmove(scorewin, 0, 0);
	wprintw(scorewin, "SCORE: %d", score);
	wrefresh(scorewin);
}

void drawSnake() {

	werase(mainwin);
	box(mainwin, 0,0);
	
	/* Draw snake bits */
	pthread_mutex_lock(&lock);
	SNAKE *p = head;
	while (p != NULL) {
		wmove(mainwin, p->y, p->x);
		wprintw(mainwin, "o");
		wrefresh(mainwin);
		p = p->next;	
	}
	pthread_mutex_unlock(&lock);
	
	/* Draw food bits */
	FOOD *f = food;
	while (f != NULL) {
		wmove(mainwin, f->y, f->x);
		wprintw(mainwin, "X");
		wrefresh(mainwin);
		f = f->next;
	}
}

void initGame() {

	paused = 0;
	running = 1;
	grow = 0;
	score = 0;
	head = malloc(sizeof(SNAKE));
	if (head == NULL) {
		endwin();
		exit(1);
	}
	food = NULL;

	/* Setup head of snake in middle of screen */
	head->y = (y-1)/2;
	head->x = x/2;
	head->direction = 'u';
	head->next = NULL;
	
	drawSnake();
	drawScore();
}

void restartGame() {

	/* Free old snake bits */
	SNAKE *p;
	while (head != NULL) {
		p = head;
		head = p->next;
		free(p);
	}
	
	/* Free old food bits */
	FOOD *f;
	while (food != NULL) {
		f = food;
		food = f->next;
		free(f);
	}
	initGame();
}

void growSnake() {

	char c;
	SNAKE *new = malloc(sizeof(SNAKE));
	pthread_mutex_lock(&lock);
	new->direction = head->direction;
	switch (new->direction) {
		case 'u':
			new->y = head->y - 1;
			new->x = head->x;
			break;
		case 'd':
			new->y = head->y + 1;
			new->x = head->x;
			break;
		case 'l':
			new->y = head->y;
			new->x = head->x - 1;
			break;
		case 'r':
			new->y = head->y;
			new->x = head->x + 1;
			break;
	}
	new->next = head;
	head = new;		
	
	pthread_mutex_unlock(&lock);	
	drawSnake(); 
}

void moveSnake() {

	char c;
	SNAKE *p = head;
	SNAKE *next;
	SNAKE temp;
	SNAKE temp2;
	
	pthread_mutex_lock(&lock);
	temp.x = p->x;
	temp.y = p->y;
	
	switch (head->direction) {
		case 'u':
			head->y = head->y - 1;
			break;
		case 'd':
			head->y = head->y + 1;
			break;
		case 'l':
			head->x = head->x - 1;
			break;
		case 'r':
			head->x = head->x + 1;
			break;
	}
	
	/* Check for collisions with walls */
	if (head->x == 1 || head->x == (x-1) || head->y == 0 || head->y == (y-2)) {
		pthread_mutex_unlock(&lock);
		restartGame();
		return;
	}
	
	/* Check for collisions with other snake bits */
	p = head->next;
	while (p != NULL) {
		if (head->y == p->y && head->x == p->x) {
			pthread_mutex_unlock(&lock);
			restartGame();
			return;
		}
		p = p->next;
	}
	
	/* Check for collisions with food */
	FOOD *f = food;
	FOOD *prev = food;
	while (f != NULL) {
		if (head->y == f->y && head->x == f->x) {
			score += 5;
			if (prev == f) {
				food = f->next;
			}
			else {
				prev->next = f->next;
			}
			free(f);
			drawScore();
			break;
		}
		prev = f;
		f = f->next;
	}
		
	p = head;
	while (p != NULL) {
		next = p->next;
		if (next != NULL){
			temp2.x = next->x;
			temp2.y = next->y;
			next->x = temp.x;
			next->y = temp.y;
			temp.x = temp2.x;
			temp.y = temp2.y;
		}
		if (next == NULL && (grow == 1)) {
			SNAKE *new = malloc(sizeof(SNAKE));
			new->x = temp.x;
			new->y = temp.y;
			new->next = NULL;
			p->next = new;
			grow = 0;
			break;
		}
		p = p->next;
	}

	pthread_mutex_unlock(&lock);
	
	drawSnake();
}


/* Check if there is a snake bit in a given direction from snake head */
int checkBit(char dir) {
	
	SNAKE *p = head->next;
	if (p == NULL) return 0; /* Only head present */
	switch (dir) {
		case 'u':
			if (p->y == (head->y - 1))
				return 1;
			else
				return 0;
		case 'd':
			if (p->y == (head->y + 1))
				return 1;
			else 
				return 0;
		case 'l':
			if (p->x == (head->x - 1))
				return 1;
			else
				return 0;
		case 'r': 
			if (p->x == (head->x +  1))
				return 1;
			else
				return 0;
	}
	return 0;
}

/* Get user key inputs */
void *keyListener() {

	int key; 
	while((key = wgetch(mainwin)) != 'q') {
		switch (key) {
			case KEY_UP:
				pthread_mutex_lock(&lock);
				if (!checkBit('u'))
					head->direction = 'u';
				pthread_mutex_unlock(&lock);
				break;		
			case KEY_DOWN:
				pthread_mutex_lock(&lock);
				if (!checkBit('d'))
					head->direction = 'd';
				pthread_mutex_unlock(&lock);
				break;		
			case KEY_LEFT:
				pthread_mutex_lock(&lock);
				if (!checkBit('l'))
					head->direction = 'l';
				pthread_mutex_unlock(&lock);
				break;		
			case KEY_RIGHT:
				pthread_mutex_lock(&lock);
				if (!checkBit('r'))
					head->direction = 'r';
				pthread_mutex_unlock(&lock);
				break;
			case ' ':
				if (!paused)
					paused = 1;
				else
					paused = 0;
				break;
		}
	}
	running = 0;
	return NULL;
}

/* Move snake after timer raises SIGALRM */
void handleTimer(int signum) {
	
	switch(signum) {
		case SIGALRM:
			if (!paused)
			moveSnake();
			break;
		default:
			break; /* Ignore other signals */
	}
}

/* Set timer with TIMEDELAY interval. Timer will raise SIGALRM */
void setTimer() {
	
	struct itimerval newValue;
	struct sigaction sa;
	
	timerclear(&newValue.it_interval);
    timerclear(&newValue.it_value);
	newValue.it_interval.tv_usec = TIMEDELAY;
	newValue.it_value.tv_usec = TIMEDELAY;
	setitimer(ITIMER_REAL, &newValue, NULL);
	
	sa.sa_handler = handleTimer;
	sa.sa_flags = 0;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGALRM, &sa, NULL);
}

int main(int argc, char *argv[]) {

	initscr();
	start_color();
	getmaxyx(stdscr, y, x);
	curs_set(0);
	mainwin = newwin(y-1, x, 0, 0);
	scorewin = newwin(1, x, y-1, 0);
	init_color(COLOR_GREEN, 0, 1000, 0);
	init_pair(1, COLOR_GREEN, COLOR_BLACK);
	wbkgd(mainwin, COLOR_PAIR(1));
	wbkgd(scorewin, COLOR_PAIR(1));
	keypad(mainwin, TRUE);
	noecho();
			
	pthread_t keyInput;
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
	pthread_mutex_init(&lock, NULL);

	setTimer();
	initGame();
	int counter = 0;
	srand(time(0));

	pthread_create(&keyInput, &attr, keyListener, NULL);
	while (running) {
		sleep(1);
		counter += 1;
		if (counter % 8 == 0) {
			grow = 1;
		}
		if (counter == 60) {
			if (!paused)
				createFood();
			counter = 0;
		}
	}

	pthread_mutex_destroy(&lock);
	SNAKE *p;
	while (head != NULL) {
		p = head;
		head = p->next;
		free(p);
	}
	endwin();
	return 0;
}

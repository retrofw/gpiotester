#include <SDL/SDL.h>
#include <SDL/SDL_image.h>
#include <SDL/SDL_ttf.h>
#include "font.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <signal.h>

#include <sys/ioctl.h>
#include <linux/vt.h>
#include <linux/kd.h>
#include <linux/fb.h>

#include <stdio.h>
#include <stdlib.h>

#include <linux/limits.h>

static const int SDL_WAKEUPEVENT = SDL_USEREVENT+1;

#ifndef TARGET_RETROFW
	#define system(x) printf(x); printf("\n")
#endif

#ifndef TARGET_RETROFW
	#define DBG(x) printf("%s:%d %s %s\n", __FILE__, __LINE__, __func__, x);
#else
	#define DBG(x)
#endif


#define WIDTH  320
#define HEIGHT 240

#define GPIO_BASE		0x10010000
#define PAPIN			((0x10010000 - GPIO_BASE) >> 2)
#define PBPIN			((0x10010100 - GPIO_BASE) >> 2)
#define PCPIN			((0x10010200 - GPIO_BASE) >> 2)
#define PDPIN			((0x10010300 - GPIO_BASE) >> 2)
#define PEPIN			((0x10010400 - GPIO_BASE) >> 2)
#define PFPIN			((0x10010500 - GPIO_BASE) >> 2)

#define BTN_X			SDLK_SPACE
#define BTN_A			SDLK_LCTRL
#define BTN_B			SDLK_LALT
#define BTN_Y			SDLK_LSHIFT
#define BTN_L			SDLK_TAB
#define BTN_R			SDLK_BACKSPACE
#define BTN_START		SDLK_RETURN
#define BTN_SELECT		SDLK_ESCAPE
#define BTN_BACKLIGHT	SDLK_3
#define BTN_POWER		SDLK_END
#define BTN_UP			SDLK_UP
#define BTN_DOWN		SDLK_DOWN
#define BTN_LEFT		SDLK_LEFT
#define BTN_RIGHT		SDLK_RIGHT
#define GPIO_TV			SDLK_WORLD_0
#define GPIO_MMC		SDLK_WORLD_1
#define GPIO_USB		SDLK_WORLD_2
#define GPIO_PHONES		SDLK_WORLD_3

const int	HAlignLeft		= 1,
			HAlignRight		= 2,
			HAlignCenter	= 4,
			VAlignTop		= 8,
			VAlignBottom	= 16,
			VAlignMiddle	= 32;

SDL_RWops *rw;
TTF_Font *font = NULL;
SDL_Surface *screen = NULL;
// SDL_Surface* img = NULL;
SDL_Rect bgrect;
SDL_Event event;

SDL_Color txtColor = {200, 200, 220};
SDL_Color titleColor = {200, 200, 0};
SDL_Color subTitleColor = {0, 200, 0};
SDL_Color powerColor = {200, 0, 0};

volatile uint32_t *memregs;
volatile uint8_t memdev = 0;
uint16_t mmcPrev, mmcStatus;
uint16_t udcPrev, udcStatus;
uint16_t tvOutPrev, tvOutStatus;
uint16_t phonesPrev, phonesStatus;


static char buf[1024];
// SDL_Surface *StretchSurface = NULL;
uint32_t *mem;

uint8_t *keys;

extern uint8_t rwfont[];

int draw_text(int x, int y, const char buf[64], SDL_Color txtColor, int align) {
	DBG("");

	SDL_Surface *msg = TTF_RenderText_Blended(font, buf, txtColor);

	if (align & HAlignCenter) {
		x -= msg->w / 2;
	} else if (align & HAlignRight) {
		x -= msg->w;
	}

	if (align & VAlignMiddle) {
		y -= msg->h / 2;
	} else if (align & VAlignTop) {
		y -= msg->h;
	}

	SDL_Rect rect;
	rect.x = x;
	rect.y = y;
	rect.w = msg->w;
	rect.h = msg->h;
	SDL_BlitSurface(msg, NULL, screen, &rect);
	SDL_FreeSurface(msg);
	return msg->w;
}

void draw_background(const char buf[64]) {
	DBG("");
	// bgrect.w = img->w;
	// bgrect.h = img->h;
	// bgrect.x = (WIDTH - bgrect.w) / 2;
	// bgrect.y = (HEIGHT - bgrect.h) / 2;
	SDL_FillRect(screen, NULL, SDL_MapRGB(screen->format, 0, 0, 0));
	// SDL_BlitSurface(img, NULL, screen, &bgrect);

	// title
	draw_text(310, 4, "RetroFW", titleColor, VAlignBottom | HAlignRight);
	draw_text(10, 4, buf, titleColor, VAlignBottom);
	draw_text(10, 230, "SELECT+START: Exit", txtColor, VAlignMiddle | HAlignLeft);
}

void draw_point(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t r, uint32_t g, uint32_t b) {
	// DBG("");
	SDL_Rect rect;
	rect.w = w;
	rect.h = h;
	rect.x = x;// + bgrect.x;
	rect.y = y;// + bgrect.y;
	SDL_FillRect(screen, &rect, SDL_MapRGB(screen->format, r, g, b));
}

void quit(int err) {
	DBG("");
	system("sync");
	// if (rw != NULL) SDL_RWclose(rw);
	if (font) TTF_CloseFont(font);
	font = NULL;
	SDL_Quit();
	TTF_Quit();
	exit(err);
}

uint16_t getMMCStatus() {
	return (memdev > 0 && !(memregs[0x10500 >> 2] >> 0 & 0b1));
}

uint16_t getUDCStatus() {
	return (memdev > 0 && (memregs[0x10300 >> 2] >> 7 & 0b1));
}

uint16_t getTVOutStatus() {
	return (memdev > 0 && !(memregs[0x10300 >> 2] >> 25 & 0b1));
}

uint16_t getPhonesStatus() {
	return (memdev > 0 && !(memregs[0x10300 >> 2] >> 6 & 0b1));
}

void pushEvent() {
	SDL_Event user_event;
	user_event.type = SDL_KEYUP;
	// user_event.user.code = 2;
	SDL_PushEvent(&user_event);


	// SDL_Event event;
	// event.type = SDL_KEYDOWN;
	// event.key.state = SDL_PRESSED;
	// event.key.keysym.sym = (SDLKey)(action - UDC_CONNECT + SDLK_WORLD_0);
	// SDL_PushEvent(&event);
	// event.type = SDL_WAKEUPEVENT;
	// SDL_PushEvent(&event);
	// event.type = SDL_KEYUP;
	// event.key.state = SDL_RELEASED;
	// SDL_PushEvent(&event);
}


static int hw_input(void *ptr)
{
	while (1) {
		udcStatus = getUDCStatus();
		if (udcPrev != udcStatus) {
			keys[GPIO_USB] = udcPrev = udcStatus;
			pushEvent();
			// InputManager::pushEvent(udcStatus);
		}
		mmcStatus = getMMCStatus();
		if (mmcPrev != mmcStatus) {
			keys[GPIO_MMC] = mmcPrev = mmcStatus;
			pushEvent();
			// InputManager::pushEvent(mmcStatus);
		}

		tvOutStatus = getTVOutStatus();
		if (tvOutPrev != tvOutStatus) {
			keys[GPIO_TV] = tvOutPrev = tvOutStatus;
			pushEvent();
			// InputManager::pushEvent(tvOutStatus);
		}

		phonesStatus = getPhonesStatus();
		if (phonesPrev != phonesStatus) {
			keys[GPIO_PHONES] = phonesPrev = phonesStatus;
			pushEvent();
			// InputManager::pushEvent(tvOutStatus);
		}

		SDL_Delay(100);
	}

	return 0;
}

int main(int argc, char* argv[]) {
	DBG("");
	signal(SIGINT, &quit);
	signal(SIGSEGV,&quit);
	signal(SIGTERM,&quit);

	char title[64] = "";
	keys = SDL_GetKeyState(NULL);

	sprintf(title, "IO TESTER");
	printf("%s\n", title);

	setenv("SDL_NOMOUSE", "1", 1);

	int fd = open("/dev/tty0", O_RDONLY);
	if (fd > 0) {
		ioctl(fd, VT_UNLOCKSWITCH, 1);
		ioctl(fd, KDSETMODE, KD_TEXT);
		ioctl(fd, KDSKBMODE, K_XLATE);
	}
	close(fd);

	if (SDL_Init(SDL_INIT_VIDEO) != 0) {
		printf("Could not initialize SDL: %s\n", SDL_GetError());
		return -1;
	}
	SDL_PumpEvents();
	SDL_ShowCursor(0);

	// screen = SDL_SetVideoMode(WIDTH, HEIGHT, 16, SDL_HWSURFACE | SDL_DOUBLEBUF);
	// screen = SDL_SetVideoMode(WIDTH, HEIGHT, 16, SDL_HWSURFACE);
	screen = SDL_SetVideoMode(WIDTH, HEIGHT, 16, SDL_SWSURFACE);

	// SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);
	SDL_EnableKeyRepeat(1, 1);
	// SDL_Delay(50);
	// SDL_PumpEvents();


	if (TTF_Init() == -1) {
		printf("failed to TTF_Init\n");
		return -1;
	}
	rw = SDL_RWFromMem(rwfont, sizeof(rwfont));
	font = TTF_OpenFontRW(rw, 1, 8);
	TTF_SetFontHinting(font, TTF_HINTING_NORMAL);
	TTF_SetFontOutline(font, 0);

	// SDL_Surface* _img = IMG_Load("backdrop.png");
	// img = SDL_DisplayFormat(_img);
	// SDL_FreeSurface(_img);

#if defined(TARGET_RETROFW)
	memdev = open("/dev/mem", O_RDWR);
	if (memdev > 0) {
		memregs = (uint32_t*)mmap(0, 0x20000, PROT_READ | PROT_WRITE, MAP_SHARED, memdev, 0x10000000);

		SDL_Thread *thread = SDL_CreateThread(hw_input, (void *)NULL);

		if (memregs == MAP_FAILED) {
			close(memdev);
		}
	}
#endif

	while (1) {
		SDL_PollEvent(&event);
		draw_background(title);

		int nextline = 0;

		for (int x = 3; x >= 0; x--) {
			sprintf(buf, "%d", x % 10);
			draw_text((3-x) * 90 + 31, 24, buf, subTitleColor, VAlignBottom);
		}

		for (int x = 31; x >= 0; x--) {
			sprintf(buf, "%d", x % 10);
			draw_text((31 - x) * 9 + 22, 35, buf, subTitleColor, VAlignBottom);
		}

		for (int y = 0; y <= 5; y++) {
			sprintf(buf, "%X", 10 + y);
			draw_text(12, y * 9 + 45, buf, subTitleColor, VAlignBottom);
		}

		uint32_t n = 0x10000;
		for (int y = 0; y < 6; y++) {
			for (int x = 31; x >= 0; x--) {
				int on = !!(memregs[n >> 2] & 1 << x);

				draw_point((31 - x) * 9 + 20, y * 9 + 50, 7, 7, 255 * on, 255 * !on, 0);
			}
			n += 0x100;
		}



// printf("A: 0x%08x ", memregs[0x10000 >> 2]);
// printf("B: 0x%08x ", memregs[0x10100 >> 2]);
// printf("C: 0x%08x ", memregs[0x10200 >> 2]);
// printf("D: 0x%08x ", memregs[0x10300 >> 2]);
// printf("E: 0x%08x ", memregs[0x10400 >> 2]);
// printf("F: 0x%08x ", memregs[0x10500 >> 2]);


		sprintf(buf, "A: 0x%08x ", memregs[0x10000 >> 2]); draw_text(12, 105, buf, subTitleColor, VAlignBottom);
		sprintf(buf, "B: 0x%08x ", memregs[0x10100 >> 2]); draw_text(12, 115, buf, subTitleColor, VAlignBottom);
		sprintf(buf, "C: 0x%08x ", memregs[0x10200 >> 2]); draw_text(12, 125, buf, subTitleColor, VAlignBottom);
		sprintf(buf, "D: 0x%08x ", memregs[0x10300 >> 2]); draw_text(12, 135, buf, subTitleColor, VAlignBottom);
		sprintf(buf, "E: 0x%08x ", memregs[0x10400 >> 2]); draw_text(12, 145, buf, subTitleColor, VAlignBottom);
		sprintf(buf, "F: 0x%08x ", memregs[0x10500 >> 2]); draw_text(12, 155, buf, subTitleColor, VAlignBottom);



		if (event.key.keysym.sym) {
			sprintf(buf, "Last key: %s", SDL_GetKeyName(event.key.keysym.sym));
			draw_text(bgrect.x + 104, 105 + nextline, buf, subTitleColor, VAlignBottom);
			nextline += 10;

			sprintf(buf, "Keysym.sym: %d", event.key.keysym.sym);
			draw_text(bgrect.x + 104, 105 + nextline, buf, subTitleColor, VAlignBottom);
			nextline += 10;

			sprintf(buf, "Keysym.scancode: %d", event.key.keysym.scancode);
			draw_text(bgrect.x + 104, 105 + nextline, buf, subTitleColor, VAlignBottom);
			nextline += 10;
		}


		SDL_Flip(screen);

		if (event.type == SDL_KEYDOWN && keys[BTN_SELECT] && keys[BTN_START]) {
			break;
		}

		// if (event.type == SDL_KEYUP) {
		// 	break;
		// }
		SDL_Delay(10);
	}

	if (memdev > 0) close(memdev);

	quit(0);
	return 0;
}

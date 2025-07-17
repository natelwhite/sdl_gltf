#define SDL_MAIN_USE_CALLBACKS

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_log.h>
#include "App.hpp"

App ctx;

SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[]) {
	if (!SDL_Init(SDL_INIT_VIDEO)) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_Init failed:\n\t%s", SDL_GetError());
		return SDL_APP_FAILURE;
	}
	if (ctx.init() != 0) { return SDL_APP_FAILURE; }
	*appstate = &ctx;
	return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void* appstate) {
	App *ctx { static_cast<App*>(appstate) };
	return ctx->iterate();
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
	switch(event->type) {
	case SDL_EVENT_QUIT:
			return SDL_APP_SUCCESS;
		break;
	case SDL_EVENT_KEY_DOWN:
		switch(event->key.key) {
		case SDLK_ESCAPE: // quit on esc key
			return SDL_APP_SUCCESS;
			break;
		}
		break;
	}
	App *ctx { static_cast<App*>(appstate) };
	return ctx->event(event);
}

void SDL_AppQuit(void* appstate, SDL_AppResult result) {
	App *ctx { static_cast<App*>(appstate) };
	ctx->quit();
}

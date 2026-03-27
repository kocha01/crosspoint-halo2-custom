#pragma once
// Forward declaration for SimDisplay SDL setup function
struct SDL_Window;
struct SDL_Renderer;
struct SDL_Texture;

void simDisplaySetSDL(SDL_Window* window, SDL_Renderer* renderer, SDL_Texture* texture);

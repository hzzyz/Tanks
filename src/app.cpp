#include "app.h"
#include "appconfig.h"
#include "engine/engine.h"
#include "app_state/game.h"

#include <iostream>
#include <stdlib.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>

App::App()
{
    m_window = nullptr;
}

App::~App()
{
    if(m_app_state != nullptr)
        delete m_app_state;
}


void App::run()
{
    is_running = true;
    //inicjalizacja SDL i utworzenie okan


    if(SDL_Init(SDL_INIT_VIDEO) == 0)
    {
        m_window = SDL_CreateWindow("TANKS", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                    AppConfig::windows_rect.w, AppConfig::windows_rect.h, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);

        if(m_window == nullptr) return;

        if(!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG)) return;
        if(TTF_Init() == -1) return;


        Engine& engine = Engine::getEngine();
        engine.initModules();
        engine.getRenderer()->loadTexture(m_window);
        engine.getRenderer()->loadFont();

        m_app_state = new Game;

        double FPS;
        Uint32 time1, time2, dt, fps_time = 0, fps_count = 0, delay = 15;
        time1 = SDL_GetTicks();
        while(is_running)
        {
            time2 = SDL_GetTicks();
            dt = time2 - time1;
            time1 = time2;

            if(m_app_state->finished())
            {
                AppState* new_state = m_app_state->nextState();
                delete m_app_state;
                m_app_state = new_state;
            }

            eventProces();

            m_app_state->update(dt);
            m_app_state->draw();

            SDL_Delay(delay);

            //FPS
            fps_time += dt; fps_count++;
            if(fps_time > 200)
            {
                FPS = (double)fps_count / fps_time * 1000;
                if(FPS > 60) delay++;
                else if(delay > 0) delay--;
                fps_time = 0; fps_count = 0;
                 //std::cout << dt << " " << FPS << " "  <<delay<< std::endl;
            }
        }

        engine.destroyModules();
    }

    SDL_DestroyWindow(m_window);
    m_window = nullptr;
    TTF_Quit();
    IMG_Quit();
    SDL_Quit();
}

void App::eventProces()
{
    SDL_Event event;
    while(SDL_PollEvent(&event))
    {
        switch(event.type)
        {
        case SDL_QUIT:
            is_running = false;
         break;

       case SDL_KEYDOWN:
            switch(event.key.keysym.sym)
            {
            case SDLK_ESCAPE:
                is_running = false;
                break;
            }
            break;
        case SDL_WINDOWEVENT:
            switch(event.window.event)
            {
                case SDL_WINDOWEVENT_RESIZED:
                case SDL_WINDOWEVENT_MAXIMIZED:
                case SDL_WINDOWEVENT_RESTORED:
                case SDL_WINDOWEVENT_SHOWN:
                    AppConfig::windows_rect.w = event.window.data1;
                    AppConfig::windows_rect.h = event.window.data2;
                    Engine::getEngine().getRenderer()->setScale((float)AppConfig::windows_rect.w / (AppConfig::map_rect.w + AppConfig::status_rect.w),
                                                                (float)AppConfig::windows_rect.h / AppConfig::map_rect.h);
                break;
            }
            break;
        }
        m_app_state->eventProces(&event);
    }
}

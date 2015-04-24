#include "game.h"
#include "../engine/engine.h"
#include "../objects/objectfactory.h"
#include "../appconfig.h"

#include <SDL2/SDL.h>
#include <iostream>
#include <fstream>
#include <algorithm>


Game::Game()
{
    m_level_columns_count = 0;
    m_level_rows_count = 0;
    m_current_level = 0;
    m_eagle = nullptr;

    nextLevel();
}

Game::~Game()
{
    clearLevel();
}

void Game::draw()
{
    Renderer* renderer = Engine::getEngine().getRenderer();
    renderer->clear();

    if(m_level_start_screen)
    {
        std::string level_name = "STAGE " + uIntToString(m_current_level);
        renderer->drawText(nullptr, level_name, {255, 255, 255, 255});
    }
    else
    {
        renderer->drawRect(&AppConfig::map_rect, {0, 0, 0, 0}, true);
        for(auto row : m_level)
            for(auto item : row)
                if(item != nullptr) item->draw();

        for(auto enemy : m_enemies) enemy->draw();
        for(auto player : m_players) player->draw();

        m_eagle->draw();

        for(auto r : m_rec)
            renderer->drawRect(r, {0, 0, 255});

        for(auto bush : m_bushes)
            if(bush != nullptr) bush->draw();

        if(m_game_over)
        {
            SDL_Point pos;
            pos.x = -1;
            pos.y = m_game_over_position;
            renderer->drawText(&pos, AppConfig::game_over_text, {255, 10, 10, 255});
        }
    }

    renderer->flush();
}

void Game::update(Uint32 dt)
{
    if(dt > 40) return;

    if(m_level_start_screen)
    {
        if(m_level_start_time > AppConfig::level_start_time)
            m_level_start_screen = false;

        m_level_start_time += dt;
    }
    else
    {

        std::vector<Player*>::iterator pl1, pl2;
        std::vector<Enemy*>::iterator en1, en2;

        //sprawdzenie kolizji czołgów graczy ze sobą
        for(pl1 = m_players.begin(); pl1 != m_players.end(); pl1++)
            for(pl2 = pl1 + 1; pl2 != m_players.end(); pl2++)
                checkCollisionTwoTanks(*pl1, *pl2, dt);


        //sprawdzenie kolizji czołgów przeciwników ze sobą
        for(en1 = m_enemies.begin(); en1 != m_enemies.end(); en1++)
             for(en2 = en1 + 1; en2 != m_enemies.end(); en2++)
                checkCollisionTwoTanks(*en1, *en2, dt);

        //sprawdzenie kolizji kuli z lewelem
        for(auto enemy : m_enemies) checkCollisionBulletWithLevel(enemy->bullet);
        for(auto player : m_players) checkCollisionBulletWithLevel(player->bullet);


        for(auto player : m_players)
            for(auto enemy : m_enemies)
            {
                //sprawdzenie kolizji czołgów przeciwników z graczami
                checkCollisionTwoTanks(player, enemy, dt);
                //sprawdzenie kolizji pocisku gracza z przeciwnikiem
                checkCollisionBulletWithTanks(player->bullet, enemy);
                //sprawdzenie kolizji pocisku gracza z pociskiem przeciwnika
                checkCollisionTwoBullets(player->bullet, enemy->bullet);
            }

        //sprawdzenie kolizji pocisku przeciknika z graczem
        for(auto enemy : m_enemies)
            for(auto player : m_players)
                checkCollisionBulletWithTanks(enemy->bullet, player);

        //Sprawdzenie kolizji czołgów z poziomem
        for(auto enemy : m_enemies) checkCollisionTankWithLevel(enemy, dt);
        for(auto player : m_players) checkCollisionTankWithLevel(player, dt);


        //Update wszystkich obiektów
        for(auto enemy : m_enemies) enemy->update(dt);
        for(auto player : m_players) player->update(dt);

        m_eagle->update(dt);

        for(auto row : m_level)
            for(auto item : row)
                if(item != nullptr) item->update(dt);

        for(auto bush : m_bushes)
            if(bush != nullptr) bush->update(dt);

        //usunięcie niepotrzebnych czołgów
        m_enemies.erase(std::remove_if(m_enemies.begin(), m_enemies.end(), [](Enemy*e){if(e->to_erase) {delete e; return true;} return false;}), m_enemies.end());
        m_players.erase(std::remove_if(m_players.begin(), m_players.end(), [](Player*p){if(p->to_erase) {delete p; return true;} return false;}), m_players.end());

        if(m_players.empty() && !m_game_over)
        {
            m_eagle->destroy();
            m_game_over_position = AppConfig::map_rect.h;
            m_game_over = true;
        }

        if(m_game_over)
        {
            if(m_game_over_position < 10) m_finished = true;
            else m_game_over_position -= AppConfig::game_over_entry_speed * dt;
        }
    }
}

void Game::eventProcess(SDL_Event *ev)
{
    if(m_players.empty()) return;
    switch(ev->type)
    {
    case SDL_KEYDOWN:
        for(auto player : m_players)
        {
            if(player->player_keys.up == ev->key.keysym.scancode)
            {
                player->setDirection(D_UP);
                player->speed = player->default_speed;
            }
            else if(player->player_keys.down == ev->key.keysym.scancode)
            {
                player->setDirection(D_DOWN);
                player->speed = player->default_speed;
            }
            else if(player->player_keys.left == ev->key.keysym.scancode)
            {
                player->setDirection(D_LEFT);
                player->speed = player->default_speed;
            }
            else if(player->player_keys.right == ev->key.keysym.scancode)
            {
                player->setDirection(D_RIGHT);
                player->speed = player->default_speed;
            }
            else if(player->player_keys.fire == ev->key.keysym.scancode)
            {
                player->fire();
            }
        }
        switch(ev->key.keysym.sym)
        {
        case SDLK_r:
            m_players.at(0)->respawn();
            break;
        case SDLK_n:
            nextLevel();
            break;
        case SDLK_b:
            m_current_level -= 2;
            nextLevel();
            break;
        case SDLK_1:
            m_enemies.push_back(new Enemy(1, 1, ST_TANK_A));
            break;
        case SDLK_2:
            m_enemies.push_back(new Enemy(50, 1, ST_TANK_B));
            break;
        case SDLK_3:
            m_enemies.push_back(new Enemy(100, 1, ST_TANK_C));
            break;
        case SDLK_4:
            m_enemies.push_back(new Enemy(150, 1, ST_TANK_D));
            break;
        }
        break;
    }
}

/*
. = puste pole
# = murek
@ = kamień
% = krzaki
~ = woda
- = lód
 */

void Game::loadLevel(std::string path)
{
    clearLevel();
    std::fstream level(path, std::ios::in);
    std::string line;
    int j = -1;

    if(level.is_open())
    {
        while(!level.eof())
        {
            std::getline(level, line);
            std::vector<Object*> row;
            j++;
            for(unsigned i = 0; i < line.size(); i++)
            {
                Object* obj;
                switch(line.at(i))
                {
                case '#' : obj = ObjectFactory::Create(i * AppConfig::tile_rect.w, j * AppConfig::tile_rect.h, ST_BRICK_WALL); break;
                case '@' : obj = ObjectFactory::Create(i * AppConfig::tile_rect.w, j * AppConfig::tile_rect.h, ST_STONE_WALL); break;
                case '%' : m_bushes.push_back(ObjectFactory::Create(i * AppConfig::tile_rect.w, j * AppConfig::tile_rect.h, ST_BUSH)); obj =  nullptr; break;
                case '~' : obj = ObjectFactory::Create(i * AppConfig::tile_rect.w, j * AppConfig::tile_rect.h, ST_WATER); break;
                case '-' : obj = ObjectFactory::Create(i * AppConfig::tile_rect.w, j * AppConfig::tile_rect.h, ST_ICE); break;
                default: obj = nullptr;
                }
                row.push_back(obj);
            }
            m_level.push_back(row);
        }
    }

    m_level_rows_count = m_level.size();
    if(m_level_rows_count)
        m_level_columns_count = m_level.at(0).size();
    else m_level_columns_count = 0;

    //tworzymy orzełka
    m_eagle = new Eagle(12 * AppConfig::tile_rect.w, (m_level_rows_count - 2) * AppConfig::tile_rect.h);

    //wyczyszczenie miejsca orzełeka
    for(int i = 12; i < 14 && i < m_level_columns_count; i++)
    {
        for(int j = m_level_rows_count - 2; j < m_level_rows_count; j++)
        {
            if(m_level.at(j).at(i) != nullptr)
            {
                delete m_level.at(j).at(i);
                m_level.at(j).at(i) = nullptr;
            }
        }
    }
}

bool Game::finished() const
{
    return m_finished;
}

AppState *Game::nextState()
{
    Game* new_game = new Game();
    return new_game;
}

void Game::clearLevel()
{
    for(auto enemy : m_enemies) delete enemy;
    m_enemies.clear();

    for(auto player : m_players) delete player;
    m_players.clear();

    for(auto row : m_level)
    {
        for(auto item : row) if(item != nullptr) delete item;
        row.clear();
    }
    m_level.clear();

    for(auto bush : m_bushes) if(bush != nullptr) delete bush;
    m_bushes.clear();

    if(m_eagle != nullptr) delete m_eagle;
    m_eagle = nullptr;
}

void Game::checkCollisionTankWithLevel(Tank* tank, Uint32 dt)
{
//    for(auto r : m_rec) delete r;
//    m_rec.clear();
    if(tank->to_erase) return;

    int row_start, row_end;
    int column_start, column_end;

    SDL_Rect pr, *lr;
    Object* o;

    //========================kolizja z elementami mapy========================
    switch(tank->direction)
    {
    case D_UP:
        row_end = tank->collision_rect.y / AppConfig::tile_rect.h;
        row_start = row_end - 1;
        column_start = tank->collision_rect.x / AppConfig::tile_rect.w - 1;
        column_end = (tank->collision_rect.x + tank->collision_rect.w) / AppConfig::tile_rect.w + 1;
        break;
    case D_RIGHT:
        column_start = (tank->collision_rect.x + tank->collision_rect.w) / AppConfig::tile_rect.w;
        column_end = column_start + 1;
        row_start = tank->collision_rect.y / AppConfig::tile_rect.h - 1;
        row_end = (tank->collision_rect.y + tank->collision_rect.h) / AppConfig::tile_rect.h + 1;
        break;
    case D_DOWN:
        row_start = (tank->collision_rect.y + tank->collision_rect.h)/ AppConfig::tile_rect.h;
        row_end = row_start + 1;
        column_start = tank->collision_rect.x / AppConfig::tile_rect.w - 1;
        column_end = (tank->collision_rect.x + tank->collision_rect.w) / AppConfig::tile_rect.w + 1;
        break;
    case D_LEFT:
        column_end = tank->collision_rect.x / AppConfig::tile_rect.w;
        column_start = column_end - 1;
        row_start = tank->collision_rect.y / AppConfig::tile_rect.h - 1;
        row_end = (tank->collision_rect.y + tank->collision_rect.h) / AppConfig::tile_rect.h + 1;
        break;
    }
    if(column_start < 0) column_start = 0;
    if(row_start < 0) row_start = 0;
    if(column_end >= m_level_columns_count) column_end = m_level_columns_count - 1;
    if(row_end >= m_level_rows_count) row_end = m_level_rows_count - 1;

    pr = tank->nextCollisionRect(dt);
    SDL_Rect intersect_rect;


    for(int i = row_start; i <= row_end; i++)
        for(int j = column_start; j <= column_end ;j++)
        {
            if(tank->stop) break;
            o = m_level.at(i).at(j);
            if(o == nullptr) continue;

            lr = &o->collision_rect;

            intersect_rect = intersectRect(lr, &pr);
            if(intersect_rect.w > 0 && intersect_rect.h > 0)
            {
                if(o->type == ST_ICE)
                {
                    if(intersect_rect.w > 10 && intersect_rect.h > 10)
                       tank->setFlag(TSF_ON_ICE);
                    continue;
                }
                else
                    tank->collide(intersect_rect);
                break;
            }
        }

    //========================kolizja z granicami mapy========================
    SDL_Rect outside_map_rect;
    //prostokąt po lewej stronie mapy
    outside_map_rect.x = -AppConfig::tile_rect.w;
    outside_map_rect.y = -AppConfig::tile_rect.h;
    outside_map_rect.w = AppConfig::tile_rect.w;
    outside_map_rect.h = AppConfig::map_rect.h + 2 * AppConfig::tile_rect.h;
    intersect_rect = intersectRect(&outside_map_rect, &pr);
    if(intersect_rect.w > 0 && intersect_rect.h > 0)
        tank->collide(intersect_rect);

    //prostokąt po prawej stronie mapy
    outside_map_rect.x = AppConfig::map_rect.w;
    outside_map_rect.y = -AppConfig::tile_rect.h;
    outside_map_rect.w = AppConfig::tile_rect.w;
    outside_map_rect.h = AppConfig::map_rect.h + 2 * AppConfig::tile_rect.h;
    intersect_rect = intersectRect(&outside_map_rect, &pr);
    if(intersect_rect.w > 0 && intersect_rect.h > 0)
        tank->collide(intersect_rect);

    //prostokąt po górnej stronie mapy
    outside_map_rect.x = 0;
    outside_map_rect.y = -AppConfig::tile_rect.h;
    outside_map_rect.w = AppConfig::map_rect.w;
    outside_map_rect.h = AppConfig::tile_rect.h;
    intersect_rect = intersectRect(&outside_map_rect, &pr);
    if(intersect_rect.w > 0 && intersect_rect.h > 0)
        tank->collide(intersect_rect);

    //prostokąt po dolnej stronie mapy
    outside_map_rect.x = 0;
    outside_map_rect.y = AppConfig::map_rect.h;
    outside_map_rect.w = AppConfig::map_rect.w;
    outside_map_rect.h = AppConfig::tile_rect.h;
    intersect_rect = intersectRect(&outside_map_rect, &pr);
    if(intersect_rect.w > 0 && intersect_rect.h > 0)
        tank->collide(intersect_rect);


   //========================kolizja z orzełkiem========================
    intersect_rect = intersectRect(&m_eagle->collision_rect, &pr);
    if(intersect_rect.w > 0 && intersect_rect.h > 0)
        tank->collide(intersect_rect);
}

void Game::checkCollisionTwoTanks(Tank* tank1, Tank* tank2, Uint32 dt)
{
    SDL_Rect cr1 = tank1->nextCollisionRect(dt);
    SDL_Rect cr2 = tank2->nextCollisionRect(dt);
    SDL_Rect intersect_rect = intersectRect(&cr1, &cr2);

    if(intersect_rect.w > 0 && intersect_rect.h > 0)
    {
        tank1->collide(intersect_rect);
        tank2->collide(intersect_rect);
    }
}

void Game::checkCollisionBulletWithLevel(Bullet* bullet)
{
    if(bullet == nullptr) return;
    if(bullet->collide) return;

    int row_start, row_end;
    int column_start, column_end;

    SDL_Rect* br, *lr;
    SDL_Rect intersect_rect;
    Object* o;

    //========================kolizja z elementami mapy========================
    switch(bullet->direction)
    {
    case D_UP:
        row_start = row_end = bullet->collision_rect.y / AppConfig::tile_rect.h;
        column_start = bullet->collision_rect.x / AppConfig::tile_rect.w;
        column_end = (bullet->collision_rect.x + bullet->collision_rect.w) / AppConfig::tile_rect.w;
        break;
    case D_RIGHT:
        column_start = column_end = (bullet->collision_rect.x + bullet->collision_rect.w) / AppConfig::tile_rect.w;
        row_start = bullet->collision_rect.y / AppConfig::tile_rect.h;
        row_end = (bullet->collision_rect.y + bullet->collision_rect.h) / AppConfig::tile_rect.h;
        break;
    case D_DOWN:
        row_start = row_end = (bullet->collision_rect.y + bullet->collision_rect.h)/ AppConfig::tile_rect.h;
        column_start = bullet->collision_rect.x / AppConfig::tile_rect.w;
        column_end = (bullet->collision_rect.x + bullet->collision_rect.w) / AppConfig::tile_rect.w;
        break;
    case D_LEFT:
        column_start = column_end = bullet->collision_rect.x / AppConfig::tile_rect.w;
        row_start = bullet->collision_rect.y / AppConfig::tile_rect.h;
        row_end = (bullet->collision_rect.y + bullet->collision_rect.h) / AppConfig::tile_rect.h;
        break;
    }
    if(column_start < 0) column_start = 0;
    if(row_start < 0) row_start = 0;
    if(column_end >= m_level_columns_count) column_end = m_level_columns_count - 1;
    if(row_end >= m_level_rows_count) row_end = m_level_rows_count - 1;

    br = &bullet->collision_rect;

    for(int i = row_start; i <= row_end; i++)
        for(int j = column_start; j <= column_end; j++)
        {
            o = m_level.at(i).at(j);
            if(o == nullptr) continue;
            if(o->type == ST_ICE || o->type == ST_WATER) continue;

            lr = &o->collision_rect;
            intersect_rect = intersectRect(lr, br);

            if(intersect_rect.w > 0 && intersect_rect.h > 0)
            {
                if(bullet->increased_damage)
                {
                    delete o;
                    m_level.at(i).at(j) = nullptr;
                }
                else if(o->type == ST_BRICK_WALL)
                {
                    Brick* brick = dynamic_cast<Brick*>(o);
                    brick->bulletHit(bullet->direction);
                    if(brick->to_erase)
                    {
                        delete brick;
                        m_level.at(i).at(j) = nullptr;
                    }
                }

                bullet->destroy();
            }
        }

    //========================kolizja z granicami mapy========================
    if(br->x < 0 || br->y < 0 || br->x + br->w > AppConfig::map_rect.w || br->y + br->h > AppConfig::map_rect.h)
    {
        bullet->destroy();
    }
    //========================kolizja z orzełkiem========================
    if(m_eagle->type == ST_EAGLE && !m_game_over)
    {
        intersect_rect = intersectRect(&m_eagle->collision_rect, br);
        if(intersect_rect.w > 0 && intersect_rect.h > 0)
        {
            bullet->destroy();
            m_eagle->destroy();
            m_game_over_position = AppConfig::map_rect.h;
            m_game_over = true;
        }
    }
}

void Game::checkCollisionBulletWithTanks(Bullet *bullet, Tank *tank)
{
    if(bullet == nullptr) return;
    if(bullet->collide) return;
    if(tank->to_erase) return;

    SDL_Rect intersect_rect = intersectRect(&bullet->collision_rect, &tank->collision_rect);

    if(intersect_rect.w > 0 && intersect_rect.h > 0)
    {
        bullet->destroy();
        tank->destroy();
    }
}

void Game::checkCollisionTwoBullets(Bullet *bullet1, Bullet *bullet2)
{
    if(bullet1 == nullptr || bullet2 == nullptr) return;
    if(bullet1->to_erase || bullet2->to_erase) return;

    SDL_Rect intersect_rect = intersectRect(&bullet1->collision_rect, &bullet2->collision_rect);

    if(intersect_rect.w > 0 && intersect_rect.h > 0)
    {
        bullet1->destroy();
        bullet2->destroy();
    }
}

std::string Game::uIntToString(unsigned num)
{
    std::string buf;
    for(; num; num /= 10) buf = "0123456789abcdef"[num % 10] + buf;
    return buf;
}

void Game::nextLevel()
{
    m_current_level++;
    if(m_current_level > 35) m_current_level = 1;
    if(m_current_level < 1) m_current_level = 35;

    m_level_start_screen = true;
    m_level_start_time = 0;
    m_game_over = false;
    m_finished = false;

    std::string level_path = AppConfig::levels_path + uIntToString(m_current_level);

    loadLevel(level_path);
//    loadLevel("levels/1a");

    Player* p1 = new Player(AppConfig::player_starting_point.at(0).x, AppConfig::player_starting_point.at(0).y, ST_PLAYER_1);
    Player* p2 = new Player(AppConfig::player_starting_point.at(1).x, AppConfig::player_starting_point.at(1).y, ST_PLAYER_2);
    p1->player_keys = AppConfig::player_keys.at(0);
    p2->player_keys = AppConfig::player_keys.at(1);
    m_players.push_back(p1);
    m_players.push_back(p2);

    m_enemies.push_back(new Enemy(AppConfig::enemy_starting_point.at(0).x, AppConfig::enemy_starting_point.at(0).y, ST_TANK_A));
    m_enemies.push_back(new Enemy(AppConfig::enemy_starting_point.at(1).x, AppConfig::enemy_starting_point.at(1).y, ST_TANK_C));
    m_enemies.push_back(new Enemy(AppConfig::enemy_starting_point.at(2).x, AppConfig::enemy_starting_point.at(2).y, ST_TANK_B));
}

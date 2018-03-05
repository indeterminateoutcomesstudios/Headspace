//
// Created by main on 2/28/18.
//

#ifndef HEADSPACE_GAME_H
#define HEADSPACE_GAME_H


#include "World.h"
#include "Console.h"

class Game {
private:
    World* world;
    Console* console;
public:
    void logMessage(std::wstring message, message_type type=info);
    bool movePlayer(int dir) {
        return world->getPlayer()->move(dir);
    }
    void render(sf::RenderWindow& window);
    Game();
    ~Game();
};


#endif //HEADSPACE_GAME_H
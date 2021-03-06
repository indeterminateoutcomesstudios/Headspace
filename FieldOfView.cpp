//
// Created by KunstDerFuge on 3/4/18.
// C++ adaptation of Mingos' Restrictive Precise Angle Shadowcasting V1.1 from libtcod.
//

#include <iostream>
#include <SFML/Graphics/RenderWindow.hpp>
#include "FieldOfView.h"
#include "Player.h"

using namespace std;

#define MAX(a,b) ((a)<(b)?(b):(a))
#define MIN(a,b) ((a)>(b)?(b):(a))

bool FieldOfView::isVisible(long x, long y) {
    return cells[mapCoordToIndex(x, y)].visible;
}

void FieldOfView::markAsVisible(long x, long y) {
    cells[mapCoordToIndex(x, y)].visible = true;
}

int FieldOfView::getViewWidthTiles(int resolutionX, int tileWidth) {
    int viewWidthInTiles = resolutionX / tileWidth;
    return viewWidthInTiles + 2;
}

int FieldOfView::getViewHeightTiles(int resolutionY, int tileWidth) {
    int viewHeightInTiles = resolutionY / tileWidth;
    return viewHeightInTiles + 2;
}

FieldOfView::FieldOfView(Player* player, const sf::RenderTexture& mapWindow, int tileWidth, WorldMap* worldMap) {
    start_angle = nullptr;
    end_angle = nullptr;
    allocated = 0;
    auto mapSize = mapWindow.getSize();
    this->player = player;
    cout << "Window size at FOV creation: " << mapSize.x << "x" << mapSize.y << endl;
    this->width = getViewWidthTiles(mapSize.x, tileWidth);
    this->height = getViewHeightTiles(mapSize.y, tileWidth);
    if (width % 2 == 0) --width;
    if (height % 2 == 0) --height;
    cout << "FOV map dimensions: " << this->width << "x" << this->height << endl;
    cout << "Creating FOV: (" << width << ", " << height << ")" << endl;
    cout << "FOV Top left: (" << left << ", " << top << ")" << endl;
    cells = vector<Cell>(static_cast<unsigned long>(width * height));
    cout << "Size of cells vector: " << cells.size() << endl;
    this->worldMap = worldMap;
}

void FieldOfView::invalidate(int tileWidth, const sf::RenderTexture& mapWindow) {
    auto mapSize = mapWindow.getSize();
    this->width = getViewWidthTiles(mapSize.x, tileWidth);
    this->height = getViewHeightTiles(mapSize.y, tileWidth);
    if (width % 2 == 0) --width;
    if (height % 2 == 0) --height;
    cout << "FOV is now " << width << "x" << height << endl;
    cells = vector<Cell>(static_cast<unsigned long>(width * height));
}

long FieldOfView::mapCoordToIndex(long x, long y) {
    x -= this->left;
    y -= this->top;
    auto out = (y * width) + x;
    if (out > cells.size())
        return false;
    return out;
}

void FieldOfView::update() {
    long square = 0;
    auto playerLocation = player->getLocation();
    auto focus = player->getFocus();
    this->left = focus.x - width / 2;
    this->top = focus.y - height / 2;
    for (long y = top; y < top + height; ++y) {
        for (long x = left; x < left + width; ++x) {
            cells[square].transparent = !worldMap->isOpaque(Point(x, y));
            ++square;
        }
    }
    for (auto cell : cells) {
        cell.visible = false;
    }
    int max_obstacles;
    /*first, zero the FOV map */
    for(long c = cells.size() - 1; c >= 0; --c) cells[c].visible = false;

    /*calculate an approximated (excessive, just in case) maximum number of obstacles per octant */
    max_obstacles = static_cast<int>(cells.size() / 7);

    if (max_obstacles > allocated) {
        allocated = max_obstacles;
        delete start_angle;
        delete end_angle;
        start_angle = new double[max_obstacles];
        end_angle = new double[max_obstacles];
    }

    /*set PC's position as visible */
    cells[mapCoordToIndex(playerLocation.x, playerLocation.y)].visible = true;

    /*compute the 4 quadrants of the map */
    int centerX = static_cast<int>(playerLocation.x - left);
    int centerY = static_cast<int>(playerLocation.y - top);
    computeQuadrant(centerX, centerY, 1, 1);
    computeQuadrant(centerX, centerY, 1, -1);
    computeQuadrant(centerX, centerY, -1, 1);
    computeQuadrant(centerX, centerY, -1, -1);
}

void FieldOfView::computeQuadrant(int player_x, int player_y, int dx, int dy) {
    FieldOfView* m = this;
    bool light_walls = true;
    int max_radius = 200;
    /*octant: vertical edge */
    {
        int iteration = 1; /*iteration of the algo for this octant */
        bool done = false;
        int total_obstacles = 0;
        int obstacles_in_last_line = 0;
        double min_angle = 0.0;
        long x,y;

        /*do while there are unblocked slopes left and the algo is within the map's boundaries
          scan progressive lines/columns from the PC outwards */
        y = player_y+dy; /*the outer slope's coordinates (first processed line) */
        if (y < 0 || y >= m->height) done = true;
        while(!done) {
            /*process cells in the line */
            auto slopes_per_cell = 1.0 / double(iteration);
            double half_slopes = slopes_per_cell * 0.5;
            auto processed_cell = int((min_angle + half_slopes) / slopes_per_cell);
            long minx = MAX(0,player_x-iteration), maxx = MIN(m->width-1,player_x+iteration);
            done = true;
            for (x = player_x + (processed_cell * dx); x >= minx && x <= maxx; x+=dx) {
                long c = x + (y * m->width);
                /*calculate slopes per cell */
                bool visible = true;
                bool extended = false;
                double centre_slope = (double)processed_cell * slopes_per_cell;
                double start_slope = centre_slope - half_slopes;
                double end_slope = centre_slope + half_slopes;
                if (obstacles_in_last_line > 0 && !m->cells[c].visible) {
                    int idx = 0;
                    if ((!m->cells[c - (m->width * dy)].visible || !m->cells[c - (m->width * dy)].transparent) && (x - dx >= 0 && x - dx < m->width && (
                            !m->cells[c - (m->width * dy) - dx].visible || !m->cells[c - (m->width * dy) - dx].transparent))) visible = false;
                    else while(visible && idx < obstacles_in_last_line) {
                            if (start_angle[idx] > end_slope || end_angle[idx] < start_slope) {
                                ++idx;
                            }
                            else {
                                if (m->cells[c].transparent) {
                                    if (centre_slope > start_angle[idx] && centre_slope < end_angle[idx])
                                        visible = false;
                                }
                                else {
                                    if (start_slope >= start_angle[idx] && end_slope <= end_angle[idx])
                                        visible = false;
                                    else {
                                        start_angle[idx] = MIN(start_angle[idx],start_slope);
                                        end_angle[idx] = MAX(end_angle[idx],end_slope);
                                        extended = true;
                                    }
                                }
                                ++idx;
                            }
                        }
                }
                if (visible) {
                    m->cells[c].visible = true;
                    done = false;
                    /*if the cell is opaque, block the adjacent slopes */
                    if (!m->cells[c].transparent) {
                        if (min_angle >= start_slope) {
                            min_angle = end_slope;
                            /* if min_angle is applied to the last cell in line, nothing more
                               needs to be checked. */
                            if (processed_cell == iteration) done = true;
                        }
                        else if (!extended) {
                            start_angle[total_obstacles] = start_slope;
                            end_angle[total_obstacles++] = end_slope;
                        }
                        if (!light_walls) m->cells[c].visible = false;
                    }
                }
                processed_cell++;
            }
            if (iteration == max_radius) done = true;
            iteration++;
            obstacles_in_last_line = total_obstacles;
            y += dy;
            if (y < 0 || y >= m->height) done = true;
        }
    }
    /*octant: horizontal edge */
    {
        int iteration = 1; /*iteration of the algo for this octant */
        bool done = false;
        int total_obstacles = 0;
        int obstacles_in_last_line = 0;
        double min_angle = 0.0;
        long x,y;

        /*do while there are unblocked slopes left and the algo is within the map's boundaries
         scan progressive lines/columns from the PC outwards */
        x = player_x+dx; /*the outer slope's coordinates (first processed line) */
        if (x < 0 || x >= m->width) done = true;
        while(!done) {
            /*process cells in the line */
            double slopes_per_cell = 1.0 / (double)(iteration);
            double half_slopes = slopes_per_cell * 0.5;
            auto processed_cell = (int)((min_angle + half_slopes) / slopes_per_cell);
            long miny = MAX(0,player_y-iteration), maxy = MIN(m->height-1,player_y+iteration);
            done = true;
            for (y = player_y + (processed_cell * dy); y >= miny && y <= maxy; y+=dy) {
                long c = x + (y * m->width);
                /*calculate slopes per cell */
                bool visible = true;
                bool extended = false;
                double centre_slope = (double)processed_cell * slopes_per_cell;
                double start_slope = centre_slope - half_slopes;
                double end_slope = centre_slope + half_slopes;
                if (obstacles_in_last_line > 0 && !m->cells[c].visible) {
                    int idx = 0;
                    if ((!m->cells[c - dx].visible || !m->cells[c - dx].transparent) && (y - dy >= 0 && y - dy < m->height && (
                            !m->cells[c - (m->width * dy) - dx].visible || !m->cells[c - (m->width * dy) - dx].transparent))) visible = false;
                    else while(visible && idx < obstacles_in_last_line) {
                            if (start_angle[idx] > end_slope || end_angle[idx] < start_slope) {
                                ++idx;
                            }
                            else {
                                if (m->cells[c].transparent) {
                                    if (centre_slope > start_angle[idx] && centre_slope < end_angle[idx])
                                        visible = false;
                                }
                                else {
                                    if (start_slope >= start_angle[idx] && end_slope <= end_angle[idx])
                                        visible = false;
                                    else {
                                        start_angle[idx] = MIN(start_angle[idx],start_slope);
                                        end_angle[idx] = MAX(end_angle[idx],end_slope);
                                        extended = true;
                                    }
                                }
                                ++idx;
                            }
                        }
                }
                if (visible) {
                    m->cells[c].visible = true;
                    done = false;
                    /*if the cell is opaque, block the adjacent slopes */
                    if (!m->cells[c].transparent) {
                        if (min_angle >= start_slope) {
                            min_angle = end_slope;
                            /* if min_angle is applied to the last cell in line, nothing more
                                 needs to be checked. */
                            if (processed_cell == iteration) done = true;
                        }
                        else if (!extended) {
                            start_angle[total_obstacles] = start_slope;
                            end_angle[total_obstacles++] = end_slope;
                        }
                        if (!light_walls) m->cells[c].visible = false;
                    }
                }
                processed_cell++;
            }
            if (iteration == max_radius) done = true;
            iteration++;
            obstacles_in_last_line = total_obstacles;
            x += dx;
            if (x < 0 || x >= m->width) done = true;
        }
    }
}

void FieldOfView::castRay(Point from, Point to) {
    // Let's get Bresenheimy
//    cout << "Casting from (" << from.x << ", " << from.y << ") to (" << to.x << ", " << to.y << ")" << endl;
    bool swapXY = abs(to.y - from.y) > abs(to.x - from.x);
    long x0, y0, x1, y1;
    if (swapXY) {
        x0 = from.y;
        y0 = from.x;
        x1 = to.y;
        y1 = to.x;
    } else {
        x0 = from.x;
        y0 = from.y;
        x1 = to.x;
        y1 = to.y;
    }
    float deltaX = x1 - x0;
    float deltaY = abs(y1 - y0);
    float error = deltaX / 2;
    long y = y0;
    int stepY = -1;
    if (y0 < y1) stepY = 1;

    if (x1 > x0) { // X is increasing
        for (long x = x0; x <= x1; ++x) {
            if (swapXY) {
                markAsVisible(y, x);
                error -= deltaY;
                if (worldMap->isOpaque(Point(y, x))) {
                    return;
                }
            } else {
                markAsVisible(x, y);
                error -= deltaY;
                if (worldMap->isOpaque(Point(x, y))) {
                    return;
                }
            }
            if (error < 0) {
                y += stepY;
                error += deltaX;
            }
        }
    } else { // X is decreasing
        for (long x = x0; x >= x1; --x) {
            if (swapXY) {
                markAsVisible(y, x);
                error -= deltaY;
                if (worldMap->isOpaque(Point(y, x))) {
                    return;
                }
            } else {
                markAsVisible(x, y);
                error -= deltaY;
                if (worldMap->isOpaque(Point(x, y))) {
                    return;
                }
            }
            if (error < 0) {
                y -= stepY;
                error -= deltaX;
            }
        }
    }
}

int FieldOfView::getWidth() {
    return width;
}

int FieldOfView::getHeight() {
    return height;
}

long FieldOfView::getTop() {
    return top;
}

long FieldOfView::getLeft() {
    return left;
}

bool FieldOfView::isVisible(Point location) {
    return isVisible(location.x, location.y);
}


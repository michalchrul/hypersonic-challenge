#include <iostream>
#include <vector>
#include <algorithm>
#include <map>
#include <random>
#include <memory>
#include <cmath>
#include <cstdio>
#include <bits/stdc++.h>

#define ROW 11
#define COL 13

using namespace std;

//TODO: make BFS work in time due
//TODO: implement items management (and entire entities system)
//TODO: change time-fixed bomb dropping to something more intelligent

enum class PlayerId : int {
    id0 = 0,
    id1 = 1,
};

struct Config {
    int height, width;
    PlayerId ownId;
};

istream & operator >> (istream & in, Config & a) {
    int ownId;
    in >> a.width >> a.height >> ownId;
    a.ownId = PlayerId(ownId);
    return in;
}

enum class EntityType {
    player = 0,
    bomb = 1,
    item = 2,
};

struct Player { EntityType type; PlayerId id;    int x, y; int bomb, range; };
struct Bomb   { EntityType type; PlayerId owner; int x, y; int time, range; };
const int bombTimer = 8;

union Entity {
    struct { EntityType type; PlayerId owner; int x, y, param1, param2; };
    Player player;
    Bomb bomb;
};

enum class Cell {    
    box = 0,
    empty = 1,
    wall = 2,
};

struct Point { int y, x; };
Point point(int y, int x) { return (Point) { y, x }; }
template <typename T>
Point point(T const & p) { return (Point) { p.y, p.x }; }
bool operator == (Point a, Point b) { return make_pair(a.y, a.x) == make_pair(b.y, b.x); }
bool operator != (Point a, Point b) { return make_pair(a.y, a.x) != make_pair(b.y, b.x); }
bool operator <  (Point a, Point b) { return make_pair(a.y, a.x) <  make_pair(b.y, b.x); }

struct Turn {
    Config config;
    vector<vector<Cell> > field;
    vector<Entity> entities;
};

istream & operator >> (istream & in, Entity & a) {
    return in >> (int &)(a.type) >> (int &)(a.owner) >> a.x >> a.y >> a.param1 >> a.param2;
}

template <typename T> vector<vector<T> > vectors(T a, size_t h, size_t w) { return vector<vector<T> >(h, vector<T>(w, a)); }

istream & operator >> (istream & in, Turn & a) {
        a.field = vectors(Cell::empty, a.config.height, a.config.width);
        for (int y = 0; y < a.config.height; ++y) {
            for (int x = 0; x < a.config.width; ++x) {
                char c; in >> c;
                //cerr << c << " ";
                if (c == '.') {
                    a.field[y][x] =  Cell::empty;
                }
                if (isdigit(c)) {
                    a.field[y][x] =  Cell::box;
                }
                if (c == 'X') {
                    a.field[y][x] =  Cell::wall;
                }
            }
            //cerr << endl ;
        }
        int n; in >> n;
        a.entities.resize(n);
        for (int i = 0; i < n; ++i) {
             in >> a.entities[i];
        }
        return in;
    }

map<PlayerId,Player> selectPlayer(vector<Entity> const & entities) {
    map<PlayerId,Player> player;
    for (auto & ent : entities) {
        if (ent.type == EntityType::player) {
            player[ent.player.id] = ent.player;
        }
    }
    return player;
}

shared_ptr<Player> findPlayer(vector<Entity> const & entities, PlayerId id) {
    auto players = selectPlayer(entities);
    return players.count(id) ? make_shared<Player>(players[id]) : nullptr;
}

enum class Action {
    move = 0,
    bomb = 1,
};

struct Command {
    Action action;
    int y, x;
};

struct Output {
    Command command;
    string message;
};

ostream & operator << (ostream & out, Command const & a) {
    const string table[] = { "MOVE", "BOMB" };
    return out << table[int(a.action)] << ' ' << a.x << ' ' << a.y;
}

ostream & operator << (ostream & out, Output const & a) {
    return out << a.command << ' ' << a.message;
}

Command newCommand(Player const & self, int x, int y, Action action) {
    Command command = {};
    command.action = action;
    command.x =  x;
    command.y =  y;
    return command;
}

struct GreaterBySecond {
    bool operator()(const pair<Point, int>& p1, const pair<Point, int>& p2) const
    {
        return p1.second > p2.second;
    }
};

struct SmallerBySecond {
    bool operator()(const pair<Point, int>& p1, const pair<Point, int>& p2) const
    {
        return p1.second < p2.second;
    }
};

ostream & operator << (ostream & out, Point const pos) {
    return out << "x:" << pos.x << " y:" << pos.y;
}

vector<pair<Point, int> > sortByDmg(map<Point, int> M)
{
    vector<pair<Point, int>> A;
    for (auto& it : M) {
        A.push_back(it);
    }
    sort(A.begin(), A.end(), GreaterBySecond());

    return A;
}

vector<pair<Point, int>> sortByDist(vector<pair<Point, int>>  M) {
    sort(M.begin(), M.end(), SmallerBySecond());
    return M;
}

bool isValid(Point p)
        {
            int x = p.x;
            int y = p.y;
            return (y >= 0) && (y < ROW) &&
                (x >= 0) && (x < COL);
        }

// BFS: https://www.geeksforgeeks.org/shortest-path-in-a-binary-maze/
struct queueNode
{
    Point pt;
    int dist;
};

int rowNum[] = {-1, 0, 0, 1};
int colNum[] = {0, -1, 1, 0};

int BFS(vector<vector<int>> mat, Point src, Point dest)
{
    if (!mat[src.x][src.y] || !mat[dest.x][dest.y])
        return -1;

    bool visited[ROW][COL];
    memset(visited, false, sizeof visited);
    
    visited[src.x][src.y] = true;

    queue<queueNode> q;

    queueNode s = {src, 0};
    q.push(s);

    while (!q.empty())
    {
        queueNode curr = q.front();
        Point pt = curr.pt;

        if (pt.x == dest.x && pt.y == dest.y)
            return curr.dist;

        q.pop();

        for (int i = 0; i < 4; i++)
        {
            int row = pt.x + rowNum[i];
            int col = pt.y + colNum[i];
            Point p = {row, col};
            
            if (isValid(p) && mat[row][col] &&
            !visited[row][col])
            {
                visited[row][col] = true;
                queueNode Adjcell = { {row, col},
                                    curr.dist + 1 };
                q.push(Adjcell);
            }
        }
    }
    // Return -1 if destination cannot be reached
    return -1;
}


class Bot {
    private:
        Config config;
        vector<Turn> turns;
        vector<Output> outputs;
        Player player;
        bool isFirstTurn = true;
        Command currCommand;
        Point newPos;
        vector<vector<Cell> > field;
        vector<Point> freePositions;
        map<Point, int> freePositionsMap;
        vector<pair<Point, int>> dmgSortedPositions;
        vector<pair<Point, int>> distSortedPositions;
        vector<Bomb> droppedBombs;
        vector<Point> excludedPositions;
        vector<vector<int>> matrixForBFS;
        vector<pair<Point, int>> distances;
        
    public:

        Bot(Config a_Config) {
            config = a_Config;
        }

        //trivial get distance 
        int getDistance(Point a, Point b)
        {
            int x_dist, y_dist;
            int ret;
            x_dist = a.x - b.x;
            y_dist = a.y - b.y;
            ret = abs(x_dist) + abs(y_dist);
            return ret;
        }

        //count how many boxes can be destroyed for a given position
        int calcDamageCount(Point position) {   
            int damageCount = 0;
            for(int row = 0 ; row < field.size() ; ++row){
                for(int col = 0 ; col < field[row].size() ; ++col ){             
                    if (field[row][col] == Cell::box)
                    {
                        if (position.x == col && abs(position.y - row) < 3) {
                            damageCount++;
                         }
                         if (position.y == row && abs(position.x - col) < 3) {
                            damageCount++;
                         }
                    }
                }
            }
            return damageCount;
        }

        void findFreePositions() {

            freePositions.clear();

            for(int row = 0 ; row < field.size() ; row++){
                for(int col = 0 ; col < field[row].size() ; col++ ){
                    if(field[row][col] == Cell::empty){
                        Point freePos;
                        freePos.x = col;
                        freePos.y = row;

                        if (!excludedPositions.empty()) { //check if position is not excluded
                            if(std::find(excludedPositions.begin(), excludedPositions.end(), freePos) != excludedPositions.end()) {
                                freePositions.push_back(freePos);
                            }
                        }
                        else {
                            freePositions.push_back(freePos);
                        }
                    }
                }
            }
        }
        
        //update bomb timers and call excludePositionsAtRisk()
        void updateBombs() { 
            //making sure if bomb was actually successully dropped
            /*if(!turns.empty()) {
                if(player.bomb == findPlayer(turns.end()->entities, player.id)->bomb) { 
                    //no? remove last added bomb
                    if(!droppedBombs.empty()) {
                        droppedBombs.pop_back();
                    }      
                }
            }*/
            if (!droppedBombs.empty()) {
                excludePostionsAtRisk();
                for(auto bomb = droppedBombs.begin(); bomb != droppedBombs.end();) {
                    bomb->time--;
                    if (bomb->time == 0) {
                        droppedBombs.erase(bomb);
                    }
                }   
            }         
        }

        //exclude positions endangered by explosions
        void excludePostionsAtRisk() {
            for(auto bomb : droppedBombs){
                int range = bomb.range;
                vector<Point> positionsToExclude;
                
                for(int row = 0 ; row < range; row++){
                    Point p;
                    p.x = bomb.x;
                    p.y = bomb.y + row;
                    if (isValid(p)) {
                        positionsToExclude.push_back(p);
                    }
                    p.y = bomb.y - row;
                    if (isValid(p)) {
                        positionsToExclude.push_back(p);
                    }
                        
                }
                for(int col = 0 ; col < range; col++){
                    Point p;
                    p.y = bomb.y;
                    p.x = bomb.x + col;
                    if (isValid(p)) {
                        positionsToExclude.push_back(p);
                    }
                    p.x = bomb.x - col;
                    if (isValid(p)) {
                        positionsToExclude.push_back(p);
                    }
                }
                excludedPositions.clear();
                excludedPositions = positionsToExclude;
            }
        }

        void prepareMatrixBFS() {
            vector<vector<int> > matrix(ROW, vector<int>(COL));

            if(!freePositions.empty()) {
                for (auto iter = freePositions.begin(); iter != freePositions.end(); ++iter) {
                    int row = iter->y;
                    int col = iter->x;
                    matrix[row][col] = 1;
                }
            }
            
            matrixForBFS = matrix;
        }

        int findShortestPath(Point start, Point end) {

            int dist = BFS(matrixForBFS, start, end);       
        
            if (dist != -1)
                cerr << "Shortest Path is " << dist << endl;
            else
                cerr << "Shortest Path doesn't exist" << endl;

            return dist;
        }

        //find best coordinates based on damage count
        Point findBestCoordinates_dmg(Turn const & turn)
        {
            field.clear();
            field = turn.field;
            findFreePositions(); //get all non-occupied positions

            freePositionsMap.clear();

            for (auto iter = freePositions.begin(); iter != freePositions.end(); ++iter) {
                int damageCount = calcDamageCount(*iter); // get damage count for each of them
                freePositionsMap.insert(pair<Point, int>(*iter, damageCount));
            }  
            dmgSortedPositions = sortByDmg(freePositionsMap); // sort them by damage count
            
            Point playerPos;
            playerPos.x = player.x;
            playerPos.y = player.y;

            Point bestPos {-1, -1};
            bestPos = dmgSortedPositions.begin()->first;
            dmgSortedPositions.erase(dmgSortedPositions.begin());

            /*
            // iterate from highest damage count downwards
            for (auto iter = dmgSortedPositions.begin(); iter != dmgSortedPositions.end(); ++iter) {      
               // cerr << "sorted positions size: " << dmgSortedPositions.size() << endl;
                //cerr << iter->second << " : " << iter->first << endl;
                
                if (getDistance(playerPos, iter->first) < 8 && getDistance(playerPos, iter->first) > 3) {
                //if (getDistance(playerPos, iter->first) < 8) { //     if position is further than 8 moves away get next one
                    bestPos = iter->first;
                    dmgSortedPositions.erase(iter);
                }
            }*/
            return bestPos;
        }

        //find best coordinates based on real distance
        Point findBestCoordinates_dist(Turn const & turn, int min)
        {
            field.clear();
            field = turn.field;
            findFreePositions();  // get all non-occupied positions
            prepareMatrixBFS();

            Point playerPos;
            playerPos.x = player.x;
            playerPos.y = player.y;

            distances.clear();
            
            int cnt = 0;
            for (auto iter = freePositions.begin(); iter != freePositions.end(); ++iter) {
                Point p = *iter;
                int distance = findShortestPath(playerPos, p); //calculate distances to all other free points
                if (distance > min) { //min has to be greater than 0 (so point can be reached and is not current point)
                    distances.push_back(pair<Point, int>(p, distance));
                }
                cnt++;
                if(cnt < 3)
                    break;
            }

            distSortedPositions = sortByDist(distances); //sort them by distance ascending

            //pick closest one or pick one at a specific distance (to avoid dying from explosion)
            //this depends on min value
            Point bestPos {-1, -1};
            bestPos = distSortedPositions.begin()->first;
            distSortedPositions.erase(distSortedPositions.begin()); 

            return bestPos;
        }

        //prepare output based on damage count
        Output prepareOutput(Turn const & turn)
        {
            PlayerId myId = config.ownId;
            shared_ptr<Player> myPlayer = findPlayer(turn.entities, myId);
            player = *myPlayer;

            updateBombs();
            field = turn.field;           
            Command command;
            Action action = Action::move;

            if (isFirstTurn) {
                action = Action::bomb;
                newPos = findBestCoordinates_dmg(turn);
                currCommand = newCommand(player, newPos.x, newPos.y, action);
                isFirstTurn = false;
            }
            
            //if (outputs.size() % 7 == 0) { 
            if (outputs.size() % 40 == 0) { //let the other bots kill themselves first so I'm higher up the ranking until BFS is working ;)
                action = Action::bomb;
                currCommand = newCommand(player,newPos.x, newPos.y, action);
            }
        
            //if (outputs.size() % 8 == 0 && !outputs.empty()) {
            if (outputs.size() % 41 == 0) {
                action = Action::move;
                newPos = findBestCoordinates_dmg(turn);
                currCommand = newCommand(player, newPos.x, newPos.y, action);
            }

            string message = "x: " + to_string(newPos.x) + " y: " + to_string(newPos.y);
            Output output;

            if (action == Action::bomb) {
                Bomb droppedBomb = {EntityType::bomb, myId, bombTimer, newPos.x, newPos.y, 3};
                droppedBombs.push_back(droppedBomb);
            }

            output.command = currCommand;
            output.message = message;
            outputs.push_back(output);
            turns.push_back(turn);

            return output;
        }

        //prepare output based on BFS that evaluates if a point is possible to reach and gets it's distance
        Output prepareOutput_B(Turn const & turn)
        {
            //TODO:
            
            //find best spot to drop a bomb and do it
            //move away to point at safe distance
            //repeat
            
            PlayerId myId = config.ownId;
            shared_ptr<Player> myPlayer = findPlayer(turn.entities, myId);
            player = *myPlayer;

            if(!isFirstTurn) {
                updateBombs();
            }
            
            field = turn.field;           
            Command command;
            Action action = Action::move;

            int turnsCnt = outputs.size();

            if (isFirstTurn) {
                newPos = findBestCoordinates_dist(turn, 1); //in first turn make sure for the bomb to not be in 0,0
                currCommand = newCommand(player, newPos.x, newPos.y, action);
                isFirstTurn = false;
            }
            else {
                if (turnsCnt == 1) {
                    action = Action::bomb;
                    newPos = findBestCoordinates_dist(turn, 3); //move away from the bomb
                    currCommand = newCommand(player, newPos.x, newPos.y, action);
                }
                if (turnsCnt == 2) {
                    newPos = findBestCoordinates_dist(turn, 3);
                    currCommand = newCommand(player, newPos.x, newPos.y, action);
                }
                if (turnsCnt % 9 == 0) {
                    action = Action::bomb;
                    currCommand = newCommand(player,newPos.x, newPos.y, action);
                }
                if (turnsCnt % 10 == 0) {
                    action = Action::move;
                    newPos = findBestCoordinates_dist(turn, 3);
                    currCommand = newCommand(player, newPos.x, newPos.y, action);
                }
                //TODO
                //determinate the number of turns after which damage count oriented positioning starts to make sense
            }

            string message = "x: " + to_string(newPos.x) + " y: " + to_string(newPos.y);
            Output output;

            if (action == Action::bomb) {
                Bomb droppedBomb = {EntityType::bomb, myId, bombTimer, newPos.x, newPos.y, 3};
                droppedBombs.push_back(droppedBomb);
            }

            output.command = currCommand;
            output.message = message;
            outputs.push_back(output);
            turns.push_back(turn);

            return output;
        }
};

int main()
{
    Config config; cin >> config;
    Bot bot(config); 

    // game loop
    while (1) {
        Turn turn = { config }; cin >> turn;
        Output output = bot.prepareOutput(turn);
        cout << output << endl;        
    }
}
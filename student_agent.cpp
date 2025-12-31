#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <random>
#include <cmath>
#include <algorithm>
#include <set>
#include <queue>
#include <limits>
#include <unordered_map>
#include <sstream>
#include <cstdint>
#include <array>
#include "agent.h"

namespace py = pybind11;
struct MoveScore {
    Move move;
    int score;
    
};


struct Move;
class StudentAgent;

using Board = std::vector<std::vector<std::map<std::string, std::string>>>;

using Position = std::pair<int, int>;
using MoveList = std::vector<Move>;


bool in_bounds(int x, int y, int rows, int cols) {
    return x >= 0 && x < cols && y >= 0 && y < rows;
}

std::vector<int> score_cols_for(int cols) {
    int w = 4;
    int start = std::max(0, (cols - w) / 2);
    std::vector<int> result;
    for (int i = start; i < start + w; ++i) {
        result.push_back(i);
    }
    return result;
}

int top_score_row() {
    return 2;
}

int bottom_score_row(int rows) {
    return rows - 3;
}

bool is_opponent_score_cell(int x, int y, const std::string& player, int rows, int cols, const std::vector<int>& score_cols) {
    if (player == "circle") {
        return (y == bottom_score_row(rows)) && (std::find(score_cols.begin(), score_cols.end(), x) != score_cols.end());
    } else {
        return (y == top_score_row()) && (std::find(score_cols.begin(), score_cols.end(), x) != score_cols.end());
    }
}

bool is_own_score_cell(int x, int y, const std::string& player, int rows, int cols, const std::vector<int>& score_cols) {
    if (player == "circle") {
        return (y == top_score_row()) && (std::find(score_cols.begin(), score_cols.end(), x) != score_cols.end());
    } else {
        return (y == bottom_score_row(rows)) && (std::find(score_cols.begin(), score_cols.end(), x) != score_cols.end());
    }
}

std::string get_opponent(const std::string& player) {
    return (player == "circle") ? "square" : "circle";
}

std::vector<Position> agent_river_flow(const Board& board, int rx, int ry, int sx, int sy,
                                       const std::string& player, int rows, int cols,
                                       const std::vector<int>& score_cols, bool river_push = false) {
    std::vector<Position> destinations;
    destinations.reserve(rows * cols);
    std::set<Position> visited;
    std::queue<Position> queue;
    queue.push({rx, ry});

    while (!queue.empty()) {
        auto [x, y] = queue.front();
        queue.pop();

        if (visited.count({x, y}) || !in_bounds(x, y, rows, cols))
            continue;
        visited.insert({x, y});

        const auto& cell = board[y][x];

        // Empty cell → possible landing
        if (cell.empty()) {
            if (!is_opponent_score_cell(x, y, player, rows, cols, score_cols)) {
                destinations.emplace_back(x, y);
            }
            continue;
        }

        // Skip if this is not a river cell (except for the first river push)
        if ((!cell.count("side") || cell.at("side") != "river") &&
            !(river_push && x == rx && y == ry)) {
            continue;
        }

        // Determine flow directions
        std::string ori = (cell.count("orientation") ? cell.at("orientation") : "horizontal");
        std::vector<Position> dirs = (ori == "horizontal") ?
                                     std::vector<Position>{{1, 0}, {-1, 0}} :
                                     std::vector<Position>{{0, 1}, {0, -1}};

        // Traverse flow
        for (auto [dx, dy] : dirs) {
            int nx = x + dx, ny = y + dy;
            while (in_bounds(nx, ny, rows, cols)) {
                if (is_opponent_score_cell(nx, ny, player, rows, cols, score_cols))
                    break;

                const auto& next_cell = board[ny][nx];

                // Empty = valid destination
                if (next_cell.empty()) {
                    destinations.emplace_back(nx, ny);
                    nx += dx;
                    ny += dy;
                    continue;
                }

                // Skip if next is the source cell
                if (nx == sx && ny == sy) {
                    nx += dx;
                    ny += dy;
                    continue;
                }

                // Continue flowing through rivers
                if (next_cell.count("side") && next_cell.at("side") == "river") {
                    queue.push({nx, ny});
                    break;
                }

                // Otherwise stop
                break;
            }
        }
    }

    // Deduplicate destinations
    std::set<Position> unique_destinations(destinations.begin(), destinations.end());
    return std::vector<Position>(unique_destinations.begin(), unique_destinations.end());
}




// ---- Move Generation ----

struct ValidMoves {
    std::set<Position> moves;
    std::vector<std::pair<Position, Position>> pushes;
};

ValidMoves agent_compute_valid_moves(const Board& board, int sx, int sy, const std::string& player, int rows, int cols, const std::vector<int>& score_cols) {
    ValidMoves result;
    
    if (!in_bounds(sx, sy, rows, cols)) {
        return result;
    }
    
    const auto& piece = board[sy][sx];
    if (piece.empty() || piece.at("owner") != player) {
        return result;
    }
    
    std::vector<Position> directions = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    
    for (auto [dx, dy] : directions) {
        int tx = sx + dx, ty = sy + dy;
        if (!in_bounds(tx, ty, rows, cols)) {
            continue;
        }
        
        // Block moving into opponent score cell
        if (is_opponent_score_cell(tx, ty, player, rows, cols, score_cols)) {
            continue;
        }
        
        const auto& target = board[ty][tx];
        
        if (target.empty()) {
            // Empty cell - direct move
            result.moves.insert({tx, ty});
        } else if (target.at("side") == "river") {
            // River - compute flow destinations
            auto flow = agent_river_flow(board, tx, ty, sx, sy, player, rows, cols, score_cols);
            for (const auto& dest : flow) {
                result.moves.insert(dest);
            }
        } else {
            // Occupied by stone - check push possibility
            if (piece.at("side") == "stone") {
                // Stone pushing stone
                int px = tx + dx, py = ty + dy;
                if (in_bounds(px, py, rows, cols) && 
                    board[py][px].empty() && 
                    !is_opponent_score_cell(px, py, player, rows, cols, score_cols)) {
                    result.pushes.push_back({{tx, ty}, {px, py}});
                }
            } else {
                    // River pushing - compute flow for pushed piece
                    const auto& river_cell = board[sy][sx];
                    std::string ori = river_cell.count("orientation") ? river_cell.at("orientation") : "horizontal";

                    auto flow = agent_river_flow(board, tx, ty, sx, sy, player, rows, cols, score_cols, true);

                    for (const auto& dest : flow) {
                        // Check alignment based on orientation
                        bool valid_push = false;

                        if (ori == "horizontal") {
                            // Only allow left/right pushes
                            valid_push = (dest.second == ty);  // same row
                        } else if (ori == "vertical") {
                            // Only allow up/down pushes
                            valid_push = (dest.first == tx);   // same column
                        }

                        if (valid_push && !is_opponent_score_cell(dest.first, dest.second, player, rows, cols, score_cols)) {
                            result.pushes.push_back({{tx, ty}, dest});
                        }
                }

            }
        }
    }
    
    return result;
}

MoveList get_valid_moves_for_piece(const Board& board, int x, int y, const std::string& player, 
                                 int rows, int cols, const std::vector<int>& score_cols) {
    MoveList moves;
    const auto& piece = board[y][x];
    if (piece.empty() || piece.at("owner") != player) {
        return moves;
    }
    
    std::vector<Position> directions = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    
    if (piece.at("side") == "stone") {
        // Stone movement
        for (auto [dx, dy] : directions) {
            int nx = x + dx, ny = y + dy;
            if (!in_bounds(nx, ny, rows, cols)) {
                continue;
            }
            
            if (is_opponent_score_cell(nx, ny, player, rows, cols, score_cols)) {
                continue;
            }
            
            if (board[ny][nx].empty()) {
                // Simple move
                moves.push_back(Move("move", {x, y}, {nx, ny}));
            } else if (board[ny][nx].at("side") == "stone") {
                // Push move
                int px = nx + dx, py = ny + dy;
                if (in_bounds(px, py, rows, cols) && 
                    board[py][px].empty() && 
                    !is_opponent_score_cell(px, py, player, rows, cols, score_cols) &&
                    !is_opponent_score_cell(nx, ny, player, rows, cols, score_cols)) {
                        if(board[ny][nx].at("owner") != player && is_own_score_cell(px, py, player, rows, cols, score_cols)){}
                        // We can't push opponent stone in our own scoring area
                        else{
                    moves.push_back(Move("push", {x, y}, {nx, ny}, {px, py}));}
                }
            }
            
            // Check for river moves
            if (!board[ny][nx].empty() && board[ny][nx].at("side") == "river") {
                auto pos = agent_river_flow(board, nx, ny, x, y, player, rows, cols, score_cols);
                for (const auto& p : pos) {
                    if (in_bounds(p.first, p.second, rows, cols) && 
                    !is_opponent_score_cell(p.first, p.second, player, rows, cols, score_cols)){
                    moves.push_back(Move("move", {x, y}, {p.first, p.second}));}
                }
            }
        }
        
        // Stone to river flips
        moves.push_back(Move("flip", {x, y}, {x, y}, {}, "horizontal"));
        moves.push_back(Move("flip", {x, y}, {x, y}, {}, "vertical"));
        
    } else if (piece.at("side") == "river"){ // River piece
        // River to stone flip
        // cout << "Reached" << endl;
        moves.push_back(Move("flip", {x, y}, {x, y}));
        
        // River rotation
        moves.push_back(Move("rotate", {x, y}, {x, y}));
        
        for (auto [dx, dy] : directions) {
            // cout << "Reached inner dir loop" << endl;
            int nx = x + dx, ny = y + dy;
            if (!in_bounds(nx, ny, rows, cols)) {
                continue;
            }
            
            if (is_opponent_score_cell(nx, ny, player, rows, cols, score_cols)) {
                continue;
            }
            
            if (board[ny][nx].empty()) {
                // Simple move
                moves.push_back(Move("move", {x, y}, {nx, ny}));
            }
            
            if (!board[ny][nx].empty()) {
                // cout << "Reached inner if" << endl;
                if (board[ny][nx].at("side") == "river") {
                    auto pos = agent_river_flow(board, nx, ny, x, y, player, rows, cols, score_cols);
                    for (const auto& p : pos) {
                        if (in_bounds(p.first, p.second, rows, cols) && 
                        !is_opponent_score_cell(p.first, p.second, player, rows, cols, score_cols)){
                        moves.push_back(Move("move", {x, y}, {p.first, p.second}));
                        }
                    }
                }
                
                if (board[ny][nx].at("side") == "stone") {
                    // cout << "Reached inner if2" << endl;
                    // Push move
                    auto pos = agent_river_flow(board, nx, ny, x, y, player, rows, cols, score_cols, true);
                    // cout << pos.size() << endl;
                    for (const auto& p : pos) {
                        // cout << "Reached innner" << endl;
                    if (in_bounds(p.first, p.second, rows, cols) && 
                    !is_opponent_score_cell(p.first, p.second, player, rows, cols, score_cols)){
                        if(board[ny][nx].at("owner") != player && is_own_score_cell(p.first, p.second, player, rows, cols, score_cols)){
                            continue;
                        }
                        moves.push_back(Move("push", {x, y}, {nx, ny}, {p.first, p.second}));}
                    }
                }
            }
        }
    }
    
    return moves;
}


int count_stones_in_scoring_area(const Board& board, const std::string& player, int rows, int cols, const std::vector<int>& score_cols) {
    int count = 0;
    int score_row = (player == "circle") ? top_score_row() : bottom_score_row(rows);
    
    for (int x : score_cols) {
        if (in_bounds(x, score_row, rows, cols)) {
            const auto& piece = board[score_row][x];
            if (!piece.empty() && piece.at("owner") == player && piece.at("side") == "stone"){
                count++;
            }
        }
    }
    
    return count;
}

double euclidean_distance(const Position& p1, const Position& p2) {
    return std::abs(p1.first - p2.first) + std::abs(p1.second - p2.second);
}

int count_rivers(const Board& board, const std::string& player, int rows, int cols, const std::vector<int>& score_cols) {
    int count = 0;
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            const auto& piece = board[r][c];
            if (!piece.empty() && piece.at("owner") == player && piece.at("side") == "river") {
                count++;
            }
        }
    }
    return count;
}

double basic_evaluate_board(const Board& board,
                            const std::string& player,
                            int rows, int cols,
                            const std::vector<int>& score_cols)
{
    double score = 0.0;
    const std::string opponent = get_opponent(player);

    std::vector<Position> score_coordinates;
    std::vector<Position> imp_coordinates;
    std::vector<Position> opp_coordinates;
    int stone_count = 0;

    // ----------- SCORING COORDINATE SETUP (unchanged logic) -----------
    if (player == "circle")
    {
        if (rows == 13)
        {
            stone_count = 4;
            score_coordinates = {{2,4},{2,5},{2,6},{2,7}};
            imp_coordinates   = {{1,4},{1,5},{1,6},{1,7}};
            opp_coordinates   = {{2,3}, {2,8}};
        }
        else if (rows == 15)
        {
            stone_count = 5;
            score_coordinates = {{2,4},{2,5},{2,6},{2,7},{2,8}};
            imp_coordinates   = {{1,4},{1,5},{1,6},{1,7},{1,8}};
            opp_coordinates   = {{2,3}, {2,9}};
        }
        else if (rows == 17)
        {
            stone_count = 6;
            score_coordinates = {{2,5},{2,6},{2,7},{2,8},{2,9},{2,10}};
            imp_coordinates   = {{1,5},{1,6},{1,7},{1,8},{1,9},{1,10}};
            opp_coordinates   = {{2,4}, {2,11}};
        }
    }
    else // square
    {
        if (rows == 13)
        {
            stone_count = 4;
            score_coordinates = {{10,4},{10,5},{10,6},{10,7}};
            imp_coordinates   = {{11,4},{11,5},{11,6},{11,7}};
            opp_coordinates   = {{10,3}, {10,8}};
        }
        else if (rows == 15)
        {
            stone_count = 5;
            score_coordinates = {{12,4},{12,5},{12,6},{12,7},{12,8}};
            imp_coordinates = {{13,4},{13,5},{13,6},{13,7},{13,8}};
            opp_coordinates   = {{12,3}, {12,9}};

        }
        else if (rows == 17)
        {
            stone_count = 6;
            score_coordinates = {
                {14,5},{14,6},{14,7},{14,8},{14,9},{14,10}
            };

            imp_coordinates = {
                {15,5},{15,6},{15,7},{15,8},{15,9},{15,10}
            };
            opp_coordinates   = {{14,4}, {14,11}};

        }
    }

    // ----------- STONES IN SCORING AREA -----------
    int player_scoring = count_stones_in_scoring_area(board, player, rows, cols, score_cols);
    int opponent_scoring = count_stones_in_scoring_area(board, opponent, rows, cols, score_cols);

    // win / loss terminal boosts
    if (player_scoring == stone_count) score += 1e7;
    if (opponent_scoring == stone_count) score -= 1e7;

    // linear scoring bonuses
    score += player_scoring * 250;
    score -= opponent_scoring * 240;

    // ----------- RIVER BONUS -----------
    score += count_rivers(board, player, rows, cols, score_cols) * 0.15;

    // ----------- OPPONENT BLOCK THREAT -----------
    if (player == "circle")
    {
        int y0 = bottom_score_row(rows);
        for (int y = y0; y <= y0 + 2; ++y)
            for (int x = 2; x <= 9; ++x)
                if (in_bounds(x, y, rows, cols))
                {
                    const auto& piece = board[y][x];
                    if (!piece.empty() && piece.at("owner") == opponent)
                        score -= 70;
                }
    }
    else // square
    {
        int y0 = top_score_row() + 1;
        for (int y = 0; y <= y0; ++y)
            for (int x = 2; x <= 9; ++x)
                if (in_bounds(x, y, rows, cols))
                {
                    const auto& piece = board[y][x];
                    if (!piece.empty() && piece.at("owner") == opponent)
                        score -= 70;
                }
    }

    // ----------- IMPORTANT POSITION BONUSES (pre-scoring columns) -----------
    for (const auto& coor : imp_coordinates)
    {
        int cy = coor.first;
        int cx = coor.second;

        // Bounds safety
        if (!in_bounds(cx, cy, rows, cols)) 
            continue;

        const auto& piece = board[cy][cx];

        // -------------------------
        // 1. PLAYER BONUS
        // -------------------------
        if (!piece.empty() && piece.at("owner") == player)
        {
            if (player == "circle")
            {
                // Good if cell below is empty
                if (cy + 1 < rows && board[cy + 1][cx].empty())
                    score += 90;
            }
            else // player == "square"
            {
                // Good if cell above is empty
                if (cy - 1 >= 0 && board[cy - 1][cx].empty())
                    score += 90;
            }
        }

        // -------------------------
        // 2. OPPONENT PENALTY
        // -------------------------
        if (!piece.empty() && piece.at("owner") == opponent)
        {
            if (player == "circle")
            {
                // Opponent sitting in our lane is BAD
                if (cy + 1 < rows && board[cy + 1][cx].empty())
                    score -= 90;   // Slightly less harsh than reward
            }
            else // player == "square"
            {
                if (cy - 1 >= 0 && board[cy - 1][cx].empty())
                    score -= 90;
            }
        }
    }
    for(const auto& coor : opp_coordinates){
        int cy = coor.first;
        int cx = coor.second;

        // Bounds safety
        if (!in_bounds(cx, cy, rows, cols)) 
            continue;

        const auto& piece = board[cy][cx];
        if (!piece.empty() && piece.at("owner") == opponent)
        {
            score -= 90;   // Slightly less harsh than reward
        }
    }
    // ----------- MAIN LOOP THROUGH BOARD -----------
    for (int y = 0; y < rows; ++y)
    {
        for (int x = 0; x < cols; ++x)
        {
            const auto& piece = board[y][x];
            if (piece.empty()) continue;

            const std::string& owner = piece.at("owner");

            // ----------- OUR PIECE -----------
            if (owner == player)
            {
                // Check if this cell was one of the scoring targets
                Position pos = {y, x};
                // erase-if-found is expensive → replace with boolean + manual removal
                for (size_t i = 0; i < score_coordinates.size(); ++i)
                {
                    if (score_coordinates[i] == pos)
                    {
                        score_coordinates.erase(score_coordinates.begin() + i);
                        break;
                    }
                }

                // advancement bonuses
                if (player == "circle")
                {
                    if (y < 2) score += 40;


                    // general advancement
                    score += 2 * (1.0 / (y + 1));
                }
                else // square
                {
                    if (y > rows - 3) score += 40;

                    score += 2 * (1.0 / (rows - y));
                }

                // important column occupancy (+10)
                for (const auto& c : imp_coordinates)
                    if (c.first == y && c.second == x)
                        score += 10;

                // distance heuristic to remaining scoring cells
                for (const auto& tgt : score_coordinates)
                {
                    double dist = euclidean_distance(tgt, pos);
                    score += 9.0 / (dist + 1.0);
                }
            }

            // ----------- OPPONENT PIECE PENALTIES -----------
            else
            {
                if (player == "circle")
                    score -= 1.7 * (1.0 / (rows - y));
                else
                    score -= 1.7 * (1.0 / (y + 1));
            }
        }
    }

    return score;
}


MoveList order_moves(const Board& board,const MoveList& moves, const std::string& player, int rows, int cols, const std::vector<int>& score_cols){
    vector<MoveScore> ordered_moves;
    ordered_moves.reserve(moves.size()); // Pre-reserve to avoid reallocations
    for(const auto& move: moves){ // Use reference to avoid copying
        auto [message, new_board] = simulate_move_on_copy(board, move, player, rows, cols, score_cols);
        if(!message) continue;
        double score = basic_evaluate_board(new_board, player, rows, cols, score_cols);
        ordered_moves.push_back({move, static_cast<int>(score)});
    }
    std::sort(ordered_moves.begin(), ordered_moves.end(), [](const MoveScore& a, const MoveScore& b) {
        return a.score > b.score;
    });
    MoveList result;
    result.reserve(ordered_moves.size()); // Pre-reserve for result
    for(const auto& mvscore : ordered_moves){ // Use reference
        result.push_back(mvscore.move);
    }
    return result;
}

// ---- Move Simulation ----

Board simulate_move_cpp(const Board& board, const Move& move, const std::string& player, int rows, int cols, const std::vector<int>& score_cols) {
    auto [message, new_board] = simulate_move_on_copy(board, move, player, rows, cols, score_cols);
    if (!message) {
        cout << "message: " << message << endl;
        cout << "move: " << move.action << endl;
        cout << "from: " << move.from[0] << ", " << move.from[1] << endl;
        cout << "to: " << move.to[0] << ", " << move.to[1] << endl;

        throw std::runtime_error("Invalid move: ");
    }
    return new_board;
}
Board empty_board(int rows, int cols) {
    Board board(rows, std::vector<std::map<std::string, std::string>>(cols));
    return board;
}

class ZobristHash {
private:
    static constexpr int MAX_ROWS = 20;
    static constexpr int MAX_COLS = 20;
    static std::array<std::array<std::array<std::array<std::array<uint64_t, 3>, 2>, 2>, MAX_COLS>, MAX_ROWS> zobrist_table;
    static uint64_t player_hash[2]; // Hash for player to move: 0=circle, 1=square
    static bool initialized;
    
    static void initialize_zobrist() {
        if (initialized) return;
        
        std::mt19937_64 rng(12345); // Fixed seed for reproducibility
        std::uniform_int_distribution<uint64_t> dist;
        
        // Initialize player hash
        player_hash[0] = dist(rng); // circle
        player_hash[1] = dist(rng); // square
        
        // Initialize position/state table
        for (int y = 0; y < MAX_ROWS; ++y) {
            for (int x = 0; x < MAX_COLS; ++x) {
                for (int owner = 0; owner < 2; ++owner) {
                    for (int side = 0; side < 2; ++side) {
                        for (int orient = 0; orient < 3; ++orient) {
                            zobrist_table[y][x][owner][side][orient] = dist(rng);
                        }
                    }
                }
            }
        }
        initialized = true;
    }
    
public:
    static uint64_t compute_hash(const Board& board, const std::string& to_move, int rows, int cols) {
        initialize_zobrist();
        
        uint64_t hash = 0;
        
        // Hash board state
        for (int y = 0; y < rows && y < MAX_ROWS; ++y) {
            for (int x = 0; x < cols && x < MAX_COLS; ++x) {
                const auto& cell = board[y][x];
                if (cell.empty()) continue;
                
                int owner = 0;
                int side = 0;
                int orient = 0;
                
                auto itO = cell.find("owner");
                if (itO != cell.end() && itO->second == "square") owner = 1;
                
                auto itS = cell.find("side");
                if (itS != cell.end() && itS->second == "river") {
                    side = 1;
                    auto itR = cell.find("orientation");
                    if (itR != cell.end()) {
                        orient = (itR->second == "horizontal") ? 1 : 2;
                    }
                }
                
                hash ^= zobrist_table[y][x][owner][side][orient];
            }
        }
        
        // Hash player to move
        int player_idx = (to_move == "circle") ? 0 : 1;
        hash ^= player_hash[player_idx];
        
        return hash;
    }
    
    static uint64_t update_hash_remove(uint64_t hash, int x, int y, const std::map<std::string, std::string>& cell) {
        if (y >= MAX_ROWS || x >= MAX_COLS || cell.empty()) return hash;
        
        int owner = 0;
        int side = 0;
        int orient = 0;
        
        auto itO = cell.find("owner");
        if (itO != cell.end() && itO->second == "square") owner = 1;
        
        auto itS = cell.find("side");
        if (itS != cell.end() && itS->second == "river") {
            side = 1;
            auto itR = cell.find("orientation");
            if (itR != cell.end()) {
                orient = (itR->second == "horizontal") ? 1 : 2;
            }
        }
        
        return hash ^ zobrist_table[y][x][owner][side][orient];
    }
    
    static uint64_t update_hash_add(uint64_t hash, int x, int y, const std::map<std::string, std::string>& cell) {
        if (y >= MAX_ROWS || x >= MAX_COLS || cell.empty()) return hash;
        
        int owner = 0;
        int side = 0;
        int orient = 0;
        
        auto itO = cell.find("owner");
        if (itO != cell.end() && itO->second == "square") owner = 1;
        
        auto itS = cell.find("side");
        if (itS != cell.end() && itS->second == "river") {
            side = 1;
            auto itR = cell.find("orientation");
            if (itR != cell.end()) {
                orient = (itR->second == "horizontal") ? 1 : 2;
            }
        }
        
        return hash ^ zobrist_table[y][x][owner][side][orient];
    }
    
    static uint64_t update_hash_player(uint64_t hash, const std::string& old_player, const std::string& new_player) {
        int old_idx = (old_player == "circle") ? 0 : 1;
        int new_idx = (new_player == "circle") ? 0 : 1;
        hash ^= player_hash[old_idx];
        hash ^= player_hash[new_idx];
        return hash;
    }
};

// Static member definitions
std::array<std::array<std::array<std::array<std::array<uint64_t, 3>, 2>, 2>, ZobristHash::MAX_COLS>, ZobristHash::MAX_ROWS> ZobristHash::zobrist_table;
uint64_t ZobristHash::player_hash[2];
bool ZobristHash::initialized = false;

// ---- Student Agent Class ----

class StudentAgent {
public:
    explicit StudentAgent(const std::string& player) 
        : player(player), opponent(get_opponent(player)), search_depth(3),fast_depth(3), gen(rd()) {
        bool set_board = false;

        // Initialize Zobrist table early to avoid first-time overhead
        ZobristHash::compute_hash(Board{}, "circle", 0, 0);

        // Pre-reserve space for all caches to reduce rehashing
        tt.reserve(80000);
        eval_cache.reserve(40000);
        moves_cache.reserve(40000);
        board_cache.reserve(80000);

        MoveList mv_list;
        MoveList mv_list_small;
        MoveList mv_list_medium;
        MoveList mv_list_large;
        if (player == "circle") {
            // if()
            mv_list_small= {
                Move("flip", {8, 9}, {8, 9}, {}, "horizontal"),
                Move("flip", {3, 9}, {3, 9}, {}, "horizontal"),
                Move("flip", {3, 8}, {3, 8}, {}, "vertical"),
                Move("flip", {8, 8}, {8, 8}, {}, "vertical"),
                Move("move", {8, 8}, {11, 9}),  //right
                Move("move", {3, 8}, {0, 9}), // left
                Move("flip", {7, 9}, {7, 9}, {}, "horizontal"),
                Move("flip", {4, 9}, {4, 9}, {}, "horizontal"),
            };
            // medium board  is of 15 rows and 14 cols with a no. of scoring areas of 5. Lets modify the pre moves for medium board
            mv_list_medium = {
                Move("flip", {3,11}, {3, 11}, {}, "horizontal"),
                Move("flip", {9, 11}, {9, 11}, {}, "horizontal"),
                Move("flip", {3, 10}, {3, 10}, {}, "vertical"),
                Move("flip", {9, 10}, {9, 10}, {}, "vertical"),
                Move("move", {3, 10}, {0, 11}),  //right
                Move("move", {9, 10}, {13, 11}), // left
                Move("flip", {4, 11}, {4, 11}, {}, "horizontal"),
                Move("flip", {8, 11}, {8, 11}, {}, "horizontal"),


            };

            mv_list_large = {
                Move("flip", {4,13}, {4, 13}, {}, "horizontal"),
                Move("flip", {11, 13}, {11, 13}, {}, "horizontal"),
                Move("flip", {4, 12}, {4, 12}, {}, "vertical"),
                Move("flip", {11, 12}, {11, 12}, {}, "vertical"),
                Move("move", {11, 12}, {15, 13}),  //right
                Move("move", {4, 12}, {0, 13}), // left
                Move("flip", {5, 13}, {5, 13}, {}, "horizontal"),
                Move("flip", {10, 13}, {10, 13}, {}, "horizontal"),
            
            };
            // mv_list = mv_list_small; // Default to small board moves
        }
         else {
            mv_list_small = {
                Move("flip", {8, 3}, {8, 3}, {}, "horizontal"),
                Move("flip", {3, 3}, {3, 3}, {}, "horizontal"),
                Move("flip", {8, 4}, {8, 4}, {}, "vertical"),
                Move("flip", {3, 4}, {3, 4}, {}, "vertical"),
                Move("move", {8, 4}, {11, 3}),
                Move("move", {3, 4}, {0, 3}),
                Move("flip", {4, 3}, {4, 3}, {}, "horizontal"),
                Move("flip", {7, 3}, {7, 3}, {}, "horizontal"),
            };
            mv_list_medium = {
                Move("flip", {9, 3}, {9, 3}, {}, "horizontal"),
                Move("flip", {9, 4}, {9, 4}, {}, "vertical"),
                Move("flip", {3, 3}, {3, 3}, {}, "horizontal"),
                Move("flip", {3, 4}, {3, 4}, {}, "vertical"),
                Move("move", {9, 4}, {13, 3}),
                Move("move", {3, 4}, {0, 3}),
                Move("flip", {8, 3}, {8, 3}, {}, "horizontal"),
                Move("flip", {4, 3}, {4, 3}, {}, "horizontal"),
            };
            mv_list_large = {
                Move("flip", {4, 3}, {4, 3}, {}, "horizontal"),
                Move("flip", {4, 4}, {4, 4}, {}, "vertical"),
                Move("flip", {11, 3}, {11, 3}, {}, "horizontal"),
                Move("flip", {11, 4}, {11, 4}, {}, "vertical"),
                Move("move", {4, 4}, {0, 3}),
                Move("move", {11, 4}, {15, 3}),
                Move("flip", {5, 3}, {5, 3}, {}, "horizontal"),
                Move("flip", {10, 3}, {10, 3}, {}, "horizontal"),
            };
        }

        this->mv_list_small = mv_list_small;
        this->mv_list_medium = mv_list_medium;
        this->mv_list_large = mv_list_large;
        this->set_board = set_board;
        this->mv_list = mv_list;
    }

    void set_board_size(int rows, int cols) {
        MoveList mv_list;
        if (rows == 13 && cols == 12) { 

            this->mv_list = mv_list_small;
        } else if (rows == 15 && cols == 14) { // Medium board
            this->mv_list = mv_list_medium;
        } else if (rows == 17 && cols == 16) { // Large board 
            this->mv_list = mv_list_large;
        } 
    }
    

    void recovery_moves(const Board&board,Move failed_move,int rows, int cols, const std::vector<int>& score_cols) {
        if(player == "circle"){
            if(failed_move.action == "move"){
                if(failed_move.to[0] == 11 && failed_move.to[1] == 9){
                    mv_list = {
                        Move("move", {3, 8}, {0, 9}),
                        Move("move", {3, 9}, {0, 1}),
                        Move("move", {5, 9}, {7, 1}),
                        Move("move", {4, 8}, {6, 1}),
                        Move("move", {6, 9}, {5, 1}),
                        Move("move", {7, 8}, {4, 1}),
                    };
                }
                else if(failed_move.to[0] == 0 && failed_move.to[1]==9){
                    mv_list = {
                        Move("move", {8, 9}, {11, 1}),
                        Move("move", {7, 8}, {4, 1}),
                        Move("move", {6, 9}, {5, 1}),
                        Move("move", {5, 9}, {6, 1}),
                        Move("move", {4, 8}, {7, 1}),
                    };
                }
                else if(failed_move.to[0] == 11 && failed_move.to[1] == 1){
                    mv_list = {
                        Move("move", {3, 9}, {0, 1}),
                        Move("move", {5, 9}, {7, 1}),
                        Move("move", {4, 8}, {6, 1}),
                        Move("move", {6, 9}, {5, 1}),
                        Move("move", {7, 8}, {4, 1}),
                    };
                }
                else if(failed_move.to[0] == 0 && failed_move.to[1] == 1){
                    mv_list = {
                        Move("move", {7, 8}, {4, 1}),
                        Move("move", {6, 9}, {5, 1}),
                        Move("move", {5, 9}, {6, 1}),
                        Move("move", {4, 8}, {7, 1}),
                    };
                }
            }
        }
        if(player=="square"){
            if(failed_move.action == "move"){
                if(failed_move.to[0] == 11 && failed_move.to[1] == 3){
                    mv_list = {
                        Move("move", {3, 4}, {0, 3}),
                        Move("move", {3, 3}, {0, 11}),
                        Move("move", {5, 3}, {7, 11}),
                        Move("move", {4, 4}, {6, 11}),
                        Move("move", {6, 3}, {5, 11}),
                        Move("move", {7, 4}, {4, 11}),
                    };
                }
                else if(failed_move.to[0] == 0 && failed_move.to[1]==3){
                    mv_list = {
                        Move("move", {8, 3}, {11, 11}),
                        Move("move", {7, 4}, {4, 11}),
                        Move("move", {6, 3}, {5, 11}),
                        Move("move", {5, 3}, {6, 11}),
                        Move("move", {4, 4}, {7, 11}),
                    };
                }
                else if(failed_move.to[0] == 11 && failed_move.to[1] == 11){
                    mv_list = {
                        Move("move", {3, 3}, {0, 11}),
                        Move("move", {5, 3}, {7, 11}),
                        Move("move", {4, 4}, {6, 11}),
                        Move("move", {6, 3}, {5, 11}),
                        Move("move", {7, 4}, {4, 11}),
                    };
                }
                else if(failed_move.to[0] == 0 && failed_move.to[1] == 11){
                    mv_list = {
                        Move("move",{7, 4}, {4, 11}),
                        Move("move", {6, 3}, {5, 11}),
                        Move("move", {5, 3}, {6, 11}),
                        Move("move", {4, 4}, {7, 11}),
                    };
            }   }
        }
    }

bool check_move(const Board& board, const Move& mv, const std::string& player, 
                int rows, int cols, const std::vector<int>& score_cols) 
{
    int fx = mv.from[0];
    int fy = mv.from[1];

    if (!in_bounds(fx, fy, rows, cols)) return false;
    const auto& pc = board[fy][fx];
    if (pc.empty() || pc.at("owner") != player) return false;

    if (mv.action == "move") {
        auto legal = agent_compute_valid_moves(board, fx, fy, player, rows, cols, score_cols);
        Position dst = {mv.to[0], mv.to[1]};
        return legal.moves.count(dst) > 0;
    }

    if (mv.action == "push") {
        auto legal = agent_compute_valid_moves(board, fx, fy, player, rows, cols, score_cols);

        Position to  = {mv.to[0], mv.to[1]};
        Position pto = {mv.pushed_to[0], mv.pushed_to[1]};

        for (auto& pr : legal.pushes) {
            if (pr.first == to && pr.second == pto)
                return true;
        }
        return false;
    }

    if (mv.action == "flip") {
        Position to = {mv.to[0], mv.to[1]};
        if(board[to.second][to.first].empty()) return false;
        const auto& pc = board[to.second][to.first];
        cout << "Its not empty" << endl;
        std::string side = pc.at("side");

        if (side == "stone") {
            if (mv.orientation.empty()) return false;
            Board tmp = board;
            tmp[fy][fx]["side"] = "river";
            tmp[fy][fx]["orientation"] = mv.orientation;
            return true;
        }

        if (side == "river") {
            return mv.orientation.empty();
        }

        return false;
    }

    if (mv.action == "rotate") {
        if (pc.at("side") != "river") return false;
        std::string new_ori =
            (pc.at("orientation") == "horizontal") ? "vertical" : "horizontal";
        Board tmp = board;
        tmp[fy][fx]["orientation"] = new_ori;
        auto flow = agent_river_flow(tmp, fx, fy, fx, fy, player, rows, cols, score_cols);
        for (auto& d : flow)
            if (is_opponent_score_cell(d.first, d.second, player, rows, cols, score_cols))
                return false;
        return true;
    }

    return false;
}

    double cached_evaluate(const Board& board, int rows, int cols, const std::vector<int>& score_cols) {
        uint64_t key = ZobristHash::compute_hash(board, player, rows, cols);
        auto it = eval_cache.find(key);
        if (it != eval_cache.end()) {
            return it->second;
        }
        double score = basic_evaluate_board(board, player, rows, cols, score_cols);
        eval_cache[key] = score;
        return score;
    }
    
    MoveList cached_generate_moves(const Board& board, const std::string& current_player, int rows, int cols, const std::vector<int>& score_cols, bool do_order = true) {
        uint64_t base_hash = ZobristHash::compute_hash(board, current_player, rows, cols);
        uint64_t key = base_hash ^ (do_order ? 0x123456789ABCDEF0ULL : 0xFEDCBA9876543210ULL);
        auto cache_it = moves_cache.find(key);
        if (cache_it != moves_cache.end()) {
            return cache_it->second;
        }
        MoveList moves = generate_all_moves(board, current_player, rows, cols, score_cols);
        if (do_order) {
            moves = order_moves(board, moves, current_player, rows, cols, score_cols);
        }
        moves_cache[key] = moves;
        return moves;
    }
    
    Board cached_simulate(const Board& board, const Move& move, const std::string& current_player, int rows, int cols, const std::vector<int>& score_cols) {
        uint64_t base_hash = ZobristHash::compute_hash(board, current_player, rows, cols);
        uint64_t move_hash = 0;
        if (!move.from.empty() && move.from.size() >= 2) {
            move_hash ^= (static_cast<uint64_t>(move.from[0]) << 32) | move.from[1];
        }
        if (!move.to.empty() && move.to.size() >= 2) {
            move_hash ^= (static_cast<uint64_t>(move.to[0]) << 16) | move.to[1];
        }
        for (char c : move.action) {
            move_hash = (move_hash << 5) ^ static_cast<uint64_t>(c);
        }
        uint64_t key = base_hash ^ move_hash;
        
        auto it = board_cache.find(key);
        if (it != board_cache.end()) {
            return it->second;
        }
        Board new_board = simulate_move_cpp(board, move, current_player, rows, cols, score_cols);
        board_cache[key] = new_board;
        return new_board;
    }

    double alphabeta(const Board& board, int depth, double alpha, double beta, bool maximizing_player, int rows, int cols, const std::vector<int>& score_cols) {
        std::string current_player = maximizing_player ? player : opponent;
        double score_check = cached_evaluate(board, rows, cols, score_cols);
        if (std::abs(score_check) == 10000 || depth == 0) {
            return score_check;
        }
        uint64_t key = ZobristHash::compute_hash(board, current_player, rows, cols);
        auto it = tt.find(key);
        if (it != tt.end() && it->second.depth >= depth && it->second.has_value) {
            return it->second.value;
        }

        auto moves = cached_generate_moves(board, current_player, rows, cols, score_cols);
        if (moves.empty()) {
            return 0;
        }

        if (maximizing_player) {
            double max_eval = -std::numeric_limits<double>::infinity();
            for (const auto& move : moves) {
                Board new_board = cached_simulate(board, move, current_player, rows, cols, score_cols);
                double eval = alphabeta(new_board, depth - 1, alpha, beta, false, rows, cols, score_cols);
                max_eval = std::max(max_eval, eval);
                alpha = std::max(alpha, eval);
                if (beta <= alpha) {
                    break;
                }
                if(eval > 1e6){
                    search_depth = 1;
                }
                else{
                    search_depth = fast_depth;
                }
            }
            tt[key] = {max_eval, depth, true};
            return max_eval;
        } else {
            double min_eval = std::numeric_limits<double>::infinity();
            for (const auto& move : moves) {
                Board new_board = cached_simulate(board, move, current_player, rows, cols, score_cols);
                double eval = alphabeta(new_board, depth - 1, alpha, beta, true, rows, cols, score_cols);
                min_eval = std::min(min_eval, eval);
                beta = std::min(beta, eval);
                if (beta <= alpha) {
                    break;
                }
            }
            tt[key] = {min_eval, depth, true};
            return min_eval;
        }
    }

    Move choose(const Board& board, int rows, int cols, const std::vector<int>& score_cols, float current_player_time, float opponent_time) {
        auto moves = generate_all_moves(board, player, rows, cols, score_cols);
        cout << "search depth" <<  search_depth << endl;
        
        moves = order_moves(board, moves, player, rows, cols, score_cols);
        if(!set_board){
            set_board_size(rows,cols);
            set_board = true;
        };
        cout << "value of current board" << basic_evaluate_board(board, player, rows, cols, score_cols) << endl;


        double alpha = -std::numeric_limits<double>::infinity();
        double beta = std::numeric_limits<double>::infinity();
        double best_value = -std::numeric_limits<double>::infinity();
        
        if (moves.empty()) {
            return Move("move", {0, 0}, {0, 0});
        }
        std::vector<Board> child_boards;
        child_boards.reserve(moves.size());
        for (const auto& m : moves) {
            child_boards.push_back(cached_simulate(board, m, player, rows, cols, score_cols));
        }
        int depth = 3;
        if(rows == 13 && cols == 12 && current_player_time < 15){
            int depth = 2;
            fast_depth = 2;
            search_depth = fast_depth;
        }
        else if (rows == 15 && cols == 14 && current_player_time < 20){
            int depth = 2;
            fast_depth = 2;
            search_depth = fast_depth;
        }
        else if (rows == 17 && cols == 16 && current_player_time < 25){
            int depth = 2;
            fast_depth = 2;
            search_depth = fast_depth;
        }
        if (!mv_list.empty()) {
            Move mv = mv_list.front();
            cout << "move " << mv.action << mv.from[0] << mv.from[1] << mv.to[0] << mv.to[1] << endl;
            mv_list.erase(mv_list.begin());
            
            if (true) {
                bool success = check_move(board, mv, player, rows, cols, score_cols);
                if(!success && rows<=13){
                    if(mv_list.size() != 0){
                        mv = mv_list.front();
                        mv_list.erase(mv_list.begin());
                        success = check_move(board, mv, player, rows, cols, score_cols);
                    }
                    else{
                        success = false;
                    }
                }
                if(success){
                Board new_board = cached_simulate(board, mv, player, rows, cols, score_cols);
                double board_value = alphabeta(new_board, 2, alpha, beta, false, rows, cols, score_cols);
                cout << "Success : " << success << endl;
                cout << "The board value: " << board_value << endl;
                if (board_value < -100) {
                    best_value = board_value;

                    cout << "Value below 100" << endl;
                    for (size_t i = 0; i < moves.size(); ++i) {
                        const Board& new_board_cached = child_boards[i];
                        double bv = alphabeta(new_board_cached, depth - 1, alpha, beta, false, rows, cols, score_cols);
                        if (bv > best_value) { best_value = bv; mv = moves[i]; }
                    }
                    return mv;
                }
                }
                if (!success) {
                    while (!mv_list.empty()) {
                        mv = mv_list.front();
                        mv_list.erase(mv_list.begin());
                        success = check_move(board, mv, player, rows, cols, score_cols);
                        if (success) {
                            return mv;
                        }
                    }
                }
                if (!success) {
                    mv = moves[0];
                Board new_board = cached_simulate(board, mv, player, rows, cols, score_cols);
                double board_value = alphabeta(new_board, depth - 1, alpha, beta, false, rows, cols, score_cols);
                cout << "Value below 100" << endl;
                for (size_t i = 0; i < moves.size(); ++i) {
                    const Board& new_board_cached = child_boards[i];
                    double bv = alphabeta(new_board_cached, depth - 1, alpha, beta, false, rows, cols, score_cols);
                    if (bv > best_value) { best_value = bv; mv = moves[i]; }
                }
                return mv;
                }
            }
            return mv;
        }
        Move best_move = moves[0];
        best_value = -std::numeric_limits<double>::infinity();
        std::vector<size_t> order(moves.size());
        for (size_t i = 0; i < moves.size(); ++i) order[i] = i;
        std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b){
            double ea = cached_evaluate(child_boards[a], rows, cols, score_cols);
            double eb = cached_evaluate(child_boards[b], rows, cols, score_cols);
            return ea > eb;
        });

        for (size_t oi = 0; oi < order.size(); ++oi) {
            size_t i = order[oi];
            const Board& new_board = child_boards[i];
            double board_value = alphabeta(new_board, search_depth - 1, alpha, beta, false, rows, cols, score_cols);

            if (board_value > best_value) { best_value = board_value; best_move = moves[i]; }

            alpha = std::max(alpha, best_value);
        }
        return best_move;
    }

private:
    std::string player;
    std::string opponent;
    int search_depth;
    bool set_board;
    int fast_depth;
    MoveList mv_list_small;
    MoveList mv_list_medium;
    MoveList mv_list_large;
    std::vector<Move> mv_list;
    std::random_device rd;
    std::mt19937 gen;
    struct TTEntry { double value; int depth; bool has_value; };
    std::unordered_map<uint64_t, TTEntry> tt;
    
    std::unordered_map<uint64_t, double> eval_cache;
    
    std::unordered_map<uint64_t, MoveList> moves_cache;
    
    std::unordered_map<uint64_t, Board> board_cache;
};

PYBIND11_MODULE(student_agent_module, m) {
    m.doc() = "Complete C++ implementation of Student Agent for Stones & Rivers game";
    
    py::class_<Move>(m, "Move")
        .def(py::init<>())
        .def(py::init<const std::string&, const std::vector<int>&, const std::vector<int>&, const std::vector<int>&, const std::string&>())
        .def_readwrite("action", &Move::action)
        .def_readwrite("from_pos", &Move::from)
        .def_readwrite("to_pos", &Move::to)
        .def_readwrite("pushed_to", &Move::pushed_to)
        .def_readwrite("orientation", &Move::orientation);

    py::class_<StudentAgent>(m, "StudentAgent")
        .def(py::init<const std::string&>())
        .def("choose", &StudentAgent::choose)
        .def("check_move", &StudentAgent::check_move)
        .def("alphabeta", &StudentAgent::alphabeta);
    
    m.def("in_bounds", &in_bounds);
    m.def("score_cols_for", &score_cols_for);
    m.def("top_score_row", &top_score_row);
    m.def("bottom_score_row", &bottom_score_row);
    m.def("is_opponent_score_cell", &is_opponent_score_cell);
    m.def("is_own_score_cell", &is_own_score_cell);
    m.def("get_opponent", &get_opponent);
    m.def("generate_all_moves", &generate_all_moves);
    m.def("basic_evaluate_board", &basic_evaluate_board);
    m.def("count_stones_in_scoring_area", &count_stones_in_scoring_area);
}

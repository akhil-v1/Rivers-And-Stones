// agent.cpp
// Complete C++ conversion of agent.py
// Provides game utilities, move validation, and base agent framework

#include "agent.h"
#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <map>
#include <set>
#include <optional>
#include <algorithm>
#include <random>
#include <queue>
#include <unordered_set>
#include <functional>
#include <chrono>
#include <cmath>

// struct Move {
//     std::string action;
//     std::vector<int> from;
//     std::vector<int> to;
//     std::vector<int> pushed_to;
//     std::string orientation;
// };
// using Board = std::vector<std::vector<std::map<std::string, std::string>>>;
using namespace std;

// ==================== GAME UTILITIES ====================

inline bool in_bounds(int x, int y, int rows, int cols) {
    return 0 <= x && x < cols && 0 <= y && y < rows;
}

inline vector<int> score_cols_for(int cols) {
    int w = 4;
    int start = max(0, (cols - w) / 2);
    vector<int> out;
    for (int i = 0; i < w; ++i) out.push_back(start + i);
    return out;
}

inline int top_score_row() { 
    return 2; 
}

inline int bottom_score_row(int rows) { 
    return rows - 3; 
}

inline string opponent_of(const string& p) {
    return (p == "circle") ? "square" : "circle";
}

inline bool is_opponent_score_cell(int x, int y, const string& player, 
                                   int rows, int cols, const vector<int>& score_cols) {
    if (player == "circle")
        return (y == bottom_score_row(rows)) && 
               (find(score_cols.begin(), score_cols.end(), x) != score_cols.end());
    else
        return (y == top_score_row()) && 
               (find(score_cols.begin(), score_cols.end(), x) != score_cols.end());
}

inline bool is_own_score_cell(int x, int y, const string& player, 
                              int rows, int cols, const vector<int>& score_cols) {
    return is_opponent_score_cell(x, y, opponent_of(player), rows, cols, score_cols);
}

inline string get_opponent(const string& player) {
    return opponent_of(player);
}

// Cell helpers
inline bool cell_empty(const map<string,string>& cell) { 
    return cell.empty(); 
}

inline string cell_owner(const map<string,string>& cell) {
    auto it = cell.find("owner");
    return it == cell.end() ? string() : it->second;
}

inline string cell_side(const map<string,string>& cell) {
    auto it = cell.find("side");
    return it == cell.end() ? string("stone") : it->second;
}

inline string cell_orientation(const map<string,string>& cell) {
    auto it = cell.find("orientation");
    return it == cell.end() ? string("horizontal") : it->second;
}

inline void set_cell(Board& board, int x, int y, const string& owner, 
                    const string& side, const string& orientation = "") {
    board[y][x]["owner"] = owner;
    board[y][x]["side"] = side;
    if (!orientation.empty()) 
        board[y][x]["orientation"] = orientation;
    else 
        board[y][x].erase("orientation");
}

inline void clear_cell(Board& board, int x, int y) {
    board[y][x].clear();
}

inline Board copy_board(const Board& b) { 
    return Board(b); 
}

// ==================== RIVER FLOW SIMULATION ====================

vector<pair<int,int>> get_river_flow_destinations(
    const Board& board, int rx, int ry, int sx, int sy, const string& player,
    int rows, int cols, const vector<int>& score_cols, bool river_push) {
    
    vector<pair<int,int>> destinations;
    set<pair<int,int>> visited;
    vector<pair<int,int>> queue;
    queue.emplace_back(rx, ry);

    while (!queue.empty()) {
        auto cur = queue.front();
        queue.erase(queue.begin());
        int x = cur.first, y = cur.second;
        
        if (visited.count({x,y}) || !in_bounds(x,y,rows,cols)) 
            continue;
        visited.insert({x,y});

        map<string,string> cell = board[y][x];
        
        // For river push, treat entry cell as the source piece
        if (river_push && x == rx && y == ry) {
            if (in_bounds(sx, sy, rows, cols))
                cell = board[sy][sx];
        }

        // Empty cell - possible destination
        if (cell_empty(cell)) {
            if (!is_opponent_score_cell(x, y, player, rows, cols, score_cols)) {
                destinations.emplace_back(x, y);
            }
            continue;
        }

        // Not a river - stop
        if (cell_side(cell) != "river") {
            continue;
        }

        // Determine flow directions based on orientation
        vector<pair<int,int>> dirs;
        if (cell_orientation(cell) == "horizontal") {
            dirs = {{1,0}, {-1,0}};
        } else {
            dirs = {{0,1}, {0,-1}};
        }

        // Follow flow in each direction
        for (auto d : dirs) {
            int dx = d.first, dy = d.second;
            int nx = x + dx, ny = y + dy;
            
            while (in_bounds(nx, ny, rows, cols)) {
                // Block flow into opponent score
                if (is_opponent_score_cell(nx, ny, player, rows, cols, score_cols)) {
                    break;
                }
                
                const auto& next_cell = board[ny][nx];
                
                // Empty - add as destination and continue
                if (cell_empty(next_cell)) {
                    destinations.emplace_back(nx, ny);
                    nx += dx; 
                    ny += dy;
                    continue;
                }
                
                // Skip source cell
                if (nx == sx && ny == sy) {
                    nx += dx; 
                    ny += dy;
                    continue;
                }
                
                // Continue through connected rivers
                if (cell_side(next_cell) == "river") {
                    queue.emplace_back(nx, ny);
                    break;
                }
                
                // Blocked by stone
                break;
            }
        }
    }

    // Remove duplicates
    vector<pair<int,int>> out;
    set<pair<int,int>> seen;
    for (auto& d : destinations) {
        if (!seen.count(d)) { 
            seen.insert(d); 
            out.push_back(d); 
        }
    }
    return out;
}

// ==================== MOVE VALIDATION ====================

struct ValidTargets {
    set<pair<int,int>> moves;
    vector<pair<pair<int,int>, pair<int,int>>> pushes; // ((tx,ty),(ptx,pty))
};

static ValidTargets compute_valid_targets(
    const Board& board, int sx, int sy, const string& player, 
    int rows, int cols, const vector<int>& score_cols) {
    
    ValidTargets result;
    
    if (!in_bounds(sx, sy, rows, cols)) 
        return result;
        
    const auto& p = board[sy][sx];
    if (cell_empty(p) || cell_owner(p) != player) 
        return result;

    vector<pair<int,int>> dirs = {{1,0}, {-1,0}, {0,1}, {0,-1}};
    
    for (auto d : dirs) {
        int dx = d.first, dy = d.second;
        int tx = sx + dx, ty = sy + dy;
        
        if (!in_bounds(tx, ty, rows, cols)) 
            continue;
            
        if (is_opponent_score_cell(tx, ty, player, rows, cols, score_cols)) 
            continue;
            
        const auto& target = board[ty][tx];
        
        if (cell_empty(target)) {
            // Empty - direct move
            result.moves.insert({tx, ty});
            
        } else if (cell_side(target) == "river") {
            // River - flow to destinations
            auto flow = get_river_flow_destinations(board, tx, ty, sx, sy, 
                                                   player, rows, cols, score_cols, false);
            for (auto& d2 : flow) 
                result.moves.insert(d2);
                
        } else {
            // Stone - check push
            if (cell_side(p) == "stone") {
                // Stone pushing stone
                int px = tx + dx, py = ty + dy;
                if (in_bounds(px, py, rows, cols) && 
                    cell_empty(board[py][px]) &&
                    !is_opponent_score_cell(px, py, player, rows, cols, score_cols)) {
                    result.pushes.push_back({{tx,ty}, {px,py}});
                }
            } else {
                // River pushing - flow for pushed piece
                auto flow = get_river_flow_destinations(board, tx, ty, sx, sy, 
                                                       player, rows, cols, score_cols, true);
                for (auto& d2 : flow) {
                    if (!is_opponent_score_cell(d2.first, d2.second, player, rows, cols, score_cols)) {
                        result.pushes.push_back({{tx,ty}, {d2.first, d2.second}});
                    }
                }
            }
        }
    }
    return result;
}

// ==================== MOVE APPLICATION ====================

static bool apply_move_action(Board& board, const Move& move, const string& player, 
                              int rows, int cols, const vector<int>& score_cols, string& out_msg) {
    if (move.from.size() < 2 || move.to.size() < 2) { 
        out_msg = "bad move format"; 
        return false; 
    }
    
    int fx = move.from[0], fy = move.from[1];
    int tx = move.to[0], ty = move.to[1];
    
    if (!in_bounds(fx, fy, rows, cols) || !in_bounds(tx, ty, rows, cols)) { 
        out_msg = "out of bounds"; 
        return false; 
    }
    
    if (is_opponent_score_cell(tx, ty, player, rows, cols, score_cols)) { 
        out_msg = "cannot move into opponent score cell"; 
        return false; 
    }
    
    if (cell_empty(board[fy][fx]) || cell_owner(board[fy][fx]) != player) { 
        out_msg = "invalid piece"; 
        return false; 
    }

    if (cell_empty(board[ty][tx])) {
        // Simple move
        board[ty][tx] = board[fy][fx];
        board[fy][fx].clear();
        out_msg = "moved";
        return true;
    }

    // Move with push
    if (move.pushed_to.size() < 2) { 
        out_msg = "destination occupied; pushed_to required"; 
        return false; 
    }
    
    int ptx = move.pushed_to[0], pty = move.pushed_to[1];
    int dx = tx - fx, dy = ty - fy;
    
    if (ptx != tx + dx || pty != ty + dy) { 
        out_msg = "invalid pushed_to"; 
        return false; 
    }
    
    if (!in_bounds(ptx, pty, rows, cols)) { 
        out_msg = "pushed_to out of bounds"; 
        return false; 
    }
    
    if (is_opponent_score_cell(ptx, pty, player, rows, cols, score_cols)) { 
        out_msg = "cannot push into opponent score"; 
        return false; 
    }
    
    if (!cell_empty(board[pty][ptx])) { 
        out_msg = "pushed_to not empty"; 
        return false; 
    }

    board[pty][ptx] = board[ty][tx];
    board[ty][tx] = board[fy][fx];
    board[fy][fx].clear();
    out_msg = "moved with push";
    return true;
}

static bool apply_push_action(Board& board, const Move& move, const string& player, 
                              int rows, int cols, const vector<int>& score_cols, string& out_msg) {
    if (move.from.size() < 2 || move.to.size() < 2 || move.pushed_to.size() < 2) { 
        out_msg = "bad push format"; 
        return false; 
    }
    
    int fx = move.from[0], fy = move.from[1];
    int tx = move.to[0], ty = move.to[1];
    int px = move.pushed_to[0], py = move.pushed_to[1];

    if (!in_bounds(fx, fy, rows, cols) || !in_bounds(tx, ty, rows, cols) || 
        !in_bounds(px, py, rows, cols)) { 
        out_msg = "out of bounds"; 
        return false; 
    }
    
    if (is_opponent_score_cell(tx, ty, player, rows, cols, score_cols) || 
        is_opponent_score_cell(px, py, player, rows, cols, score_cols)) { 
        out_msg = "push would move into opponent score cell"; 
        return false; 
    }
    
    if (cell_empty(board[fy][fx]) || cell_owner(board[fy][fx]) != player) { 
        out_msg = "invalid piece"; 
        return false; 
    }
    
    if (cell_empty(board[ty][tx])) { 
        out_msg = "'to' must be occupied"; 
        return false; 
    }
    
    if (!cell_empty(board[py][px])) { 
        out_msg = "pushed_to not empty"; 
        return false; 
    }

    board[py][px] = board[ty][tx];
    board[ty][tx] = board[fy][fx];
    board[fy][fx].clear();
    
    // River converts to stone after push (game rule)
    if (cell_side(board[ty][tx]) == "river") {
        board[ty][tx]["side"] = "stone";
        board[ty][tx].erase("orientation");
    }
    
    out_msg = "pushed";
    return true;
}

static bool apply_flip_action(Board& board, const Move& move, const string& player, 
                              int rows, int cols, const vector<int>& score_cols, string& out_msg) {
    if (move.from.size() < 2) { 
        out_msg = "bad flip format"; 
        return false; 
    }
    
    int fx = move.from[0], fy = move.from[1];
    
    if (!in_bounds(fx, fy, rows, cols)) { 
        out_msg = "out of bounds"; 
        return false; 
    }
    
    if (cell_empty(board[fy][fx]) || cell_owner(board[fy][fx]) != player) { 
        out_msg = "invalid piece"; 
        return false; 
    }

    if (cell_side(board[fy][fx]) == "stone") {
        // Stone to river
        string orientation = move.orientation;
        if (orientation != "horizontal" && orientation != "vertical") { 
            out_msg = "stone->river needs orientation"; 
            return false; 
        }
        
        // Temporarily set to river and check safety
        string old_side = board[fy][fx]["side"];
        board[fy][fx]["side"] = "river";
        board[fy][fx]["orientation"] = orientation;
        
        auto flow = get_river_flow_destinations(board, fx, fy, fx, fy, player, rows, cols, score_cols,false);
        
        // Revert for safety check
        board[fy][fx]["side"] = old_side;
        board[fy][fx].erase("orientation");
        
        for (auto& d : flow) {
            if (is_opponent_score_cell(d.first, d.second, player, rows, cols, score_cols)) { 
                out_msg = "flip would allow flow into opponent score cell"; 
                return false; 
            }
        }
        
        // Apply flip
        board[fy][fx]["side"] = "river";
        board[fy][fx]["orientation"] = orientation;
        out_msg = "flipped to river";
        return true;
    } else {
        // River to stone
        board[fy][fx]["side"] = "stone";
        board[fy][fx].erase("orientation");
        out_msg = "flipped to stone";
        return true;
    }
}

static bool apply_rotate_action(Board& board, const Move& move, const string& player, 
                                int rows, int cols, const vector<int>& score_cols, string& out_msg) {
    if (move.from.size() < 2) { 
        out_msg = "bad rotate format"; 
        return false; 
    }
    
    int fx = move.from[0], fy = move.from[1];
    
    if (!in_bounds(fx, fy, rows, cols)) { 
        out_msg = "out of bounds"; 
        return false; 
    }
    
    if (cell_empty(board[fy][fx]) || cell_owner(board[fy][fx]) != player || 
        cell_side(board[fy][fx]) != "river") { 
        out_msg = "invalid rotate"; 
        return false; 
    }

    // Try rotation
    string old_orientation = cell_orientation(board[fy][fx]);
    string new_orientation = (old_orientation == "horizontal") ? "vertical" : "horizontal";
    board[fy][fx]["orientation"] = new_orientation;
    
    // Check flow safety after rotation
    auto flow = get_river_flow_destinations(board, fx, fy, fx, fy, player, rows, cols, score_cols, false);
    
    for (auto& d : flow) {
        if (is_opponent_score_cell(d.first, d.second, player, rows, cols, score_cols)) {
            // Revert rotation
            board[fy][fx]["orientation"] = old_orientation;
            out_msg = "rotate would allow flow into opponent score cell";
            return false;
        }
    }
    
    out_msg = "rotated";
    return true;
}

static pair<bool, string> agent_apply_move(Board& board, const Move& move, const string& player, 
                                           int rows, int cols, const vector<int>& score_cols) {
    string msg;
    bool ok = false;
    
    if (move.action == "move") {
        ok = apply_move_action(board, move, player, rows, cols, score_cols, msg);
    } else if (move.action == "push") {
        ok = apply_push_action(board, move, player, rows, cols, score_cols, msg);
    } else if (move.action == "flip") {
        ok = apply_flip_action(board, move, player, rows, cols, score_cols, msg);
    } else if (move.action == "rotate") {
        ok = apply_rotate_action(board, move, player, rows, cols, score_cols, msg);
    } else {
        msg = "unknown action";
    }
    
    return {ok, msg};
}

// ==================== MOVE GENERATION ====================

vector<Move> generate_all_moves(const Board& board, const string& player, 
                               int rows, int cols, const vector<int>& score_cols) {
    vector<Move> moves;
    vector<pair<int,int>> dirs = {{1,0}, {-1,0}, {0,1}, {0,-1}};
    
    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < cols; ++x) {
            const auto& p = board[y][x];
            if (cell_empty(p) || cell_owner(p) != player) 
                continue;
            
            if (cell_side(p) == "stone") {
                // Stone moves
                for (auto d : dirs) {
                    int nx = x + d.first, ny = y + d.second;
                    if (!in_bounds(nx, ny, rows, cols)) 
                        continue;
                    
                    if (is_opponent_score_cell(nx, ny, player, rows, cols, score_cols)) 
                        continue;
                    
                    if (cell_empty(board[ny][nx])) {
                        moves.emplace_back("move", vector<int>{x,y}, vector<int>{nx,ny});
                    } else {
                        const auto& target = board[ny][nx];
                        if (cell_side(target) == "river") {
                            auto flow = get_river_flow_destinations(board, nx, ny, x, y, 
                                                                   player, rows, cols, score_cols, false);
                            for (auto& d2 : flow) 
                                moves.emplace_back("move", vector<int>{x,y}, 
                                                 vector<int>{d2.first, d2.second});
                        } else {
                            // Push opponent stone
                            int px = nx + d.first, py = ny + d.second;
                            if (in_bounds(px, py, rows, cols) &&
                                cell_empty(board[py][px]) &&
                                // !is_own_score_cell(px, py, player, rows, cols, score_cols) &&
                                !is_own_score_cell(px, py, get_opponent(player), rows, cols, score_cols) ){
                                    if(target.at("owner") != player && is_own_score_cell(px, py, player, rows, cols, score_cols)){continue;}
                                // !is_opponent_score_cell(px, py, get_opponent(player), rows, cols, score_cols)) {
                                moves.emplace_back("push", vector<int>{x,y}, 
                                                 vector<int>{nx,ny}, vector<int>{px,py});
                                
                            }
                        }
                    }
                }
                
                // Stone flip to river
                for (auto& ori : {"horizontal", "vertical"}) {
                    Board tmp = copy_board(board);
                    if (tmp[y][x].empty()) continue;
                    tmp[y][x]["side"] = "river";
                    tmp[y][x]["orientation"] = ori;
                    auto flow = get_river_flow_destinations(tmp, x, y, x, y, player, rows, cols, score_cols, false);
                    bool safe = true;
                    for (auto& d2 : flow) {
                        if (is_opponent_score_cell(d2.first, d2.second, player, rows, cols, score_cols)) { 
                            safe = false; 
                            break; 
                        }
                    }
                    if (safe) 
                        moves.emplace_back("flip", vector<int>{x,y}, vector<int>{}, 
                                         vector<int>{}, string(ori));
                }
                
            } else { // River piece
                // River flip to stone
                moves.emplace_back("flip", vector<int>{x,y}, vector<int>{}, vector<int>{}, string());
                
                // River rotate
                string new_ori = (cell_orientation(p) == "horizontal") ? "vertical" : "horizontal";
                Board tmp = copy_board(board);
                tmp[y][x]["orientation"] = new_ori;
                auto flow = get_river_flow_destinations(tmp, x, y, x, y, player, rows, cols, score_cols, false);
                bool safe = true;
                for (auto& d2 : flow) {
                    if (is_opponent_score_cell(d2.first, d2.second, player, rows, cols, score_cols)) { 
                        safe = false; 
                        break; 
                    }
                }
                if (safe) 
                    moves.emplace_back("rotate", vector<int>{x,y}, vector<int>{}, vector<int>{}, string());
                
                // River movement (similar to stones)
                for (auto d : dirs) {
                    int nx = x + d.first, ny = y + d.second;
                    if (!in_bounds(nx, ny, rows, cols)) 
                        continue;
                    
                    if (is_opponent_score_cell(nx, ny, player, rows, cols, score_cols)) 
                        continue;
                    
                    if (cell_empty(board[ny][nx])) {
                        moves.emplace_back("move", vector<int>{x,y}, vector<int>{nx,ny});
                    } else {
                        const auto& target = board[ny][nx];
                        if (cell_side(target) == "river") {
                            auto flow = get_river_flow_destinations(board, nx, ny, x, y, 
                                                                   player, rows, cols, score_cols, false);
                            for (auto& d2 : flow) 
                                moves.emplace_back("move", vector<int>{x,y}, 
                                                 vector<int>{d2.first, d2.second});
                        } else {
                            // River pushing stone
                            auto flow = get_river_flow_destinations(board, nx, ny, x, y, 
                                                                   player, rows, cols, score_cols, true);
                            for (auto& d2 : flow) {
                                if (!is_opponent_score_cell(d2.first, d2.second, player, rows, cols, score_cols)) {
                                    moves.emplace_back("push", vector<int>{x,y}, 
                                                     vector<int>{nx,ny}, vector<int>{d2.first, d2.second});
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    
    return moves;
}

// ==================== BOARD EVALUATION ====================

int count_scoring_pieces(const Board& board, const string& player, 
                        int rows, int cols, const vector<int>& score_cols) {
    int n = 0;
    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < cols; ++x) {
            const auto& cell = board[y][x];
            if (!cell_empty(cell) && cell_owner(cell) == player && 
                cell_side(cell) == "stone" &&
                is_own_score_cell(x, y, player, rows, cols, score_cols)) {
                ++n;
            }
        }
    }
    return n;
}

// double basic_evaluate_board(const Board& board, const string& player, 
//                            int rows, int cols, const vector<int>& score_cols) {
//     double score = 0.0;
//     int top_row = top_score_row();
//     int bottom_row = bottom_score_row(rows);
    
//     for (int y = 0; y < rows; ++y) {
//         for (int x = 0; x < cols; ++x) {
//             const auto& piece = board[y][x];
//             if (cell_empty(piece)) 
//                 continue;
            
//             if (cell_owner(piece) == player && cell_side(piece) == "stone") {
//                 score += 1.0;
                
//                 // Bonus for stones in own scoring area
//                 if (is_own_score_cell(x, y, player, rows, cols, score_cols)) {
//                     score += 10.0;
//                 }
                
//                 // Small bonus for advancing toward opponent
//                 if (player == "circle") {
//                     score += (rows - y) * 0.1;
//                 } else {
//                     score += y * 0.1;
//                 }
//             } else if (cell_owner(piece) == opponent_of(player) && cell_side(piece) == "stone") {
//                 score -= 1.0;
                
//                 // Penalty if opponent has stones in their scoring area
//                 if (is_own_score_cell(x, y, opponent_of(player), rows, cols, score_cols)) {
//                     score -= 10.0;
//                 }
//             }
//         }
//     }
    
//     return score;
// }

// ==================== MOVE SIMULATION ====================

pair<bool, Board> simulate_move_on_copy(const Board& board, const Move& move, 
                                       const string& player, int rows, int cols, 
                                       const vector<int>& score_cols) {
    Board cp = copy_board(board);
    auto [ok, msg] = agent_apply_move(cp, move, player, rows, cols, score_cols);
    return {ok, cp};
}

// ==================== BASE AGENT CLASS ====================

class BaseAgent {
public:
    string player;
    string opponent;
    
    BaseAgent(const string& p) : player(p), opponent(opponent_of(p)) {}
    virtual ~BaseAgent() = default;

    // Pure virtual - must be implemented by derived classes
    virtual optional<Move> choose(const Board& board, int rows, int cols, 
                                 const vector<int>& score_cols, 
                                 double current_player_time, double opponent_time) = 0;

    // Helper methods available to all agents
    vector<Move> generate_all_moves_for(const Board& board, int rows, int cols, 
                                        const vector<int>& score_cols) {
        return generate_all_moves(board, player, rows, cols, score_cols);
    }

    // double evaluate_board(const Board& board, int rows, int cols, 
    //                      const vector<int>& score_cols) {
    //     return basic_evaluate_board(board, player, rows, cols, score_cols);
    // }

    pair<bool, Board> simulate_move(const Board& board, const Move& move, 
                                   int rows, int cols, const vector<int>& score_cols) {
        return simulate_move_on_copy(board, move, player, rows, cols, score_cols);
    }
};

// ==================== RANDOM AGENT ====================

class RandomAgent : public BaseAgent {
    mt19937 rng;
    
public:
    RandomAgent(const string& p) : BaseAgent(p) {
        rng.seed((unsigned) chrono::high_resolution_clock::now().time_since_epoch().count());
    }
    
    optional<Move> choose(const Board& board, int rows, int cols, 
                        const vector<int>& score_cols, 
                        double current_player_time, double opponent_time) override {
        auto moves = generate_all_moves_for(board, rows, cols, score_cols);
        if (moves.empty()) 
            return nullopt;
        uniform_int_distribution<int> dist(0, (int)moves.size() - 1);
        return moves[dist(rng)];
    }
};

// ==================== AGENT FACTORY ====================

unique_ptr<BaseAgent> get_agent(const string& player, const string& strategy) {
    string s = strategy;
    transform(s.begin(), s.end(), s.begin(), ::tolower);
    
    if (s == "random") 
        return make_unique<RandomAgent>(player);
    
    // For student agent, would need to link with StudentAgent class
    // Fallback to random
    return make_unique<RandomAgent>(player);
}

// ==================== HELPER FUNCTIONS (for debugging) ====================

void print_board(const Board& board) {
    int rows = (int)board.size();
    int cols = (rows > 0) ? (int)board[0].size() : 0;
    
    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < cols; ++x) {
            if (cell_empty(board[y][x])) {
                cout << ". ";
            } else {
                string o = cell_owner(board[y][x]);
                string s = cell_side(board[y][x]);
                char ch = (o == "circle") ? 'C' : 'S';
                if (s == "river") 
                    ch = (o == "circle") ? 'c' : 's'; // lowercase for river
                cout << ch << ' ';
            }
        }
        cout << '\n';
    }
}

Board create_empty_board(int rows, int cols) {
    return Board(rows, vector<map<string,string>>(cols));
}

Board create_default_start_board(int rows, int cols) {
    Board board = create_empty_board(rows, cols);
    int width = min(6, max(2, cols - 6));
    int start_col = (cols - width) / 2;
    
    vector<int> start_cols;
    for (int c = start_col; c < start_col + width; ++c) 
        start_cols.push_back(c);
    
    vector<int> top_rows = {3, 4};
    vector<int> bot_rows = {rows - 5, rows - 4};
    
    for (int r : top_rows) 
        for (int c : start_cols) 
            set_cell(board, c, r, "square", "stone");
    
    for (int r : bot_rows) 
        for (int c : start_cols) 
            set_cell(board, c, r, "circle", "stone");
    
    return board;
}
#pragma once
#include <vector>
#include <string>
#include <memory>
#include <map>
#include <optional>
#include <set>
// #include <
using namespace std;

struct Piece {
    string owner;          // "circle" or "square"
    string side;           // "stone" or "river"
    optional<string> orientation; // "horizontal" or "vertical" or nullopt

    Piece() = default;
    Piece(const string &o, const string &s, optional<string> orient = nullopt)
        : owner(o), side(s), orientation(orient) {}
};

using Board = std::vector<std::vector<std::map<std::string, std::string>>>;

struct Move {
    std::string action;
    std::vector<int> from;
    std::vector<int> to;
    std::vector<int> pushed_to;
    std::string orientation;

    Move() = default;
    Move(const std::string& act, const std::vector<int>& f, const std::vector<int>& t,
         const std::vector<int>& pt = {}, const std::string& orient = "")
        : action(act), from(f), to(t), pushed_to(pt), orientation(orient) {}
};
// Helper pair hash for unordered sets/maps
struct PairHash {
    size_t operator()(pair<int,int> p) const noexcept {
        return std::hash<long long>()(((long long)p.first << 32) ^ (unsigned long long)p.second);
    }
};
struct PairEq { bool operator()(pair<int,int> a, pair<int,int> b) const noexcept { return a==b; } };
pair<bool, Board> simulate_move_on_copy(const Board &board, const Move &move,const std::string& player, int rows, int cols, const vector<int> &score_cols);
vector<Move> generate_all_moves(const Board &board, const string &player, int rows, int cols, const vector<int> &score_cols);
vector<pair<int,int>> get_river_flow_destinations(
    const Board& board, int rx, int ry, int sx, int sy, const string& player,
    int rows, int cols, const vector<int>& score_cols, bool river_push);
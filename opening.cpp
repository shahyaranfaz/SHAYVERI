#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <algorithm>

#define OPENINGS_COUNT 96151
#define MAX_DEPTH 24
#define MAX_MOVE_LEN (MAX_DEPTH * 5)

struct Opening {
    std::string moves;
    int games_played = 0;
    int white_wins = 0;
    int draws = 0;
    int black_wins = 0;
    float avg_elo = 0;
};

struct MoveCandidate {
    std::string move;
    double total_score = 0;

    bool operator<(const MoveCandidate& other) const {
        return total_score > other.total_score;
    }
};

long long count_moves(const std::string& moves) {
    if (moves.empty()) return 0;
    return std::count(moves.begin(), moves.end(), ' ') + 1;
}

class OpeningBook {
public:
    explicit OpeningBook(const std::string& filename) {
        std::ifstream file(filename);
        std::string line;
        bool header_skipped = false;
        while (std::getline(file, line) && openings.size() < OPENINGS_COUNT) {
            if (!header_skipped) {
                header_skipped = true;
                continue;
            }
            std::istringstream ss(line);
            std::string token;
            Opening o;
            if (!std::getline(ss, token, ',')) continue;
            o.moves = token;
            if (std::getline(ss, token, ',')) o.games_played = std::stoi(token);
            if (std::getline(ss, token, ',')) o.white_wins = std::stoi(token);
            if (std::getline(ss, token, ',')) o.draws = std::stoi(token);
            if (std::getline(ss, token, ',')) o.black_wins = std::stoi(token);
            if (std::getline(ss, token, ',')) o.avg_elo = std::stof(token);
            openings.push_back(o);
        }
    }

    std::string get_best_book_move(const std::string& game_history) const {
        const long long move_count = game_history.empty() ? 0 : count_moves(game_history);
        const bool is_white_to_move = (move_count % 2 == 0);
        const size_t hist_len = game_history.size();

        std::vector<MoveCandidate> candidates;

        for (const auto& opening : openings) {
            if (opening.moves.compare(0, hist_len, game_history, 0, hist_len) == 0) {
                if (hist_len > 0 && opening.moves.size() > hist_len && opening.moves[hist_len] != ' ') continue;
                if (opening.moves.size() <= hist_len) continue;
                const size_t move_pos = (hist_len == 0) ? 0 : hist_len + 1;
                const size_t next_space = opening.moves.find(' ', move_pos);
                std::string next_move = opening.moves.substr(move_pos, next_space - move_pos);
                if (next_move.empty()) continue;
                const double elo_weight = opening.avg_elo / 2600.0;
                const int wins = is_white_to_move ? opening.white_wins : opening.black_wins;
                const double score = wins * elo_weight;
                auto it = std::find_if(
                    candidates.begin(), candidates.end(),
                    [&](const MoveCandidate& mc) { return mc.move == next_move; }
                );
                if (it != candidates.end()) {
                    it->total_score += score;
                } else if (candidates.size() < 256) {
                    candidates.push_back(MoveCandidate{ next_move, score });
                }
            }
        }
        if (candidates.empty()) return "";
        const auto best = std::max_element(candidates.begin(), candidates.end());
        return best->move;
    }

private:
    std::vector<Opening> openings;
};
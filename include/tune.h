#ifndef TUNE_H
#define TUNE_H

// Centralized tuning parameters for SPSA / Texel tuning.
// All values are intentionally non-const so they can be adjusted at runtime
// by a tuner without recompilation.  The defaults match the hand-tuned values
// used during normal play.

namespace ShayBot {
namespace Tune {

// ----------------------------------------------------------------
// Search parameters
// ----------------------------------------------------------------

// Singular extensions
inline int se_min_depth       = 8;
inline int se_depth_margin    = 3;
inline int se_margin          = 20;
inline int se_reduction_denom = 2;

// History gravity
inline int history_max         = 16384;
inline int history_bonus_mult  = 300;
inline int history_bonus_sub   = 200;
inline int history_bonus_limit = 1500;

// History blending weights (percent)
inline int main_history_weight = 100;
inline int cmh_weight          = 100;
inline int fmh_weight          = 100;

// Pruning margins
inline int rfp_margin_mult   = 120;
inline int fp_base           = 150;
inline int fp_mult           = 150;
inline int lmp_base          = 3;
inline int lmp_mult          = 2;
inline int see_pruning_margin = -100;

// ----------------------------------------------------------------
// Evaluation parameters
// ----------------------------------------------------------------

// Misc
inline int TEMPO_BONUS       = 8;
inline int BISHOP_PAIR_BONUS = 30;

// Passed pawns (indexed by relative rank 0-7)
inline int PASSED_PAWN_BONUS_MG[8] = { 0,  5, 10, 20,  30,  50,  70, 0 };
inline int PASSED_PAWN_BONUS_EG[8] = { 0, 10, 20, 40,  60,  90, 120, 0 };

// Candidate / connected / outside passed
inline int CANDIDATE_PAWN_BONUS_MG   = 8;
inline int CANDIDATE_PAWN_BONUS_EG   = 12;
inline int CONNECTED_PASSED_BONUS_MG = 10;
inline int CONNECTED_PASSED_BONUS_EG = 18;
inline int OUTSIDE_PASSED_BONUS_MG   = 8;
inline int OUTSIDE_PASSED_BONUS_EG   = 20;

// Pawn structure penalties
inline int ISOLATED_PAWN_PENALTY_MG = -12;
inline int ISOLATED_PAWN_PENALTY_EG = -8;
inline int DOUBLED_PAWN_PENALTY_MG  = -14;
inline int DOUBLED_PAWN_PENALTY_EG  = -10;
inline int BACKWARD_PAWN_PENALTY_MG = -10;
inline int BACKWARD_PAWN_PENALTY_EG = -6;
inline int SUPPORTED_PAWN_BONUS_MG  = 6;
inline int WEAK_PAWN_PENALTY_MG     = -8;
inline int PAWN_ISLAND_PENALTY_MG   = -10;
inline int PAWN_ISLAND_PENALTY_EG   = -6;

// Pawn center control
inline int PAWN_CENTER_BONUS_MG     = 6;
inline int PAWN_CENTER_BONUS_EG     = 2;
inline int PAWN_EXT_CENTER_BONUS_MG = 3;
inline int PAWN_EXT_CENTER_BONUS_EG = 1;

// Pawn storm
inline int PAWN_STORM_BASE      = 4;
inline int PAWN_STORM_RANK_MULT = 2;

// King safety
inline int KING_SHIELD_MISSING_PENALTY  = -14;
inline int KING_OPEN_FILE_PENALTY       = -10;
inline int KING_SEMI_OPEN_FILE_PENALTY  = -6;
inline int KING_ESCAPE_BONUS            = 4;
inline int KING_ATTACKER_WEIGHT[7]      = { 0, 0, 2, 2, 3, 5, 0 };
inline int KING_ATTACK_COUNT_BONUS[8]   = { 0, 0, 3, 8, 16, 26, 38, 52 };
inline int KING_DANGER_DIVISOR          = 8;
inline int KING_DANGER_MAX              = 500;

// Mobility
inline int MOBILITY_KNIGHT_MG = 4;
inline int MOBILITY_BISHOP_MG = 4;
inline int MOBILITY_ROOK_MG   = 2;
inline int MOBILITY_QUEEN_MG  = 1;
inline int MOBILITY_KNIGHT_EG = 3;
inline int MOBILITY_BISHOP_EG = 4;
inline int MOBILITY_ROOK_EG   = 3;
inline int MOBILITY_QUEEN_EG  = 2;

// File/diagonal openness multipliers (percent)
inline int OPEN_FILE_MULTIPLIER          = 120;
inline int SEMI_OPEN_FILE_MULTIPLIER     = 110;
inline int CLOSED_FILE_MULTIPLIER        = 100;
inline int BISHOP_OPENNESS_BASE          = 100;
inline int BISHOP_OPENNESS_MAX_BONUS     = 30;
inline int BISHOP_OPENNESS_SQUARE_WEIGHT = 2;

// Territory
inline int CENTER_BONUS           = 8;
inline int EXT_CENTER_BONUS       = 4;
inline int ENEMY_HALF_BONUS       = 3;
inline int SEVENTH_RANK_BONUS_MG  = 20;
inline int SEVENTH_RANK_BONUS_EG  = 30;

// Coordination
inline int DEFENDED_PIECE_BONUS      = 4;
inline int SHARED_TARGET_BONUS       = 8;
inline int BATTERY_ROOK_QUEEN_BONUS  = 12;
inline int BATTERY_BISHOP_QUEEN_BONUS= 8;
inline int SUPPORT_CHAIN_BONUS       = 6;

// Tactical pressure
inline int UNDEFENDED_ATTACK_BONUS       = 6;
inline int PIN_BONUS                     = 15;
inline int OVERLOADED_DEFENDER_BONUS     = 10;
inline int UNRECIPROCATED_PRESSURE_BONUS = 2;
inline int UNDEFENDED_VALUE_DIVISOR      = 40;

// Threats — bonus when a pawn attacks an enemy piece of the given type
inline int THREAT_BY_PAWN_MG[7]  = { 0,  0, 60, 65, 75, 80,  0 };
inline int THREAT_BY_PAWN_EG[7]  = { 0,  0, 40, 45, 55, 60,  0 };
// Bonus when a minor attacks an enemy piece of higher value
inline int THREAT_BY_MINOR_MG[7] = { 0,  0,  0,  0, 30, 45,  0 };
inline int THREAT_BY_MINOR_EG[7] = { 0,  0,  0,  0, 20, 30,  0 };
// Bonus when a rook attacks an enemy queen
inline int THREAT_BY_ROOK_MG = 25;
inline int THREAT_BY_ROOK_EG = 18;

// Hanging piece penalties
inline int HANGING_BASE_PENALTY_MG = 20;
inline int HANGING_BASE_PENALTY_EG = 12;
inline int HANGING_VALUE_DIVISOR   = 40;

// Outposts
inline int KNIGHT_OUTPOST_MG = 20;
inline int KNIGHT_OUTPOST_EG = 14;
inline int BISHOP_OUTPOST_MG = 12;
inline int BISHOP_OUTPOST_EG = 8;
inline int ROOK_OUTPOST_MG   = 14;
inline int ROOK_OUTPOST_EG   = 12;
inline int QUEEN_OUTPOST_MG  = 10;
inline int QUEEN_OUTPOST_EG  = 8;

// Development / initiative
inline int DEVELOPMENT_BONUS = 5;
inline int CASTLED_BONUS     = 12;

} // namespace Tune
} // namespace ShayBot

#endif // TUNE_H

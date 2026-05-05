#ifndef TUNE_H
#define TUNE_H

// Centralized parameters for SPSA / Texel tuning.
// All numeric values are intentionally non-const (inline variables, C++17) so
// a tuner can modify them at runtime without recompilation.
//
// ODR-safety: tuning_registry and handle_setoption are marked `inline` so this
// header can be safely included in multiple translation units (C++17 inline
// variable / inline function rules guarantee a single definition).

#include "types.h"

#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace chess { class Board; }

namespace SHAYVERI {

namespace Tune {

// ================================================================
// Search constants
// ================================================================

// Compile-time limits (used in array sizes / constant expressions)
static constexpr int MAX_PLY    = 128; //
static constexpr int INF        = 1000000; //
static constexpr int MATE_SCORE = 900000; //

// Aspiration window initial delta
inline int ASP_DELTA = 32; //

// Singular extensions
inline int se_min_depth       =  9; //
inline int se_depth_margin    =  2; //
inline int se_margin          = 55; //
inline int se_reduction_denom =  3; //

// History gravity / bonus
static constexpr int history_max = 16384; //
inline int history_bonus_mult    =   463; //
inline int history_bonus_sub     =   164; //
inline int history_bonus_limit   =  2967; //

// History blending weights (percent, divided by 100 before accumulation)
inline int main_history_weight = 79; //
inline int cmh_weight          = 83; //
inline int fmh_weight          = 36; //

// Pruning margins
inline int rfp_margin_mult    =   84; //
inline int fp_base            =  195; //
inline int fp_mult            =  653; //
inline int lmp_base           =    3; //
inline int lmp_mult           =    1; //
inline int see_pruning_margin = -281; //

// LMR formula coefficients: reduction = lmr_base + log(d)*log(m)/lmr_scale
inline double lmr_base  = 1.25; //
inline double lmr_scale = 1.79; //

// ================================================================
// Piece values
// ================================================================

// Per-piece (indexed by Piece enum), MG and EG
inline int PIECE_VALUES_MG[PIECE_COUNT] = {
    0,   114, 402, 446, 581, 1066, 0,
         114, 402, 446, 581, 1066, 0,
};
inline int PIECE_VALUES_EG[PIECE_COUNT] = {
    0,   126, 366, 371, 679, 1077, 0,
         126, 366, 371, 679, 1077, 0,
};

// Indexed by PieceType (0-6).  King = 20000 for SEE termination.
static constexpr int PTYPE_VALUE[7]  = { 0, 100, 320, 330,  500,  900, 20000 }; //
// Indexed by PieceType (0-6).  King = 0 for MVV-LVA / capture ordering.
static constexpr int PTYPE_VALUES[7] = { 0, 100, 320, 330,  500,  900,     0 }; //

// ================================================================
// Phase (tapered eval)
// ================================================================

static constexpr int MAX_PHASE = 24; //
static constexpr int PHASE_WEIGHTS[PIECE_COUNT] = { //
    0, 0, 1, 1, 2, 4, 0,
       0, 1, 1, 2, 4, 0,
};

// ================================================================
// Piece-square tables  (white perspective, index 0 = a1)
// ================================================================

inline int PST_PAWN_MG[64] = {
      0,   0,   0,   0,   0,   0,   0,   0,
    -21, -38, -48, -45, -49, -16,  -6, -37,
    -21, -44, -38, -36, -28, -12,  -5, -14,
    -13, -27, -16, -20,   0,  -7, -16, -20,
      5, -10,  -7,  11,  18,  28,   0,  -5,
     14,  21,  19,  13,  28, 113,  65,  20,
    126,  85,  87,  88,  84,  35, -36,  -9,
      0,   0,   0,   0,   0,   0,   0,   0,
};
inline int PST_PAWN_EG[64] = {
      0,   0,   0,   0,   0,   0,   0,   0,
     11,  10,   3,   6,  19,  -2,  -7, -18,
      7,   7,  -1,   5,   6,  -5,  -6, -14,
     13,  11, -10,  -9, -12, -10,   1,  -8,
     29,  20,  11, -15,  -9, -12,   2,  -2,
     53,  42,  28,   8,  -1, -10, -12,  14,
     33,  51,  46,  20,   4,  43,  54,  30,
      0,   0,   0,   0,   0,   0,   0,   0,
};

inline int PST_KNIGHT_MG[64] = {
     -47,  -10,  -32,  -19,   -1,   -2,   -8,  -75,
     -31,  -28,  -16,    7,    1,   -3,  -20,   -8,
     -25,  -18,   -9,   16,   20,    1,    0,  -11,
     -12,    4,    9,   14,   26,   16,   46,   20,
      -4,   -5,   12,   35,    2,   36,   -2,   36,
     -40,   -1,   22,   39,   67,   95,   47,   41,
     -26,  -26,    8,   41,   55,   62,   -8,    7,
    -111,  -33,  -31,   -5,    9,  -22,  -25,  -80,
};
inline int PST_KNIGHT_EG[64] = {
    -40, -57, -24, -12, -23, -29, -47, -44,
    -31,   3, -14,  -9,  -5, -22,  -9, -23,
    -30,   1, -12,   9,   5, -14,  -3, -14,
      4,  13,  20,  24,  23,  26,  19,   6,
      2,  12,  17,  29,  36,  22,  32,   9,
     -2,  12,  16,  11,  10,  15,  11,   9,
     -5,  13,   9,  31,  20,   2,  16, -10,
    -58,   5,  22,  21,  22,  25,  10, -55,
};

inline int PST_BISHOP_MG[64] = {
    -32,   2,   3, -30, -25,   1,  -2,   1,
     -3,  13,   8,   2,   0,  11,  31,  18,
     -4,  -7,   7,   5,   8,   9,  17,  13,
    -11,   5,  -2,  22,  29,  -4,   2,  16,
     -8,   6,  16,  22,  24,  34,   7,  -9,
     -7,  11,  23,  17,  49,  63,  60,  21,
    -44, -29,   6, -26, -17,  -5, -49, -49,
    -21, -15, -51, -28, -30, -49,  -8, -17,
};
inline int PST_BISHOP_EG[64] = {
    -13, -16, -25,  -3,   2, -13, -14,  -2,
    -16, -23, -15,  -4,  -4, -19, -30, -35,
     -1,   1,  -4,   1,   3,  -7,  -8,   2,
      8,  -3,   1, -10, -15,   0,   9,   7,
      8,   3, -18, -18, -17, -13,  13,  21,
     18,   8,  -7, -11,  -6,  -7,  -1,  19,
     26,  12,   8,  17,  10,   6,  13,  17,
     27,  18,  19,  20,  16,  18,  10,  17,
};

inline int PST_ROOK_MG[64] = {
    -17, -19, -12,   0,  -1,   3,  -3, -15,
    -48, -41, -30, -22, -14,  -7, -13, -61,
    -40, -41, -39, -33, -25, -34, -26, -44,
    -43, -47, -36, -29, -22, -26, -16, -30,
    -23, -17,   3,  26,  16,  26,   9,  -5,
    -15,  25,  22,  47,  66,  65,  69,  42,
     -9, -24,   4,  19,  27,  49,  34,  58,
     54,  47,   9,  22,  20,  49,  56,  60,
};
inline int PST_ROOK_EG[64] = {
    -37, -21, -17, -27, -32, -19, -27, -28,
    -13, -15, -12, -16, -26, -35, -36, -21,
    -17,  -3,  -4, -10, -16,  -9, -17, -12,
     15,  23,  17,   8,   6,  10,   6,   8,
     22,  20,  14,  10,  11,  13,  12,  19,
     26,  14,  16,   5,   1,   9,   1,  12,
     -2,   5,  -1,  -3,  -7, -18, -10, -17,
      7,  19,  33,  27,  37,  38,  37,  27,
};

inline int PST_QUEEN_MG[64] = {
    -51,   1,   4,  19,  25,  -4, -25, -11,
      5,  19,  31,  26,  31,  48,  37,  10,
     -7,   2,   7,   8,  17,  16,  23,  26,
     -3,  -3, -11, -12,  -2,   9,   9,  36,
    -17, -25, -15, -48, -25,   6,  14,   6,
    -21, -28, -31, -23,   6,  16,  22, -18,
    -37, -76, -49, -50, -53,   4, -35,  50,
     -3,  12,  12,   6,  19,  27,  36,  38,
};
inline int PST_QUEEN_EG[64] = {
     -52,  -59,  -43,  -45,  -65,  -58,  -52,  -48,
     -21,  -46,  -67,  -38,  -40, -110, -109,  -40,
       0,  -15,  -20,  -23,  -22,   -2,  -15,   -5,
      19,   -1,  -12,  -19,   -5,   29,   39,   46,
      23,   28,   -6,    8,   27,   49,   61,   60,
      26,   15,   16,   25,   27,   46,   48,   56,
      25,   16,    9,   26,   45,   24,   19,   36,
       9,   17,   22,   15,   26,   34,   35,   32,
};

inline int PST_KING_MG[64] = {
     20, 30, 10,  0,  0, 10, 30, 20,
     20, 20,  0,  0,  0,  0, 20, 20,
    -10,-20,-20,-20,-20,-20,-20,-10,
    -20,-30,-30,-40,-40,-30,-30,-20,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
};
inline int PST_KING_EG[64] = {
    -50,-30,-30,-30,-30,-30,-30,-50,
    -30,-30,  0,  0,  0,  0,-30,-30,
    -30,-10, 20, 30, 30, 20,-10,-30,
    -30,-10, 30, 40, 40, 30,-10,-30,
    -30,-10, 30, 40, 40, 30,-10,-30,
    -30,-10, 20, 30, 30, 20,-10,-30,
    -30,-20,-10,  0,  0,-10,-20,-30,
    -50,-40,-30,-20,-20,-30,-40,-50,
};

// ================================================================
// Evaluation parameters
// ================================================================

// Misc
inline int TEMPO_BONUS_MG = 27;
inline int TEMPO_BONUS_EG = 11;
inline int BISHOP_PAIR_BONUS_MG = 22;
inline int BISHOP_PAIR_BONUS_EG = 11;

// Passed pawns (indexed by relative rank 0-7)
inline int PASSED_PAWN_BONUS_MG[8] = { 0, 2, 4, 6, 33, 76, 119, 0 };
inline int PASSED_PAWN_BONUS_EG[8] = { 0, 5, 8, 39, 72, 125, 164, 0 };

// Candidate / connected / outside passed
inline int CANDIDATE_PAWN_BONUS_MG   =  9;
inline int CANDIDATE_PAWN_BONUS_EG   = 11;
inline int CONNECTED_PASSED_BONUS_MG = 8;
inline int CONNECTED_PASSED_BONUS_EG = 23;
inline int OUTSIDE_PASSED_BONUS_MG   = 17;
inline int OUTSIDE_PASSED_BONUS_EG   =  8;

// Pawn structure penalties
inline int ISOLATED_PAWN_PENALTY_MG =  -1;
inline int ISOLATED_PAWN_PENALTY_EG = -14;
inline int DOUBLED_PAWN_PENALTY_MG  = -11;
inline int DOUBLED_PAWN_PENALTY_EG  = -15;
inline int BACKWARD_PAWN_PENALTY_MG =  -8;
inline int BACKWARD_PAWN_PENALTY_EG =  -3;
inline int SUPPORTED_PAWN_BONUS_MG  =  15;
inline int SUPPORTED_PAWN_BONUS_EG  =   7;
inline int WEAK_PAWN_PENALTY_MG     = -21;
inline int WEAK_PAWN_PENALTY_EG     = -10;
inline int PAWN_ISLAND_PENALTY_MG   = -25;
inline int PAWN_ISLAND_PENALTY_EG   = -13;

// Pawn storm toward enemy king
inline int PAWN_STORM_BASE      = -43;
inline int PAWN_STORM_RANK_MULT =  14;

// King safety
inline int KING_SHIELD_MISSING_PENALTY = -21;
inline int KING_OPEN_FILE_PENALTY      = -20;
inline int KING_SEMI_OPEN_FILE_PENALTY = -11;
inline int KING_ESCAPE_BONUS           =   4;
inline int KING_ATTACKER_WEIGHT[7]     = { 0, 0, 2,  3,  4, 11, 0 };
inline int KING_ATTACK_COUNT_BONUS[8]  = { 0, 0, 7, 25, 33, 44, 27, 16 };
inline int KING_DANGER_DIVISOR         =  10;
inline int KING_DANGER_MAX             = 641;

// Mobility
inline int MOBILITY_KNIGHT_MG =  8;
inline int MOBILITY_KNIGHT_EG =  7;
inline int MOBILITY_BISHOP_MG =  5;
inline int MOBILITY_BISHOP_EG =  9;
inline int MOBILITY_ROOK_MG   =  3;
inline int MOBILITY_ROOK_EG   =  5;
inline int MOBILITY_QUEEN_MG  =  0;
inline int MOBILITY_QUEEN_EG  = 14;

// File/diagonal openness multipliers (percent)
inline int OPEN_FILE_MULTIPLIER             = 121;
inline int SEMI_OPEN_FILE_MULTIPLIER        = 117;
static constexpr int CLOSED_FILE_MULTIPLIER = 100; //
static constexpr int BISHOP_OPENNESS_BASE   = 100; //
inline int BISHOP_OPENNESS_MAX_BONUS        =  34;
inline int BISHOP_OPENNESS_SQUARE_WEIGHT    =   2;

// Territory
inline int SEVENTH_RANK_BONUS_MG       =  16;
inline int SEVENTH_RANK_BONUS_EG       =  24;
inline int QUEEN_SEVENTH_RANK_BONUS_MG = -11;
inline int QUEEN_SEVENTH_RANK_BONUS_EG =  48;

// Coordination
inline int DEFENDED_PIECE_BONUS_MG       =  0;
inline int DEFENDED_PIECE_BONUS_EG       =  0;
inline int SHARED_TARGET_BONUS_MG        =  0;
inline int SHARED_TARGET_BONUS_EG        = 42;
inline int BATTERY_ROOK_QUEEN_BONUS_MG   =  5;
inline int BATTERY_ROOK_QUEEN_BONUS_EG   = 56;
inline int BATTERY_BISHOP_QUEEN_BONUS_MG =  8;
inline int BATTERY_BISHOP_QUEEN_BONUS_EG = 40;
inline int SUPPORT_CHAIN_BONUS_MG        =  4;
inline int SUPPORT_CHAIN_BONUS_EG        = 13;

// Tactical pressure
inline int UNDEFENDED_ATTACK_BONUS          = 18;
inline int PIN_BONUS_MG                     = 18;
inline int PIN_BONUS_EG                     = 18;
inline int OVERLOADED_DEFENDER_BONUS_MG     =  2;
inline int OVERLOADED_DEFENDER_BONUS_EG     =  0;
inline int UNRECIPROCATED_PRESSURE_BONUS_MG =  1;
inline int UNRECIPROCATED_PRESSURE_BONUS_EG = 13;
inline int UNDEFENDED_VALUE_DIVISOR         = 66;

// Threats – bonus when a pawn attacks an enemy piece of the given type
inline int THREAT_BY_PAWN_MG[7] = { 0, 0, 48, 45, 59, 26, 0 };
inline int THREAT_BY_PAWN_EG[7] = { 0, 0, 25, 51, 0, 18, 0 };
// Bonus when a minor attacks an enemy piece of higher value
inline int THREAT_BY_MINOR_MG[7] = { 0, 0, 0, 0, 46, 15, 0 };
inline int THREAT_BY_MINOR_EG[7] = { 0, 0, 0, 0, 0, 8, 0 };
// Bonus when a rook attacks an enemy queen
inline int THREAT_BY_ROOK_MG = 28;
inline int THREAT_BY_ROOK_EG =  0;

// Hanging piece penalties
inline int HANGING_BASE_PENALTY_MG = 36;
inline int HANGING_BASE_PENALTY_EG =  0;
inline int HANGING_VALUE_DIVISOR   = 21;

// Outposts
inline int KNIGHT_OUTPOST_MG =  8;
inline int KNIGHT_OUTPOST_EG = 33;
inline int BISHOP_OUTPOST_MG = 21;
inline int BISHOP_OUTPOST_EG = 13;
inline int ROOK_OUTPOST_MG   = 23;
inline int ROOK_OUTPOST_EG   =  0;
inline int QUEEN_OUTPOST_MG  = -2;
inline int QUEEN_OUTPOST_EG  = 25;

// Development / initiative
inline int DEVELOPMENT_BONUS = 12;
inline int CASTLED_BONUS     =  5;

// ================================================================
// Tuning infrastructure
// ================================================================

struct TuningOption {
    void*       ptr;
    enum Type { INT, DOUBLE } type;
    int         min_val;     // used for UCI spin min  (ignored for DOUBLE)
    int         max_val;     // used for UCI spin max  (ignored for DOUBLE)
    std::string default_str; // actual default, output verbatim in UCI response
};

// `inline` on the map and the function body guarantees ODR-safety when this
// header is included in more than one translation unit (C++17).
inline std::unordered_map<std::string, TuningOption> tuning_registry = {

    // Core search
    {"ASP_Delta",           {&ASP_DELTA,              TuningOption::INT,    18,    55,  "32"}},
    {"LMR_Base",            {&lmr_base,               TuningOption::DOUBLE,  0,     0,   "1.25"}}, // DOUBLE ignores min/max
    {"LMR_Scale",           {&lmr_scale,              TuningOption::DOUBLE,  0,     0,   "1.79"}}, // DOUBLE ignores min/max
    {"RFP_Margin",          {&rfp_margin_mult,        TuningOption::INT,    40,   160,  "84"}},
    {"FP_Base",             {&fp_base,                TuningOption::INT,   110,   330,  "195"}},
    {"FP_Mult",             {&fp_mult,                TuningOption::INT,   400,   950,  "653"}},
    {"LMP_Base",            {&lmp_base,               TuningOption::INT,     2,     7,   "3"}},
    {"LMP_Mult",            {&lmp_mult,               TuningOption::INT,     1,     6,   "1"}},   // Skipped pass 2
    {"SEE_Pruning_Margin",  {&see_pruning_margin,     TuningOption::INT,  -420,  -120, "-281"}},
    {"SE_Min_Depth",        {&se_min_depth,           TuningOption::INT,     5,    16,   "9"}},
    {"SE_Depth_Margin",     {&se_depth_margin,        TuningOption::INT,     1,     5,   "2"}},
    {"SE_Margin",           {&se_margin,              TuningOption::INT,    25,    95,  "55"}},
    {"SE_Reduction_Denom",  {&se_reduction_denom,     TuningOption::INT,     1,     6,   "3"}},

    // History
    {"History_Bonus_Mult",  {&history_bonus_mult,     TuningOption::INT,   280,   720,  "463"}},
    {"History_Bonus_Sub",   {&history_bonus_sub,      TuningOption::INT,    60,   340,  "164"}},
    {"History_Bonus_Limit", {&history_bonus_limit,    TuningOption::INT,  1500,  4500, "2967"}},
    {"Main_History_Weight", {&main_history_weight,    TuningOption::INT,    25,   160,   "79"}},
    {"CMH_Weight",          {&cmh_weight,             TuningOption::INT,    15,   160,   "83"}},
    {"FMH_Weight",          {&fmh_weight,             TuningOption::INT,     0,   130,   "36"}},

    // Eval – misc
    {"Tempo_Bonus_MG",      {&TEMPO_BONUS_MG,         TuningOption::INT,     4,    48,   "22"}},
    {"Tempo_Bonus_EG",      {&TEMPO_BONUS_EG,         TuningOption::INT,     4,    48,   "22"}},
    {"Tempo_Bonus",         {&TEMPO_BONUS_MG,         TuningOption::INT,     4,    48,   "22"}},
    {"Bishop_Pair_Bonus_MG", {&BISHOP_PAIR_BONUS_MG,  TuningOption::INT,     8,    55,   "22"}},
    {"Bishop_Pair_Bonus_EG", {&BISHOP_PAIR_BONUS_EG,  TuningOption::INT,     4,    40,   "11"}},
    {"Bishop_Pair_Bonus",    {&BISHOP_PAIR_BONUS_MG,  TuningOption::INT,     8,    55,   "22"}},
    {"Development_Bonus",   {&DEVELOPMENT_BONUS,      TuningOption::INT,     0,    28,   "12"}},
    {"Castled_Bonus",       {&CASTLED_BONUS,          TuningOption::INT,     0,    38,    "5"}},

    // Mobility
    {"Mobility_Knight_MG",  {&MOBILITY_KNIGHT_MG,     TuningOption::INT,     0,    16,    "6"}},
    {"Mobility_Bishop_MG",  {&MOBILITY_BISHOP_MG,     TuningOption::INT,     2,    22,   "10"}},
    {"Mobility_Rook_MG",    {&MOBILITY_ROOK_MG,       TuningOption::INT,     0,    14,    "5"}},
    {"Mobility_Queen_MG",   {&MOBILITY_QUEEN_MG,      TuningOption::INT,     0,    12,    "4"}},
    {"Mobility_Knight_EG",  {&MOBILITY_KNIGHT_EG,     TuningOption::INT,     0,    16,    "5"}},
    {"Mobility_Bishop_EG",  {&MOBILITY_BISHOP_EG,     TuningOption::INT,     0,    16,    "4"}},
    {"Mobility_Rook_EG",    {&MOBILITY_ROOK_EG,       TuningOption::INT,     0,    12,    "2"}},
    {"Mobility_Queen_EG",   {&MOBILITY_QUEEN_EG,      TuningOption::INT,     0,     8,    "1"}},

    // Pawn structure
    {"Isolated_MG",         {&ISOLATED_PAWN_PENALTY_MG, TuningOption::INT, -35,     0,   "-15"}},
    {"Isolated_EG",         {&ISOLATED_PAWN_PENALTY_EG, TuningOption::INT, -28,     0,   "-20"}},
    {"Doubled_MG",          {&DOUBLED_PAWN_PENALTY_MG,  TuningOption::INT, -45,     0,  "-12"}},
    {"Doubled_EG",          {&DOUBLED_PAWN_PENALTY_EG,  TuningOption::INT, -30,     0,   "-9"}},
    {"Backward_MG",         {&BACKWARD_PAWN_PENALTY_MG, TuningOption::INT, -40,     0,  "-13"}},
    {"Backward_EG",         {&BACKWARD_PAWN_PENALTY_EG, TuningOption::INT, -25,     0,   "-8"}},
    {"Supported_Pawn_MG",   {&SUPPORTED_PAWN_BONUS_MG,  TuningOption::INT,   3,    38,   "14"}},
    {"Supported_Pawn_EG",   {&SUPPORTED_PAWN_BONUS_EG,  TuningOption::INT,   3,    38,   "7"}},
    {"Weak_Pawn_MG",        {&WEAK_PAWN_PENALTY_MG,     TuningOption::INT, -50,    -3,  "-18"}},
    {"Weak_Pawn_EG",        {&WEAK_PAWN_PENALTY_EG,     TuningOption::INT, -50,    -3,  "-9"}},
    {"Pawn_Island_MG",      {&PAWN_ISLAND_PENALTY_MG,   TuningOption::INT, -60,    -5,  "-32"}},
    {"Pawn_Island_EG",      {&PAWN_ISLAND_PENALTY_EG,   TuningOption::INT, -25,     0,   "-5"}},
    {"Pawn_Storm_Base",     {&PAWN_STORM_BASE,          TuningOption::INT,   0,    16,    "2"}},
    {"Pawn_Storm_Rank_Mult",{&PAWN_STORM_RANK_MULT,     TuningOption::INT,   0,     9,    "1"}},

    // Passed pawns (ranks 1-6; ranks 0 and 7 are always 0)
    // Note: Moved to Texel tuning. Ranges kept as original.
    {"PassedMG_R1",         {&PASSED_PAWN_BONUS_MG[1], TuningOption::INT,  0,   20,   "5"}},
    {"PassedMG_R2",         {&PASSED_PAWN_BONUS_MG[2], TuningOption::INT,  0,   30,   "10"}},
    {"PassedMG_R3",         {&PASSED_PAWN_BONUS_MG[3], TuningOption::INT,  0,   50,   "20"}},
    {"PassedMG_R4",         {&PASSED_PAWN_BONUS_MG[4], TuningOption::INT,  0,   80,   "30"}},
    {"PassedMG_R5",         {&PASSED_PAWN_BONUS_MG[5], TuningOption::INT,  0,   120,  "50"}},
    {"PassedMG_R6",         {&PASSED_PAWN_BONUS_MG[6], TuningOption::INT,  0,   150,  "70"}},
    {"PassedEG_R1",         {&PASSED_PAWN_BONUS_EG[1], TuningOption::INT,  0,   30,   "10"}},
    {"PassedEG_R2",         {&PASSED_PAWN_BONUS_EG[2], TuningOption::INT,  0,   50,   "20"}},
    {"PassedEG_R3",         {&PASSED_PAWN_BONUS_EG[3], TuningOption::INT,  0,   80,   "40"}},
    {"PassedEG_R4",         {&PASSED_PAWN_BONUS_EG[4], TuningOption::INT,  0,   130,  "60"}},
    {"PassedEG_R5",         {&PASSED_PAWN_BONUS_EG[5], TuningOption::INT,  0,   180,  "90"}},
    {"PassedEG_R6",         {&PASSED_PAWN_BONUS_EG[6], TuningOption::INT,  0,   220, "120"}},
    {"Candidate_Pawn_MG",   {&CANDIDATE_PAWN_BONUS_MG,   TuningOption::INT, 0,  30,   "8"}},
    {"Candidate_Pawn_EG",   {&CANDIDATE_PAWN_BONUS_EG,   TuningOption::INT, 0,  40,   "12"}},
    {"Connected_Passed_MG", {&CONNECTED_PASSED_BONUS_MG, TuningOption::INT, 0,  30,   "10"}},
    {"Connected_Passed_EG", {&CONNECTED_PASSED_BONUS_EG, TuningOption::INT, 0,  50,   "18"}},
    {"Outside_Passed_MG",   {&OUTSIDE_PASSED_BONUS_MG,   TuningOption::INT, 0,  30,   "8"}},
    {"Outside_Passed_EG",   {&OUTSIDE_PASSED_BONUS_EG,   TuningOption::INT, 0,  60,   "20"}},

    // King safety
    {"King_Shield_Penalty",    {&KING_SHIELD_MISSING_PENALTY, TuningOption::INT, -60,    -5,  "-27"}},
    {"King_Open_File_Penalty", {&KING_OPEN_FILE_PENALTY,      TuningOption::INT, -55,    -5,  "-29"}},
    {"King_SemiOpen_Penalty",  {&KING_SEMI_OPEN_FILE_PENALTY, TuningOption::INT, -25,     0,   "-6"}},
    {"King_Escape_Bonus",      {&KING_ESCAPE_BONUS,           TuningOption::INT,   0,    20,    "4"}},
    {"King_Danger_Divisor",    {&KING_DANGER_DIVISOR,         TuningOption::INT,   2,    22,    "10"}},
    {"King_Danger_Max",        {&KING_DANGER_MAX,             TuningOption::INT, 250,  1100,  "641"}},
    // Attacker weights – indices 2-5 (0,1,6 are always 0)
    {"King_Attacker_Knight",   {&KING_ATTACKER_WEIGHT[2],     TuningOption::INT,   0,    12,    "2"}},
    {"King_Attacker_Bishop",   {&KING_ATTACKER_WEIGHT[3],     TuningOption::INT,   0,    14,    "3"}},
    {"King_Attacker_Rook",     {&KING_ATTACKER_WEIGHT[4],     TuningOption::INT,   0,    14,    "4"}},
    {"King_Attacker_Queen",    {&KING_ATTACKER_WEIGHT[5],     TuningOption::INT,   4,    28,   "11"}},
    // Attack count bonus – indices 2-7 (0,1 are always 0)
    {"KingAtk_2",              {&KING_ATTACK_COUNT_BONUS[2],  TuningOption::INT,   0,    22,    "7"}},
    {"KingAtk_3",              {&KING_ATTACK_COUNT_BONUS[3],  TuningOption::INT,   4,    55,   "25"}},
    {"KingAtk_4",              {&KING_ATTACK_COUNT_BONUS[4],  TuningOption::INT,   8,    80,   "33"}},
    {"KingAtk_5",              {&KING_ATTACK_COUNT_BONUS[5],  TuningOption::INT,  12,    95,   "44"}},
    {"KingAtk_6",              {&KING_ATTACK_COUNT_BONUS[6],  TuningOption::INT,   0,    90,   "27"}},
    {"KingAtk_7",              {&KING_ATTACK_COUNT_BONUS[7],  TuningOption::INT,   0,    90,   "16"}},

    // Territory
    {"Seventh_Rank_MG",       {&SEVENTH_RANK_BONUS_MG, TuningOption::INT,   3,    38,   "16"}},
    {"Seventh_Rank_EG",       {&SEVENTH_RANK_BONUS_EG, TuningOption::INT,   6,    65,   "22"}},
    {"Queen_Seventh_MG",      {&QUEEN_SEVENTH_RANK_BONUS_MG, TuningOption::INT,  0, 30,  "8"}},
    {"Queen_Seventh_EG",      {&QUEEN_SEVENTH_RANK_BONUS_EG, TuningOption::INT,  0, 40,  "11"}},

    // Coordination
    {"Defended_Piece_Bonus_MG",       {&DEFENDED_PIECE_BONUS_MG,       TuningOption::INT,   0,   18,   "3"}},
    {"Defended_Piece_Bonus_EG",       {&DEFENDED_PIECE_BONUS_EG,       TuningOption::INT,   0,   18,   "3"}},
    {"Defended_Piece_Bonus",          {&DEFENDED_PIECE_BONUS_MG,       TuningOption::INT,   0,   18,   "3"}},
    {"Shared_Target_Bonus_MG",        {&SHARED_TARGET_BONUS_MG,        TuningOption::INT,   0,   32,   "8"}},
    {"Shared_Target_Bonus_EG",        {&SHARED_TARGET_BONUS_EG,        TuningOption::INT,   0,   32,   "8"}},
    {"Shared_Target_Bonus",           {&SHARED_TARGET_BONUS_MG,        TuningOption::INT,   0,   32,   "8"}},
    {"Battery_Rook_Queen_MG",         {&BATTERY_ROOK_QUEEN_BONUS_MG,   TuningOption::INT,   0,   40,  "10"}},
    {"Battery_Rook_Queen_EG",         {&BATTERY_ROOK_QUEEN_BONUS_EG,   TuningOption::INT,   0,   40,  "10"}},
    {"Battery_Rook_Queen",            {&BATTERY_ROOK_QUEEN_BONUS_MG,   TuningOption::INT,   0,   40,  "10"}},
    {"Battery_Bishop_Queen_MG",       {&BATTERY_BISHOP_QUEEN_BONUS_MG, TuningOption::INT,   0,   30,  "11"}},
    {"Battery_Bishop_Queen_EG",       {&BATTERY_BISHOP_QUEEN_BONUS_EG, TuningOption::INT,   0,   30,  "11"}},
    {"Battery_Bishop_Queen",          {&BATTERY_BISHOP_QUEEN_BONUS_MG, TuningOption::INT,   0,   30,  "11"}},
    {"Support_Chain_Bonus_MG",        {&SUPPORT_CHAIN_BONUS_MG,        TuningOption::INT,   0,   24,   "5"}},
    {"Support_Chain_Bonus_EG",        {&SUPPORT_CHAIN_BONUS_EG,        TuningOption::INT,   0,   24,   "5"}},
    {"Support_Chain_Bonus",           {&SUPPORT_CHAIN_BONUS_MG,        TuningOption::INT,   0,   24,   "5"}},

    // Tactical pressure
    {"Undefended_Attack_Bonus",   {&UNDEFENDED_ATTACK_BONUS,      TuningOption::INT,   4,   42,  "18"}},
    {"Pin_Bonus_MG",              {&PIN_BONUS_MG,                 TuningOption::INT,  10,   65,  "36"}},
    {"Pin_Bonus_EG",              {&PIN_BONUS_EG,                 TuningOption::INT,   5,   45,  "18"}},
    {"Pin_Bonus",                 {&PIN_BONUS_MG,                 TuningOption::INT,  10,   65,  "36"}},
    {"Overloaded_Defender_Bonus_MG", {&OVERLOADED_DEFENDER_BONUS_MG, TuningOption::INT, 0, 30, "4"}}, // Skipped pass 2
    {"Overloaded_Defender_Bonus_EG", {&OVERLOADED_DEFENDER_BONUS_EG, TuningOption::INT, 0, 25, "2"}}, // Skipped pass 2
    {"Overloaded_Defender_Bonus", {&OVERLOADED_DEFENDER_BONUS_MG, TuningOption::INT, 0, 30, "4"}}, // Legacy MG alias
    {"Unrec_Pressure_Bonus_MG",   {&UNRECIPROCATED_PRESSURE_BONUS_MG,TuningOption::INT,0,   18,   "5"}},
    {"Unrec_Pressure_Bonus_EG",   {&UNRECIPROCATED_PRESSURE_BONUS_EG,TuningOption::INT,0,   18,   "2"}},
    {"Unrec_Pressure_Bonus",      {&UNRECIPROCATED_PRESSURE_BONUS_MG,TuningOption::INT,0,   18,   "5"}},
    {"Undefended_Value_Div",      {&UNDEFENDED_VALUE_DIVISOR,     TuningOption::INT,  15,  130,  "66"}},

    // Threats
    // Pawn threats vs piece types 2-5 (knight/bishop/rook/queen)
    {"Threat_Pawn_Knight_MG", {&THREAT_BY_PAWN_MG[2], TuningOption::INT,  12,  100,  "42"}},
    {"Threat_Pawn_Bishop_MG", {&THREAT_BY_PAWN_MG[3], TuningOption::INT,  35,  145,  "89"}},
    {"Threat_Pawn_Rook_MG",   {&THREAT_BY_PAWN_MG[4], TuningOption::INT,  55,  160, "113"}},
    {"Threat_Pawn_Queen_MG",  {&THREAT_BY_PAWN_MG[5], TuningOption::INT,  40,  160,  "89"}},
    {"Threat_Pawn_Knight_EG", {&THREAT_BY_PAWN_EG[2], TuningOption::INT,  30,  140,  "77"}},
    {"Threat_Pawn_Bishop_EG", {&THREAT_BY_PAWN_EG[3], TuningOption::INT,  40,  155,  "94"}},
    {"Threat_Pawn_Rook_EG",   {&THREAT_BY_PAWN_EG[4], TuningOption::INT,   8,  100,  "39"}},
    {"Threat_Pawn_Queen_EG",  {&THREAT_BY_PAWN_EG[5], TuningOption::INT,  25,  140,  "71"}},
    // Minor threats vs rook/queen
    {"Threat_Minor_Rook_MG",  {&THREAT_BY_MINOR_MG[4], TuningOption::INT,   0,   70,  "26"}},
    {"Threat_Minor_Queen_MG", {&THREAT_BY_MINOR_MG[5], TuningOption::INT,   0,   65,  "11"}},
    {"Threat_Minor_Rook_EG",  {&THREAT_BY_MINOR_EG[4], TuningOption::INT,   0,   60,  "10"}},
    {"Threat_Minor_Queen_EG", {&THREAT_BY_MINOR_EG[5], TuningOption::INT,  12,  110,  "54"}},
    // Rook threatens queen
    {"Threat_Rook_MG",        {&THREAT_BY_ROOK_MG,    TuningOption::INT,  15,  120,  "58"}},
    {"Threat_Rook_EG",        {&THREAT_BY_ROOK_EG,    TuningOption::INT,   8,  100,  "48"}},
    // Hanging
    {"Hanging_Penalty_MG",    {&HANGING_BASE_PENALTY_MG, TuningOption::INT,  15,  110,  "54"}},
    {"Hanging_Penalty_EG",    {&HANGING_BASE_PENALTY_EG, TuningOption::INT,   0,   40,   "6"}}, // Skipped pass 2
    {"Hanging_Value_Div",     {&HANGING_VALUE_DIVISOR,   TuningOption::INT,   4,   90,  "21"}},

    // Outposts
    {"Knight_Outpost_MG",  {&KNIGHT_OUTPOST_MG, TuningOption::INT,   0,   45,  "18"}},
    {"Knight_Outpost_EG",  {&KNIGHT_OUTPOST_EG, TuningOption::INT,   0,   35,   "10"}},
    {"Bishop_Outpost_MG",  {&BISHOP_OUTPOST_MG, TuningOption::INT,   0,   38,  "10"}},
    {"Bishop_Outpost_EG",  {&BISHOP_OUTPOST_EG, TuningOption::INT,   0,   32,  "20"}},
    {"Rook_Outpost_MG",    {&ROOK_OUTPOST_MG,   TuningOption::INT,   0,   55,  "28"}},
    {"Rook_Outpost_EG",    {&ROOK_OUTPOST_EG,   TuningOption::INT,   0,   38,  "12"}},
    {"Queen_Outpost_MG",   {&QUEEN_OUTPOST_MG,  TuningOption::INT,   0,   30,   "4"}},
    {"Queen_Outpost_EG",   {&QUEEN_OUTPOST_EG,  TuningOption::INT,   0,   25,   "8"}},

    // File / diagonal openness
    {"Open_File_Mult",         {&OPEN_FILE_MULTIPLIER,          TuningOption::INT, 100,  165, "121"}},
    {"Semi_Open_File_Mult",    {&SEMI_OPEN_FILE_MULTIPLIER,     TuningOption::INT, 100,  140, "117"}},
    {"Bishop_Openness_Max",    {&BISHOP_OPENNESS_MAX_BONUS,     TuningOption::INT,   5,   80,  "34"}},
    {"Bishop_Openness_SqWt",   {&BISHOP_OPENNESS_SQUARE_WEIGHT, TuningOption::INT,   0,    8,   "2"}},
};

// Applies a value string received from "setoption name X value Y".
// Marked inline so the definition is ODR-safe across translation units.
inline void handle_setoption(const std::string& name, const std::string& value) {
    auto it = tuning_registry.find(name);
    if (it == tuning_registry.end()) return;
    auto& opt = it->second;
    if (opt.type == TuningOption::INT)
        *static_cast<int*>(opt.ptr) = std::stoi(value);
    else
        *static_cast<double*>(opt.ptr) = std::stod(value);
}

#ifndef coefficients_t
    #define coefficients_t std::vector<int16_t>
#endif

#ifndef tune_t
    #define tune_t double
#endif

#ifndef pair_t
    #define pair_t std::array<tune_t, 2>
#endif

#ifndef parameters_t
    #define parameters_t std::vector<pair_t>
#endif

struct EvalResult {
    coefficients_t coefficients;
    tune_t         score         = 0;
    tune_t         endgame_scale = 1;
};

enum class PhaseStages { Midgame = 0, Endgame = 1 };

// Push an {mg, eg} pair from two separate ints.
inline void push_pair(parameters_t& p, int mg, int eg) {
    p.push_back({static_cast<tune_t>(mg), static_cast<tune_t>(eg)});
}

// Each trace field is I32[2] where [0] = WHITE count, [1] = BLACK count.
// The coefficient is white_count - black_count.
inline void push_coeff(coefficients_t& c, const I32 f[2]) {
    c.push_back(static_cast<I16>(f[0] - f[1]));
}
inline void push_coeff_arr(coefficients_t& c, const I32 (*arr)[2], int n) {
    for (int i = 0; i < n; ++i)
        c.push_back(static_cast<I16>(arr[i][0] - arr[i][1]));
}

class TexelTuner {
public:
    // src/texel.cpp can be built with -DSHAYBOT_TEXEL_PHASE=N:
    // 0 all, 1 core eval, 2 pawn extras, 3 activity, 4 king/tactics, 5 all but pst/material
    // Tuner configuration constants
    constexpr static bool    includes_additional_score      = true;
    constexpr static bool    supports_external_chess_eval   = false;
    constexpr static bool    retune_from_zero               = false;
    constexpr static tune_t  preferred_k                    = 0;
    constexpr static I32     max_epoch                      = 12001;
    constexpr static bool    enable_qsearch                 = false;
    constexpr static bool    filter_in_check                = false;
    constexpr static tune_t  initial_learning_rate          = 0.01;
    constexpr static I32     learning_rate_drop_interval    = 3000;
    constexpr static tune_t  learning_rate_drop_ratio       = 0.6;
    constexpr static I32     data_load_print_interval       = 100000;

    // Returns the vector of initial {mg, eg} parameter pairs drawn from tune.h.
    // Parameter ordering is documented inside the implementation file.
    static parameters_t get_initial_parameters();

    // Parses a FEN string, runs the trace evaluation and returns an EvalResult
    // whose coefficients represent linear feature counts and whose score field
    // holds the full static evaluation expressed from White's perspective.
    // The upstream tuner derives the residual itself when
    // includes_additional_score is true.
    static EvalResult get_fen_eval_result(const std::string& fen);

    // Converts a chess::Board (texel-tuner's chess.hpp Board) to a FEN string
    // and delegates to get_fen_eval_result.
    static EvalResult get_external_eval_result(const chess::Board& board);

    // Prints the tuned parameters in tune.h initialiser format to stdout.
    static void print_parameters(const parameters_t& parameters);
};

} // namespace Tune
} // namespace SHAYVERI

#endif // TUNE_H

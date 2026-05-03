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

#include <string>
#include <unordered_map>

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
    0, 100, 320, 330, 500, 900, 0,
       100, 320, 330, 500, 900, 0,
};
inline int PIECE_VALUES_EG[PIECE_COUNT] = {
    0, 100, 310, 330, 510, 900, 0,
       100, 310, 330, 510, 900, 0,
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
      0,   0,   0,   0,   0,   0,  0,   0,
     98, 134,  61,  95,  68, 126, 34, -11,
     -6,   7,  26,  31,  65,  56, 25, -20,
    -14,  13,   6,  21,  23,  12, 17, -23,
    -27,  -2,  -5,  12,  17,   6, 10, -25,
    -26,  -4,  -4, -10,   3,   3, 33, -12,
    -35,  -1, -20, -23, -15,  24, 38, -22,
      0,   0,   0,   0,   0,   0,  0,   0,
};
inline int PST_PAWN_EG[64] = {
      0,   0,   0,   0,   0,   0,   0,   0,
    178, 173, 158, 134, 147, 132, 165, 187,
     94, 100,  85,  67,  56,  53,  82,  84,
     32,  24,  13,   5,  -2,   4,  17,  17,
     13,   9,  -3,  -7,  -7,  -8,   3,  -1,
      4,   7,  -6,   1,   0,  -5,  -1,  -8,
     13,   8,   8,  10,  13,   0,   2,  -7,
      0,   0,   0,   0,   0,   0,   0,   0,
};

inline int PST_KNIGHT_MG[64] = {
    -167, -89, -34, -49,  61, -97, -15, -107,
     -73, -41,  72,  36,  23,  62,   7,  -17,
     -47,  60,  37,  65,  84, 129,  73,   44,
      -9,  17,  19,  53,  37,  69,  18,   22,
     -13,   4,  16,  13,  28,  19,  21,   -8,
     -23,  -9,  12,  10,  19,  17,  25,  -16,
     -29, -53, -12,  -3,  -1,  18, -14,  -19,
    -105, -21, -58, -33, -17, -28, -19,  -23,
};
inline int PST_KNIGHT_EG[64] = {
    -58, -38, -13, -28, -31, -27, -63, -99,
    -25,  -8, -25,  -2,  -9, -25, -24, -52,
    -24, -20,  10,   9,  -1,  -9, -19, -41,
    -17,   3,  22,  22,  22,  11,   8, -18,
    -18,  -6,  16,  25,  16,  17,   4, -18,
    -23,  -3,  -1,  15,  10,  -3, -20, -22,
    -42, -20, -10,  -5,  -2, -20, -23, -44,
    -29, -51, -23, -15, -22, -18, -50, -64,
};

inline int PST_BISHOP_MG[64] = {
    -29,   4, -82, -37, -25, -42,   7,  -8,
    -26,  16, -18, -13,  30,  59,  18, -47,
    -16,  37,  43,  40,  35,  50,  37,  -2,
     -4,   5,  19,  50,  37,  37,   7,  -2,
     -6,  13,  13,  26,  34,  12,  10,   4,
      0,  15,  15,  15,  14,  27,  18,  10,
      4,  15,  16,   0,   7,  21,  33,   1,
    -33,  -3, -14, -21, -13, -12, -39, -21,
};
inline int PST_BISHOP_EG[64] = {
    -14, -21, -11,  -8, -7,  -9, -17, -24,
     -8,  -4,   7, -12, -3, -13,  -4, -14,
      2,  -8,   0,  -1, -2,   6,   0,   4,
     -3,   9,  12,   9, 14,  10,   3,   2,
     -6,   3,  13,  19,  7,  10,  -3,  -9,
    -12,  -3,   8,  10, 13,   3,  -7, -15,
    -14, -18,  -7,  -1,  4,  -9, -15, -27,
    -23,  -9, -23,  -5, -9, -16,  -5, -17,
};

inline int PST_ROOK_MG[64] = {
     32,  42,  32,  51, 63,  9,  31,  43,
     27,  32,  58,  62, 80, 67,  26,  44,
     -5,  19,  26,  36, 17, 45,  61,  16,
    -24, -11,   7,  26, 24, 35,  -8, -20,
    -36, -26, -12,  -1,  9, -7,   6, -23,
    -45, -25, -16, -17,  3,  0,  -5, -33,
    -44, -16, -20,  -9, -1, 11,  -6, -71,
    -19, -13,   1,  17, 16,  7, -37, -26,
};
inline int PST_ROOK_EG[64] = {
    13, 10, 18, 15, 12,  12,   8,   5,
    11, 13, 13, 11, -3,   3,   8,   3,
     7,  7,  7,  5,  4,  -3,  -5,  -3,
     4,  3, 13,  1,  2,   1,  -1,   2,
     3,  5,  8,  4, -5,  -6,  -8, -11,
    -4,  0, -5, -1, -7, -12,  -8, -16,
    -6, -6,  0,  2, -9,  -9, -11,  -3,
    -9,  2,  3, -1, -5, -13,   4, -20,
};

inline int PST_QUEEN_MG[64] = {
    -28,   0,  29,  12,  59,  44,  43,  45,
    -24, -39,  -5,   1, -16,  57,  28,  54,
    -13, -17,   7,   8,  29,  56,  47,  57,
    -27, -27, -16, -16,  -1,  17,  -2,   1,
     -9, -26,  -9, -10,  -2,  -4,   3,  -3,
    -14,   2, -11,  -2,  -5,   2,  14,   5,
    -35,  -8,  11,   2,   8,  15,  -3,   1,
     -1, -18,  -9,  10, -15, -25, -31, -50,
};
inline int PST_QUEEN_EG[64] = {
     -9,  22,  22,  27,  27,  19,  10,  20,
    -17,  20,  32,  41,  58,  25,  30,   0,
    -20,   6,   9,  49,  47,  35,  19,   9,
      3,  22,  24,  45,  57,  40,  57,  36,
    -18,  28,  19,  47,  31,  34,  39,  23,
    -16, -27,  15,   6,   9,  17,  10,   5,
    -22, -23, -30, -16, -16, -23, -36, -32,
    -33, -28, -22, -43,  -5, -32, -20, -41,
};

inline int PST_KING_MG[64] = {
    -65,  23,  16, -15, -56, -34,   2,  13,
     29,  -1, -20,  -7,  -8,  -4, -38, -29,
     -9,  24,   2, -16, -20,   6,  22, -22,
    -17, -20, -12, -27, -30, -25, -14, -36,
    -49,  -1, -27, -39, -46, -44, -33, -51,
    -14, -14, -22, -46, -44, -30, -15, -27,
      1,   7,  -8, -64, -43, -16,   9,   8,
    -15,  36,  12, -54,   8, -28,  24,  14,
};
inline int PST_KING_EG[64] = {
    -74, -35, -18, -18, -11,  15,   4, -17,
    -12,  17,  14,  17,  17,  38,  23,  11,
     10,  17,  23,  15,  20,  45,  44,  13,
     -8,  22,  24,  27,  26,  33,  26,   3,
    -18,  -4,  21,  24,  27,  23,   9, -11,
    -19,  -3,  11,  21,  23,  16,   7,  -9,
    -27, -11,   4,  13,  14,   4,  -5, -17,
    -53, -34, -21, -11, -28, -14, -24, -43
};

// ================================================================
// Evaluation parameters
// ================================================================

// Misc
inline int TEMPO_BONUS       = 22;
inline int BISHOP_PAIR_BONUS = 22;

// Passed pawns (indexed by relative rank 0-7)
inline int PASSED_PAWN_BONUS_MG[8] = { 0,  5, 10, 20,  30,  50,  70, 0 };
inline int PASSED_PAWN_BONUS_EG[8] = { 0, 10, 20, 40,  60,  90, 120, 0 };

// Candidate / connected / outside passed
inline int CANDIDATE_PAWN_BONUS_MG   =  8;
inline int CANDIDATE_PAWN_BONUS_EG   = 12;
inline int CONNECTED_PASSED_BONUS_MG = 10;
inline int CONNECTED_PASSED_BONUS_EG = 18;
inline int OUTSIDE_PASSED_BONUS_MG   =  8;
inline int OUTSIDE_PASSED_BONUS_EG   = 20;

// Pawn structure penalties
inline int ISOLATED_PAWN_PENALTY_MG = -15;
inline int ISOLATED_PAWN_PENALTY_EG = -20;
inline int DOUBLED_PAWN_PENALTY_MG  = -12;
inline int DOUBLED_PAWN_PENALTY_EG  =  -9;
inline int BACKWARD_PAWN_PENALTY_MG = -13;
inline int BACKWARD_PAWN_PENALTY_EG =  -8;
inline int SUPPORTED_PAWN_BONUS_MG  =  14;
inline int WEAK_PAWN_PENALTY_MG     = -18;
inline int PAWN_ISLAND_PENALTY_MG   = -32;
inline int PAWN_ISLAND_PENALTY_EG   =  -5;

// Pawn center control
inline int PAWN_CENTER_BONUS_MG     = 7;
inline int PAWN_CENTER_BONUS_EG     = 2;
inline int PAWN_EXT_CENTER_BONUS_MG = 2;
inline int PAWN_EXT_CENTER_BONUS_EG = 3;

// Pawn storm toward enemy king
inline int PAWN_STORM_BASE      = 2;
inline int PAWN_STORM_RANK_MULT = 1;

// King safety
inline int KING_SHIELD_MISSING_PENALTY = -27;
inline int KING_OPEN_FILE_PENALTY      = -29;
inline int KING_SEMI_OPEN_FILE_PENALTY =  -6;
inline int KING_ESCAPE_BONUS           =   4;
inline int KING_ATTACKER_WEIGHT[7]     = { 0, 0, 2,  3,  4, 11, 0 };
inline int KING_ATTACK_COUNT_BONUS[8]  = { 0, 0, 7, 25, 33, 44, 27, 16 };
inline int KING_DANGER_DIVISOR         =  10;
inline int KING_DANGER_MAX             = 641;

// Mobility
inline int MOBILITY_KNIGHT_MG =  6;
inline int MOBILITY_BISHOP_MG = 10;
inline int MOBILITY_ROOK_MG   =  5;
inline int MOBILITY_QUEEN_MG  =  4;
inline int MOBILITY_KNIGHT_EG =  5;
inline int MOBILITY_BISHOP_EG =  4;
inline int MOBILITY_ROOK_EG   =  2;
inline int MOBILITY_QUEEN_EG  =  1;

// File/diagonal openness multipliers (percent)
inline int OPEN_FILE_MULTIPLIER             = 121;
inline int SEMI_OPEN_FILE_MULTIPLIER        = 117;
static constexpr int CLOSED_FILE_MULTIPLIER = 100; //
static constexpr int BISHOP_OPENNESS_BASE   = 100; //
inline int BISHOP_OPENNESS_MAX_BONUS        =  34;
inline int BISHOP_OPENNESS_SQUARE_WEIGHT    =   2;

// Territory
inline int CENTER_BONUS          =  7;
inline int EXT_CENTER_BONUS      =  5;
inline int ENEMY_HALF_BONUS      =  4;
inline int SEVENTH_RANK_BONUS_MG = 16;
inline int SEVENTH_RANK_BONUS_EG = 22;

// Coordination
inline int DEFENDED_PIECE_BONUS       =  3;
inline int SHARED_TARGET_BONUS        =  8;
inline int BATTERY_ROOK_QUEEN_BONUS   = 10;
inline int BATTERY_BISHOP_QUEEN_BONUS = 11;
inline int SUPPORT_CHAIN_BONUS        =  5;

// Tactical pressure
inline int UNDEFENDED_ATTACK_BONUS        = 18;
inline int PIN_BONUS                      = 36;
inline int OVERLOADED_DEFENDER_BONUS      =  4;
inline int UNRECIPROCATED_PRESSURE_BONUS  =  5;
inline int UNDEFENDED_VALUE_DIVISOR       = 66;

// Threats – bonus when a pawn attacks an enemy piece of the given type
inline int THREAT_BY_PAWN_MG[7]  = { 0,  0, 42, 89, 113, 89,  0 };
inline int THREAT_BY_PAWN_EG[7]  = { 0,  0, 77, 94,  39, 71,  0 };
// Bonus when a minor attacks an enemy piece of higher value
inline int THREAT_BY_MINOR_MG[7] = { 0,  0,  0,  0, 26, 11,  0 };
inline int THREAT_BY_MINOR_EG[7] = { 0,  0,  0,  0, 10, 54,  0 };
// Bonus when a rook attacks an enemy queen
inline int THREAT_BY_ROOK_MG = 58;
inline int THREAT_BY_ROOK_EG = 48;

// Hanging piece penalties
inline int HANGING_BASE_PENALTY_MG = 54;
inline int HANGING_BASE_PENALTY_EG =  6;
inline int HANGING_VALUE_DIVISOR   = 21;

// Outposts
inline int KNIGHT_OUTPOST_MG = 18;
inline int KNIGHT_OUTPOST_EG = 10;
inline int BISHOP_OUTPOST_MG = 10;
inline int BISHOP_OUTPOST_EG = 20;
inline int ROOK_OUTPOST_MG   = 28;
inline int ROOK_OUTPOST_EG   = 12;
inline int QUEEN_OUTPOST_MG  =  4;
inline int QUEEN_OUTPOST_EG  =  8;

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

    // ── Core search ──────────────────────────────────────────────
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

    // ── History ──────────────────────────────────────────────────
    {"History_Bonus_Mult",  {&history_bonus_mult,     TuningOption::INT,   280,   720,  "463"}},
    {"History_Bonus_Sub",   {&history_bonus_sub,      TuningOption::INT,    60,   340,  "164"}},
    {"History_Bonus_Limit", {&history_bonus_limit,    TuningOption::INT,  1500,  4500, "2967"}},
    {"Main_History_Weight", {&main_history_weight,    TuningOption::INT,    25,   160,   "79"}},
    {"CMH_Weight",          {&cmh_weight,             TuningOption::INT,    15,   160,   "83"}},
    {"FMH_Weight",          {&fmh_weight,             TuningOption::INT,     0,   130,   "36"}},

    // ── Eval – misc ───────────────────────────────────────────────
    {"Tempo_Bonus",         {&TEMPO_BONUS,            TuningOption::INT,     4,    48,   "22"}},
    {"Bishop_Pair_Bonus",   {&BISHOP_PAIR_BONUS,      TuningOption::INT,     8,    55,   "22"}},
    {"Development_Bonus",   {&DEVELOPMENT_BONUS,      TuningOption::INT,     0,    28,   "12"}},
    {"Castled_Bonus",       {&CASTLED_BONUS,          TuningOption::INT,     0,    38,    "5"}},

    // ── Mobility ─────────────────────────────────────────────────
    {"Mobility_Knight_MG",  {&MOBILITY_KNIGHT_MG,     TuningOption::INT,     0,    16,    "6"}},
    {"Mobility_Bishop_MG",  {&MOBILITY_BISHOP_MG,     TuningOption::INT,     2,    22,   "10"}},
    {"Mobility_Rook_MG",    {&MOBILITY_ROOK_MG,       TuningOption::INT,     0,    14,    "5"}},
    {"Mobility_Queen_MG",   {&MOBILITY_QUEEN_MG,      TuningOption::INT,     0,    12,    "4"}},
    {"Mobility_Knight_EG",  {&MOBILITY_KNIGHT_EG,     TuningOption::INT,     0,    16,    "5"}},
    {"Mobility_Bishop_EG",  {&MOBILITY_BISHOP_EG,     TuningOption::INT,     0,    16,    "4"}},
    {"Mobility_Rook_EG",    {&MOBILITY_ROOK_EG,       TuningOption::INT,     0,    12,    "2"}},
    {"Mobility_Queen_EG",   {&MOBILITY_QUEEN_EG,      TuningOption::INT,     0,     8,    "1"}},

    // ── Pawn structure ────────────────────────────────────────────
    {"Isolated_MG",         {&ISOLATED_PAWN_PENALTY_MG, TuningOption::INT, -35,     0,   "-15"}},
    {"Isolated_EG",         {&ISOLATED_PAWN_PENALTY_EG, TuningOption::INT, -28,     0,   "-20"}},
    {"Doubled_MG",          {&DOUBLED_PAWN_PENALTY_MG,  TuningOption::INT, -45,     0,  "-12"}},
    {"Doubled_EG",          {&DOUBLED_PAWN_PENALTY_EG,  TuningOption::INT, -30,     0,   "-9"}},
    {"Backward_MG",         {&BACKWARD_PAWN_PENALTY_MG, TuningOption::INT, -40,     0,  "-13"}},
    {"Backward_EG",         {&BACKWARD_PAWN_PENALTY_EG, TuningOption::INT, -25,     0,   "-8"}},
    {"Supported_Pawn_MG",   {&SUPPORTED_PAWN_BONUS_MG,  TuningOption::INT,   3,    38,   "14"}},
    {"Weak_Pawn_MG",        {&WEAK_PAWN_PENALTY_MG,     TuningOption::INT, -50,    -3,  "-18"}},
    {"Pawn_Island_MG",      {&PAWN_ISLAND_PENALTY_MG,   TuningOption::INT, -60,    -5,  "-32"}},
    {"Pawn_Island_EG",      {&PAWN_ISLAND_PENALTY_EG,   TuningOption::INT, -25,     0,   "-5"}},
    {"Pawn_Center_MG",      {&PAWN_CENTER_BONUS_MG,     TuningOption::INT,   0,    24,    "7"}},
    {"Pawn_Center_EG",      {&PAWN_CENTER_BONUS_EG,     TuningOption::INT,   0,    10,    "2"}},  // Skipped pass 2
    {"Pawn_ExtCenter_MG",   {&PAWN_EXT_CENTER_BONUS_MG, TuningOption::INT,   0,    12,    "2"}},  // Skipped pass 2
    {"Pawn_ExtCenter_EG",   {&PAWN_EXT_CENTER_BONUS_EG, TuningOption::INT,   0,    14,    "3"}},
    {"Pawn_Storm_Base",     {&PAWN_STORM_BASE,          TuningOption::INT,   0,    16,    "2"}},
    {"Pawn_Storm_Rank_Mult",{&PAWN_STORM_RANK_MULT,     TuningOption::INT,   0,     9,    "1"}},

    // ── Passed pawns (ranks 1-6; ranks 0 and 7 are always 0) ──────
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

    // ── King safety ───────────────────────────────────────────────
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

    // ── Territory ─────────────────────────────────────────────────
    {"Center_Bonus",          {&CENTER_BONUS,          TuningOption::INT,   0,    24,    "7"}},
    {"Ext_Center_Bonus",      {&EXT_CENTER_BONUS,      TuningOption::INT,   0,    18,    "5"}},
    {"Enemy_Half_Bonus",      {&ENEMY_HALF_BONUS,      TuningOption::INT,   0,    18,    "4"}},
    {"Seventh_Rank_MG",       {&SEVENTH_RANK_BONUS_MG, TuningOption::INT,   3,    38,   "16"}},
    {"Seventh_Rank_EG",       {&SEVENTH_RANK_BONUS_EG, TuningOption::INT,   6,    65,   "22"}},

    // ── Coordination ─────────────────────────────────────────────
    {"Defended_Piece_Bonus",       {&DEFENDED_PIECE_BONUS,       TuningOption::INT,   0,   18,   "3"}},
    {"Shared_Target_Bonus",        {&SHARED_TARGET_BONUS,        TuningOption::INT,   0,   32,  "8"}},
    {"Battery_Rook_Queen",         {&BATTERY_ROOK_QUEEN_BONUS,   TuningOption::INT,   0,   40,  "10"}},
    {"Battery_Bishop_Queen",       {&BATTERY_BISHOP_QUEEN_BONUS, TuningOption::INT,   0,   30,   "11"}},
    {"Support_Chain_Bonus",        {&SUPPORT_CHAIN_BONUS,        TuningOption::INT,   0,   24,   "5"}},

    // ── Tactical pressure ─────────────────────────────────────────
    {"Undefended_Attack_Bonus",   {&UNDEFENDED_ATTACK_BONUS,      TuningOption::INT,   4,   42,  "18"}},
    {"Pin_Bonus",                 {&PIN_BONUS,                    TuningOption::INT,  10,   65,  "36"}},
    {"Overloaded_Defender_Bonus", {&OVERLOADED_DEFENDER_BONUS,    TuningOption::INT,   0,   30,   "4"}}, // Skipped pass 2
    {"Unrec_Pressure_Bonus",      {&UNRECIPROCATED_PRESSURE_BONUS,TuningOption::INT,   0,   18,   "5"}},
    {"Undefended_Value_Div",      {&UNDEFENDED_VALUE_DIVISOR,     TuningOption::INT,  15,  130,  "66"}},

    // ── Threats ───────────────────────────────────────────────────
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

    // ── Outposts ──────────────────────────────────────────────────
    {"Knight_Outpost_MG",  {&KNIGHT_OUTPOST_MG, TuningOption::INT,   0,   45,  "18"}},
    {"Knight_Outpost_EG",  {&KNIGHT_OUTPOST_EG, TuningOption::INT,   0,   35,   "10"}},
    {"Bishop_Outpost_MG",  {&BISHOP_OUTPOST_MG, TuningOption::INT,   0,   38,  "10"}},
    {"Bishop_Outpost_EG",  {&BISHOP_OUTPOST_EG, TuningOption::INT,   0,   32,  "20"}},
    {"Rook_Outpost_MG",    {&ROOK_OUTPOST_MG,   TuningOption::INT,   0,   55,  "28"}},
    {"Rook_Outpost_EG",    {&ROOK_OUTPOST_EG,   TuningOption::INT,   0,   38,  "12"}},
    {"Queen_Outpost_MG",   {&QUEEN_OUTPOST_MG,  TuningOption::INT,   0,   30,   "4"}},
    {"Queen_Outpost_EG",   {&QUEEN_OUTPOST_EG,  TuningOption::INT,   0,   25,   "8"}},

    // ── File / diagonal openness ──────────────────────────────────
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

} // namespace Tune
} // namespace ShayBot

#endif // TUNE_H
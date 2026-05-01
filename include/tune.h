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
inline int se_margin          = 61; //
inline int se_reduction_denom =  3; //

// History gravity / bonus
static constexpr int history_max = 16384; //
inline int history_bonus_mult    =   468; //
inline int history_bonus_sub     =   165; //
inline int history_bonus_limit   =  3006; //

// History blending weights (percent, divided by 100 before accumulation)
inline int main_history_weight = 71; //
inline int cmh_weight          = 51; //
inline int fmh_weight          = 43; //

// Pruning margins
inline int rfp_margin_mult    =   79; //
inline int fp_base            =  192; //
inline int fp_mult            =  577; //
inline int lmp_base           =    3; //
inline int lmp_mult           =    1; //
inline int see_pruning_margin = -273; //

// LMR formula coefficients: reduction = lmr_base + log(d)*log(m)/lmr_scale
inline double lmr_base  = 1.33; //
inline double lmr_scale = 1.77; //

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
     0,  0,  0,  0,  0,  0,  0,  0,
     5, 10, 10,-20,-20, 10, 10,  5,
     5, -5,-10,  0,  0,-10, -5,  5,
     0,  0,  0, 20, 20,  0,  0,  0,
     5,  5, 10, 25, 25, 10,  5,  5,
    10, 10, 20, 30, 30, 20, 10, 10,
    50, 50, 50, 50, 50, 50, 50, 50,
     0,  0,  0,  0,  0,  0,  0,  0,
};
inline int PST_PAWN_EG[64] = {
     0,  0,  0,  0,  0,  0,  0,  0,
    10, 15, 15, -5, -5, 15, 15, 10,
    10,  0,  0, 10, 10,  0,  0, 10,
     5,  5, 10, 20, 20, 10,  5,  5,
    10, 10, 20, 30, 30, 20, 10, 10,
    20, 20, 30, 40, 40, 30, 20, 20,
    60, 60, 60, 60, 60, 60, 60, 60,
     0,  0,  0,  0,  0,  0,  0,  0,
};
inline int PST_KNIGHT_MG[64] = {
    -50,-40,-30,-30,-30,-30,-40,-50,
    -40,-20,  0,  5,  5,  0,-20,-40,
    -30,  5, 10, 15, 15, 10,  5,-30,
    -30,  0, 15, 20, 20, 15,  0,-30,
    -30,  5, 15, 20, 20, 15,  5,-30,
    -30,  0, 10, 15, 15, 10,  0,-30,
    -40,-20,  0,  0,  0,  0,-20,-40,
    -50,-40,-30,-30,-30,-30,-40,-50,
};
inline int PST_KNIGHT_EG[64] = {
    -40,-30,-20,-20,-20,-20,-30,-40,
    -30,-10,  0,  5,  5,  0,-10,-30,
    -20,  5, 10, 15, 15, 10,  5,-20,
    -20,  0, 15, 20, 20, 15,  0,-20,
    -20,  5, 15, 20, 20, 15,  5,-20,
    -20,  0, 10, 15, 15, 10,  0,-20,
    -30,-10,  0,  0,  0,  0,-10,-30,
    -40,-30,-20,-20,-20,-20,-30,-40,
};
inline int PST_BISHOP_MG[64] = {
    -20,-10,-10,-10,-10,-10,-10,-20,
    -10,  5,  0,  0,  0,  0,  5,-10,
    -10, 10, 10, 10, 10, 10, 10,-10,
    -10,  0, 10, 10, 10, 10,  0,-10,
    -10,  5,  5, 10, 10,  5,  5,-10,
    -10,  0,  5, 10, 10,  5,  0,-10,
    -10,  0,  0,  0,  0,  0,  0,-10,
    -20,-10,-10,-10,-10,-10,-10,-20,
};
inline int PST_BISHOP_EG[64] = {
    -15,-10,-10,-10,-10,-10,-10,-15,
    -10,  5,  0,  0,  0,  0,  5,-10,
    -10, 10, 10, 10, 10, 10, 10,-10,
    -10,  0, 10, 10, 10, 10,  0,-10,
    -10,  5,  5, 10, 10,  5,  5,-10,
    -10,  0,  5, 10, 10,  5,  0,-10,
    -10,  0,  0,  0,  0,  0,  0,-10,
    -15,-10,-10,-10,-10,-10,-10,-15,
};
inline int PST_ROOK_MG[64] = {
      0,  0,  0,  5,  5,  0,  0,  0,
     -5,  0,  0,  0,  0,  0,  0, -5,
     -5,  0,  0,  0,  0,  0,  0, -5,
     -5,  0,  0,  0,  0,  0,  0, -5,
     -5,  0,  0,  0,  0,  0,  0, -5,
     -5,  0,  0,  0,  0,  0,  0, -5,
      5, 10, 10, 10, 10, 10, 10,  5,
      0,  0,  0,  0,  0,  0,  0,  0,
};
inline int PST_ROOK_EG[64] = {
    5,  5,  5, 10, 10,  5,  5,  5,
    0,  0,  0,  5,  5,  0,  0,  0,
    0,  0,  0,  5,  5,  0,  0,  0,
    0,  0,  0,  5,  5,  0,  0,  0,
    0,  0,  0,  5,  5,  0,  0,  0,
    0,  0,  0,  5,  5,  0,  0,  0,
   10, 10, 10, 15, 15, 10, 10, 10,
    5,  5,  5, 10, 10,  5,  5,  5,
};
inline int PST_QUEEN_MG[64] = {
    -20,-10,-10, -5, -5,-10,-10,-20,
    -10,  0,  5,  0,  0,  0,  0,-10,
    -10,  5,  5,  5,  5,  5,  0,-10,
      0,  0,  5,  5,  5,  5,  0, -5,
     -5,  0,  5,  5,  5,  5,  0, -5,
    -10,  0,  5,  5,  5,  5,  0,-10,
    -10,  0,  0,  0,  0,  0,  0,-10,
    -20,-10,-10, -5, -5,-10,-10,-20,
};
inline int PST_QUEEN_EG[64] = {
    -10, -5, -5, -5, -5, -5, -5,-10,
     -5,  0,  5,  5,  5,  5,  0, -5,
     -5,  5, 10, 10, 10, 10,  5, -5,
     -5,  0, 10, 15, 15, 10,  0, -5,
     -5,  0, 10, 15, 15, 10,  0, -5,
     -5,  5, 10, 10, 10, 10,  5, -5,
     -5,  0,  5,  5,  5,  5,  0, -5,
    -10, -5, -5, -5, -5, -5, -5,-10,
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
inline int TEMPO_BONUS       = 24; //
inline int BISHOP_PAIR_BONUS = 21; //

// Passed pawns (indexed by relative rank 0-7)
inline int PASSED_PAWN_BONUS_MG[8] = { 0,  5, 10, 20,  30,  50,  70, 0 }; //
inline int PASSED_PAWN_BONUS_EG[8] = { 0, 10, 20, 40,  60,  90, 120, 0 }; //

// Candidate / connected / outside passed
inline int CANDIDATE_PAWN_BONUS_MG   =  8; //
inline int CANDIDATE_PAWN_BONUS_EG   = 12; //
inline int CONNECTED_PASSED_BONUS_MG = 10; //
inline int CONNECTED_PASSED_BONUS_EG = 18; //
inline int OUTSIDE_PASSED_BONUS_MG   =  8; //
inline int OUTSIDE_PASSED_BONUS_EG   = 20; //

// Pawn structure penalties
inline int ISOLATED_PAWN_PENALTY_MG =  -2; //
inline int ISOLATED_PAWN_PENALTY_EG =  -2; //
inline int DOUBLED_PAWN_PENALTY_MG  = -10; //
inline int DOUBLED_PAWN_PENALTY_EG  =  -6; //
inline int BACKWARD_PAWN_PENALTY_MG = -13; //
inline int BACKWARD_PAWN_PENALTY_EG =  -2; //
inline int SUPPORTED_PAWN_BONUS_MG  =  15; //
inline int WEAK_PAWN_PENALTY_MG     = -18; //
inline int PAWN_ISLAND_PENALTY_MG   = -28; //
inline int PAWN_ISLAND_PENALTY_EG   =  -4; //

// Pawn center control
inline int PAWN_CENTER_BONUS_MG     = 8; //
inline int PAWN_CENTER_BONUS_EG     = 0; //
inline int PAWN_EXT_CENTER_BONUS_MG = 0; //
inline int PAWN_EXT_CENTER_BONUS_EG = 3; //

// Pawn storm toward enemy king
inline int PAWN_STORM_BASE      = 2; //
inline int PAWN_STORM_RANK_MULT = 1; //

// King safety
inline int KING_SHIELD_MISSING_PENALTY = -27; //
inline int KING_OPEN_FILE_PENALTY      = -26; //
inline int KING_SEMI_OPEN_FILE_PENALTY =  -4; //
inline int KING_ESCAPE_BONUS           =   4; //
inline int KING_ATTACKER_WEIGHT[7]     = { 0, 0, 2,  4,  3, 12, 0 }; //
inline int KING_ATTACK_COUNT_BONUS[8]  = { 0, 0, 8, 24, 36, 44, 30, 18 }; //
inline int KING_DANGER_DIVISOR         =   8; //
inline int KING_DANGER_MAX             = 634; //

// Mobility
inline int MOBILITY_KNIGHT_MG =  6; //
inline int MOBILITY_BISHOP_MG = 11; //
inline int MOBILITY_ROOK_MG   =  5; //
inline int MOBILITY_QUEEN_MG  =  5; //
inline int MOBILITY_KNIGHT_EG =  5; //
inline int MOBILITY_BISHOP_EG =  5; //
inline int MOBILITY_ROOK_EG   =  2; //
inline int MOBILITY_QUEEN_EG  =  1; //

// File/diagonal openness multipliers (percent)
inline int OPEN_FILE_MULTIPLIER             = 129; //
inline int SEMI_OPEN_FILE_MULTIPLIER        = 113; //
static constexpr int CLOSED_FILE_MULTIPLIER = 100; //
static constexpr int BISHOP_OPENNESS_BASE   = 100; //
inline int BISHOP_OPENNESS_MAX_BONUS        =  42; //
inline int BISHOP_OPENNESS_SQUARE_WEIGHT    =   2; //

// Territory
inline int CENTER_BONUS          =  8; //
inline int EXT_CENTER_BONUS      =  6; //
inline int ENEMY_HALF_BONUS      =  6; //
inline int SEVENTH_RANK_BONUS_MG = 13; //
inline int SEVENTH_RANK_BONUS_EG = 29; //

// Coordination
inline int DEFENDED_PIECE_BONUS       =  3; //
inline int SHARED_TARGET_BONUS        = 11; //
inline int BATTERY_ROOK_QUEEN_BONUS   = 11; //
inline int BATTERY_BISHOP_QUEEN_BONUS =  8; //
inline int SUPPORT_CHAIN_BONUS        =  7; //

// Tactical pressure
inline int UNDEFENDED_ATTACK_BONUS        = 19; //
inline int PIN_BONUS                      = 35; //
inline int OVERLOADED_DEFENDER_BONUS      =  0; //
inline int UNRECIPROCATED_PRESSURE_BONUS  =  5; //
inline int UNDEFENDED_VALUE_DIVISOR       = 65; //

// Threats – bonus when a pawn attacks an enemy piece of the given type
inline int THREAT_BY_PAWN_MG[7]  = { 0,  0, 44, 87, 109, 92,  0 }; //
inline int THREAT_BY_PAWN_EG[7]  = { 0,  0, 78, 90,  41, 70,  0 }; //
// Bonus when a minor attacks an enemy piece of higher value
inline int THREAT_BY_MINOR_MG[7] = { 0,  0,  0,  0, 24, 12,  0 }; //
inline int THREAT_BY_MINOR_EG[7] = { 0,  0,  0,  0, 11, 51,  0 }; //
// Bonus when a rook attacks an enemy queen
inline int THREAT_BY_ROOK_MG = 60; //
inline int THREAT_BY_ROOK_EG = 47; //

// Hanging piece penalties
inline int HANGING_BASE_PENALTY_MG = 58; //
inline int HANGING_BASE_PENALTY_EG =  0; //
inline int HANGING_VALUE_DIVISOR   = 27; //

// Outposts
inline int KNIGHT_OUTPOST_MG = 14; //
inline int KNIGHT_OUTPOST_EG =  5; //
inline int BISHOP_OUTPOST_MG = 11; //
inline int BISHOP_OUTPOST_EG = 12; //
inline int ROOK_OUTPOST_MG   = 24; //
inline int ROOK_OUTPOST_EG   = 12; //
inline int QUEEN_OUTPOST_MG  =  7; //
inline int QUEEN_OUTPOST_EG  =  7; //

// Development / initiative
inline int DEVELOPMENT_BONUS = 12; //
inline int CASTLED_BONUS     =  8; //

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
    {"LMR_Base",            {&lmr_base,               TuningOption::DOUBLE,  0,     0,   "1.33"}}, // DOUBLE ignores min/max
    {"LMR_Scale",           {&lmr_scale,              TuningOption::DOUBLE,  0,     0,   "1.77"}}, // DOUBLE ignores min/max
    {"RFP_Margin",          {&rfp_margin_mult,        TuningOption::INT,    40,   160,  "79"}},
    {"FP_Base",             {&fp_base,                TuningOption::INT,   110,   330,  "192"}},
    {"FP_Mult",             {&fp_mult,                TuningOption::INT,   400,   950,  "577"}},
    {"LMP_Base",            {&lmp_base,               TuningOption::INT,     2,     7,   "3"}},
    {"LMP_Mult",            {&lmp_mult,               TuningOption::INT,     1,     6,   "1"}},   // Skipped pass 2
    {"SEE_Pruning_Margin",  {&see_pruning_margin,     TuningOption::INT,  -420,  -120, "-273"}},
    {"SE_Min_Depth",        {&se_min_depth,           TuningOption::INT,     5,    16,   "9"}},
    {"SE_Depth_Margin",     {&se_depth_margin,        TuningOption::INT,     1,     5,   "2"}},
    {"SE_Margin",           {&se_margin,              TuningOption::INT,    25,    95,  "61"}},
    {"SE_Reduction_Denom",  {&se_reduction_denom,     TuningOption::INT,     1,     6,   "3"}},

    // ── History ──────────────────────────────────────────────────
    {"History_Bonus_Mult",  {&history_bonus_mult,     TuningOption::INT,   280,   720,  "468"}},
    {"History_Bonus_Sub",   {&history_bonus_sub,      TuningOption::INT,    60,   340,  "165"}},
    {"History_Bonus_Limit", {&history_bonus_limit,    TuningOption::INT,  1500,  4500, "3006"}},
    {"Main_History_Weight", {&main_history_weight,    TuningOption::INT,    25,   160,   "71"}},
    {"CMH_Weight",          {&cmh_weight,             TuningOption::INT,    15,   160,   "51"}},
    {"FMH_Weight",          {&fmh_weight,             TuningOption::INT,     0,   130,   "43"}},

    // ── Eval – misc ───────────────────────────────────────────────
    {"Tempo_Bonus",         {&TEMPO_BONUS,            TuningOption::INT,     4,    48,   "24"}},
    {"Bishop_Pair_Bonus",   {&BISHOP_PAIR_BONUS,      TuningOption::INT,     8,    55,   "21"}},
    {"Development_Bonus",   {&DEVELOPMENT_BONUS,      TuningOption::INT,     0,    28,   "12"}},
    {"Castled_Bonus",       {&CASTLED_BONUS,          TuningOption::INT,     0,    38,    "8"}},

    // ── Mobility ─────────────────────────────────────────────────
    {"Mobility_Knight_MG",  {&MOBILITY_KNIGHT_MG,     TuningOption::INT,     0,    16,    "6"}},
    {"Mobility_Bishop_MG",  {&MOBILITY_BISHOP_MG,     TuningOption::INT,     2,    22,   "11"}},
    {"Mobility_Rook_MG",    {&MOBILITY_ROOK_MG,       TuningOption::INT,     0,    14,    "5"}},
    {"Mobility_Queen_MG",   {&MOBILITY_QUEEN_MG,      TuningOption::INT,     0,    12,    "5"}},
    {"Mobility_Knight_EG",  {&MOBILITY_KNIGHT_EG,     TuningOption::INT,     0,    16,    "5"}},
    {"Mobility_Bishop_EG",  {&MOBILITY_BISHOP_EG,     TuningOption::INT,     0,    16,    "5"}},
    {"Mobility_Rook_EG",    {&MOBILITY_ROOK_EG,       TuningOption::INT,     0,    12,    "2"}},
    {"Mobility_Queen_EG",   {&MOBILITY_QUEEN_EG,      TuningOption::INT,     0,     8,    "1"}},

    // ── Pawn structure ────────────────────────────────────────────
    {"Isolated_MG",         {&ISOLATED_PAWN_PENALTY_MG, TuningOption::INT, -35,     0,   "-2"}},
    {"Isolated_EG",         {&ISOLATED_PAWN_PENALTY_EG, TuningOption::INT, -28,     0,   "-2"}},
    {"Doubled_MG",          {&DOUBLED_PAWN_PENALTY_MG,  TuningOption::INT, -45,     0,  "-10"}},
    {"Doubled_EG",          {&DOUBLED_PAWN_PENALTY_EG,  TuningOption::INT, -30,     0,   "-6"}},
    {"Backward_MG",         {&BACKWARD_PAWN_PENALTY_MG, TuningOption::INT, -40,     0,  "-13"}},
    {"Backward_EG",         {&BACKWARD_PAWN_PENALTY_EG, TuningOption::INT, -25,     0,   "-2"}},
    {"Supported_Pawn_MG",   {&SUPPORTED_PAWN_BONUS_MG,  TuningOption::INT,   3,    38,   "15"}},
    {"Weak_Pawn_MG",        {&WEAK_PAWN_PENALTY_MG,     TuningOption::INT, -50,    -3,  "-18"}},
    {"Pawn_Island_MG",      {&PAWN_ISLAND_PENALTY_MG,   TuningOption::INT, -60,    -5,  "-28"}},
    {"Pawn_Island_EG",      {&PAWN_ISLAND_PENALTY_EG,   TuningOption::INT, -25,     0,   "-4"}},
    {"Pawn_Center_MG",      {&PAWN_CENTER_BONUS_MG,     TuningOption::INT,   0,    24,    "8"}},
    {"Pawn_Center_EG",      {&PAWN_CENTER_BONUS_EG,     TuningOption::INT,   0,    10,    "0"}},  // Skipped pass 2
    {"Pawn_ExtCenter_MG",   {&PAWN_EXT_CENTER_BONUS_MG, TuningOption::INT,   0,    12,    "0"}},  // Skipped pass 2
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
    {"King_Open_File_Penalty", {&KING_OPEN_FILE_PENALTY,      TuningOption::INT, -55,    -5,  "-26"}},
    {"King_SemiOpen_Penalty",  {&KING_SEMI_OPEN_FILE_PENALTY, TuningOption::INT, -25,     0,   "-4"}},
    {"King_Escape_Bonus",      {&KING_ESCAPE_BONUS,           TuningOption::INT,   0,    20,    "4"}},
    {"King_Danger_Divisor",    {&KING_DANGER_DIVISOR,         TuningOption::INT,   2,    22,    "8"}},
    {"King_Danger_Max",        {&KING_DANGER_MAX,             TuningOption::INT, 250,  1100,  "634"}},
    // Attacker weights – indices 2-5 (0,1,6 are always 0)
    {"King_Attacker_Knight",   {&KING_ATTACKER_WEIGHT[2],     TuningOption::INT,   0,    12,    "2"}},
    {"King_Attacker_Bishop",   {&KING_ATTACKER_WEIGHT[3],     TuningOption::INT,   0,    14,    "4"}},
    {"King_Attacker_Rook",     {&KING_ATTACKER_WEIGHT[4],     TuningOption::INT,   0,    14,    "3"}},
    {"King_Attacker_Queen",    {&KING_ATTACKER_WEIGHT[5],     TuningOption::INT,   4,    28,   "12"}},
    // Attack count bonus – indices 2-7 (0,1 are always 0)
    {"KingAtk_2",              {&KING_ATTACK_COUNT_BONUS[2],  TuningOption::INT,   0,    22,    "8"}},
    {"KingAtk_3",              {&KING_ATTACK_COUNT_BONUS[3],  TuningOption::INT,   4,    55,   "24"}},
    {"KingAtk_4",              {&KING_ATTACK_COUNT_BONUS[4],  TuningOption::INT,   8,    80,   "36"}},
    {"KingAtk_5",              {&KING_ATTACK_COUNT_BONUS[5],  TuningOption::INT,  12,    95,   "44"}},
    {"KingAtk_6",              {&KING_ATTACK_COUNT_BONUS[6],  TuningOption::INT,   0,    90,   "30"}},
    {"KingAtk_7",              {&KING_ATTACK_COUNT_BONUS[7],  TuningOption::INT,   0,    90,   "18"}},

    // ── Territory ─────────────────────────────────────────────────
    {"Center_Bonus",          {&CENTER_BONUS,          TuningOption::INT,   0,    24,    "8"}},
    {"Ext_Center_Bonus",      {&EXT_CENTER_BONUS,      TuningOption::INT,   0,    18,    "6"}},
    {"Enemy_Half_Bonus",      {&ENEMY_HALF_BONUS,      TuningOption::INT,   0,    18,    "6"}},
    {"Seventh_Rank_MG",       {&SEVENTH_RANK_BONUS_MG, TuningOption::INT,   3,    38,   "13"}},
    {"Seventh_Rank_EG",       {&SEVENTH_RANK_BONUS_EG, TuningOption::INT,   6,    65,   "29"}},

    // ── Coordination ─────────────────────────────────────────────
    {"Defended_Piece_Bonus",       {&DEFENDED_PIECE_BONUS,       TuningOption::INT,   0,   18,   "3"}},
    {"Shared_Target_Bonus",        {&SHARED_TARGET_BONUS,        TuningOption::INT,   0,   32,  "11"}},
    {"Battery_Rook_Queen",         {&BATTERY_ROOK_QUEEN_BONUS,   TuningOption::INT,   0,   40,  "11"}},
    {"Battery_Bishop_Queen",       {&BATTERY_BISHOP_QUEEN_BONUS, TuningOption::INT,   0,   30,   "8"}},
    {"Support_Chain_Bonus",        {&SUPPORT_CHAIN_BONUS,        TuningOption::INT,   0,   24,   "7"}},

    // ── Tactical pressure ─────────────────────────────────────────
    {"Undefended_Attack_Bonus",   {&UNDEFENDED_ATTACK_BONUS,      TuningOption::INT,   4,   42,  "19"}},
    {"Pin_Bonus",                 {&PIN_BONUS,                    TuningOption::INT,  10,   65,  "35"}},
    {"Overloaded_Defender_Bonus", {&OVERLOADED_DEFENDER_BONUS,    TuningOption::INT,   0,   30,   "0"}}, // Skipped pass 2
    {"Unrec_Pressure_Bonus",      {&UNRECIPROCATED_PRESSURE_BONUS,TuningOption::INT,   0,   18,   "5"}},
    {"Undefended_Value_Div",      {&UNDEFENDED_VALUE_DIVISOR,     TuningOption::INT,  15,  130,  "65"}},

    // ── Threats ───────────────────────────────────────────────────
    // Pawn threats vs piece types 2-5 (knight/bishop/rook/queen)
    {"Threat_Pawn_Knight_MG", {&THREAT_BY_PAWN_MG[2], TuningOption::INT,  12,  100,  "44"}},
    {"Threat_Pawn_Bishop_MG", {&THREAT_BY_PAWN_MG[3], TuningOption::INT,  35,  145,  "87"}},
    {"Threat_Pawn_Rook_MG",   {&THREAT_BY_PAWN_MG[4], TuningOption::INT,  55,  160, "109"}},
    {"Threat_Pawn_Queen_MG",  {&THREAT_BY_PAWN_MG[5], TuningOption::INT,  40,  160,  "92"}},
    {"Threat_Pawn_Knight_EG", {&THREAT_BY_PAWN_EG[2], TuningOption::INT,  30,  140,  "78"}},
    {"Threat_Pawn_Bishop_EG", {&THREAT_BY_PAWN_EG[3], TuningOption::INT,  40,  155,  "90"}},
    {"Threat_Pawn_Rook_EG",   {&THREAT_BY_PAWN_EG[4], TuningOption::INT,   8,  100,  "41"}},
    {"Threat_Pawn_Queen_EG",  {&THREAT_BY_PAWN_EG[5], TuningOption::INT,  25,  140,  "70"}},
    // Minor threats vs rook/queen
    {"Threat_Minor_Rook_MG",  {&THREAT_BY_MINOR_MG[4], TuningOption::INT,   0,   70,  "24"}},
    {"Threat_Minor_Queen_MG", {&THREAT_BY_MINOR_MG[5], TuningOption::INT,   0,   65,  "12"}},
    {"Threat_Minor_Rook_EG",  {&THREAT_BY_MINOR_EG[4], TuningOption::INT,   0,   60,  "11"}},
    {"Threat_Minor_Queen_EG", {&THREAT_BY_MINOR_EG[5], TuningOption::INT,  12,  110,  "51"}},
    // Rook threatens queen
    {"Threat_Rook_MG",        {&THREAT_BY_ROOK_MG,    TuningOption::INT,  15,  120,  "60"}},
    {"Threat_Rook_EG",        {&THREAT_BY_ROOK_EG,    TuningOption::INT,   8,  100,  "47"}},
    // Hanging
    {"Hanging_Penalty_MG",    {&HANGING_BASE_PENALTY_MG, TuningOption::INT,  15,  110,  "58"}},
    {"Hanging_Penalty_EG",    {&HANGING_BASE_PENALTY_EG, TuningOption::INT,   0,   40,   "0"}}, // Skipped pass 2
    {"Hanging_Value_Div",     {&HANGING_VALUE_DIVISOR,   TuningOption::INT,   4,   90,  "27"}},

    // ── Outposts ──────────────────────────────────────────────────
    {"Knight_Outpost_MG",  {&KNIGHT_OUTPOST_MG, TuningOption::INT,   0,   45,  "14"}},
    {"Knight_Outpost_EG",  {&KNIGHT_OUTPOST_EG, TuningOption::INT,   0,   35,   "5"}},
    {"Bishop_Outpost_MG",  {&BISHOP_OUTPOST_MG, TuningOption::INT,   0,   38,  "11"}},
    {"Bishop_Outpost_EG",  {&BISHOP_OUTPOST_EG, TuningOption::INT,   0,   32,  "12"}},
    {"Rook_Outpost_MG",    {&ROOK_OUTPOST_MG,   TuningOption::INT,   0,   55,  "24"}},
    {"Rook_Outpost_EG",    {&ROOK_OUTPOST_EG,   TuningOption::INT,   0,   38,  "12"}},
    {"Queen_Outpost_MG",   {&QUEEN_OUTPOST_MG,  TuningOption::INT,   0,   30,   "7"}},
    {"Queen_Outpost_EG",   {&QUEEN_OUTPOST_EG,  TuningOption::INT,   0,   25,   "7"}},

    // ── File / diagonal openness ──────────────────────────────────
    {"Open_File_Mult",         {&OPEN_FILE_MULTIPLIER,          TuningOption::INT, 100,  165, "129"}},
    {"Semi_Open_File_Mult",    {&SEMI_OPEN_FILE_MULTIPLIER,     TuningOption::INT, 100,  140, "113"}},
    {"Bishop_Openness_Max",    {&BISHOP_OPENNESS_MAX_BONUS,     TuningOption::INT,   5,   80,  "42"}},
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
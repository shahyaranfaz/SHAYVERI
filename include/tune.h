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

namespace ShayBot {

namespace Tune {

// ================================================================
// Search constants
// ================================================================

// Compile-time limits (used in array sizes / constant expressions)
static constexpr int MAX_PLY    = 128;
static constexpr int INF        = 1000000;
static constexpr int MATE_SCORE = 900000;

// Aspiration window initial delta
inline int ASP_DELTA = 32;

// Singular extensions
inline int se_min_depth       = 14;
inline int se_depth_margin    = 2;
inline int se_margin          = 60;
inline int se_reduction_denom = 3;

// History gravity / bonus
inline int history_max         = 16384;
inline int history_bonus_mult  = 300;
inline int history_bonus_sub   = 200;
inline int history_bonus_limit = 1500;

// History blending weights (percent, divided by 100 before accumulation)
inline int main_history_weight = 100;
inline int cmh_weight          = 100;
inline int fmh_weight          = 100;

// Pruning margins
inline int rfp_margin_mult    = 134;
inline int fp_base            = 292;
inline int fp_mult            = 150;
inline int lmp_base           = 8;
inline int lmp_mult           = 1;
inline int see_pruning_margin = -202;

// LMR formula coefficients: reduction = lmr_base + log(d)*log(m)/lmr_scale
inline double lmr_base  = 1.236;
inline double lmr_scale = 1.796;

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
inline int PTYPE_VALUE[7]  = { 0, 100, 320, 330,  500,  900, 20000 };
// Indexed by PieceType (0-6).  King = 0 for MVV-LVA / capture ordering.
inline int PTYPE_VALUES[7] = { 0, 100, 320, 330,  500,  900,     0 };

// ================================================================
// Phase (tapered eval)
// ================================================================

inline int MAX_PHASE = 24;
inline int PHASE_WEIGHTS[PIECE_COUNT] = {
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
inline int TEMPO_BONUS       = 8;
inline int BISHOP_PAIR_BONUS = 30;

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
inline int ISOLATED_PAWN_PENALTY_MG = -12;
inline int ISOLATED_PAWN_PENALTY_EG =  -8;
inline int DOUBLED_PAWN_PENALTY_MG  = -14;
inline int DOUBLED_PAWN_PENALTY_EG  = -10;
inline int BACKWARD_PAWN_PENALTY_MG = -10;
inline int BACKWARD_PAWN_PENALTY_EG =  -6;
inline int SUPPORTED_PAWN_BONUS_MG  =   6;
inline int WEAK_PAWN_PENALTY_MG     =  -8;
inline int PAWN_ISLAND_PENALTY_MG   = -10;
inline int PAWN_ISLAND_PENALTY_EG   =  -6;

// Pawn center control
inline int PAWN_CENTER_BONUS_MG     = 6;
inline int PAWN_CENTER_BONUS_EG     = 2;
inline int PAWN_EXT_CENTER_BONUS_MG = 3;
inline int PAWN_EXT_CENTER_BONUS_EG = 1;

// Pawn storm toward enemy king
inline int PAWN_STORM_BASE      = 4;
inline int PAWN_STORM_RANK_MULT = 2;

// King safety
inline int KING_SHIELD_MISSING_PENALTY = -14;
inline int KING_OPEN_FILE_PENALTY      = -10;
inline int KING_SEMI_OPEN_FILE_PENALTY =  -6;
inline int KING_ESCAPE_BONUS           =   4;
inline int KING_ATTACKER_WEIGHT[7]     = { 0, 0, 2, 2, 3, 5, 0 };
inline int KING_ATTACK_COUNT_BONUS[8]  = { 0, 0, 3, 8, 16, 26, 38, 52 };
inline int KING_DANGER_DIVISOR         =   8;
inline int KING_DANGER_MAX             = 500;

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
inline int BISHOP_OPENNESS_MAX_BONUS     =  30;
inline int BISHOP_OPENNESS_SQUARE_WEIGHT =   2;

// Territory
inline int CENTER_BONUS          =  8;
inline int EXT_CENTER_BONUS      =  4;
inline int ENEMY_HALF_BONUS      =  3;
inline int SEVENTH_RANK_BONUS_MG = 20;
inline int SEVENTH_RANK_BONUS_EG = 30;

// Coordination
inline int DEFENDED_PIECE_BONUS       =  4;
inline int SHARED_TARGET_BONUS        =  8;
inline int BATTERY_ROOK_QUEEN_BONUS   = 12;
inline int BATTERY_BISHOP_QUEEN_BONUS =  8;
inline int SUPPORT_CHAIN_BONUS        =  6;

// Tactical pressure
inline int UNDEFENDED_ATTACK_BONUS        =  6;
inline int PIN_BONUS                      = 15;
inline int OVERLOADED_DEFENDER_BONUS      = 10;
inline int UNRECIPROCATED_PRESSURE_BONUS  =  2;
inline int UNDEFENDED_VALUE_DIVISOR       = 40;

// Threats – bonus when a pawn attacks an enemy piece of the given type
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
inline int BISHOP_OUTPOST_EG =  8;
inline int ROOK_OUTPOST_MG   = 14;
inline int ROOK_OUTPOST_EG   = 12;
inline int QUEEN_OUTPOST_MG  = 10;
inline int QUEEN_OUTPOST_EG  =  8;

// Development / initiative
inline int DEVELOPMENT_BONUS = 5;
inline int CASTLED_BONUS     = 12;

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
    {"ASP_Delta",           {&ASP_DELTA,              TuningOption::INT,     5,    100,  "32"}},
    {"LMR_Base",            {&lmr_base,               TuningOption::DOUBLE,  0,    0,    "1.236"}},
    {"LMR_Scale",           {&lmr_scale,              TuningOption::DOUBLE,  0,    0,    "1.796"}},
    {"RFP_Margin",          {&rfp_margin_mult,        TuningOption::INT,    50,   250,  "134"}},
    {"FP_Base",             {&fp_base,                TuningOption::INT,    100,   500,  "292"}},
    {"FP_Mult",            {&fp_mult,                 TuningOption::INT,   100,   800,  "400"}},
    {"LMP_Base",           {&lmp_base,                TuningOption::INT,     2,    10,   "6"}},
    {"LMP_Mult",           {&lmp_mult,                TuningOption::INT,     1,     6,   "1"}},
    {"SEE_Pruning_Margin", {&see_pruning_margin,      TuningOption::INT,  -300,   -50,  "-202"}},
    {"SE_Min_Depth",       {&se_min_depth,            TuningOption::INT,     6,    18,  "14"}},
    {"SE_Depth_Margin",    {&se_depth_margin,         TuningOption::INT,     1,     6,   "2"}},
    {"SE_Margin",          {&se_margin,               TuningOption::INT,    20,    80,  "60"}},
    {"SE_Reduction_Denom", {&se_reduction_denom,      TuningOption::INT,     1,     4,   "3"}},

    // ── History ──────────────────────────────────────────────────
    {"History_Bonus_Mult",  {&history_bonus_mult,     TuningOption::INT,    100,  600,  "300"}},
    {"History_Bonus_Sub",   {&history_bonus_sub,      TuningOption::INT,    50,   400,  "200"}},
    {"History_Bonus_Limit", {&history_bonus_limit,    TuningOption::INT,    500,  3000, "1500"}},
    {"Main_History_Weight", {&main_history_weight,    TuningOption::INT,    50,   200,  "100"}},
    {"CMH_Weight",          {&cmh_weight,             TuningOption::INT,    0,    200,  "100"}},
    {"FMH_Weight",          {&fmh_weight,             TuningOption::INT,    0,    200,  "100"}},

    // ── Eval – misc ───────────────────────────────────────────────
    {"Tempo_Bonus",         {&TEMPO_BONUS,            TuningOption::INT,    0,    30,   "8"}},
    {"Bishop_Pair_Bonus",   {&BISHOP_PAIR_BONUS,      TuningOption::INT,    10,   60,   "30"}},
    {"Development_Bonus",   {&DEVELOPMENT_BONUS,      TuningOption::INT,    0,    20,   "5"}},
    {"Castled_Bonus",       {&CASTLED_BONUS,          TuningOption::INT,    0,    40,   "12"}},

    // ── Mobility ─────────────────────────────────────────────────
    {"Mobility_Knight_MG",  {&MOBILITY_KNIGHT_MG,    TuningOption::INT,    0,    12,   "4"}},
    {"Mobility_Bishop_MG",  {&MOBILITY_BISHOP_MG,    TuningOption::INT,    0,    12,   "4"}},
    {"Mobility_Rook_MG",    {&MOBILITY_ROOK_MG,      TuningOption::INT,    0,    8,    "2"}},
    {"Mobility_Queen_MG",   {&MOBILITY_QUEEN_MG,     TuningOption::INT,    0,    6,    "1"}},
    {"Mobility_Knight_EG",  {&MOBILITY_KNIGHT_EG,    TuningOption::INT,    0,    12,   "3"}},
    {"Mobility_Bishop_EG",  {&MOBILITY_BISHOP_EG,    TuningOption::INT,    0,    12,   "4"}},
    {"Mobility_Rook_EG",    {&MOBILITY_ROOK_EG,      TuningOption::INT,    0,    10,   "3"}},
    {"Mobility_Queen_EG",   {&MOBILITY_QUEEN_EG,     TuningOption::INT,    0,    6,    "2"}},

    // ── Pawn structure ────────────────────────────────────────────
    {"Isolated_MG",         {&ISOLATED_PAWN_PENALTY_MG, TuningOption::INT, -40,  0,    "-12"}},
    {"Isolated_EG",         {&ISOLATED_PAWN_PENALTY_EG, TuningOption::INT, -30,  0,    "-8"}},
    {"Doubled_MG",          {&DOUBLED_PAWN_PENALTY_MG,  TuningOption::INT, -40,  0,    "-14"}},
    {"Doubled_EG",          {&DOUBLED_PAWN_PENALTY_EG,  TuningOption::INT, -30,  0,    "-10"}},
    {"Backward_MG",         {&BACKWARD_PAWN_PENALTY_MG, TuningOption::INT, -30,  0,    "-10"}},
    {"Backward_EG",         {&BACKWARD_PAWN_PENALTY_EG, TuningOption::INT, -20,  0,    "-6"}},
    {"Supported_Pawn_MG",   {&SUPPORTED_PAWN_BONUS_MG,  TuningOption::INT,  0,   20,   "6"}},
    {"Weak_Pawn_MG",        {&WEAK_PAWN_PENALTY_MG,     TuningOption::INT, -30,  0,    "-8"}},
    {"Pawn_Island_MG",      {&PAWN_ISLAND_PENALTY_MG,   TuningOption::INT, -30,  0,    "-10"}},
    {"Pawn_Island_EG",      {&PAWN_ISLAND_PENALTY_EG,   TuningOption::INT, -20,  0,    "-6"}},
    {"Pawn_Center_MG",      {&PAWN_CENTER_BONUS_MG,     TuningOption::INT,  0,   20,   "6"}},
    {"Pawn_Center_EG",      {&PAWN_CENTER_BONUS_EG,     TuningOption::INT,  0,   10,   "2"}},
    {"Pawn_ExtCenter_MG",   {&PAWN_EXT_CENTER_BONUS_MG, TuningOption::INT,  0,   12,   "3"}},
    {"Pawn_ExtCenter_EG",   {&PAWN_EXT_CENTER_BONUS_EG, TuningOption::INT,  0,   6,    "1"}},
    {"Pawn_Storm_Base",     {&PAWN_STORM_BASE,           TuningOption::INT,  0,   15,   "4"}},
    {"Pawn_Storm_Rank_Mult",{&PAWN_STORM_RANK_MULT,      TuningOption::INT,  0,   8,    "2"}},

    // ── Passed pawns (ranks 1-6; ranks 0 and 7 are always 0) ──────
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
    {"King_Shield_Penalty",    {&KING_SHIELD_MISSING_PENALTY, TuningOption::INT, -40,  0,    "-14"}},
    {"King_Open_File_Penalty", {&KING_OPEN_FILE_PENALTY,      TuningOption::INT, -30,  0,    "-10"}},
    {"King_SemiOpen_Penalty",  {&KING_SEMI_OPEN_FILE_PENALTY, TuningOption::INT, -20,  0,    "-6"}},
    {"King_Escape_Bonus",      {&KING_ESCAPE_BONUS,           TuningOption::INT,  0,   15,   "4"}},
    {"King_Danger_Divisor",    {&KING_DANGER_DIVISOR,         TuningOption::INT,  2,   20,   "8"}},
    {"King_Danger_Max",        {&KING_DANGER_MAX,             TuningOption::INT,  100, 800,  "500"}},
    // Attacker weights – indices 2-5 (0,1,6 are always 0)
    {"King_Attacker_Knight",   {&KING_ATTACKER_WEIGHT[2],     TuningOption::INT,  0,   8,    "2"}},
    {"King_Attacker_Bishop",   {&KING_ATTACKER_WEIGHT[3],     TuningOption::INT,  0,   8,    "2"}},
    {"King_Attacker_Rook",     {&KING_ATTACKER_WEIGHT[4],     TuningOption::INT,  0,   10,   "3"}},
    {"King_Attacker_Queen",    {&KING_ATTACKER_WEIGHT[5],     TuningOption::INT,  0,   14,   "5"}},
    // Attack count bonus – indices 2-7 (0,1 are always 0)
    {"KingAtk_2",              {&KING_ATTACK_COUNT_BONUS[2],  TuningOption::INT,  0,   15,   "3"}},
    {"KingAtk_3",              {&KING_ATTACK_COUNT_BONUS[3],  TuningOption::INT,  0,   25,   "8"}},
    {"KingAtk_4",              {&KING_ATTACK_COUNT_BONUS[4],  TuningOption::INT,  0,   50,   "16"}},
    {"KingAtk_5",              {&KING_ATTACK_COUNT_BONUS[5],  TuningOption::INT,  0,   80,   "26"}},
    {"KingAtk_6",              {&KING_ATTACK_COUNT_BONUS[6],  TuningOption::INT,  0,   100,  "38"}},
    {"KingAtk_7",              {&KING_ATTACK_COUNT_BONUS[7],  TuningOption::INT,  0,   130,  "52"}},

    // ── Territory ─────────────────────────────────────────────────
    {"Center_Bonus",          {&CENTER_BONUS,          TuningOption::INT,  0,   20,   "8"}},
    {"Ext_Center_Bonus",      {&EXT_CENTER_BONUS,      TuningOption::INT,  0,   12,   "4"}},
    {"Enemy_Half_Bonus",      {&ENEMY_HALF_BONUS,      TuningOption::INT,  0,   10,   "3"}},
    {"Seventh_Rank_MG",       {&SEVENTH_RANK_BONUS_MG, TuningOption::INT,  5,   50,   "20"}},
    {"Seventh_Rank_EG",       {&SEVENTH_RANK_BONUS_EG, TuningOption::INT,  5,   70,   "30"}},

    // ── Coordination ─────────────────────────────────────────────
    {"Defended_Piece_Bonus",       {&DEFENDED_PIECE_BONUS,       TuningOption::INT, 0,  15,  "4"}},
    {"Shared_Target_Bonus",        {&SHARED_TARGET_BONUS,        TuningOption::INT, 0,  25,  "8"}},
    {"Battery_Rook_Queen",         {&BATTERY_ROOK_QUEEN_BONUS,   TuningOption::INT, 0,  35,  "12"}},
    {"Battery_Bishop_Queen",       {&BATTERY_BISHOP_QUEEN_BONUS, TuningOption::INT, 0,  25,  "8"}},
    {"Support_Chain_Bonus",        {&SUPPORT_CHAIN_BONUS,        TuningOption::INT, 0,  20,  "6"}},

    // ── Tactical pressure ─────────────────────────────────────────
    {"Undefended_Attack_Bonus",   {&UNDEFENDED_ATTACK_BONUS,      TuningOption::INT, 0,   20,  "6"}},
    {"Pin_Bonus",                 {&PIN_BONUS,                     TuningOption::INT, 0,   40,  "15"}},
    {"Overloaded_Defender_Bonus", {&OVERLOADED_DEFENDER_BONUS,     TuningOption::INT, 0,   30,  "10"}},
    {"Unrec_Pressure_Bonus",      {&UNRECIPROCATED_PRESSURE_BONUS, TuningOption::INT, 0,   10,  "2"}},
    {"Undefended_Value_Div",      {&UNDEFENDED_VALUE_DIVISOR,      TuningOption::INT, 10,  100, "40"}},

    // ── Threats ───────────────────────────────────────────────────
    // Pawn threats vs piece types 2-5 (knight/bishop/rook/queen)
    {"Threat_Pawn_Knight_MG", {&THREAT_BY_PAWN_MG[2], TuningOption::INT, 20, 120, "60"}},
    {"Threat_Pawn_Bishop_MG", {&THREAT_BY_PAWN_MG[3], TuningOption::INT, 20, 120, "65"}},
    {"Threat_Pawn_Rook_MG",   {&THREAT_BY_PAWN_MG[4], TuningOption::INT, 25, 130, "75"}},
    {"Threat_Pawn_Queen_MG",  {&THREAT_BY_PAWN_MG[5], TuningOption::INT, 25, 150, "80"}},
    {"Threat_Pawn_Knight_EG", {&THREAT_BY_PAWN_EG[2], TuningOption::INT, 10,  90, "40"}},
    {"Threat_Pawn_Bishop_EG", {&THREAT_BY_PAWN_EG[3], TuningOption::INT, 10,  90, "45"}},
    {"Threat_Pawn_Rook_EG",   {&THREAT_BY_PAWN_EG[4], TuningOption::INT, 15, 100, "55"}},
    {"Threat_Pawn_Queen_EG",  {&THREAT_BY_PAWN_EG[5], TuningOption::INT, 15, 110, "60"}},
    // Minor threats vs rook/queen
    {"Threat_Minor_Rook_MG",  {&THREAT_BY_MINOR_MG[4], TuningOption::INT, 0,  80, "30"}},
    {"Threat_Minor_Queen_MG", {&THREAT_BY_MINOR_MG[5], TuningOption::INT, 0, 100, "45"}},
    {"Threat_Minor_Rook_EG",  {&THREAT_BY_MINOR_EG[4], TuningOption::INT, 0,  60, "20"}},
    {"Threat_Minor_Queen_EG", {&THREAT_BY_MINOR_EG[5], TuningOption::INT, 0,  80, "30"}},
    // Rook threatens queen
    {"Threat_Rook_MG",        {&THREAT_BY_ROOK_MG, TuningOption::INT,  0,  70, "25"}},
    {"Threat_Rook_EG",        {&THREAT_BY_ROOK_EG, TuningOption::INT,  0,  50, "18"}},
    // Hanging
    {"Hanging_Penalty_MG",    {&HANGING_BASE_PENALTY_MG, TuningOption::INT,  0,  60, "20"}},
    {"Hanging_Penalty_EG",    {&HANGING_BASE_PENALTY_EG, TuningOption::INT,  0,  40, "12"}},
    {"Hanging_Value_Div",     {&HANGING_VALUE_DIVISOR,   TuningOption::INT, 10, 100, "40"}},

    // ── Outposts ──────────────────────────────────────────────────
    {"Knight_Outpost_MG",  {&KNIGHT_OUTPOST_MG, TuningOption::INT, 0, 50, "20"}},
    {"Knight_Outpost_EG",  {&KNIGHT_OUTPOST_EG, TuningOption::INT, 0, 35, "14"}},
    {"Bishop_Outpost_MG",  {&BISHOP_OUTPOST_MG, TuningOption::INT, 0, 35, "12"}},
    {"Bishop_Outpost_EG",  {&BISHOP_OUTPOST_EG, TuningOption::INT, 0, 25,  "8"}},
    {"Rook_Outpost_MG",    {&ROOK_OUTPOST_MG,   TuningOption::INT, 0, 35, "14"}},
    {"Rook_Outpost_EG",    {&ROOK_OUTPOST_EG,   TuningOption::INT, 0, 30, "12"}},
    {"Queen_Outpost_MG",   {&QUEEN_OUTPOST_MG,  TuningOption::INT, 0, 25, "10"}},
    {"Queen_Outpost_EG",   {&QUEEN_OUTPOST_EG,  TuningOption::INT, 0, 20,  "8"}},

    // ── File / diagonal openness ──────────────────────────────────
    {"Open_File_Mult",         {&OPEN_FILE_MULTIPLIER,          TuningOption::INT, 100, 150, "120"}},
    {"Semi_Open_File_Mult",    {&SEMI_OPEN_FILE_MULTIPLIER,     TuningOption::INT, 100, 130, "110"}},
    {"Bishop_Openness_Max",    {&BISHOP_OPENNESS_MAX_BONUS,     TuningOption::INT,   0,  60,  "30"}},
    {"Bishop_Openness_SqWt",   {&BISHOP_OPENNESS_SQUARE_WEIGHT, TuningOption::INT,   0,   6,   "2"}},
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
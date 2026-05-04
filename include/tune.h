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
    0, 100, 809, 845, 1184, 2555, 0,
       100, 809, 845, 1184, 2555, 0,
};
inline int PIECE_VALUES_EG[PIECE_COUNT] = {
    0, 100, 713, 726, 1219, 2271, 0,
       100, 713, 726, 1219, 2271, 0,
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
       0,    0,    0,    0,    0,    0,    0,    0,
     219,  151,  117,  103,  137,   81, -110,  -55,
      40,   77,   39,   20,   51,  189,  187,   67,
      21,   46,   -2,   39,   60,   58,   76,   25,
      21,   41,    4,   23,   47,   37,   79,   17,
      19,   20,   -1,   11,   61,   73,  102,   29,
     130,  154,   71,  109,   85,  196,  124,   21,
       0,    0,    0,    0,    0,    0,    0,    0,
};
inline int PST_PAWN_EG[64] = {
       0,    0,    0,    0,    0,    0,    0,    0,
     153,  201,  207,  191,  175,  229,  257,  166,
     208,  206,   69,   50,   54,   50,  151,  162,
     176,  156,   31,   40,   54,   16,  146,  126,
     162,  146,    4,   37,   31,   -9,  119,  112,
     211,  211,   85,   70,   62,   51,  172,  168,
     300,  291,  269,  247,  279,  241,  255,  262,
       0,    0,    0,    0,    0,    0,    0,    0,
};
inline int PST_KNIGHT_MG[64] = {
    -492, -207, -229,  -85,    9, -199,  -81, -371,
    -124, -141,  -35,   33,   53,   96,  -95,  -41,
    -123,  -51, -138, -102,  -54,    3,   61,   17,
     -58,  -24, -142, -101, -118,  -95,   13,   12,
     147,  204,   61,  112,  109,  123,  265,  218,
      80,  215,   38,  103,  125,  145,  248,  190,
      48,   80,  222,  212,  197,  230,  143,  135,
    -135,   63,   89,   94,  224,   63,  138,  -58,
};
inline int PST_KNIGHT_EG[64] = {
    -141,  -97,  -37,  -52,  -72,  -29,  -99, -192,
    -114,  -63,  -60,  -22,  -38,  -88,  -59, -137,
     -93,  -44, -205, -205, -208, -221,  -65,  -87,
     -81,  -60, -196, -114, -123, -191,  -32,  -76,
     222,  260,  126,  188,  187,  121,  277,  225,
     171,  221,   79,  106,   93,   58,  224,  179,
     163,  239,  199,  235,  231,  190,  210,  156,
     139,  119,  198,  199,  182,  175,  110,   79,
};
inline int PST_BISHOP_MG[64] = {
    -120,  -52, -170, -109, -100, -177,   -1,  -53,
     -84,  -52,    5,  -54,  -33,    0,  -57,  -84,
     -34,    4, -134, -140,  -92,  -55,   91,   20,
     -39,    4, -147,  -79,  -82, -117,   10,  -19,
     190,  219,   74,  167,  158,   83,  215,  222,
     187,  251,  108,  102,   97,  113,  267,  221,
     187,  244,  208,  199,  240,  286,  273,  190,
     204,  225,  127,  135,  149,  164,  214,  205,
};
inline int PST_BISHOP_EG[64] = {
     -30,  -24,  -32,  -18,  -30,  -23,  -51,  -51,
     -20,  -40,  -32,  -15,  -18,  -44,  -38,  -54,
     -27,  -16, -197, -198, -191, -203,  -40,  -27,
     -33,  -16, -200, -153, -159, -201,  -15,  -23,
     259,  270,  107,  135,  133,  102,  276,  261,
     252,  249,   85,   90,   97,   87,  254,  260,
     215,  219,  238,  238,  249,  218,  216,  186,
     201,  205,  213,  242,  252,  231,  218,  220,
};
inline int PST_ROOK_MG[64] = {
     141,  134,   94,  134,  129,  214,  200,  210,
      -5,    4,   54,   96,  108,  173,  121,  100,
     -26,   66,   73,  111,  165,  185,  213,   96,
     -38,    2,   39,   85,   83,   75,   90,   24,
     161,  181,  205,  237,  236,  250,  245,  194,
     179,  216,  220,  234,  230,  245,  295,  210,
     200,  223,  262,  275,  304,  301,  268,  208,
     245,  264,  266,  301,  312,  257,  283,  261,
};
inline int PST_ROOK_EG[64] = {
     156,  188,  213,  201,  209,  177,  185,  147,
     168,  180,  173,  170,  155,  130,  140,  135,
     194,  177,  177,  164,  147,  143,  126,  153,
     200,  193,  192,  183,  176,  174,  161,  172,
     490,  498,  507,  482,  482,  480,  461,  473,
     456,  471,  477,  468,  460,  458,  437,  445,
     465,  465,  474,  468,  440,  433,  429,  444,
     467,  456,  471,  456,  443,  464,  444,  442,
};
inline int PST_QUEEN_MG[64] = {
     213,  225,  234,  252,  255,  269,  354,  343,
     149,  127,  191,  159,  163,  296,  159,  324,
     198,  219,   46,   70,  121,  148,  341,  239,
     208,  200,   61,  110,  139,   92,  273,  248,
     432,  430,  279,  343,  372,  331,  482,  505,
     440,  450,  311,  311,  346,  372,  547,  562,
     440,  433,  482,  484,  469,  571,  543,  534,
     431,  467,  497,  491,  543,  488,  489,  517,
};
inline int PST_QUEEN_EG[64] = {
     373,  395,  422,  402,  442,  427,  391,  346,
     363,  405,  393,  459,  504,  414,  475,  383,
     344,  334,  241,  253,  269,  297,  463,  498,
     335,  409,  219,  243,  252,  328,  538,  505,
     629,  671,  501,  499,  510,  563,  752,  743,
     585,  618,  473,  505,  495,  501,  628,  594,
     558,  603,  589,  624,  639,  503,  486,  546,
     545,  543,  565,  571,  551,  540,  519,  542,
};
inline int PST_KING_MG[64] = {
     148,  275,  225,  107,  224,  291,  270,   36,
     257,  252,  235,  214,   81,  145,  120,  -20,
      28,  150,  139,   13,  -32,  101,  199, -158,
     -15,  189,  -15, -121, -129,   49,   66, -144,
     -83,   79,   29,  -74,  -53,  -35,   30, -221,
    -141,    1,   -3,  -39,  -60,  -57,  -42, -184,
     -51,  -75,  -83, -131, -127, -120, -105, -106,
    -159,    1,  -53, -175, -132, -202,  -48,  -57,
};
inline int PST_KING_EG[64] = {
    -330,  -76,  -42,  -12,  -29,  -12,   35, -266,
     -74,   22,   43,   22,   59,   73,   77,   -1,
      12,   49,   65,   70,   73,   82,   58,   50,
     -13,   12,   76,   81,   82,   65,   41,    7,
     -23,   21,   54,   77,   74,   69,   30,    9,
      11,    4,   38,   48,   56,   73,   48,   16,
     -33,    7,   18,   27,   30,   55,   14,  -27,
    -199, -110,  -39,  -41,  -85,    1,  -52, -148,
};

// ================================================================
// Evaluation parameters
// ================================================================

// Misc
inline int TEMPO_BONUS       = 50;
inline int BISHOP_PAIR_BONUS = 22; // EG=11

// Passed pawns (indexed by relative rank 0-7)
inline int PASSED_PAWN_BONUS_MG[8] = { 0,  6,  6,  23,  85, 169, 289, 0 };
inline int PASSED_PAWN_BONUS_EG[8] = { 0, 21, 34, 102, 156, 256, 372, 0 };

// Candidate / connected / outside passed
inline int CANDIDATE_PAWN_BONUS_MG   = 20;
inline int CANDIDATE_PAWN_BONUS_EG   = 26;
inline int CONNECTED_PASSED_BONUS_MG =  2;
inline int CONNECTED_PASSED_BONUS_EG = 57;
inline int OUTSIDE_PASSED_BONUS_MG   = 37;
inline int OUTSIDE_PASSED_BONUS_EG   = 30;

// Pawn structure penalties
inline int ISOLATED_PAWN_PENALTY_MG = -21;
inline int ISOLATED_PAWN_PENALTY_EG = -44;
inline int DOUBLED_PAWN_PENALTY_MG  = -29;
inline int DOUBLED_PAWN_PENALTY_EG  = -34;
inline int BACKWARD_PAWN_PENALTY_MG = -24;
inline int BACKWARD_PAWN_PENALTY_EG = -15;
inline int SUPPORTED_PAWN_BONUS_MG  =  34;
inline int WEAK_PAWN_PENALTY_MG     =   5;
inline int PAWN_ISLAND_PENALTY_MG   = -62;
inline int PAWN_ISLAND_PENALTY_EG   =  14;

// Pawn center control
inline int PAWN_CENTER_BONUS_MG     =  47;
inline int PAWN_CENTER_BONUS_EG     =  63;
inline int PAWN_EXT_CENTER_BONUS_MG =  45;
inline int PAWN_EXT_CENTER_BONUS_EG = 110;

// Pawn storm toward enemy king
inline int PAWN_STORM_BASE      = -65;
inline int PAWN_STORM_RANK_MULT =  24;

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
inline int MOBILITY_KNIGHT_MG = 14;
inline int MOBILITY_KNIGHT_EG = 15;
inline int MOBILITY_BISHOP_MG = 16;
inline int MOBILITY_BISHOP_EG = 17;
inline int MOBILITY_ROOK_MG   = 11;
inline int MOBILITY_ROOK_EG   =  7;
inline int MOBILITY_QUEEN_MG  =  7;
inline int MOBILITY_QUEEN_EG  =  7;

// File/diagonal openness multipliers (percent)
inline int OPEN_FILE_MULTIPLIER             = 121;
inline int SEMI_OPEN_FILE_MULTIPLIER        = 117;
static constexpr int CLOSED_FILE_MULTIPLIER = 100; //
static constexpr int BISHOP_OPENNESS_BASE   = 100; //
inline int BISHOP_OPENNESS_MAX_BONUS        =  34;
inline int BISHOP_OPENNESS_SQUARE_WEIGHT    =   2;

// Territory
inline int CENTER_BONUS          = 149; // EG=122
inline int EXT_CENTER_BONUS      = 166; // EG=170
inline int ENEMY_HALF_BONUS      = 219; // EG=298
inline int SEVENTH_RANK_BONUS_MG =  16;
inline int SEVENTH_RANK_BONUS_EG =  22;

// Coordination
inline int DEFENDED_PIECE_BONUS       = -1;
inline int SHARED_TARGET_BONUS        =  6;
inline int BATTERY_ROOK_QUEEN_BONUS   = 14;
inline int BATTERY_BISHOP_QUEEN_BONUS = 22;
inline int SUPPORT_CHAIN_BONUS        =  5;

// Tactical pressure
inline int UNDEFENDED_ATTACK_BONUS        = 18;
inline int PIN_BONUS                      = 55;
inline int OVERLOADED_DEFENDER_BONUS      = 12;
inline int UNRECIPROCATED_PRESSURE_BONUS  = -2;
inline int UNDEFENDED_VALUE_DIVISOR       = 66;

// Threats – bonus when a pawn attacks an enemy piece of the given type
inline int THREAT_BY_PAWN_MG[7] = { 0, 0, 123, 145, 185, 130, 0 };
inline int THREAT_BY_PAWN_EG[7] = { 0, 0, 112, 172,  32,  19, 0 };
// Bonus when a minor attacks an enemy piece of higher value
inline int THREAT_BY_MINOR_MG[7] = { 0, 0, 0, 0,  92, 32, 0 };
inline int THREAT_BY_MINOR_EG[7] = { 0, 0, 0, 0, -31, 21, 0 };
// Bonus when a rook attacks an enemy queen
inline int THREAT_BY_ROOK_MG = 99;
inline int THREAT_BY_ROOK_EG = -4;

// Hanging piece penalties
inline int HANGING_BASE_PENALTY_MG = 54;
inline int HANGING_BASE_PENALTY_EG =  6;
inline int HANGING_VALUE_DIVISOR   = 21;

// Outposts
inline int KNIGHT_OUTPOST_MG =  27;
inline int KNIGHT_OUTPOST_EG =  61;
inline int BISHOP_OUTPOST_MG =  56;
inline int BISHOP_OUTPOST_EG =  28;
inline int ROOK_OUTPOST_MG   =  64;
inline int ROOK_OUTPOST_EG   =  19;
inline int QUEEN_OUTPOST_MG  = -43;
inline int QUEEN_OUTPOST_EG  = 110;

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
    {"Tempo_Bonus",         {&TEMPO_BONUS,            TuningOption::INT,     4,    48,   "22"}},
    {"Bishop_Pair_Bonus",   {&BISHOP_PAIR_BONUS,      TuningOption::INT,     8,    55,   "22"}},
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
    {"Weak_Pawn_MG",        {&WEAK_PAWN_PENALTY_MG,     TuningOption::INT, -50,    -3,  "-18"}},
    {"Pawn_Island_MG",      {&PAWN_ISLAND_PENALTY_MG,   TuningOption::INT, -60,    -5,  "-32"}},
    {"Pawn_Island_EG",      {&PAWN_ISLAND_PENALTY_EG,   TuningOption::INT, -25,     0,   "-5"}},
    {"Pawn_Center_MG",      {&PAWN_CENTER_BONUS_MG,     TuningOption::INT,   0,    24,    "7"}},
    {"Pawn_Center_EG",      {&PAWN_CENTER_BONUS_EG,     TuningOption::INT,   0,    10,    "2"}},  // Skipped pass 2
    {"Pawn_ExtCenter_MG",   {&PAWN_EXT_CENTER_BONUS_MG, TuningOption::INT,   0,    12,    "2"}},  // Skipped pass 2
    {"Pawn_ExtCenter_EG",   {&PAWN_EXT_CENTER_BONUS_EG, TuningOption::INT,   0,    14,    "3"}},
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
    {"Center_Bonus",          {&CENTER_BONUS,          TuningOption::INT,   0,    24,    "7"}},
    {"Ext_Center_Bonus",      {&EXT_CENTER_BONUS,      TuningOption::INT,   0,    18,    "5"}},
    {"Enemy_Half_Bonus",      {&ENEMY_HALF_BONUS,      TuningOption::INT,   0,    18,    "4"}},
    {"Seventh_Rank_MG",       {&SEVENTH_RANK_BONUS_MG, TuningOption::INT,   3,    38,   "16"}},
    {"Seventh_Rank_EG",       {&SEVENTH_RANK_BONUS_EG, TuningOption::INT,   6,    65,   "22"}},

    // Coordination
    {"Defended_Piece_Bonus",       {&DEFENDED_PIECE_BONUS,       TuningOption::INT,   0,   18,   "3"}},
    {"Shared_Target_Bonus",        {&SHARED_TARGET_BONUS,        TuningOption::INT,   0,   32,  "8"}},
    {"Battery_Rook_Queen",         {&BATTERY_ROOK_QUEEN_BONUS,   TuningOption::INT,   0,   40,  "10"}},
    {"Battery_Bishop_Queen",       {&BATTERY_BISHOP_QUEEN_BONUS, TuningOption::INT,   0,   30,   "11"}},
    {"Support_Chain_Bonus",        {&SUPPORT_CHAIN_BONUS,        TuningOption::INT,   0,   24,   "5"}},

    // Tactical pressure
    {"Undefended_Attack_Bonus",   {&UNDEFENDED_ATTACK_BONUS,      TuningOption::INT,   4,   42,  "18"}},
    {"Pin_Bonus",                 {&PIN_BONUS,                    TuningOption::INT,  10,   65,  "36"}},
    {"Overloaded_Defender_Bonus", {&OVERLOADED_DEFENDER_BONUS,    TuningOption::INT,   0,   30,   "4"}}, // Skipped pass 2
    {"Unrec_Pressure_Bonus",      {&UNRECIPROCATED_PRESSURE_BONUS,TuningOption::INT,   0,   18,   "5"}},
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
    // Tuner configuration constants
    constexpr static bool    includes_additional_score      = true;
    constexpr static bool    supports_external_chess_eval   = false;
    constexpr static bool    retune_from_zero               = false;
    constexpr static tune_t  preferred_k                    = 0;
    constexpr static I32     max_epoch                      = 5001;
    constexpr static bool    enable_qsearch                 = false;
    constexpr static bool    filter_in_check                = false;
    constexpr static tune_t  initial_learning_rate          = 1;
    constexpr static I32     learning_rate_drop_interval    = 10000;
    constexpr static tune_t  learning_rate_drop_ratio       = 1;
    constexpr static I32     data_load_print_interval       = 10000;

    // Returns the vector of initial {mg, eg} parameter pairs drawn from tune.h.
    // Parameter ordering is documented inside the implementation file.
    static parameters_t get_initial_parameters();

    // Parses a FEN string, runs the trace evaluation and returns an EvalResult
    // whose coefficients represent linear feature counts and whose score field
    // holds the non-linear residual (king safety, development, etc.) already
    // tapered and expressed from White's perspective.
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
#ifndef TUNE_H
#define TUNE_H

// Home for all parameters for the engine

#include "types.h"

#include <algorithm>
#include <cmath>
#include <exception>
#include <string>
#include <unordered_map>

namespace SHAYVERI {

namespace Tune {

// ===== SEARCH CONSTANTS =====

// Compile-time limits.
static constexpr int MAX_PLY             =   128;
static constexpr int INF                 = 32001;
static constexpr int MATE_SCORE          = 32000;
static constexpr int TB_SCORE            = 30000;
static constexpr int TB_THRESHOLD        = TB_SCORE - MAX_PLY;
static constexpr int CORRHIST_TABLE_SIZE = 16384;

// Internal tactical and move-ordering values. These are not tuned material terms.
// Tactical exchange and threat analysis needs a prohibitive king value. Capture
// ordering does not.
static constexpr int TACTICAL_PIECE_VALUES[7]      = { 0, 100, 320, 330, 500, 900, 20000 };
static constexpr int CAPTURE_ORDER_PIECE_VALUES[7] = { 0, 100, 320, 330, 500, 900,     0 };

// Aspiration windows.
inline int asp_delta     = 41;
inline int asp_min_depth =  4;
inline int asp_growth    =  2;

// Reverse futility pruning.
inline int rfp_margin    = 68;
inline int rfp_max_depth =  5;

// Futility pruning.
inline int fp_base      = 217;
inline int fp_mult      = 762;
inline int fp_max_depth =   4;

// Late move pruning.
inline int lmp_base = 2;
inline int lmp_mult = 1;

// Static exchange pruning.
inline int see_margin    = -161;
inline int see_max_depth =    4;

// Quiescence search.
inline int qs_delta_margin    = 204;
inline int qs_see_margin      = 100;
inline int qs_futility_margin =   0;
inline int qs_start_depth     =   8;
inline int qs_min_depth       =  -6;

// Null-move pruning.
inline int nmp_margin_mult      =  15;
inline int nmp_eval_divisor     = 183;
inline int nmp_base_reduction   =   3;
inline int nmp_depth_divisor    =   3;
inline int nmp_min_depth        =   3;
inline int nmp_reduction_min    =   0;
inline int nmp_reduction_max    =   8;
inline int nmp_verify_min_depth =  10;

// Singular extensions.
inline int se_extension                  =   1;
inline int se_min_depth                  =  11;
inline int se_depth_margin               =   2;
inline int se_margin                     =  74;
inline int se_reduction_denom            =   5;
inline int se_multicut                   =   1;
inline int se_negative_extensions        =   0;
inline int se_negative_tt_extension      =   0;
inline int se_negative_cutnode_extension =   0;
inline int se_double_extensions          =   0;
inline int se_double_margin              =   0;
inline int se_double_extension           =   0;
inline int se_triple_extensions          =   0;
inline int se_triple_margin              =   0;
inline int se_triple_extension           =   0;

// Check extensions.
inline int check_extension = 0;

// Internal iterative reduction.
inline int iir_reduction = 1;
inline int iir_min_depth = 4;

// Late move reductions.
inline double lmr_base                   = 1.25954;
inline double lmr_scale                  = 1.80102;
inline int    improving_lmr_reduction    =      -2;
inline int    cutnode_lmr_reduction      =       3;
inline int    lmr_pv_offset              =       0;
inline int    lmr_nonpv_offset           =       0;
inline int    lmr_min_depth              =       3;
inline int    lmr_extra_move_threshold   =       6;
inline int    lmr_extra_min_depth        =       6;
inline int    lmr_good_history           =     800;
inline int    lmr_bad_history            =    -800;
inline int    lmr_extra_reduction        =       1;
inline int    lmr_good_history_reduction =      -1;
inline int    lmr_bad_history_reduction  =       1;

// History gravity and blending weights, divided by 100 before accumulation.
inline int history_max            = 16384;
inline int history_bonus_mult     =   468;
inline int history_bonus_sub      =   165;
inline int history_bonus_limit    =  3090;
inline int main_history_weight    =    85;
inline int cmh_weight             =    75;
inline int fmh_weight             =    30;
inline int capture_history_weight =   100;

// History pruning.
inline int history_pruning_threshold = 0;
inline int history_pruning_min_depth = 0;
inline int history_pruning_min_moves = 0;

// Static evaluation correction history.
inline int corrhist_scale       =  256;
inline int corrhist_bonus_mult  =   32;
inline int corrhist_bonus_limit =  768;
inline int corrhist_max         = 8192;
inline int corrhist_depth_cap   =    4;

// ProbCut.
inline int probcut_margin       = 50;
inline int probcut_reduction    =  3;
inline int probcut_min_depth    =  5;
inline int probcut_max_captures =  3;

// Time-management policy.
inline int    time_no_clock_ms              =  100;
inline int    time_moves_to_go_min          =    1;
inline int    time_moves_to_go_max          =   80;
inline int    time_default_moves_with_inc   =   24;
inline int    time_default_moves_no_inc     =   32;
inline double time_increment_fraction       = 0.75;
inline double time_bank_ceiling_fraction    = 0.82;
inline double time_hard_bound_multiplier    =  4.0;
inline int    time_soft_min_ms              =    5;
inline int    time_hard_min_ms              =   10;
inline int    time_no_inc_emergency_ms      =  250;
inline int    time_no_inc_emergency_max_ms  =   10;
inline int    time_stable_first_iterations  =    4;
inline int    time_stable_second_iterations =    8;
inline double time_stable_first_scale       = 0.80;
inline double time_stable_second_scale      = 0.80;
inline int    time_best_change_min_depth    =    6;
inline double time_best_change_scale        = 1.35;
inline int    time_eval_drop_min_depth      =    6;
inline int    time_eval_drop_first_cp       =   30;
inline int    time_eval_drop_second_cp      =   60;
inline double time_eval_drop_first_scale    = 1.50;
inline double time_eval_drop_second_scale   = 1.50;

// Root best-move node-share scaling.
inline int    time_node_min_depth   =    6;
inline double time_node_base_scale  = 1.10;
inline double time_node_share_slope = 0.40;
inline double time_node_min_scale   = 0.75;
inline double time_node_max_scale   = 1.10;

// Multi-depth evaluation-stability scaling.
inline int    time_eval_stability_min_depth =    8;
inline int    time_eval_stability_window    =    4;
inline int    time_eval_stability_max_range =   20;
inline double time_eval_stability_scale     = 0.90;

// ===== TUNING INFRASTRUCTURE =====

struct TuningOption {
    void* ptr;
    enum Type { INT, DOUBLE } type;
    int min_val;
    int max_val;
    std::string default_str;
};

inline std::unordered_map<std::string, TuningOption> tuning_registry = {
    /* Batch 1: 5+0.05
    {                   "ASP_Delta", {                     &asp_delta,    TuningOption::INT,    16,    80,      "41"}},
    {                     "FP_Base", {                       &fp_base,    TuningOption::INT,    80,   350,     "217"}},
    {                "FP_Max_Depth", {                  &fp_max_depth,    TuningOption::INT,     1,    12,       "4"}},
    {                     "FP_Mult", {                       &fp_mult,    TuningOption::INT,   300,  1000,     "762"}},
    {               "IIR_Min_Depth", {                 &iir_min_depth,    TuningOption::INT,     1,    12,       "4"}},
    {                    "LMP_Base", {                      &lmp_base,    TuningOption::INT,     1,     8,       "2"}},
    {                    "LMP_Mult", {                      &lmp_mult,    TuningOption::INT,     1,     3,       "1"}},
    {                    "LMR_Base", {                      &lmr_base, TuningOption::DOUBLE,     0,     0, "1.25954"}},
    {                   "LMR_Scale", {                     &lmr_scale, TuningOption::DOUBLE,     0,     0, "1.80102"}},
    {             "QS_Delta_Margin", {               &qs_delta_margin,    TuningOption::INT,    50,   350,     "204"}},
    {                  "RFP_Margin", {                    &rfp_margin,    TuningOption::INT,    30,   120,      "68"}},
    {               "RFP_Max_Depth", {                 &rfp_max_depth,    TuningOption::INT,     1,    12,       "5"}},
    {                  "SEE_Margin", {                    &see_margin,    TuningOption::INT,  -600,   -40,    "-161"}},
    */

    /* Batch 2: 10+0.1
    {               "ASP_Min_Depth", {                 &asp_min_depth,    TuningOption::INT,     1,    12,       "4"}}, // default
    {          "CorrHist_Depth_Cap", {            &corrhist_depth_cap,    TuningOption::INT,     1,    32,       "4"}}, // default
    {     "Improving_LMR_Reduction", {       &improving_lmr_reduction,    TuningOption::INT,    -3,     0,      "-2"}}, // default
    {             "LMR_Bad_History", {               &lmr_bad_history,    TuningOption::INT, -4096,     0,    "-800"}}, // default
    {         "LMR_Extra_Min_Depth", {           &lmr_extra_min_depth,    TuningOption::INT,     1,    16,       "6"}}, // default
    {    "LMR_Extra_Move_Threshold", {      &lmr_extra_move_threshold,    TuningOption::INT,     1,    32,       "6"}}, // default
    {            "LMR_Good_History", {              &lmr_good_history,    TuningOption::INT,     0,  4096,     "800"}}, // default
    {               "LMR_Min_Depth", {                 &lmr_min_depth,    TuningOption::INT,     1,    12,       "3"}}, // default
    {            "LMR_NonPV_Offset", {              &lmr_nonpv_offset,    TuningOption::INT,     0,     3,       "0"}}, // default
    {               "LMR_PV_Offset", {                 &lmr_pv_offset,    TuningOption::INT,    -3,     0,       "0"}}, // default
    {           "NMP_Reduction_Min", {             &nmp_reduction_min,    TuningOption::INT,     0,    16,       "0"}}, // default
    {                "QS_Min_Depth", {                  &qs_min_depth,    TuningOption::INT,   -16,     0,      "-6"}}, // default
    {               "QS_SEE_Margin", {                 &qs_see_margin,    TuningOption::INT,     0,   200,     "100"}}, // default
    {               "SEE_Max_Depth", {                 &see_max_depth,    TuningOption::INT,     1,    16,       "4"}}, // default
    */

    /* Batch 3: 5+0.05
    {          "NMP_Base_Reduction", {            &nmp_base_reduction,    TuningOption::INT,     2,     5,       "3"}},
    {           "NMP_Depth_Divisor", {             &nmp_depth_divisor,    TuningOption::INT,     2,     8,       "3"}},
    {            "NMP_Eval_Divisor", {              &nmp_eval_divisor,    TuningOption::INT,    80,   500,     "183"}},
    {             "NMP_Margin_Mult", {               &nmp_margin_mult,    TuningOption::INT,     0,    60,      "15"}},
    {           "NMP_Reduction_Max", {             &nmp_reduction_max,    TuningOption::INT,     2,    12,       "8"}},
    {        "NMP_Verify_Min_Depth", {          &nmp_verify_min_depth,    TuningOption::INT,     8,    20,      "10"}},
    {             "SE_Depth_Margin", {               &se_depth_margin,    TuningOption::INT,     1,     8,       "2"}},
    {                   "SE_Margin", {                     &se_margin,    TuningOption::INT,    25,   100,      "74"}},
    {                "SE_Min_Depth", {                  &se_min_depth,    TuningOption::INT,     1,    16,      "11"}},
    {          "SE_Reduction_Denom", {            &se_reduction_denom,    TuningOption::INT,     2,     6,       "5"}},
    */

    /* Batch 4: 5+0.05
    {                  "CMH_Weight", {                    &cmh_weight,    TuningOption::INT,    15,   160,      "75"}},
    {                  "FMH_Weight", {                    &fmh_weight,    TuningOption::INT,     0,   130,      "30"}},
    {         "History_Bonus_Limit", {           &history_bonus_limit,    TuningOption::INT,  1500,  4500,    "3090"}},
    {          "History_Bonus_Mult", {            &history_bonus_mult,    TuningOption::INT,   280,   720,     "468"}},
    {           "History_Bonus_Sub", {             &history_bonus_sub,    TuningOption::INT,    60,   340,     "165"}},
    {                 "History_Max", {                   &history_max,    TuningOption::INT,  1024, 32768,   "16384"}}, // default
    {         "Main_History_Weight", {           &main_history_weight,    TuningOption::INT,    25,   160,      "85"}},
    */

    /* Batch 5: 10+0.1, tuning failed validation
    {              "ProbCut_Margin", {                &probcut_margin,    TuningOption::INT,     0,   300,      "50"}},
    {        "ProbCut_Max_Captures", {          &probcut_max_captures,    TuningOption::INT,     1,     8,       "3"}},
    {           "ProbCut_Min_Depth", {             &probcut_min_depth,    TuningOption::INT,     3,    12,       "5"}},
    {           "ProbCut_Reduction", {             &probcut_reduction,    TuningOption::INT,     1,     5,       "3"}},
    */

    /* TIME PARAMETERS FOR FUTURE TUNING -- all untuned
    {           "Time_Bank_Ceiling", {    &time_bank_ceiling_fraction, TuningOption::DOUBLE,     0,     0,    "0.82"}},
    {      "Time_Best_Change_Depth", {    &time_best_change_min_depth,    TuningOption::INT,     1,    20,       "6"}},
    {      "Time_Best_Change_Scale", {        &time_best_change_scale, TuningOption::DOUBLE,     0,     0,    "1.35"}},
    {      "Time_Default_Moves_Inc", {   &time_default_moves_with_inc,    TuningOption::INT,     4,    80,      "24"}},
    {    "Time_Default_Moves_NoInc", {     &time_default_moves_no_inc,    TuningOption::INT,     4,    80,      "32"}},
    {        "Time_Eval_Drop_Depth", {      &time_eval_drop_min_depth,    TuningOption::INT,     1,    20,       "6"}},
    {     "Time_Eval_Drop_First_CP", {       &time_eval_drop_first_cp,    TuningOption::INT,     1,   500,      "30"}},
    {  "Time_Eval_Drop_First_Scale", {    &time_eval_drop_first_scale, TuningOption::DOUBLE,     0,     0,    "1.50"}},
    {    "Time_Eval_Drop_Second_CP", {      &time_eval_drop_second_cp,    TuningOption::INT,     1,  1000,      "60"}},
    {"Time_Eval_Drop_Second_Scale", {   &time_eval_drop_second_scale, TuningOption::DOUBLE,     0,     0,    "1.50"}},
    {      "Time_Eval_Stable_Depth", { &time_eval_stability_min_depth,    TuningOption::INT,     1,    20,       "8"}},
    {      "Time_Eval_Stable_Range", { &time_eval_stability_max_range,    TuningOption::INT,     0,   200,      "20"}},
    {      "Time_Eval_Stable_Scale", {     &time_eval_stability_scale, TuningOption::DOUBLE,     0,     0,    "0.90"}},
    {     "Time_Eval_Stable_Window", {    &time_eval_stability_window,    TuningOption::INT,     2,    16,       "4"}},
    {        "Time_Hard_Bound_Mult", {    &time_hard_bound_multiplier, TuningOption::DOUBLE,     0,     0,     "4.0"}},
    {            "Time_Hard_Min_MS", {              &time_hard_min_ms,    TuningOption::INT,     1,   500,      "10"}},
    {     "Time_Increment_Fraction", {       &time_increment_fraction, TuningOption::DOUBLE,     0,     0,    "0.75"}},
    {        "Time_Moves_To_Go_Max", {          &time_moves_to_go_max,    TuningOption::INT,    10,   120,      "80"}},
    {        "Time_Moves_To_Go_Min", {          &time_moves_to_go_min,    TuningOption::INT,     1,    20,       "1"}},
    {            "Time_No_Clock_MS", {              &time_no_clock_ms,    TuningOption::INT,    10,  1000,     "100"}},
    {    "Time_NoInc_Emergency_Max", {  &time_no_inc_emergency_max_ms,    TuningOption::INT,     1,   100,      "10"}},
    {     "Time_NoInc_Emergency_MS", {      &time_no_inc_emergency_ms,    TuningOption::INT,     0,  2000,     "250"}},
    {        "Time_Node_Base_Scale", {          &time_node_base_scale, TuningOption::DOUBLE,     0,     0,    "1.10"}},
    {         "Time_Node_Max_Scale", {           &time_node_max_scale, TuningOption::DOUBLE,     0,     0,    "1.10"}},
    {         "Time_Node_Min_Depth", {           &time_node_min_depth,    TuningOption::INT,     1,    20,       "6"}},
    {         "Time_Node_Min_Scale", {           &time_node_min_scale, TuningOption::DOUBLE,     0,     0,    "0.75"}},
    {       "Time_Node_Share_Slope", {         &time_node_share_slope, TuningOption::DOUBLE,     0,     0,    "0.40"}},
    {            "Time_Soft_Min_MS", {              &time_soft_min_ms,    TuningOption::INT,     1,   200,       "5"}},
    {     "Time_Stable_First_Iters", {  &time_stable_first_iterations,    TuningOption::INT,     1,    20,       "4"}},
    {     "Time_Stable_First_Scale", {       &time_stable_first_scale, TuningOption::DOUBLE,     0,     0,    "0.80"}},
    {    "Time_Stable_Second_Iters", { &time_stable_second_iterations,    TuningOption::INT,     1,    30,       "8"}},
    {    "Time_Stable_Second_Scale", {      &time_stable_second_scale, TuningOption::DOUBLE,     0,     0,    "0.80"}},
    */

    /* FROZEN / DO NOT TUNE -- prior tuning or direct ablation and untuned:
    {                  "ASP_Growth", {                    &asp_growth,    TuningOption::INT,     2,     4,       "2"}},
    {      "Capture_History_Weight", {        &capture_history_weight,    TuningOption::INT,     0,   400,     "100"}},
    {             "Check_Extension", {               &check_extension,    TuningOption::INT,     0,     2,       "0"}},
    {        "CorrHist_Bonus_Limit", {          &corrhist_bonus_limit,    TuningOption::INT,   128,  4096,     "768"}}, // tuned
    {         "CorrHist_Bonus_Mult", {           &corrhist_bonus_mult,    TuningOption::INT,     0,   128,      "32"}}, // tuned
    {                "CorrHist_Max", {                  &corrhist_max,    TuningOption::INT,  2048, 32768,    "8192"}}, // tuned
    {              "CorrHist_Scale", {                &corrhist_scale,    TuningOption::INT,    64,   512,     "256"}}, // critical at both TC, tuned
    {       "CutNode_LMR_Reduction", {         &cutnode_lmr_reduction,    TuningOption::INT,     0,     3,       "3"}},
    {               "IIR_Reduction", {                 &iir_reduction,    TuningOption::INT,     0,     2,       "1"}},
    {   "LMR_Bad_History_Reduction", {     &lmr_bad_history_reduction,    TuningOption::INT,     0,     2,       "1"}},
    {         "LMR_Extra_Reduction", {           &lmr_extra_reduction,    TuningOption::INT,     0,     2,       "1"}},
    {  "LMR_Good_History_Reduction", {    &lmr_good_history_reduction,    TuningOption::INT,    -2,     0,      "-1"}},
    {               "NMP_Min_Depth", {                 &nmp_min_depth,    TuningOption::INT,     1,    12,       "3"}},
    {              "QS_Start_Depth", {                &qs_start_depth,    TuningOption::INT,     1,    16,       "8"}},
    {                "SE_Extension", {                  &se_extension,    TuningOption::INT,     0,     2,       "1"}},
    {                 "SE_MultiCut", {                   &se_multicut,    TuningOption::INT,     0,     1,       "1"}},
    */

    /* DISABLED AFTER HARMFUL ABLATION:
    {   "History_Pruning_Min_Depth", {     &history_pruning_min_depth,    TuningOption::INT,     0,     8,       "0"}},
    {   "History_Pruning_Min_Moves", {     &history_pruning_min_moves,    TuningOption::INT,     0,    16,       "0"}},
    {   "History_Pruning_Threshold", {     &history_pruning_threshold,    TuningOption::INT, -1200,     0,       "0"}},
    {          "QS_Futility_Margin", {            &qs_futility_margin,    TuningOption::INT,     0,   300,       "0"}},
    */

    /* DISABLED AFTER ABLATION:
    {         "SE_Double_Extension", {           &se_double_extension,    TuningOption::INT,     0,     3,       "0"}},
    {        "SE_Double_Extensions", {          &se_double_extensions,    TuningOption::INT,     0,     1,       "0"}},
    {            "SE_Double_Margin", {              &se_double_margin,    TuningOption::INT,     0,   400,       "0"}},
    {   "SE_Negative_Cut_Extension", { &se_negative_cutnode_extension,    TuningOption::INT,    -4,     0,       "0"}},
    {      "SE_Negative_Extensions", {        &se_negative_extensions,    TuningOption::INT,     0,     1,       "0"}},
    {    "SE_Negative_TT_Extension", {      &se_negative_tt_extension,    TuningOption::INT,    -4,     0,       "0"}},
    {         "SE_Triple_Extension", {           &se_triple_extension,    TuningOption::INT,     0,     4,       "0"}},
    {        "SE_Triple_Extensions", {          &se_triple_extensions,    TuningOption::INT,     0,     1,       "0"}},
    {            "SE_Triple_Margin", {              &se_triple_margin,    TuningOption::INT,     0,   600,       "0"}},
    */
};

// Applies a UCI setoption value and leaves the old value unchanged on failure.
inline bool handle_setoption(const std::string& name, const std::string& value) {
    auto it = tuning_registry.find(name);
    if (it == tuning_registry.end()) return false;
    auto& opt = it->second;
    try {
        std::size_t parsed_chars = 0;
        if (opt.type == TuningOption::INT) {
            const int parsed = std::stoi(value, &parsed_chars);
            if (parsed_chars != value.size()) return false;
            *static_cast<int*>(opt.ptr) = std::clamp(parsed, opt.min_val, opt.max_val);
        } else {
            const double parsed = std::stod(value, &parsed_chars);
            if (parsed_chars != value.size() || !std::isfinite(parsed)) return false;
            *static_cast<double*>(opt.ptr) = parsed;
        }
    } catch (const std::exception&) {
        return false;
    }
    return true;
}

} // namespace Tune

} // namespace SHAYVERI

#endif // TUNE_H

#include "SkinnedMeshManager.h"
#include "Snowdrop.h"
#include "Main.h"
#include "ItemDescriptorCache.h"
#include "EquipPipelineProbe.h"
#include "imgui/imgui.h"

#include <Windows.h>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <iostream>

// ─── curated model lists (display name + asset path) ─────────────────────────
// Backpack list seeded from the user's prior table; extend others as you
// catalog more assets. The UI also exposes a free-text custom path field per
// slot, so the list is just convenience.

// Curated lists rebuilt 2026-05-12 to use .mitem base names (no path
// prefix, no .mgraphobject suffix). The ApplyEquipByName flow used by
// the Apply button derives the lookup key from the assetPath's basename,
// which for these entries IS the .mitem name. Templates, generated_*,
// and *_premade_template entries are deliberately omitted — those don't
// resolve to real items in InventoryConfig.
static const SkinnedMeshManager::ModelSwapEntry s_backpackModels_[] =
{
    { "Start",                         "player_back_start" },
    { "Survival Start",                "player_back_survival_start" },
    { "Ex 1",                          "player_back_ex_1" },
    { "Support Generated Item",                          "player_back_support_generated_item" },
    { "Offensive Generated Item",                          "player_back_offensive_generated_item" },
    { "Defensive Generated Item",                          "player_back_defensive_generated_item" },
    { "Generated Item",                          "player_back_generated_item" },
    // T1 sets
    { "Set T1 Defensive",              "player_back_set_t1_defensive" },
    { "Set T1 Offensive",              "player_back_set_t1_offensive" },
    { "Set T1 Sniper",                 "player_back_set_t1_sniper" },
    { "Set T1 Solo",                   "player_back_set_t1_solo" },
    { "Set T1 Support",                "player_back_set_t1_support" },
    // T2 sets
    { "Set T2 Defensive",              "player_back_set_t2_defensive" },
    { "Set T2 Offensive",              "player_back_set_t2_offensive" },
    { "Set T2 Sniper",                 "player_back_set_t2_sniper" },
    { "Set T2 Support",                "player_back_set_t2_support" },
    // T3 sets
    { "Set T3 Defensive",              "player_back_set_t3_defensive" },
    { "Set T3 Hybrid",                 "player_back_set_t3_hybrid" },
    { "Set T3 Offensive",              "player_back_set_t3_offensive" },
    { "Set T3 Sniper",                 "player_back_set_t3_sniper" },
    { "Set T3 Solo",                   "player_back_set_t3_solo" },
    { "Set T3 Support",                "player_back_set_t3_support" },
    // T4
    { "Set T4 Offensive",              "player_back_set_t4_offensive" },
    // Classified T1
    { "Classified Set T1 Offensive",   "player_back_classified_set_t1_offensive" },
    { "Classified Set T1 Sniper",      "player_back_classified_set_t1_sniper" },
    { "Classified Set T1 Solo",        "player_back_classified_set_t1_solo" },
    { "Classified Set T1 Support",     "player_back_classified_set_t1_support" },
    // Classified T2
    { "Classified Set T2 Defensive",   "player_back_classified_set_t2_defensive" },
    { "Classified Set T2 Offensive",   "player_back_classified_set_t2_offensive" },
    { "Classified Set T2 Sniper",      "player_back_classified_set_t2_sniper" },
    { "Classified Set T2 Support",     "player_back_classified_set_t2_support" },
    // Classified T3
    { "Classified Set T3 Defensive",   "player_back_classified_set_t3_defensive" },
    { "Classified Set T3 Hybrid",      "player_back_classified_set_t3_hybrid" },
    { "Classified Set T3 Offensive",   "player_back_classified_set_t3_offensive" },
    { "Classified Set T3 Sniper",      "player_back_classified_set_t3_sniper" },
    { "Classified Set T3 Solo",        "player_back_classified_set_t3_solo" },
    { "Classified Set T3 Support",     "player_back_classified_set_t3_support" },
    // ULC (DLC content)
    { "ULC CBRN",                      "player_back_ulc_cbrn" },
    { "ULC Firefighter",               "player_back_ulc_firefighter" },
    { "ULC Hunter",                    "player_back_ulc_hunter" },
    { "ULC National Guard",            "player_back_ulc_national_guard" },
    { "ULC Paramedic",                 "player_back_ulc_paramedic" },
    { "ULC Police",                    "player_back_ulc_police" },
    { "ULC Survivor",                  "player_back_ulc_survivor" },
};

static const SkinnedMeshManager::ModelSwapEntry s_ChestPlateModels_[] =
{
    { "Start",                         "player_chest_start" },
    { "Survival Start",                "player_chest_survival_start" },
    { "Ex 1",                          "player_chest_ex_1" },
    { "Set T1 Defensive",              "player_chest_set_t1_defensive" },
    { "Set T1 Offensive",              "player_chest_set_t1_offensive" },
    { "Set T1 Sniper",                 "player_chest_set_t1_sniper" },
    { "Set T1 Solo",                   "player_chest_set_t1_solo" },
    { "Set T1 Support",                "player_chest_set_t1_support" },
    { "Set T2 Defensive",              "player_chest_set_t2_defensive" },
    { "Set T2 Offensive",              "player_chest_set_t2_offensive" },
    { "Set T2 Sniper",                 "player_chest_set_t2_sniper" },
    { "Set T2 Support",                "player_chest_set_t2_support" },
    { "Set T3 Defensive",              "player_chest_set_t3_defensive" },
    { "Set T3 Hybrid",                 "player_chest_set_t3_hybrid" },
    { "Set T3 Offensive",              "player_chest_set_t3_offensive" },
    { "Set T3 Sniper",                 "player_chest_set_t3_sniper" },
    { "Set T3 Solo",                   "player_chest_set_t3_solo" },
    { "Set T3 Support",                "player_chest_set_t3_support" },
    { "Set T4 Offensive",              "player_chest_set_t4_offensive" },
    { "Classified Set T1 Offensive",   "player_chest_classified_set_t1_offensive" },
    { "Classified Set T1 Sniper",      "player_chest_classified_set_t1_sniper" },
    { "Classified Set T1 Solo",        "player_chest_classified_set_t1_solo" },
    { "Classified Set T1 Support",     "player_chest_classified_set_t1_support" },
    { "Classified Set T2 Defensive",   "player_chest_classified_set_t2_defensive" },
    { "Classified Set T2 Offensive",   "player_chest_classified_set_t2_offensive" },
    { "Classified Set T2 Sniper",      "player_chest_classified_set_t2_sniper" },
    { "Classified Set T2 Support",     "player_chest_classified_set_t2_support" },
    { "Classified Set T3 Defensive",   "player_chest_classified_set_t3_defensive" },
    { "Classified Set T3 Hybrid",      "player_chest_classified_set_t3_hybrid" },
    { "Classified Set T3 Offensive",   "player_chest_classified_set_t3_offensive" },
    { "Classified Set T3 Sniper",      "player_chest_classified_set_t3_sniper" },
    { "Classified Set T3 Solo",        "player_chest_classified_set_t3_solo" },
    { "Classified Set T3 Support",     "player_chest_classified_set_t3_support" },
};

static const SkinnedMeshManager::ModelSwapEntry s_HolsterModels_[] =
{
    { "Start",                         "player_thighs_start" },
    { "Survival Start",                "player_thighs_survival_start" },
    { "Ex 1",                          "player_thighs_ex_1" },
    { "Set T1 Defensive",              "player_thighs_set_t1_defensive" },
    { "Set T1 Offensive",              "player_thighs_set_t1_offensive" },
    { "Set T1 Sniper",                 "player_thighs_set_t1_sniper" },
    { "Set T1 Solo",                   "player_thighs_set_t1_solo" },
    { "Set T1 Support",                "player_thighs_set_t1_support" },
    { "Set T2 Defensive",              "player_thighs_set_t2_defensive" },
    { "Set T2 Offensive",              "player_thighs_set_t2_offensive" },
    { "Set T2 Sniper",                 "player_thighs_set_t2_sniper" },
    { "Set T2 Support",                "player_thighs_set_t2_support" },
    { "Set T3 Defensive",              "player_thighs_set_t3_defensive" },
    { "Set T3 Hybrid",                 "player_thighs_set_t3_hybrid" },
    { "Set T3 Offensive",              "player_thighs_set_t3_offensive" },
    { "Set T3 Sniper",                 "player_thighs_set_t3_sniper" },
    { "Set T3 Solo",                   "player_thighs_set_t3_solo" },
    { "Set T3 Support",                "player_thighs_set_t3_support" },
    { "Set T4 Offensive",              "player_thighs_set_t4_offensive" },
    { "Classified Set T1 Offensive",   "player_thighs_classified_set_t1_offensive" },
    { "Classified Set T1 Sniper",      "player_thighs_classified_set_t1_sniper" },
    { "Classified Set T1 Solo",        "player_thighs_classified_set_t1_solo" },
    { "Classified Set T1 Support",     "player_thighs_classified_set_t1_support" },
    { "Classified Set T2 Defensive",   "player_thighs_classified_set_t2_defensive" },
    { "Classified Set T2 Offensive",   "player_thighs_classified_set_t2_offensive" },
    { "Classified Set T2 Sniper",      "player_thighs_classified_set_t2_sniper" },
    { "Classified Set T2 Support",     "player_thighs_classified_set_t2_support" },
    { "Classified Set T3 Defensive",   "player_thighs_classified_set_t3_defensive" },
    { "Classified Set T3 Hybrid",      "player_thighs_classified_set_t3_hybrid" },
    { "Classified Set T3 Offensive",   "player_thighs_classified_set_t3_offensive" },
    { "Classified Set T3 Sniper",      "player_thighs_classified_set_t3_sniper" },
    { "Classified Set T3 Solo",        "player_thighs_classified_set_t3_solo" },
    { "Classified Set T3 Support",     "player_thighs_classified_set_t3_support" },
};

static const SkinnedMeshManager::ModelSwapEntry s_KneePadModels_[] =
{
    { "Start",                         "player_knees_start" },
    { "Survival Start",                "player_knees_survival_start" },
    { "Ex 1",                          "player_knees_ex_1" },
    { "Set T1 Defensive",              "player_knees_set_t1_defensive" },
    { "Set T1 Offensive",              "player_knees_set_t1_offensive" },
    { "Set T1 Sniper",                 "player_knees_set_t1_sniper" },
    { "Set T1 Solo",                   "player_knees_set_t1_solo" },
    { "Set T1 Support",                "player_knees_set_t1_support" },
    { "Set T2 Defensive",              "player_knees_set_t2_defensive" },
    { "Set T2 Offensive",              "player_knees_set_t2_offensive" },
    { "Set T2 Sniper",                 "player_knees_set_t2_sniper" },
    { "Set T2 Support",                "player_knees_set_t2_support" },
    { "Set T3 Defensive",              "player_knees_set_t3_defensive" },
    { "Set T3 Hybrid",                 "player_knees_set_t3_hybrid" },
    { "Set T3 Offensive",              "player_knees_set_t3_offensive" },
    { "Set T3 Sniper",                 "player_knees_set_t3_sniper" },
    { "Set T3 Solo",                   "player_knees_set_t3_solo" },
    { "Set T3 Support",                "player_knees_set_t3_support" },
    { "Set T4 Offensive",              "player_knees_set_t4_offensive" },
    { "Classified Set T1 Offensive",   "player_knees_classified_set_t1_offensive" },
    { "Classified Set T1 Sniper",      "player_knees_classified_set_t1_sniper" },
    { "Classified Set T1 Solo",        "player_knees_classified_set_t1_solo" },
    { "Classified Set T1 Support",     "player_knees_classified_set_t1_support" },
    { "Classified Set T2 Defensive",   "player_knees_classified_set_t2_defensive" },
    { "Classified Set T2 Offensive",   "player_knees_classified_set_t2_offensive" },
    { "Classified Set T2 Sniper",      "player_knees_classified_set_t2_sniper" },
    { "Classified Set T2 Support",     "player_knees_classified_set_t2_support" },
    { "Classified Set T3 Defensive",   "player_knees_classified_set_t3_defensive" },
    { "Classified Set T3 Hybrid",      "player_knees_classified_set_t3_hybrid" },
    { "Classified Set T3 Offensive",   "player_knees_classified_set_t3_offensive" },
    { "Classified Set T3 Sniper",      "player_knees_classified_set_t3_sniper" },
    { "Classified Set T3 Solo",        "player_knees_classified_set_t3_solo" },
    { "Classified Set T3 Support",     "player_knees_classified_set_t3_support" },
};

static const SkinnedMeshManager::ModelSwapEntry s_GloveModels_[] =
{
    { "Start",                         "player_hands_start" },
    { "Survival Start",                "player_hands_survival_start" },
    { "Ex 1",                          "player_hands_ex_1" },
    { "Set T1 Defensive",              "player_hands_set_t1_defensive" },
    { "Set T1 Offensive",              "player_hands_set_t1_offensive" },
    { "Set T1 Sniper",                 "player_hands_set_t1_sniper" },
    { "Set T1 Solo",                   "player_hands_set_t1_solo" },
    { "Set T1 Support",                "player_hands_set_t1_support" },
    { "Set T2 Defensive",              "player_hands_set_t2_defensive" },
    { "Set T2 Offensive",              "player_hands_set_t2_offensive" },
    { "Set T2 Sniper",                 "player_hands_set_t2_sniper" },
    { "Set T2 Support",                "player_hands_set_t2_support" },
    { "Set T3 Defensive",              "player_hands_set_t3_defensive" },
    { "Set T3 Hybrid",                 "player_hands_set_t3_hybrid" },
    { "Set T3 Offensive",              "player_hands_set_t3_offensive" },
    { "Set T3 Sniper",                 "player_hands_set_t3_sniper" },
    { "Set T3 Solo",                   "player_hands_set_t3_solo" },
    { "Set T3 Support",                "player_hands_set_t3_support" },
    { "Set T4 Offensive",              "player_hands_set_t4_offensive" },
    { "Classified Set T1 Offensive",   "player_hands_classified_set_t1_offensive" },
    { "Classified Set T1 Sniper",      "player_hands_classified_set_t1_sniper" },
    { "Classified Set T1 Solo",        "player_hands_classified_set_t1_solo" },
    { "Classified Set T1 Support",     "player_hands_classified_set_t1_support" },
    { "Classified Set T2 Defensive",   "player_hands_classified_set_t2_defensive" },
    { "Classified Set T2 Offensive",   "player_hands_classified_set_t2_offensive" },
    { "Classified Set T2 Sniper",      "player_hands_classified_set_t2_sniper" },
    { "Classified Set T2 Support",     "player_hands_classified_set_t2_support" },
    { "Classified Set T3 Defensive",   "player_hands_classified_set_t3_defensive" },
    { "Classified Set T3 Hybrid",      "player_hands_classified_set_t3_hybrid" },
    { "Classified Set T3 Offensive",   "player_hands_classified_set_t3_offensive" },
    { "Classified Set T3 Sniper",      "player_hands_classified_set_t3_sniper" },
    { "Classified Set T3 Solo",        "player_hands_classified_set_t3_solo" },
    { "Classified Set T3 Support",     "player_hands_classified_set_t3_support" },
};

static const SkinnedMeshManager::ModelSwapEntry s_HatModels_[] =
{
    // Starts
    { "Start",                   "player_hat_start" },
    { "Start 2",                 "player_hat_start_2" },
    // Letter series (base)
    { "A 1", "player_hat_a_1" }, { "A 2", "player_hat_a_2" }, { "A 3", "player_hat_a_3" }, { "A 4", "player_hat_a_4" },
    { "B 1", "player_hat_b_1" }, { "B 2", "player_hat_b_2" }, { "B 3", "player_hat_b_3" }, { "B 4", "player_hat_b_4" },
    { "B T2 1", "player_hat_b_t2_1" }, { "B T2 2", "player_hat_b_t2_2" }, { "B T2 3", "player_hat_b_t2_3" }, { "B T2 4", "player_hat_b_t2_4" },
    { "C 1", "player_hat_c_1" }, { "C 2", "player_hat_c_2" }, { "C 3", "player_hat_c_3" }, { "C 4", "player_hat_c_4" },
    { "D 1", "player_hat_d_1" }, { "D 2", "player_hat_d_2" }, { "D 3", "player_hat_d_3" }, { "D 4", "player_hat_d_4" },
    { "D T3 1", "player_hat_d_t3_1" }, { "D T3 2", "player_hat_d_t3_2" }, { "D T3 3", "player_hat_d_t3_3" }, { "D T3 4", "player_hat_d_t3_4" },
    { "E 1", "player_hat_e_1" }, { "E 2", "player_hat_e_2" }, { "E 3", "player_hat_e_3" }, { "E 4", "player_hat_e_4" },
    { "E T2 1", "player_hat_e_t2_1" }, { "E T2 2", "player_hat_e_t2_2" }, { "E T2 3", "player_hat_e_t2_3" }, { "E T2 4", "player_hat_e_t2_4" },
    { "E T3 1", "player_hat_e_t3_1" }, { "E T3 2", "player_hat_e_t3_2" }, { "E T3 3", "player_hat_e_t3_3" }, { "E T3 4", "player_hat_e_t3_4" },
    { "F 1", "player_hat_f_1" }, { "F 2", "player_hat_f_2" }, { "F 3", "player_hat_f_3" }, { "F 4", "player_hat_f_4" },
    { "F T2 1", "player_hat_f_t2_1" }, { "F T2 2", "player_hat_f_t2_2" }, { "F T2 3", "player_hat_f_t2_3" }, { "F T2 4", "player_hat_f_t2_4" },
    { "F T3 1", "player_hat_f_t3_1" }, { "F T3 2", "player_hat_f_t3_2" }, { "F T3 3", "player_hat_f_t3_3" }, { "F T3 4", "player_hat_f_t3_4" },
    { "G 1", "player_hat_g_1" }, { "G 2", "player_hat_g_2" }, { "G 3", "player_hat_g_3" }, { "G 4", "player_hat_g_4" },
    { "H 1", "player_hat_h_1" }, { "H 2", "player_hat_h_2" }, { "H 3", "player_hat_h_3" }, { "H 4", "player_hat_h_4" },
    { "H T3 1", "player_hat_h_t3_1" }, { "H T3 2", "player_hat_h_t3_2" }, { "H T3 3", "player_hat_h_t3_3" }, { "H T3 4", "player_hat_h_t3_4" },
    { "I 1", "player_hat_i_1" }, { "I 2", "player_hat_i_2" }, { "I 3", "player_hat_i_3" }, { "I 4", "player_hat_i_4" },
    { "J 1", "player_hat_j_1" }, { "J 2", "player_hat_j_2" }, { "J 3", "player_hat_j_3" }, { "J 4", "player_hat_j_4" },
    { "J T2 1", "player_hat_j_t2_1" }, { "J T2 2", "player_hat_j_t2_2" }, { "J T2 3", "player_hat_j_t2_3" }, { "J T2 4", "player_hat_j_t2_4" },
    { "J T3 1", "player_hat_j_t3_1" }, { "J T3 2", "player_hat_j_t3_2" }, { "J T3 3", "player_hat_j_t3_3" }, { "J T3 4", "player_hat_j_t3_4" },
    // TU4 update
    { "TU4 1", "player_hat_tu4_1" }, { "TU4 2", "player_hat_tu4_2" }, { "TU4 3", "player_hat_tu4_3" }, { "TU4 4", "player_hat_tu4_4" },
    // DLC
    { "DLC1 Ann1",               "player_hat_dlc1_ann1" },
    { "DLC BG Redstorm 2",       "player_hat_dlc_bg_redstorm2" },
    // Season cosmetics
    { "Season 4 Ambush",         "player_hat_season4_ambush" },
    { "Season Toxic",            "player_hat_season_toxic" },
    // Mystery box hats
    { "MBox 4 A", "player_hat_mbox_4_a" }, { "MBox 4 B", "player_hat_mbox_4_b" }, { "MBox 4 C", "player_hat_mbox_4_c" },
    { "MBox 4 D", "player_hat_mbox_4_d" }, { "MBox 4 E", "player_hat_mbox_4_e" },
    { "MBox 5 A", "player_hat_mbox_5_a" }, { "MBox 5 C", "player_hat_mbox_5_c" }, { "MBox 5 D", "player_hat_mbox_5_d" }, { "MBox 5 E", "player_hat_mbox_5_e" },
    { "MBox A1", "player_hat_mbox_a1" },
    { "MBox A1 1", "player_hat_mbox_a1_1" }, { "MBox A1 2", "player_hat_mbox_a1_2" }, { "MBox A1 3", "player_hat_mbox_a1_3" },
    { "MBox A1 4", "player_hat_mbox_a1_4" }, { "MBox A1 5", "player_hat_mbox_a1_5" },
    { "MBox A2", "player_hat_mbox_a2" }, { "MBox A3", "player_hat_mbox_a3" }, { "MBox A4", "player_hat_mbox_a4" },
    { "MBox B1", "player_hat_mbox_b1" },
    { "MBox B1 0", "player_hat_mbox_b1_0" }, { "MBox B1 1", "player_hat_mbox_b1_1" }, { "MBox B1 2", "player_hat_mbox_b1_2" },
    { "MBox B1 3", "player_hat_mbox_b1_3" }, { "MBox B1 4", "player_hat_mbox_b1_4" },
    { "MBox B2", "player_hat_mbox_b2" }, { "MBox B3", "player_hat_mbox_b3" }, { "MBox B4", "player_hat_mbox_b4" },
    { "MBox Bucket", "player_hat_mbox_bucket" },
    { "MBox C1", "player_hat_mbox_c1" },
    { "MBox C1 1", "player_hat_mbox_c1_1" }, { "MBox C1 2", "player_hat_mbox_c1_2" }, { "MBox C1 3", "player_hat_mbox_c1_3" }, { "MBox C1 4", "player_hat_mbox_c1_4" },
    { "MBox C2", "player_hat_mbox_c2" }, { "MBox C3", "player_hat_mbox_c3" }, { "MBox C4", "player_hat_mbox_c4" },
    { "MBox Chullo 1", "player_hat_mbox_chullo_1" }, { "MBox Chullo 2", "player_hat_mbox_chullo_2" },
    { "MBox Chullo 3", "player_hat_mbox_chullo_3" }, { "MBox Chullo 4", "player_hat_mbox_chullo_4" },
    { "MBox Earflap", "player_hat_mbox_earflap" },
    { "MBox Miner", "player_hat_mbox_miner" },
    { "MBox Newsy", "player_hat_mbox_newsy" },
    { "MBox Print Forest 01", "player_hat_mbox_print_forest01" },
    { "MBox Print Wild 01", "player_hat_mbox_print_wild01" },
    { "MBox Print Winter 01", "player_hat_mbox_print_winter01" },
    { "MBox Ushanka", "player_hat_mbox_ushanka" },
    // ULC (DLC content)
    { "ULC Alphabridge",          "player_hat_ulc_alphabridge" },
    { "ULC Astronaut",            "player_hat_ulc_astronaut" },
    { "ULC Astronaut 2",          "player_hat_ulc_astronaut2" },
    { "ULC Beta",                 "player_hat_ulc_beta" },
    { "ULC CBRN",                 "player_hat_ulc_cbrn" },
    { "ULC Cent",                 "player_hat_ulc_cent" },
    { "ULC Claus",                "player_hat_ulc_claus" },
    { "ULC Contractor",           "player_hat_ulc_contractor" },
    { "ULC Covert Adjudicator",   "player_hat_ulc_covert_adjudicator" },
    { "ULC Darkness",             "player_hat_ulc_darkness" },
    { "ULC Delta",                "player_hat_ulc_delta" },
    { "ULC Fac Cleaners",         "player_hat_ulc_fac_cleaners" },
    { "ULC Fac JTF",              "player_hat_ulc_fac_jtf" },
    { "ULC Fac LMB",              "player_hat_ulc_fac_lmb" },
    { "ULC Fac LMB3",             "player_hat_ulc_fac_lmb3" },
    { "ULC Fac Riker",            "player_hat_ulc_fac_riker" },
    { "ULC Firecrest",            "player_hat_ulc_firecrest" },
    { "ULC Firefighter",          "player_hat_ulc_firefighter" },
    { "ULC Fireproof",            "player_hat_ulc_fireproof" },
    { "ULC Freelancer",           "player_hat_ulc_freelancer" },
    { "ULC Frontline",            "player_hat_ulc_frontline" },
    { "ULC Grec",                 "player_hat_ulc_grec" },
    { "ULC GV DE",                "player_hat_ulc_gv_de" },
    { "ULC GV FM",                "player_hat_ulc_gv_fm" },
    { "ULC GV HF",                "player_hat_ulc_gv_hf" },
    { "ULC GV LS",                "player_hat_ulc_gv_ls" },
    { "ULC GV MK",                "player_hat_ulc_gv_mk" },
    { "ULC GV NP",                "player_hat_ulc_gv_np" },
    { "ULC GV RC",                "player_hat_ulc_gv_rc" },
    { "ULC GV SC",                "player_hat_ulc_gv_sc" },
    { "ULC GV ST",                "player_hat_ulc_gv_st" },
    { "ULC GV TC",                "player_hat_ulc_gv_tc" },
    { "ULC Hunter",               "player_hat_ulc_hunter" },
    { "ULC Lucky",                "player_hat_ulc_lucky" },
    { "ULC Marine Desert",        "player_hat_ulc_marine_desert" },
    { "ULC Marine Snow",          "player_hat_ulc_marine_snow" },
    { "ULC Marine Urban",         "player_hat_ulc_marine_urban" },
    { "ULC Marine Woodland",      "player_hat_ulc_marine_woodland" },
    { "ULC MC Gang",              "player_hat_ulc_mc_gang" },
    { "ULC MC Police",            "player_hat_ulc_mc_police" },
    { "ULC MC Retro",             "player_hat_ulc_mc_retro" },
    { "ULC Mercenary",            "player_hat_ulc_mercenary" },
    { "ULC Mileod",               "player_hat_ulc_mileod" },
    { "ULC Mil Sniper",           "player_hat_ulc_milsniper" },
    { "ULC Mt Rescue",            "player_hat_ulc_mt_rescue" },
    { "ULC National Guard",       "player_hat_ulc_national_guard" },
    { "ULC Noel",                 "player_hat_ulc_noel" },
    { "ULC NY Trooper",           "player_hat_ulc_ny_trooper" },
    { "ULC Operator",             "player_hat_ulc_operator" },
    { "ULC Parade 1",             "player_hat_ulc_parade_1" },
    { "ULC Parade 2",             "player_hat_ulc_parade_2" },
    { "ULC Parade 3",             "player_hat_ulc_parade_3" },
    { "ULC Parade 4",             "player_hat_ulc_parade_4" },
    { "ULC Paramedic",            "player_hat_ulc_paramedic" },
    { "ULC Pilot",                "player_hat_ulc_pilot" },
    { "ULC PMC201",               "player_hat_ulc_pmc201" },
    { "ULC PMC202",               "player_hat_ulc_pmc202" },
    { "ULC Police",               "player_hat_ulc_police" },
    { "ULC Punk",                 "player_hat_ulc_punk" },
    { "ULC Rave",                 "player_hat_ulc_rave" },
    { "ULC Sheriff",              "player_hat_ulc_sheriff" },
    { "ULC Siege",                "player_hat_ulc_siege" },
    { "ULC Splinter Cell",        "player_hat_ulc_splinter_cell" },
    { "ULC Sport Baseball",       "player_hat_ulc_sport_baseball" },
    { "ULC Sport Hockey",         "player_hat_ulc_sport_hockey" },
    { "ULC Sport Racing Driver",  "player_hat_ulc_sport_racing_driver" },
    { "ULC Sport Snowboard",      "player_hat_ulc_sport_snowboard" },
    { "ULC Survivor",             "player_hat_ulc_survivor" },
    { "ULC SW E1",                "player_hat_ulc_sw_e1" },
    { "ULC SW W1",                "player_hat_ulc_sw_w1" },
    { "ULC SW W2",                "player_hat_ulc_sw_w2" },
    { "ULC SWAT",                 "player_hat_ulc_swat" },
    { "ULC Tactical Adjudicator", "player_hat_ulc_tactical_adjudicator" },
    { "ULC Upper East 1",         "player_hat_ulc_upper_east_1" },
    { "ULC Upper East 2",         "player_hat_ulc_upper_east_2" },
    { "ULC Upper East 3",         "player_hat_ulc_upper_east_3" },
    { "ULC Upper East 4",         "player_hat_ulc_upper_east_4" },
    { "ULC Yuletide",             "player_hat_ulc_yuletide" },
};

// "Gas Mask" maps to the Face slot (slot 2 in m_Clothes). The engine
// stores face-slot items under .mitem names beginning with player_face_*.
static const SkinnedMeshManager::ModelSwapEntry s_GasMaskModels_[] =
{
    { "Start",                         "player_face_start" },
    { "Survival Start",                "player_face_survival_start" },
    { "Ex 1",                          "player_face_ex_1" },
    { "Set T1 Defensive",              "player_face_set_t1_defensive" },
    { "Set T1 Offensive",              "player_face_set_t1_offensive" },
    { "Set T1 Sniper",                 "player_face_set_t1_sniper" },
    { "Set T1 Solo",                   "player_face_set_t1_solo" },
    { "Set T1 Support",                "player_face_set_t1_support" },
    { "Set T2 Defensive",              "player_face_set_t2_defensive" },
    { "Set T2 Offensive",              "player_face_set_t2_offensive" },
    { "Set T2 Sniper",                 "player_face_set_t2_sniper" },
    { "Set T2 Support",                "player_face_set_t2_support" },
    { "Set T3 Defensive",              "player_face_set_t3_defensive" },
    { "Set T3 Hybrid",                 "player_face_set_t3_hybrid" },
    { "Set T3 Offensive",              "player_face_set_t3_offensive" },
    { "Set T3 Sniper",                 "player_face_set_t3_sniper" },
    { "Set T3 Solo",                   "player_face_set_t3_solo" },
    { "Set T3 Support",                "player_face_set_t3_support" },
    { "Set T4 Offensive",              "player_face_set_t4_offensive" },
    { "Classified Set T1 Offensive",   "player_face_classified_set_t1_offensive" },
    { "Classified Set T1 Sniper",      "player_face_classified_set_t1_sniper" },
    { "Classified Set T1 Solo",        "player_face_classified_set_t1_solo" },
    { "Classified Set T1 Support",     "player_face_classified_set_t1_support" },
    { "Classified Set T2 Defensive",   "player_face_classified_set_t2_defensive" },
    { "Classified Set T2 Offensive",   "player_face_classified_set_t2_offensive" },
    { "Classified Set T2 Sniper",      "player_face_classified_set_t2_sniper" },
    { "Classified Set T2 Support",     "player_face_classified_set_t2_support" },
    { "Classified Set T3 Defensive",   "player_face_classified_set_t3_defensive" },
    { "Classified Set T3 Hybrid",      "player_face_classified_set_t3_hybrid" },
    { "Classified Set T3 Offensive",   "player_face_classified_set_t3_offensive" },
    { "Classified Set T3 Sniper",      "player_face_classified_set_t3_sniper" },
    { "Classified Set T3 Solo",        "player_face_classified_set_t3_solo" },
    { "Classified Set T3 Support",     "player_face_classified_set_t3_support" },
};

static const SkinnedMeshManager::ModelSwapEntry s_ShirtModels_[] =
{
    { "Start",                   "player_shirt_start" },
    { "Start 2",                 "player_shirt_start_2" },
    { "Start 3",                 "player_shirt_start_3" },
    { "Survival Hazmat",         "player_shirt_survival_hazmat" },
    { "Survival Hazmat 2",       "player_shirt_survival_hazmat_2" },
    { "Survival Hazmat 3",       "player_shirt_survival_hazmat_3" },
    { "Survival Hazmat 4",       "player_shirt_survival_hazmat_4" },
    { "A 1", "player_shirt_a_1" }, { "A 2", "player_shirt_a_2" }, { "A 3", "player_shirt_a_3" }, { "A 4", "player_shirt_a_4" },
    { "B 1", "player_shirt_b_1" }, { "B 2", "player_shirt_b_2" }, { "B 3", "player_shirt_b_3" }, { "B 4", "player_shirt_b_4" },
    { "C 1", "player_shirt_c_1" }, { "C 2", "player_shirt_c_2" }, { "C 3", "player_shirt_c_3" }, { "C 4", "player_shirt_c_4" },
    { "D 1", "player_shirt_d_1" }, { "D 2", "player_shirt_d_2" }, { "D 3", "player_shirt_d_3" }, { "D 4", "player_shirt_d_4" },
    { "E 1", "player_shirt_e_1" }, { "E 2", "player_shirt_e_2" }, { "E 3", "player_shirt_e_3" }, { "E 4", "player_shirt_e_4" },
    { "F 1", "player_shirt_f_1" }, { "F 2", "player_shirt_f_2" }, { "F 3", "player_shirt_f_3" }, { "F 4", "player_shirt_f_4" },
    { "G 1", "player_shirt_g_1" }, { "G 2", "player_shirt_g_2" }, { "G 3", "player_shirt_g_3" }, { "G 4", "player_shirt_g_4" },
    { "H 1", "player_shirt_h_1" }, { "H 2", "player_shirt_h_2" }, { "H 3", "player_shirt_h_3" }, { "H 4", "player_shirt_h_4" },
    { "I 1", "player_shirt_i_1" }, { "I 2", "player_shirt_i_2" }, { "I 3", "player_shirt_i_3" }, { "I 4", "player_shirt_i_4" },
    { "J 1", "player_shirt_j_1" }, { "J 2", "player_shirt_j_2" }, { "J 3", "player_shirt_j_3" }, { "J 4", "player_shirt_j_4" },
    { "K 1", "player_shirt_k_1" }, { "K 2", "player_shirt_k_2" }, { "K 3", "player_shirt_k_3" }, { "K 4", "player_shirt_k_4" },
    { "TU4 1", "player_shirt_tu4_1" }, { "TU4 2", "player_shirt_tu4_2" }, { "TU4 3", "player_shirt_tu4_3" }, { "TU4 4", "player_shirt_tu4_4" },
    { "DLC1 Ann1",               "player_shirt_dlc1_ann1" },
    { "DLC BG Redstorm 2",       "player_shirt_dlc_bg_redstorm2" },
    { "Season 2 Assault",        "player_shirt_season2_assault" },
    { "Season 3 Strike",         "player_shirt_season3_strike" },
    { "Season 4 Ambush",         "player_shirt_season4_ambush" },
    { "Season Toxic",            "player_shirt_season_toxic" },
    { "MBox A1", "player_shirt_mbox_a1" }, { "MBox A2", "player_shirt_mbox_a2" },
    { "MBox B1", "player_shirt_mbox_b1" }, { "MBox B2", "player_shirt_mbox_b2" },
    { "MBox C1", "player_shirt_mbox_c1" }, { "MBox C2", "player_shirt_mbox_c2" },
    { "MBox D1", "player_shirt_mbox_d1" }, { "MBox D2", "player_shirt_mbox_d2" },
    { "MBox Fireproof",          "player_shirt_mbox_fireproof" },
    { "ULC Alphabridge",          "player_shirt_ulc_alphabridge" },
    { "ULC Astronaut",            "player_shirt_ulc_astronaut" },
    { "ULC Astronaut 2",          "player_shirt_ulc_astronaut2" },
    { "ULC CBRN",                 "player_shirt_ulc_cbrn" },
    { "ULC Contractor",           "player_shirt_ulc_contractor" },
    { "ULC Covert Adjudicator",   "player_shirt_ulc_covert_adjudicator" },
    { "ULC Darkness",             "player_shirt_ulc_darkness" },
    { "ULC Delta",                "player_shirt_ulc_delta" },
    { "ULC Fac Cleaners",         "player_shirt_ulc_fac_cleaners" },
    { "ULC Fac JTF",              "player_shirt_ulc_fac_jtf" },
    { "ULC Fac LMB",              "player_shirt_ulc_fac_lmb" },
    { "ULC Fac LMB3",             "player_shirt_ulc_fac_lmb3" },
    { "ULC Fac Riker",            "player_shirt_ulc_fac_riker" },
    { "ULC Firecrest",            "player_shirt_ulc_firecrest" },
    { "ULC Freelancer",           "player_shirt_ulc_freelancer" },
    { "ULC Frontline",            "player_shirt_ulc_frontline" },
    { "ULC Grec",                 "player_shirt_ulc_grec" },
    { "ULC GV DE",                "player_shirt_ulc_gv_de" },
    { "ULC GV FM",                "player_shirt_ulc_gv_fm" },
    { "ULC GV HF",                "player_shirt_ulc_gv_hf" },
    { "ULC GV LS",                "player_shirt_ulc_gv_ls" },
    { "ULC GV MK",                "player_shirt_ulc_gv_mk" },
    { "ULC GV NP",                "player_shirt_ulc_gv_np" },
    { "ULC GV RC",                "player_shirt_ulc_gv_rc" },
    { "ULC GV SC",                "player_shirt_ulc_gv_sc" },
    { "ULC GV ST",                "player_shirt_ulc_gv_st" },
    { "ULC GV TC",                "player_shirt_ulc_gv_tc" },
    { "ULC Hunter",               "player_shirt_ulc_hunter" },
    { "ULC Marine Desert",        "player_shirt_ulc_marine_desert" },
    { "ULC Marine Snow",          "player_shirt_ulc_marine_snow" },
    { "ULC Marine Urban",         "player_shirt_ulc_marine_urban" },
    { "ULC Marine Woodland",      "player_shirt_ulc_marine_woodland" },
    { "ULC MC Gang",              "player_shirt_ulc_mc_gang" },
    { "ULC MC Police",            "player_shirt_ulc_mc_police" },
    { "ULC MC Retro",             "player_shirt_ulc_mc_retro" },
    { "ULC Mercenary",            "player_shirt_ulc_mercenary" },
    { "ULC Mileod",               "player_shirt_ulc_mileod" },
    { "ULC Mil Sniper",           "player_shirt_ulc_milsniper" },
    { "ULC Mt Rescue",            "player_shirt_ulc_mt_rescue" },
    { "ULC Noel",                 "player_shirt_ulc_noel" },
    { "ULC NY Trooper",           "player_shirt_ulc_ny_trooper" },
    { "ULC Operator",             "player_shirt_ulc_operator" },
    { "ULC Parade 1",             "player_shirt_ulc_parade_1" },
    { "ULC Parade 2",             "player_shirt_ulc_parade_2" },
    { "ULC Parade 3",             "player_shirt_ulc_parade_3" },
    { "ULC Parade 4",             "player_shirt_ulc_parade_4" },
    { "ULC Pilot",                "player_shirt_ulc_pilot" },
    { "ULC PMC201",               "player_shirt_ulc_pmc201" },
    { "ULC PMC202",               "player_shirt_ulc_pmc202" },
    { "ULC Seeker",               "player_shirt_ulc_seeker" },
    { "ULC Sheriff",              "player_shirt_ulc_sheriff" },
    { "ULC Siege",                "player_shirt_ulc_siege" },
    { "ULC Splinter Cell",        "player_shirt_ulc_splinter_cell" },
    { "ULC Sport Baseball",       "player_shirt_ulc_sport_baseball" },
    { "ULC Sport Hockey",         "player_shirt_ulc_sport_hockey" },
    { "ULC Sport Racing Driver",  "player_shirt_ulc_sport_racing_driver" },
    { "ULC Sport Snowboard",      "player_shirt_ulc_sport_snowboard" },
    { "ULC SW E1",                "player_shirt_ulc_sw_e1" },
    { "ULC SW E2",                "player_shirt_ulc_sw_e2" },
    { "ULC SW W1",                "player_shirt_ulc_sw_w1" },
    { "ULC SW W2",                "player_shirt_ulc_sw_w2" },
    { "ULC SWAT",                 "player_shirt_ulc_swat" },
    { "ULC Tactical Adjudicator", "player_shirt_ulc_tactical_adjudicator" },
    { "ULC Uplay",                "player_shirt_ulc_uplay" },
    { "ULC Upper East 1",         "player_shirt_ulc_upper_east_1" },
    { "ULC Upper East 2",         "player_shirt_ulc_upper_east_2" },
    { "ULC Upper East 3",         "player_shirt_ulc_upper_east_3" },
    { "ULC Upper East 4",         "player_shirt_ulc_upper_east_4" },
    { "ULC Yuletide",             "player_shirt_ulc_yuletide" },
};

static const SkinnedMeshManager::ModelSwapEntry s_FootModels_[] =
{
    { "Start",                   "player_shoes_start" },
    { "Start 2",                 "player_shoes_start_2" },
    { "Survival Hazmat",         "player_shoes_survival_hazmat" },
    { "A 1", "player_shoes_a_1" }, { "A 2", "player_shoes_a_2" }, { "A 3", "player_shoes_a_3" }, { "A 4", "player_shoes_a_4" },
    { "B 1", "player_shoes_b_1" }, { "B 2", "player_shoes_b_2" }, { "B 3", "player_shoes_b_3" }, { "B 4", "player_shoes_b_4" },
    { "C 1", "player_shoes_c_1" }, { "C 2", "player_shoes_c_2" }, { "C 3", "player_shoes_c_3" }, { "C 4", "player_shoes_c_4" },
    { "D 1", "player_shoes_d_1" }, { "D 2", "player_shoes_d_2" }, { "D 3", "player_shoes_d_3" }, { "D 4", "player_shoes_d_4" },
    { "E 1", "player_shoes_e_1" }, { "E 2", "player_shoes_e_2" }, { "E 3", "player_shoes_e_3" }, { "E 4", "player_shoes_e_4" },
    { "F 1", "player_shoes_f_1" }, { "F 2", "player_shoes_f_2" }, { "F 3", "player_shoes_f_3" }, { "F 4", "player_shoes_f_4" },
    { "G 1", "player_shoes_g_1" }, { "G 2", "player_shoes_g_2" }, { "G 3", "player_shoes_g_3" }, { "G 4", "player_shoes_g_4" },
    { "H 1", "player_shoes_h_1" }, { "H 2", "player_shoes_h_2" }, { "H 3", "player_shoes_h_3" }, { "H 4", "player_shoes_h_4" },
    { "I 1", "player_shoes_i_1" }, { "I 2", "player_shoes_i_2" }, { "I 3", "player_shoes_i_3" }, { "I 4", "player_shoes_i_4" },
    { "J 1", "player_shoes_j_1" }, { "J 2", "player_shoes_j_2" }, { "J 3", "player_shoes_j_3" }, { "J 4", "player_shoes_j_4" },
    { "K 1", "player_shoes_k_1" }, { "K 2", "player_shoes_k_2" }, { "K 3", "player_shoes_k_3" }, { "K 4", "player_shoes_k_4" },
    { "L 1", "player_shoes_l_1" }, { "L 2", "player_shoes_l_2" }, { "L 3", "player_shoes_l_3" }, { "L 4", "player_shoes_l_4" },
    { "M 1", "player_shoes_m_1" }, { "M 2", "player_shoes_m_2" }, { "M 3", "player_shoes_m_3" }, { "M 4", "player_shoes_m_4" },
    { "N 1", "player_shoes_n_1" }, { "N 2", "player_shoes_n_2" }, { "N 3", "player_shoes_n_3" }, { "N 4", "player_shoes_n_4" },
    { "TU4 1", "player_shoes_tu4_1" }, { "TU4 2", "player_shoes_tu4_2" }, { "TU4 3", "player_shoes_tu4_3" }, { "TU4 4", "player_shoes_tu4_4" },
    { "DLC1 Ann1",               "player_shoes_dlc1_ann1" },
    { "DLC BG Redstorm 2",       "player_shoes_dlc_bg_redstorm2" },
    { "Season 2 Assault",        "player_shoes_season2_assault" },
    { "Season 3 Strike",         "player_shoes_season3_strike" },
    { "Season 4 Ambush",         "player_shoes_season4_ambush" },
    { "Season Toxic",            "player_shoes_season_toxic" },
    { "MBox A", "player_shoes_mbox_a" }, { "MBox B", "player_shoes_mbox_b" }, { "MBox C", "player_shoes_mbox_c" }, { "MBox D", "player_shoes_mbox_d" },
    { "MBox Fisher 1", "player_shoes_mbox_fisher_1" }, { "MBox Fisher 2", "player_shoes_mbox_fisher_2" }, { "MBox Fisher 3", "player_shoes_mbox_fisher_3" },
    { "MBox Goth",   "player_shoes_mbox_goth" },
    { "MBox Miner",  "player_shoes_mbox_miner" },
    { "MBox Snow 1", "player_shoes_mbox_snow_1" }, { "MBox Snow 2", "player_shoes_mbox_snow_2" }, { "MBox Snow 3", "player_shoes_mbox_snow_3" }, { "MBox Snow 4", "player_shoes_mbox_snow_4" },
    { "ULC Alphabridge",          "player_shoes_ulc_alphabridge" },
    { "ULC Astronaut",            "player_shoes_ulc_astronaut" },
    { "ULC Astronaut 2",          "player_shoes_ulc_astronaut2" },
    { "ULC CBRN",                 "player_shoes_ulc_cbrn" },
    { "ULC Claus",                "player_shoes_ulc_claus" },
    { "ULC Contractor",           "player_shoes_ulc_contractor" },
    { "ULC Covert Adjudicator",   "player_shoes_ulc_covert_adjudicator" },
    { "ULC Darkness",             "player_shoes_ulc_darkness" },
    { "ULC Delta",                "player_shoes_ulc_delta" },
    { "ULC Fac Cleaners",         "player_shoes_ulc_fac_cleaners" },
    { "ULC Fac JTF",              "player_shoes_ulc_fac_jtf" },
    { "ULC Fac LMB",              "player_shoes_ulc_fac_lmb" },
    { "ULC Fac LMB3",             "player_shoes_ulc_fac_lmb3" },
    { "ULC Fac Riker",            "player_shoes_ulc_fac_riker" },
    { "ULC Firecrest",            "player_shoes_ulc_firecrest" },
    { "ULC Firefighter",          "player_shoes_ulc_firefighter" },
    { "ULC Fireproof",            "player_shoes_ulc_fireproof" },
    { "ULC Freelancer",           "player_shoes_ulc_freelancer" },
    { "ULC Frontline",            "player_shoes_ulc_frontline" },
    { "ULC Grec",                 "player_shoes_ulc_grec" },
    { "ULC GV DE",                "player_shoes_ulc_gv_de" },
    { "ULC GV FM",                "player_shoes_ulc_gv_fm" },
    { "ULC GV HF",                "player_shoes_ulc_gv_hf" },
    { "ULC GV LS",                "player_shoes_ulc_gv_ls" },
    { "ULC GV MK",                "player_shoes_ulc_gv_mk" },
    { "ULC GV NP",                "player_shoes_ulc_gv_np" },
    { "ULC GV RC",                "player_shoes_ulc_gv_rc" },
    { "ULC GV SC",                "player_shoes_ulc_gv_sc" },
    { "ULC GV ST",                "player_shoes_ulc_gv_st" },
    { "ULC GV TC",                "player_shoes_ulc_gv_tc" },
    { "ULC Hunter",               "player_shoes_ulc_hunter" },
    { "ULC Lucky",                "player_shoes_ulc_lucky" },
    { "ULC Marine Desert",        "player_shoes_ulc_marine_desert" },
    { "ULC Marine Snow",          "player_shoes_ulc_marine_snow" },
    { "ULC Marine Urban",         "player_shoes_ulc_marine_urban" },
    { "ULC Marine Woodland",      "player_shoes_ulc_marine_woodland" },
    { "ULC MC Gang",              "player_shoes_ulc_mc_gang" },
    { "ULC MC Police",            "player_shoes_ulc_mc_police" },
    { "ULC MC Retro",             "player_shoes_ulc_mc_retro" },
    { "ULC Mercenary",            "player_shoes_ulc_mercenary" },
    { "ULC Mileod",               "player_shoes_ulc_mileod" },
    { "ULC Mil Sniper",           "player_shoes_ulc_milsniper" },
    { "ULC Mt Rescue",            "player_shoes_ulc_mt_rescue" },
    { "ULC National Guard",       "player_shoes_ulc_national_guard" },
    { "ULC NY Trooper",           "player_shoes_ulc_ny_trooper" },
    { "ULC Operator",             "player_shoes_ulc_operator" },
    { "ULC Parade 1",             "player_shoes_ulc_parade_1" },
    { "ULC Parade 2",             "player_shoes_ulc_parade_2" },
    { "ULC Parade 3",             "player_shoes_ulc_parade_3" },
    { "ULC Parade 4",             "player_shoes_ulc_parade_4" },
    { "ULC Paramedic",            "player_shoes_ulc_paramedic" },
    { "ULC Pilot",                "player_shoes_ulc_pilot" },
    { "ULC PMC201",               "player_shoes_ulc_pmc201" },
    { "ULC PMC202",               "player_shoes_ulc_pmc202" },
    { "ULC Police",               "player_shoes_ulc_police" },
    { "ULC Punk",                 "player_shoes_ulc_punk" },
    { "ULC Rave",                 "player_shoes_ulc_rave" },
    { "ULC Seeker",               "player_shoes_ulc_seeker" },
    { "ULC Sheriff",              "player_shoes_ulc_sheriff" },
    { "ULC Splinter Cell",        "player_shoes_ulc_splinter_cell" },
    { "ULC Sport Baseball",       "player_shoes_ulc_sport_baseball" },
    { "ULC Sport Hockey",         "player_shoes_ulc_sport_hockey" },
    { "ULC Sport Racing Driver",  "player_shoes_ulc_sport_racing_driver" },
    { "ULC Sport Snowboard",      "player_shoes_ulc_sport_snowboard" },
    { "ULC Survivor",             "player_shoes_ulc_survivor" },
    { "ULC SW E1",                "player_shoes_ulc_sw_e1" },
    { "ULC SW E2",                "player_shoes_ulc_sw_e2" },
    { "ULC SW W1",                "player_shoes_ulc_sw_w1" },
    { "ULC SW W2",                "player_shoes_ulc_sw_w2" },
    { "ULC SWAT",                 "player_shoes_ulc_swat" },
    { "ULC Tactical Adjudicator", "player_shoes_ulc_tactical_adjudicator" },
    { "ULC Uplay",                "player_shoes_ulc_uplay" },
    { "ULC Upper East 1",         "player_shoes_ulc_upper_east_1" },
    { "ULC Upper East 2",         "player_shoes_ulc_upper_east_2" },
    { "ULC Upper East 3",         "player_shoes_ulc_upper_east_3" },
    { "ULC Upper East 4",         "player_shoes_ulc_upper_east_4" },
};

static const SkinnedMeshManager::ModelSwapEntry s_ScarfModels_[] =
{
    { "Start",                   "player_scarf_start" },
    { "Start 2",                 "player_scarf_start_2" },
    { "Survival Hazmat",         "player_scarf_survival_hazmat" },
    { "A 1", "player_scarf_a_1" }, { "A 2", "player_scarf_a_2" }, { "A 3", "player_scarf_a_3" }, { "A 4", "player_scarf_a_4" },
    { "B 1", "player_scarf_b_1" }, { "B 2", "player_scarf_b_2" }, { "B 3", "player_scarf_b_3" }, { "B 4", "player_scarf_b_4" },
    { "C 1", "player_scarf_c_1" }, { "C 2", "player_scarf_c_2" }, { "C 3", "player_scarf_c_3" }, { "C 4", "player_scarf_c_4" },
    { "D 1", "player_scarf_d_1" }, { "D 2", "player_scarf_d_2" }, { "D 3", "player_scarf_d_3" }, { "D 4", "player_scarf_d_4" },
    { "E 1", "player_scarf_e_1" }, { "E 2", "player_scarf_e_2" }, { "E 3", "player_scarf_e_3" }, { "E 4", "player_scarf_e_4" },
    { "F 1", "player_scarf_f_1" }, { "F 2", "player_scarf_f_2" }, { "F 3", "player_scarf_f_3" }, { "F 4", "player_scarf_f_4" },
    { "G 1", "player_scarf_g_1" }, { "G 2", "player_scarf_g_2" }, { "G 3", "player_scarf_g_3" }, { "G 4", "player_scarf_g_4" },
    { "H 1", "player_scarf_h_1" }, { "H 2", "player_scarf_h_2" }, { "H 3", "player_scarf_h_3" }, { "H 4", "player_scarf_h_4" },
    { "I 1", "player_scarf_i_1" }, { "I 2", "player_scarf_i_2" }, { "I 3", "player_scarf_i_3" }, { "I 4", "player_scarf_i_4" },
    { "J 1", "player_scarf_j_1" }, { "J 2", "player_scarf_j_2" }, { "J 3", "player_scarf_j_3" }, { "J 4", "player_scarf_j_4" },
    { "K 1", "player_scarf_k_1" }, { "K 2", "player_scarf_k_2" }, { "K 3", "player_scarf_k_3" }, { "K 4", "player_scarf_k_4" },
    { "L 1", "player_scarf_l_1" }, { "L 2", "player_scarf_l_2" }, { "L 3", "player_scarf_l_3" }, { "L 4", "player_scarf_l_4" },
    { "M 1", "player_scarf_m_1" }, { "M 2", "player_scarf_m_2" }, { "M 3", "player_scarf_m_3" }, { "M 4", "player_scarf_m_4" },
    { "N 1", "player_scarf_n_1" }, { "N 2", "player_scarf_n_2" }, { "N 3", "player_scarf_n_3" }, { "N 4", "player_scarf_n_4" },
    { "TU4 1", "player_scarf_tu4_1" }, { "TU4 2", "player_scarf_tu4_2" }, { "TU4 3", "player_scarf_tu4_3" }, { "TU4 4", "player_scarf_tu4_4" },
    { "DLC1 Ann1",               "player_scarf_dlc1_ann1" },
    { "DLC BG Redstorm 2",       "player_scarf_dlc_bg_redstorm2" },
    { "Season 2 Assault",        "player_scarf_season2_assault" },
    { "Season 3 Strike",         "player_scarf_season3_strike" },
    { "Season 4 Ambush",         "player_scarf_season4_ambush" },
    { "Season Toxic",            "player_scarf_season_toxic" },
    { "MBox A", "player_scarf_mbox_a" }, { "MBox B", "player_scarf_mbox_b" }, { "MBox C", "player_scarf_mbox_c" }, { "MBox D", "player_scarf_mbox_d" },
    { "MBox Bolivia",            "player_scarf_mbox_bolivia" },
    { "MBox Earmuffs",           "player_scarf_mbox_earmuffs" },
    { "MBox Headphones",         "player_scarf_mbox_headphones" },
    { "MBox Fisher 1", "player_scarf_mbox_fisher_1" }, { "MBox Fisher 2", "player_scarf_mbox_fisher_2" }, { "MBox Fisher 3", "player_scarf_mbox_fisher_3" },
    { "MBox Goth",   "player_scarf_mbox_goth" },
    { "MBox Miner",  "player_scarf_mbox_miner" },
    { "MBox Snow 1", "player_scarf_mbox_snow_1" }, { "MBox Snow 2", "player_scarf_mbox_snow_2" }, { "MBox Snow 3", "player_scarf_mbox_snow_3" }, { "MBox Snow 4", "player_scarf_mbox_snow_4" },
    { "ULC Alphabridge",          "player_scarf_ulc_alphabridge" },
    { "ULC Astronaut",            "player_scarf_ulc_astronaut" },
    { "ULC Astronaut 2",          "player_scarf_ulc_astronaut2" },
    { "ULC CBRN",                 "player_scarf_ulc_cbrn" },
    { "ULC Claus",                "player_scarf_ulc_claus" },
    { "ULC Contractor",           "player_scarf_ulc_contractor" },
    { "ULC Covert Adjudicator",   "player_scarf_ulc_covert_adjudicator" },
    { "ULC Darkness",             "player_scarf_ulc_darkness" },
    { "ULC Delta",                "player_scarf_ulc_delta" },
    { "ULC Fac Cleaners",         "player_scarf_ulc_fac_cleaners" },
    { "ULC Fac JTF",              "player_scarf_ulc_fac_jtf" },
    { "ULC Fac LMB",              "player_scarf_ulc_fac_lmb" },
    { "ULC Fac LMB3",             "player_scarf_ulc_fac_lmb3" },
    { "ULC Fac Riker",            "player_scarf_ulc_fac_riker" },
    { "ULC Firecrest",            "player_scarf_ulc_firecrest" },
    { "ULC Firefighter",          "player_scarf_ulc_firefighter" },
    { "ULC Fireproof",            "player_scarf_ulc_fireproof" },
    { "ULC Freelancer",           "player_scarf_ulc_freelancer" },
    { "ULC Frontline",            "player_scarf_ulc_frontline" },
    { "ULC Grec",                 "player_scarf_ulc_grec" },
    { "ULC GV DE",                "player_scarf_ulc_gv_de" },
    { "ULC GV FM",                "player_scarf_ulc_gv_fm" },
    { "ULC GV HF",                "player_scarf_ulc_gv_hf" },
    { "ULC GV LS",                "player_scarf_ulc_gv_ls" },
    { "ULC GV MK",                "player_scarf_ulc_gv_mk" },
    { "ULC GV NP",                "player_scarf_ulc_gv_np" },
    { "ULC GV RC",                "player_scarf_ulc_gv_rc" },
    { "ULC GV SC",                "player_scarf_ulc_gv_sc" },
    { "ULC GV ST",                "player_scarf_ulc_gv_st" },
    { "ULC GV TC",                "player_scarf_ulc_gv_tc" },
    { "ULC Hunter",               "player_scarf_ulc_hunter" },
    { "ULC Lucky",                "player_scarf_ulc_lucky" },
    { "ULC Marine Desert",        "player_scarf_ulc_marine_desert" },
    { "ULC Marine Snow",          "player_scarf_ulc_marine_snow" },
    { "ULC Marine Urban",         "player_scarf_ulc_marine_urban" },
    { "ULC Marine Woodland",      "player_scarf_ulc_marine_woodland" },
    { "ULC MC Gang",              "player_scarf_ulc_mc_gang" },
    { "ULC MC Police",            "player_scarf_ulc_mc_police" },
    { "ULC MC Retro",             "player_scarf_ulc_mc_retro" },
    { "ULC Mercenary",            "player_scarf_ulc_mercenary" },
    { "ULC Mileod",               "player_scarf_ulc_mileod" },
    { "ULC Mil Sniper",           "player_scarf_ulc_milsniper" },
    { "ULC Mt Rescue",            "player_scarf_ulc_mt_rescue" },
    { "ULC National Guard",       "player_scarf_ulc_national_guard" },
    { "ULC NY Trooper",           "player_scarf_ulc_ny_trooper" },
    { "ULC Operator",             "player_scarf_ulc_operator" },
    { "ULC Parade 1",             "player_scarf_ulc_parade_1" },
    { "ULC Parade 2",             "player_scarf_ulc_parade_2" },
    { "ULC Parade 3",             "player_scarf_ulc_parade_3" },
    { "ULC Parade 4",             "player_scarf_ulc_parade_4" },
    { "ULC Paramedic",            "player_scarf_ulc_paramedic" },
    { "ULC Pilot",                "player_scarf_ulc_pilot" },
    { "ULC PMC201",               "player_scarf_ulc_pmc201" },
    { "ULC PMC202",               "player_scarf_ulc_pmc202" },
    { "ULC Police",               "player_scarf_ulc_police" },
    { "ULC Punk",                 "player_scarf_ulc_punk" },
    { "ULC Rave",                 "player_scarf_ulc_rave" },
    { "ULC Seeker",               "player_scarf_ulc_seeker" },
    { "ULC Sheriff",              "player_scarf_ulc_sheriff" },
    { "ULC Splinter Cell",        "player_scarf_ulc_splinter_cell" },
    { "ULC Sport Baseball",       "player_scarf_ulc_sport_baseball" },
    { "ULC Sport Hockey",         "player_scarf_ulc_sport_hockey" },
    { "ULC Sport Racing Driver",  "player_scarf_ulc_sport_racing_driver" },
    { "ULC Sport Snowboard",      "player_scarf_ulc_sport_snowboard" },
    { "ULC Survivor",             "player_scarf_ulc_survivor" },
    { "ULC SW E1",                "player_scarf_ulc_sw_e1" },
    { "ULC SW E2",                "player_scarf_ulc_sw_e2" },
    { "ULC SW W1",                "player_scarf_ulc_sw_w1" },
    { "ULC SW W2",                "player_scarf_ulc_sw_w2" },
    { "ULC SWAT",                 "player_scarf_ulc_swat" },
    { "ULC Tactical Adjudicator", "player_scarf_ulc_tactical_adjudicator" },
    { "ULC Uplay",                "player_scarf_ulc_uplay" },
    { "ULC Upper East 1",         "player_scarf_ulc_upper_east_1" },
    { "ULC Upper East 2",         "player_scarf_ulc_upper_east_2" },
    { "ULC Upper East 3",         "player_scarf_ulc_upper_east_3" },
    { "ULC Upper East 4",         "player_scarf_ulc_upper_east_4" },
};

static const SkinnedMeshManager::ModelSwapEntry s_PantsModels_[] =
{
    { "Start",                   "player_pants_start" },
    { "Start 2",                 "player_pants_start_2" },
    { "Survival Hazmat",         "player_pants_survival_hazmat" },
    { "A 1", "player_pants_a_1" }, { "A 2", "player_pants_a_2" }, { "A 3", "player_pants_a_3" }, { "A 4", "player_pants_a_4" },
    { "B 1", "player_pants_b_1" }, { "B 2", "player_pants_b_2" }, { "B 3", "player_pants_b_3" }, { "B 4", "player_pants_b_4" },
    { "C 1", "player_pants_c_1" }, { "C 2", "player_pants_c_2" }, { "C 3", "player_pants_c_3" }, { "C 4", "player_pants_c_4" },
    { "D 1", "player_pants_d_1" }, { "D 2", "player_pants_d_2" }, { "D 3", "player_pants_d_3" }, { "D 4", "player_pants_d_4" },
    { "E 1", "player_pants_e_1" }, { "E 2", "player_pants_e_2" }, { "E 3", "player_pants_e_3" }, { "E 4", "player_pants_e_4" },
    { "F 1", "player_pants_f_1" }, { "F 2", "player_pants_f_2" }, { "F 3", "player_pants_f_3" }, { "F 4", "player_pants_f_4" },
    { "G 1", "player_pants_g_1" }, { "G 2", "player_pants_g_2" }, { "G 3", "player_pants_g_3" }, { "G 4", "player_pants_g_4" },
    { "H 1", "player_pants_h_1" }, { "H 2", "player_pants_h_2" }, { "H 3", "player_pants_h_3" }, { "H 4", "player_pants_h_4" },
    { "I 1", "player_pants_i_1" }, { "I 2", "player_pants_i_2" }, { "I 3", "player_pants_i_3" }, { "I 4", "player_pants_i_4" },
    { "J 1", "player_pants_j_1" }, { "J 2", "player_pants_j_2" }, { "J 3", "player_pants_j_3" }, { "J 4", "player_pants_j_4" },
    { "K 1", "player_pants_k_1" }, { "K 2", "player_pants_k_2" }, { "K 3", "player_pants_k_3" }, { "K 4", "player_pants_k_4" },
    { "L 1", "player_pants_l_1" }, { "L 2", "player_pants_l_2" }, { "L 3", "player_pants_l_3" }, { "L 4", "player_pants_l_4" },
    { "M 1", "player_pants_m_1" }, { "M 2", "player_pants_m_2" }, { "M 3", "player_pants_m_3" }, { "M 4", "player_pants_m_4" },
    { "N 1", "player_pants_n_1" }, { "N 2", "player_pants_n_2" }, { "N 3", "player_pants_n_3" }, { "N 4", "player_pants_n_4" },
    { "TU4 1", "player_pants_tu4_1" }, { "TU4 2", "player_pants_tu4_2" }, { "TU4 3", "player_pants_tu4_3" }, { "TU4 4", "player_pants_tu4_4" },
    { "DLC1 Ann1",               "player_pants_dlc1_ann1" },
    { "DLC BG Redstorm 2",       "player_pants_dlc_bg_redstorm2" },
    { "Season 2 Assault",        "player_pants_season2_assault" },
    { "Season 3 Strike",         "player_pants_season3_strike" },
    { "Season 4 Ambush",         "player_pants_season4_ambush" },
    { "Season Toxic",            "player_pants_season_toxic" },
    { "MBox A", "player_pants_mbox_a" }, { "MBox B", "player_pants_mbox_b" }, { "MBox C", "player_pants_mbox_c" }, { "MBox D", "player_pants_mbox_d" },
    { "MBox Fisher 1", "player_pants_mbox_fisher_1" }, { "MBox Fisher 2", "player_pants_mbox_fisher_2" }, { "MBox Fisher 3", "player_pants_mbox_fisher_3" },
    { "MBox Goth",   "player_pants_mbox_goth" },
    { "MBox Miner",  "player_pants_mbox_miner" },
    { "MBox Snow 1", "player_pants_mbox_snow_1" }, { "MBox Snow 2", "player_pants_mbox_snow_2" }, { "MBox Snow 3", "player_pants_mbox_snow_3" }, { "MBox Snow 4", "player_pants_mbox_snow_4" },
    { "ULC Alphabridge",          "player_pants_ulc_alphabridge" },
    { "ULC Astronaut",            "player_pants_ulc_astronaut" },
    { "ULC Astronaut 2",          "player_pants_ulc_astronaut2" },
    { "ULC CBRN",                 "player_pants_ulc_cbrn" },
    { "ULC Claus",                "player_pants_ulc_claus" },
    { "ULC Contractor",           "player_pants_ulc_contractor" },
    { "ULC Covert Adjudicator",   "player_pants_ulc_covert_adjudicator" },
    { "ULC Darkness",             "player_pants_ulc_darkness" },
    { "ULC Delta",                "player_pants_ulc_delta" },
    { "ULC Fac Cleaners",         "player_pants_ulc_fac_cleaners" },
    { "ULC Fac JTF",              "player_pants_ulc_fac_jtf" },
    { "ULC Fac LMB",              "player_pants_ulc_fac_lmb" },
    { "ULC Fac LMB3",             "player_pants_ulc_fac_lmb3" },
    { "ULC Fac Riker",            "player_pants_ulc_fac_riker" },
    { "ULC Firecrest",            "player_pants_ulc_firecrest" },
    { "ULC Firefighter",          "player_pants_ulc_firefighter" },
    { "ULC Fireproof",            "player_pants_ulc_fireproof" },
    { "ULC Freelancer",           "player_pants_ulc_freelancer" },
    { "ULC Frontline",            "player_pants_ulc_frontline" },
    { "ULC Grec",                 "player_pants_ulc_grec" },
    { "ULC GV DE",                "player_pants_ulc_gv_de" },
    { "ULC GV FM",                "player_pants_ulc_gv_fm" },
    { "ULC GV HF",                "player_pants_ulc_gv_hf" },
    { "ULC GV LS",                "player_pants_ulc_gv_ls" },
    { "ULC GV MK",                "player_pants_ulc_gv_mk" },
    { "ULC GV NP",                "player_pants_ulc_gv_np" },
    { "ULC GV RC",                "player_pants_ulc_gv_rc" },
    { "ULC GV SC",                "player_pants_ulc_gv_sc" },
    { "ULC GV ST",                "player_pants_ulc_gv_st" },
    { "ULC GV TC",                "player_pants_ulc_gv_tc" },
    { "ULC Hunter",               "player_pants_ulc_hunter" },
    { "ULC Lucky",                "player_pants_ulc_lucky" },
    { "ULC Marine Desert",        "player_pants_ulc_marine_desert" },
    { "ULC Marine Snow",          "player_pants_ulc_marine_snow" },
    { "ULC Marine Urban",         "player_pants_ulc_marine_urban" },
    { "ULC Marine Woodland",      "player_pants_ulc_marine_woodland" },
    { "ULC MC Gang",              "player_pants_ulc_mc_gang" },
    { "ULC MC Police",            "player_pants_ulc_mc_police" },
    { "ULC MC Retro",             "player_pants_ulc_mc_retro" },
    { "ULC Mercenary",            "player_pants_ulc_mercenary" },
    { "ULC Mileod",               "player_pants_ulc_mileod" },
    { "ULC Mil Sniper",           "player_pants_ulc_milsniper" },
    { "ULC Mt Rescue",            "player_pants_ulc_mt_rescue" },
    { "ULC National Guard",       "player_pants_ulc_national_guard" },
    { "ULC NY Trooper",           "player_pants_ulc_ny_trooper" },
    { "ULC Operator",             "player_pants_ulc_operator" },
    { "ULC Parade 1",             "player_pants_ulc_parade_1" },
    { "ULC Parade 2",             "player_pants_ulc_parade_2" },
    { "ULC Parade 3",             "player_pants_ulc_parade_3" },
    { "ULC Parade 4",             "player_pants_ulc_parade_4" },
    { "ULC Paramedic",            "player_pants_ulc_paramedic" },
    { "ULC Pilot",                "player_pants_ulc_pilot" },
    { "ULC PMC201",               "player_pants_ulc_pmc201" },
    { "ULC PMC202",               "player_pants_ulc_pmc202" },
    { "ULC Police",               "player_pants_ulc_police" },
    { "ULC Punk",                 "player_pants_ulc_punk" },
    { "ULC Rave",                 "player_pants_ulc_rave" },
    { "ULC Seeker",               "player_pants_ulc_seeker" },
    { "ULC Sheriff",              "player_pants_ulc_sheriff" },
    { "ULC Splinter Cell",        "player_pants_ulc_splinter_cell" },
    { "ULC Sport Baseball",       "player_pants_ulc_sport_baseball" },
    { "ULC Sport Hockey",         "player_pants_ulc_sport_hockey" },
    { "ULC Sport Racing Driver",  "player_pants_ulc_sport_racing_driver" },
    { "ULC Sport Snowboard",      "player_pants_ulc_sport_snowboard" },
    { "ULC Survivor",             "player_pants_ulc_survivor" },
    { "ULC SW E1",                "player_pants_ulc_sw_e1" },
    { "ULC SW E2",                "player_pants_ulc_sw_e2" },
    { "ULC SW W1",                "player_pants_ulc_sw_w1" },
    { "ULC SW W2",                "player_pants_ulc_sw_w2" },
    { "ULC SWAT",                 "player_pants_ulc_swat" },
    { "ULC Tactical Adjudicator", "player_pants_ulc_tactical_adjudicator" },
    { "ULC Uplay",                "player_pants_ulc_uplay" },
    { "ULC Upper East 1",         "player_pants_ulc_upper_east_1" },
    { "ULC Upper East 2",         "player_pants_ulc_upper_east_2" },
    { "ULC Upper East 3",         "player_pants_ulc_upper_east_3" },
    { "ULC Upper East 4",         "player_pants_ulc_upper_east_4" },
};

static const SkinnedMeshManager::ModelSwapEntry s_JacketModels_[] =
{
    { "Start",                   "player_jacket_start" },
    { "Start 2",                 "player_jacket_start_2" },
    { "Survival Hazmat",         "player_jacket_survival_hazmat" },
    { "A 1", "player_jacket_a_1" }, { "A 2", "player_jacket_a_2" }, { "A 3", "player_jacket_a_3" }, { "A 4", "player_jacket_a_4" },
    { "B 1", "player_jacket_b_1" }, { "B 2", "player_jacket_b_2" }, { "B 3", "player_jacket_b_3" }, { "B 4", "player_jacket_b_4" },
    { "C 1", "player_jacket_c_1" }, { "C 2", "player_jacket_c_2" }, { "C 3", "player_jacket_c_3" }, { "C 4", "player_jacket_c_4" },
    { "D 1", "player_jacket_d_1" }, { "D 2", "player_jacket_d_2" }, { "D 3", "player_jacket_d_3" }, { "D 4", "player_jacket_d_4" },
    { "E 1", "player_jacket_e_1" }, { "E 2", "player_jacket_e_2" }, { "E 3", "player_jacket_e_3" }, { "E 4", "player_jacket_e_4" },
    { "F 1", "player_jacket_f_1" }, { "F 2", "player_jacket_f_2" }, { "F 3", "player_jacket_f_3" }, { "F 4", "player_jacket_f_4" },
    { "G 1", "player_jacket_g_1" }, { "G 2", "player_jacket_g_2" }, { "G 3", "player_jacket_g_3" }, { "G 4", "player_jacket_g_4" },
    { "H 1", "player_jacket_h_1" }, { "H 2", "player_jacket_h_2" }, { "H 3", "player_jacket_h_3" }, { "H 4", "player_jacket_h_4" },
    { "I 1", "player_jacket_i_1" }, { "I 2", "player_jacket_i_2" }, { "I 3", "player_jacket_i_3" }, { "I 4", "player_jacket_i_4" },
    { "J 1", "player_jacket_j_1" }, { "J 2", "player_jacket_j_2" }, { "J 3", "player_jacket_j_3" }, { "J 4", "player_jacket_j_4" },
    { "K 1", "player_jacket_k_1" }, { "K 2", "player_jacket_k_2" }, { "K 3", "player_jacket_k_3" }, { "K 4", "player_jacket_k_4" },
    { "L 1", "player_jacket_l_1" }, { "L 2", "player_jacket_l_2" }, { "L 3", "player_jacket_l_3" }, { "L 4", "player_jacket_l_4" },
    { "M 1", "player_jacket_m_1" }, { "M 2", "player_jacket_m_2" }, { "M 3", "player_jacket_m_3" }, { "M 4", "player_jacket_m_4" },
    { "N 1", "player_jacket_n_1" }, { "N 2", "player_jacket_n_2" }, { "N 3", "player_jacket_n_3" }, { "N 4", "player_jacket_n_4" },
    { "TU4 1", "player_jacket_tu4_1" }, { "TU4 2", "player_jacket_tu4_2" }, { "TU4 3", "player_jacket_tu4_3" }, { "TU4 4", "player_jacket_tu4_4" },
    { "DLC1 Ann1",               "player_jacket_dlc1_ann1" },
    { "DLC BG Redstorm 2",       "player_jacket_dlc_bg_redstorm2" },
    { "Season 2 Assault",        "player_jacket_season2_assault" },
    { "Season 3 Strike",         "player_jacket_season3_strike" },
    { "Season 4 Ambush",         "player_jacket_season4_ambush" },
    { "Season Toxic",            "player_jacket_season_toxic" },
    { "MBox A", "player_jacket_mbox_a" }, { "MBox B", "player_jacket_mbox_b" }, { "MBox C", "player_jacket_mbox_c" }, { "MBox D", "player_jacket_mbox_d" },
    { "MBox Fisher 1", "player_jacket_mbox_fisher_1" }, { "MBox Fisher 2", "player_jacket_mbox_fisher_2" }, { "MBox Fisher 3", "player_jacket_mbox_fisher_3" },
    { "MBox Ghillie",   "player_jacket_mbox_ghillie" },
    { "MBox Goth",   "player_jacket_mbox_goth" },
    { "MBox Miner",  "player_jacket_mbox_miner" },
    { "MBox Reflex", "player_jacket_mbox_reflex" },
    { "MBox Varsity","player_jacket_mbox_varsity" },
    { "MBox Snow 1", "player_jacket_mbox_snow_1" }, { "MBox Snow 2", "player_jacket_mbox_snow_2" }, { "MBox Snow 3", "player_jacket_mbox_snow_3" }, { "MBox Snow 4", "player_jacket_mbox_snow_4" },
    { "ULC Alphabridge",          "player_jacket_ulc_alphabridge" },
    { "ULC Astronaut",            "player_jacket_ulc_astronaut" },
    { "ULC Astronaut 2",          "player_jacket_ulc_astronaut2" },
    { "ULC CBRN",                 "player_jacket_ulc_cbrn" },
    { "ULC Claus",                "player_jacket_ulc_claus" },
    { "ULC Contractor",           "player_jacket_ulc_contractor" },
    { "ULC Covert Adjudicator",   "player_jacket_ulc_covert_adjudicator" },
    { "ULC Darkness",             "player_jacket_ulc_darkness" },
    { "ULC Delta",                "player_jacket_ulc_delta" },
    { "ULC Fac Cleaners",         "player_jacket_ulc_fac_cleaners" },
    { "ULC Fac JTF",              "player_jacket_ulc_fac_jtf" },
    { "ULC Fac LMB",              "player_jacket_ulc_fac_lmb" },
    { "ULC Fac LMB3",             "player_jacket_ulc_fac_lmb3" },
    { "ULC Fac Riker",            "player_jacket_ulc_fac_riker" },
    { "ULC Firecrest",            "player_jacket_ulc_firecrest" },
    { "ULC Firefighter",          "player_jacket_ulc_firefighter" },
    { "ULC Fireproof",            "player_jacket_ulc_fireproof" },
    { "ULC Freelancer",           "player_jacket_ulc_freelancer" },
    { "ULC Frontline",            "player_jacket_ulc_frontline" },
    { "ULC Grec",                 "player_jacket_ulc_grec" },
    { "ULC GV DE",                "player_jacket_ulc_gv_de" },
    { "ULC GV FM",                "player_jacket_ulc_gv_fm" },
    { "ULC GV HF",                "player_jacket_ulc_gv_hf" },
    { "ULC GV LS",                "player_jacket_ulc_gv_ls" },
    { "ULC GV MK",                "player_jacket_ulc_gv_mk" },
    { "ULC GV NP",                "player_jacket_ulc_gv_np" },
    { "ULC GV RC",                "player_jacket_ulc_gv_rc" },
    { "ULC GV SC",                "player_jacket_ulc_gv_sc" },
    { "ULC GV ST",                "player_jacket_ulc_gv_st" },
    { "ULC GV TC",                "player_jacket_ulc_gv_tc" },
    { "ULC Hunter",               "player_jacket_ulc_hunter" },
    { "ULC Lucky",                "player_jacket_ulc_lucky" },
    { "ULC Marine Desert",        "player_jacket_ulc_marine_desert" },
    { "ULC Marine Snow",          "player_jacket_ulc_marine_snow" },
    { "ULC Marine Urban",         "player_jacket_ulc_marine_urban" },
    { "ULC Marine Woodland",      "player_jacket_ulc_marine_woodland" },
    { "ULC MC Gang",              "player_jacket_ulc_mc_gang" },
    { "ULC MC Police",            "player_jacket_ulc_mc_police" },
    { "ULC MC Retro",             "player_jacket_ulc_mc_retro" },
    { "ULC Mercenary",            "player_jacket_ulc_mercenary" },
    { "ULC Mileod",               "player_jacket_ulc_mileod" },
    { "ULC Mil Sniper",           "player_jacket_ulc_milsniper" },
    { "ULC Mt Rescue",            "player_jacket_ulc_mt_rescue" },
    { "ULC National Guard",       "player_jacket_ulc_national_guard" },
    { "ULC NY Trooper",           "player_jacket_ulc_ny_trooper" },
    { "ULC Operator",             "player_jacket_ulc_operator" },
    { "ULC Parade 1",             "player_jacket_ulc_parade_1" },
    { "ULC Parade 2",             "player_jacket_ulc_parade_2" },
    { "ULC Parade 3",             "player_jacket_ulc_parade_3" },
    { "ULC Parade 4",             "player_jacket_ulc_parade_4" },
    { "ULC Paramedic",            "player_jacket_ulc_paramedic" },
    { "ULC Pilot",                "player_jacket_ulc_pilot" },
    { "ULC PMC201",               "player_jacket_ulc_pmc201" },
    { "ULC PMC202",               "player_jacket_ulc_pmc202" },
    { "ULC Police",               "player_jacket_ulc_police" },
    { "ULC Punk",                 "player_jacket_ulc_punk" },
    { "ULC Rave",                 "player_jacket_ulc_rave" },
    { "ULC Seeker",               "player_jacket_ulc_seeker" },
    { "ULC Sheriff",              "player_jacket_ulc_sheriff" },
    { "ULC Splinter Cell",        "player_jacket_ulc_splinter_cell" },
    { "ULC Sport Baseball",       "player_jacket_ulc_sport_baseball" },
    { "ULC Sport Hockey",         "player_jacket_ulc_sport_hockey" },
    { "ULC Sport Racing Driver",  "player_jacket_ulc_sport_racing_driver" },
    { "ULC Sport Snowboard",      "player_jacket_ulc_sport_snowboard" },
    { "ULC Survivor",             "player_jacket_ulc_survivor" },
    { "ULC SW E1",                "player_jacket_ulc_sw_e1" },
    { "ULC SW E2",                "player_jacket_ulc_sw_e2" },
    { "ULC SW W1",                "player_jacket_ulc_sw_w1" },
    { "ULC SW W2",                "player_jacket_ulc_sw_w2" },
    { "ULC SWAT",                 "player_jacket_ulc_swat" },
    { "ULC Tactical Adjudicator", "player_jacket_ulc_tactical_adjudicator" },
    { "ULC Uplay",                "player_jacket_ulc_uplay" },
    { "ULC Upper East 1",         "player_jacket_ulc_upper_east_1" },
    { "ULC Upper East 2",         "player_jacket_ulc_upper_east_2" },
    { "ULC Upper East 3",         "player_jacket_ulc_upper_east_3" },
    { "ULC Upper East 4",         "player_jacket_ulc_upper_east_4" },
};

static const SkinnedMeshManager::ModelSwapEntry s_CosmeticMaskModels_[] =
{
    { "Mask Only (Tox)",      "ch_pm_maskonly_tox" },
    { "Tox 01",               "ch_pm_mask_tox_01" },
    { "Tox 02",               "ch_pm_mask_tox_02" },
    { "Tox 03",               "ch_pm_mask_tox_03" },
    { "Tox 04",               "ch_pm_mask_tox_04" },
    { "Preorder 01",          "ch_pm_mask_preorder01" },
    { "Preorder 02",          "ch_pm_mask_preorder02" },
    { "MM 01",                "ch_pm_mask_mm_01" },
    { "MM 02",                "ch_pm_mask_mm_02" },
    { "MM 03",                "ch_pm_mask_mm_03" },
    { "Hunter",               "ch_pm_mask_hun" },
    { "Hunter 02",            "ch_pm_mask_hun_02" },
    { "Hunter 03",            "ch_pm_mask_hun_03" },
    { "GE3 01",               "ch_pm_mask_ge3_01" },
    { "GE3 02",               "ch_pm_mask_ge3_02" },
    { "GE3 03",               "ch_pm_mask_ge3_03" },
    { "GE4 01",               "ch_pm_mask_ge4_01" },
    { "GE4 02",               "ch_pm_mask_ge4_02" },
    { "GE4 03",               "ch_pm_mask_ge4_03" },
    { "GE5 01",               "ch_pm_mask_ge5_01" },
    { "GE5 02",               "ch_pm_mask_ge5_02" },
    { "GE5 03",               "ch_pm_mask_ge5_03" },
    { "GE6 01",               "ch_pm_mask_ge6_01" },
    { "GE6 02",               "ch_pm_mask_ge6_02" },
    { "GE6 03",               "ch_pm_mask_ge6_03" },
};


// Stub lists — populate the arrays below as you catalog paths per slot.
// The UI also exposes a free-text custom-path field per slot, so an empty list
// just means "no curated entries" — the swap still works via the textbox.
//
// To add entries for, say, hats:
//   static const SkinnedMeshManager::ModelSwapEntry s_hatEntries[] = {
//       { "Bandana",  "rogue/graph objects/gear/ca_cm_h_..." },
//       ...
//   };
//   then change s_hatModels to point at s_hatEntries and set its count.

struct ModelList
{
    const SkinnedMeshManager::ModelSwapEntry* entries;
    int count;
};

#define MK_LIST(arr) { arr, (int)(sizeof(arr) / sizeof((arr)[0])) }

static const ModelList s_backpackModels   = MK_LIST(s_backpackModels_);
static const ModelList s_hatModels        = MK_LIST(s_HatModels_);
static const ModelList s_glovesModels     = MK_LIST(s_GloveModels_);
static const ModelList s_jacketModels     = MK_LIST(s_JacketModels_);
static const ModelList s_shirtModels      = MK_LIST(s_ShirtModels_);
static const ModelList s_chestplateModels = MK_LIST(s_ChestPlateModels_);
static const ModelList s_pantsModels      = MK_LIST(s_PantsModels_);
static const ModelList s_thighModels      = MK_LIST(s_HolsterModels_);
static const ModelList s_feetModels       = MK_LIST(s_FootModels_);
static const ModelList s_scarfModels      = MK_LIST(s_ScarfModels_);
static const ModelList s_kneepadsModels   = MK_LIST(s_KneePadModels_);
static const ModelList s_gasMaskModels    = MK_LIST(s_GasMaskModels_);
static const ModelList s_cosmeticMaskModels = MK_LIST(s_CosmeticMaskModels_);

#undef MK_LIST

// ─── forward decls — Update() needs GetPlayerAppearance which is defined
//      far below the lifecycle block. Re-declaring here keeps Update()
//      self-contained without reshuffling the file.
static TD::AppearanceManager* GetPlayerAppearance(std::string* errOut);

// ─── lifecycle ────────────────────────────────────────────────────────────────

SkinnedMeshManager::SkinnedMeshManager() = default;
SkinnedMeshManager::~SkinnedMeshManager() = default;

// ─── per-slot mod tracking (for soft-replacement / auto-revert) ──────────────

namespace
{
    // Soft-replacement tracking. Apply mutates engine state (writes path,
    // inserts a bucket, pokes m_DirtyFlag). After consumption settles a frame
    // or two later, we snapshot m_AttachHashmap_Count and m_AssetRecords_Count
    // as the "stable baseline." Any subsequent change to either count means
    // the engine has done something on its own (user equipped / modified
    // anything), so we proactively undo our mutations: remove our injected
    // AttachBucket via the engine's own hashmap_remove, and walk m_AssetRecords
    // to drop any Item* whose asset paths still reference our mod path.
    struct SlotModState
    {
        bool         active           = false;
        std::string  modPath;
        int          settleFrames     = 0;     // wait a few frames for engine consumption
        int          baselineHashCt   = -1;
        int          baselineRecordCt = -1;
    };
    static SlotModState s_modState[27];

    constexpr int kSettleFramesBeforeBaseline = 4;

    // Auto-reapply state. g_autoReapply gates the whole feature; s_lastApplied
    // stores the most-recently-applied mod path per slot so we can re-do the
    // Apply if the engine reverts our mutation. Throttled to ~0.5s with a
    // single global timer (one check sweeps all slots).
    static bool        g_autoReapply       = false;
    static std::string s_lastApplied[27];
    static double      s_lastReapplyCheck  = 0.0;

    // Per-slot cooldown after a successful Apply: AutoReapplyOnDrift will
    // refuse to reapply slot N until kReapplyCooldownSec seconds after its
    // last commit. Without this, multi-slot mods oscillate: applying slot
    // 7 triggers a consume pass that briefly knocks slot 0, AutoReapply
    // reapplies slot 0 which knocks slot 7, etc. — flicker. The cooldown
    // gives the engine time to settle on the multi-slot state before we
    // consider the slot drifted.
    static double      s_lastApplyTime[27] = {};
    constexpr double   kReapplyCooldownSec = 1.5;
}

void SkinnedMeshManager::Update()
{
    ScanLiveSlots();
    SoftRevertOnEngineActivity();
    AutoReapplyOnDrift();

    // Pattern A+ orphan-bucket cleanup. After a Pattern A+ injection, if
    // the engine later replaces our wrapper in m_AssetRecords (player
    // UI-equipped something), the AttachHashmap bucket we inserted is
    // left behind and the renderer keeps rendering its mesh alongside
    // the new item. This maintainer detects "our wrapper is gone" and
    // removes the orphan bucket. No-op when there are no tracked
    // injections (the common case).
    {
        std::string err;
        if (TD::AppearanceManager* am = GetPlayerAppearance(&err))
            EquipPipelineProbe::MaintainInjections(am);
    }
}

// SoftRevertOnEngineActivity is defined after GetPlayerAppearance and the
// POD-helper namespace below — it depends on both, so the body has to live
// further down in the translation unit.

// ─── path classification ──────────────────────────────────────────────────────

static bool ContainsCI(const char* hay, const char* needle)
{
    if (!hay || !needle) return false;
    for (; *hay; ++hay)
    {
        const char* h = hay;
        const char* n = needle;
        while (*n && *h && std::tolower((unsigned char)*h) == std::tolower((unsigned char)*n))
            ++h, ++n;
        if (!*n) return true;
    }
    return false;
}

SkinnedMeshManager::GearType SkinnedMeshManager::ClassifyPath(const char* path)
{
    if (!path || !*path) return GearType::Unknown;

    // Special-case prefixes that don't follow ca_<g>_<type> pattern.
    if (ContainsCI(path, "ch_pm_mask")) return GearType::GasMask;
    if (ContainsCI(path, "/ca_hg_") || ContainsCI(path, "/cp_hg_")) return GearType::GasMask;

    // Need at least "ca_<g>_<type>" — extract everything after the
    // gender token. The prefix from ExtractGearPrefix is e.g. "ca",
    // so we need the next-next token.
    const char* gear = std::strstr(path, "gear/");
    const char* base = gear ? gear + 5 : path;

    // tokens[0] = "ca", tokens[1] = "cm"/"cf", tokens[2] = body type letter(s)
    char tok[3][16] = {};
    int t = 0;
    int i = 0;
    while (*base && t < 3)
    {
        if (*base == '_' || *base == '.')
        {
            tok[t][i] = '\0';
            ++t;
            i = 0;
            if (*base == '.') break;
        }
        else if (i + 1 < (int)sizeof(tok[t]))
        {
            tok[t][i++] = (char)std::tolower((unsigned char)*base);
        }
        ++base;
    }
    if (t < 3) return GearType::Unknown;

    const char* type = tok[2];

    if (std::strcmp(type, "b")  == 0) return GearType::Backpack;
    if (std::strcmp(type, "l1") == 0) return GearType::Shirt;
    if (std::strcmp(type, "l2") == 0) return GearType::Chestplate;
    if (std::strcmp(type, "l3") == 0) return GearType::Jacket;
    if (std::strcmp(type, "p")  == 0) return GearType::Pants;
    if (std::strcmp(type, "t")  == 0) return GearType::Thigh;
    if (std::strcmp(type, "f")  == 0) return GearType::Feet;
    if (std::strcmp(type, "s")  == 0) return GearType::Scarf;
    if (std::strcmp(type, "k")  == 0) return GearType::Kneepads;
    if (std::strcmp(type, "h")  == 0)
    {
        // Hand/glove sub-classification: paths like "ca_cm_h_gv_st" or names containing "gloves"
        if (ContainsCI(path, "_gv_") || ContainsCI(path, "gloves")) return GearType::Gloves;
        return GearType::Hat;
    }
    if (std::strcmp(type, "hg") == 0) return GearType::GasMask;

    return GearType::Unknown;
}

const char* SkinnedMeshManager::GearTypeName(GearType t)
{
    switch (t)
    {
    case GearType::Backpack:   return "Backpack";
    case GearType::Shirt:      return "Shirt (L1)";
    case GearType::Chestplate: return "Chestplate (L2)";
    case GearType::Jacket:     return "Jacket (L3)";
    case GearType::Pants:      return "Pants";
    case GearType::Thigh:      return "Thigh Holster";
    case GearType::Feet:       return "Shoes / Boots";
    case GearType::Scarf:      return "Scarf";
    case GearType::Kneepads:   return "Kneepads";
    case GearType::Hat:        return "Hat";
    case GearType::Gloves:     return "Gloves";
    case GearType::GasMask:    return "Gas Mask";
    case GearType::CosmeticMask: return "Cosmetic Mask";
    default:                   return "Unknown";
    }
}

// Locked-down per-slot mapping. Verified by live observation: each index
// in m_Clothes[27] is consistently the same body part / outfit role across
// characters and sessions. Slots 0-5 are the armor-functional layer; slots
// 6-12 are the vanity layer. Indices 13..26 are unused by the equip pipeline
// on this build and surface as Unknown (kept in the UI for diagnostics).
SkinnedMeshManager::GearType SkinnedMeshManager::SlotGearType(int slotIndex)
{
    switch (slotIndex)
    {
    case 0:  return GearType::Backpack;
    case 1:  return GearType::Chestplate;     // L2
    case 2:  return GearType::GasMask;
    case 3:  return GearType::Gloves;
    case 4:  return GearType::Kneepads;
    case 5:  return GearType::Thigh;          // holster / thigh bag
    case 6:  return GearType::Hat;
    case 7:  return GearType::Jacket;         // L3
    case 8:  return GearType::Pants;
    case 9:  return GearType::Scarf;
    case 10: return GearType::Shirt;          // L1
    case 11: return GearType::Feet;           // shoes / boots
    case 12: return GearType::CosmeticMask;
    default: return GearType::Unknown;
    }
}

// ─── POD-only helpers (SEH allowed; no C++ destructors in scope) ────────────

// Forward declaration: FindPlayerAgent is defined further down at file scope,
// but the GatherDiagInfo helper inside the anonymous namespace below needs to
// see it. Static is fine — same translation unit.
// (GetPlayerAppearance forward declaration lives earlier in the file —
//  near the lifecycle block — because Update() also needs it.)
static TD::Agent* FindPlayerAgent(TD::World* world, int* outFoundIdx);

namespace
{
    struct RawSlotInfo
    {
        bool          valid;
        bool          isHeap;
        std::uint32_t cap;
        char          path[260];   // copied out of game memory before C++ string handling
    };

    // Reads all 27 m_Clothes entries into a POD array. Returns false if the read
    // hit an access violation (player likely despawning) — caller treats as scan failure.
    //
    // NOTE: A slot is considered "populated" if it has a non-empty m_Path. We do
    // NOT require slot.m_pSlot to be non-null — during in-game equipment /
    // customization menus the engine briefly clears m_pSlot but the cached path
    // stays intact, and we want the UI (and override drift detection) to keep
    // working through that window.
    bool ReadAllSlotsGuarded(TD::AppearanceManager* am, RawSlotInfo* out)
    {
        std::memset(out, 0, sizeof(RawSlotInfo) * 27);
        __try
        {
            for (int i = 0; i < 27; ++i)
            {
                const auto& slot = am->m_Clothes[i];

                const BYTE* b = slot.m_Path.bytes;
                bool isHeap = b[0x0F] != 0;
                out[i].isHeap = isHeap;

                if (isHeap)
                {
                    const char* heap = *(const char* const*)b;
                    if (!heap) continue;
                    std::uint32_t cap = *(const std::uint32_t*)(heap - 4);
                    if (cap == 0 || cap > 0x1000) continue;
                    std::size_t copy = (cap < sizeof(out[i].path) - 1) ? cap : sizeof(out[i].path) - 1;
                    std::memcpy(out[i].path, heap, copy);
                    out[i].path[copy] = '\0';
                    out[i].cap   = cap;
                    // Mark as valid/mutatable as long as we have a heap allocation,
                    // even if the engine has NUL'd the first byte (Character_ApplyClothingId
                    // does that to "clear" a slot — the allocation survives so we can
                    // write our own path back into it).
                    out[i].valid = true;
                }
                else
                {
                    std::memcpy(out[i].path, b, 15);
                    out[i].path[15] = '\0';
                    out[i].cap   = 0;
                    out[i].valid = (out[i].path[0] != '\0');
                }
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }
        return true;
    }

    // VirtualProtect + memcpy + memset, guarded against AV. Restores page
    // protection in all paths. Returns true on a successful copy.
    bool GuardedHeapWrite(char* dst, const char* src, std::size_t srcLen, std::uint32_t capacity)
    {
        DWORD oldProt = 0;
        if (!VirtualProtect(dst, capacity, PAGE_READWRITE, &oldProt)) return false;
        bool ok = true;
        __try
        {
            std::memcpy(dst, src, srcLen + 1);
            if (srcLen + 1 < capacity)
                std::memset(dst + srcLen + 1, 0, capacity - srcLen - 1);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            ok = false;
        }
        DWORD tmp;
        VirtualProtect(dst, capacity, oldProt, &tmp);
        return ok;
    }

    // Walks m_AttachHashmap_Buckets and rewrites m_ClothingId on every bucket
    // matching slotId, replacing it with a sentinel value that the engine's
    // per-slot processing won't match. Used to suppress the original bag's
    // bucket before we insert our own — without this both Item*s end up in
    // m_AssetRecords and both meshes render ("two bags" symptom).
    //
    // We deliberately do NOT mutate m_ModelPath or remove the bucket from the
    // hashmap. m_ClothingId is at offset 0x3C, outside the hashmap key, so the
    // entry's hash stays valid and later lookups / inserts / Character_ApplyClothingId
    // removals still operate correctly. The disqualified bucket leaks (lives on
    // until character despawn), which is acceptable.
    constexpr std::uint32_t kClothingIdSuppressed = 0xFFFFFFFFu;
    int DisqualifyBucketsForSlot(TD::AppearanceManager* am, int slotId)
    {
        int n = 0;
        __try
        {
            auto* buckets = am->m_AttachHashmap_Buckets;
            int count = am->m_AttachHashmap_Count;
            if (!buckets || count <= 0) return 0;

            for (int i = 0; i < count; ++i)
            {
                if (buckets[i].m_ClothingId == (std::uint32_t)slotId)
                {
                    buckets[i].m_ClothingId = kClothingIdSuppressed;
                    ++n;
                }
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            // return whatever we got before the AV
        }
        return n;
    }

    // Reads a SnowdropString at `sstr` (16 bytes) into outPath. Heap-mode
    // dereferences the pointer at +0; inline-mode reads the bytes directly.
    bool ReadSnowdropStringAt(const BYTE* sstr, char* outPath, std::size_t outSize)
    {
        if (!sstr || !outPath || outSize == 0) return false;
        outPath[0] = '\0';
        __try
        {
            const char* path = nullptr;
            if (sstr[0x0F] == 0)
            {
                path = (const char*)sstr;
            }
            else
            {
                path = *(const char* const*)sstr;
                if (!path) return false;
            }
            std::size_t i = 0;
            while (i + 1 < outSize && path[i]) { outPath[i] = path[i]; ++i; }
            outPath[i] = '\0';
            return outPath[0] != '\0';
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }
    }

    // Item base class has its asset paths spread across 8 explicit
    // SnowdropStrings (+24, +128, +368, +384, +400, +416, +600, +616) plus 3
    // AgentAssetRef structs whose first SnowdropString lives at +192, +256, +320
    // (AgentAssetRef base offset + 16). Different subclass tags populate
    // different fields, so we have to scan every candidate location.
    bool ItemContainsPath(void* itemPtr, const char* targetPath)
    {
        if (!itemPtr || !targetPath) return false;
        static const int kPathOffsets[] = {
             24, 128, 368, 384, 400, 416, 600, 616,
            192, 216, 256, 280, 320, 344,
        };
        char path[260];
        for (int off : kPathOffsets)
        {
            if (!ReadSnowdropStringAt((const BYTE*)itemPtr + off, path, sizeof(path)))
                continue;
            if (_stricmp(path, targetPath) == 0) return true;
        }
        return false;
    }

    // Walks m_AssetRecords looking for an Item* containing targetPath in any of
    // its candidate path slots. Returns the index, or -1 if not found.
    int FindAssetRecordByPath(TD::AppearanceManager* am, const char* targetPath)
    {
        if (!am || !targetPath) return -1;
        __try
        {
            void** arr = am->m_AssetRecords_Ptr;
            int count = am->m_AssetRecords_Count;
            if (!arr || count <= 0) return -1;

            for (int i = 0; i < count; ++i)
                if (ItemContainsPath(arr[i], targetPath)) return i;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
        return -1;
    }

    // Returns the index of `itemPtr` in m_AssetRecords, or -1 if not present.
    int FindAssetRecordByPtr(TD::AppearanceManager* am, void* itemPtr)
    {
        if (!am || !itemPtr) return -1;
        __try
        {
            void** arr = am->m_AssetRecords_Ptr;
            int count = am->m_AssetRecords_Count;
            if (!arr || count <= 0) return -1;
            for (int i = 0; i < count; ++i)
                if (arr[i] == itemPtr) return i;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
        return -1;
    }

    // Removes m_AssetRecords[index] by shifting subsequent entries down and
    // decrementing the count. Does NOT call the Item dtor — the Item* is left
    // unreferenced (small leak), which is far safer than guessing the engine's
    // ref-counting / virtual destructor convention from outside.
    bool RemoveAssetRecordAt(TD::AppearanceManager* am, int index)
    {
        if (!am || index < 0) return false;
        __try
        {
            void** arr = am->m_AssetRecords_Ptr;
            int count = am->m_AssetRecords_Count;
            if (!arr || index >= count) return false;

            for (int i = index; i < count - 1; ++i)
                arr[i] = arr[i + 1];
            arr[count - 1] = nullptr;
            am->m_AssetRecords_Count = count - 1;
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }
    }

    // POD helper: read m_AttachHashmap_Count and m_AssetRecords_Count under
    // a single SEH guard. Lives in this anonymous namespace so the C++-aware
    // SoftRevertOnEngineActivity (which holds std::string state) can call it
    // without tripping MSVC C2712.
    struct CountSnapshot
    {
        bool ok;
        int  hashCt;
        int  recordCt;
    };

    CountSnapshot ReadCountsGuarded(TD::AppearanceManager* am)
    {
        CountSnapshot s{};
        s.ok = false;
        if (!am) return s;
        __try
        {
            s.hashCt   = am->m_AttachHashmap_Count;
            s.recordCt = am->m_AssetRecords_Count;
            s.ok       = true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            s.ok = false;
        }
        return s;
    }

    // Engine's AttachHashmap remove (sub_1650620). Cleanly drops the bucket
    // whose m_ModelPath equals `path`. Used in SoftRevert to undo our injected
    // bucket once we detect any engine activity. Verified via decompile of
    // sub_16679B0 — that's exactly how Character_ApplyClothingId removes
    // matching buckets internally.
    bool CallHashmapRemoveGuarded(TD::AppearanceManager* am, const char* path)
    {
        typedef __int64 (__fastcall *PFN)(void* hashmap, const char* path);
        PFN fn = (PFN)(g_pBase + 0x1650620);
        __try
        {
            fn((void*)((__int64)am + 0x18), path);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }
        return true;
    }

    // Engine's per-slot reset / "apply clothing id" (sub_16679B0). Removes
    // stale AttachBuckets where m_ClothingId == id and ref-drops any Item*
    // currently bound to that slot in m_AssetRecords. Used as a clean reset
    // before we install our own bucket — without it, the original bag's
    // Item* stays in m_AssetRecords and renders alongside ours ("two bags").
    bool CallApplyClothingIdGuarded(TD::AppearanceManager* am, std::uint32_t slotId)
    {
        typedef __int64 (__fastcall *PFN)(TD::AppearanceManager*, std::uint32_t*);
        PFN fn = (PFN)(g_pBase + 0x16679B0);
        __try
        {
            fn(am, &slotId);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }
        return true;
    }

    // Engine's "model load trigger" (sub_162FDA0). Properly inserts a new
    // AttachBucket into m_AttachHashmap via sub_1544E60, with m_SlotName /
    // m_ClothingId filled in. Using the engine's insert keeps the hashmap key
    // hash and the entry array consistent (in-place mutation of an existing
    // bucket's m_ModelPath corrupts the key hash and crashes the in-game UI
    // when it later tries to remove or look up the bucket).
    bool CallModelLoadTriggerGuarded(TD::AppearanceManager* am,
                                     TD::SnowdropString* path,
                                     std::uint32_t slotId)
    {
        typedef __int64 (__fastcall *PFN)(TD::AppearanceManager*, TD::SnowdropString*, std::uint32_t*);
        PFN fn = (PFN)(g_pBase + 0x162FDA0);
        __try
        {
            fn(am, path, &slotId);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }
        return true;
    }

    // Engine's SnowdropString assign function (sub_116830). Reallocates the
    // string's heap buffer if the new content is longer than the existing
    // capacity, using the engine's own allocator — which is the only safe way
    // to grow a SnowdropString without a free-time allocator-mismatch crash.
    // Used as the fallback when the new path doesn't fit in the current cap.
    bool CallStringAssignGuarded(TD::SnowdropString* str, const char* newPath)
    {
        typedef void (__fastcall *PFN)(TD::SnowdropString*, const char*);
        PFN fn = (PFN)(g_pBase + 0x116830);
        __try
        {
            fn(str, newPath);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }
        return true;
    }

    // Engine's `sync_character_clothing_and_accessories` (sub_16083F0). Clones
    // the current AppearanceManager state into the staging arrays
    // (m_PendingAttach, secondary slot table, m_DynArrayA, m_PathStrings2[3])
    // and sets m_NeedsResync=1. SetClothingIdList calls this first when
    // m_AssetRecords_Count==0 — we mirror that gate so our pipeline matches
    // the engine's ordering exactly.
    bool CallSyncGuarded(TD::AppearanceManager* am)
    {
        typedef __int64 (__fastcall *PFN)(TD::AppearanceManager*);
        PFN fn = (PFN)(g_pBase + 0x16083F0);
        __try
        {
            fn(am);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }
        return true;
    }

    // POD-only diagnostic snapshot. SEH-guarded reads of agent fields live here
    // so DrawUI doesn't have to host __try (it has C++ destructors all over).
    struct DiagInfo
    {
        int                    agentCount;
        int                    playerIdx;
        TD::Agent*             player;
        int                    playerType;
        TD::AppearanceManager* am;
    };

    void GatherDiagInfo(DiagInfo* out)
    {
        out->agentCount = 0;
        out->playerIdx  = -1;
        out->player     = nullptr;
        out->playerType = -1;
        out->am         = nullptr;

        auto* rc = TD::RogueClient::Singleton();
        if (!rc) return;
        auto* client = rc->m_pClient;
        if (!client) return;
        auto* world = client->m_pWorld;
        if (!world || !world->m_AgentArray) return;

        out->agentCount = world->m_AgentCount;
        out->player     = FindPlayerAgent(world, &out->playerIdx);
        if (!out->player) return;

        __try { out->playerType = *(int*)((__int64)out->player + 0x3A4); }
        __except (EXCEPTION_EXECUTE_HANDLER) { out->playerType = -2; }

        out->am = out->player->m_pAppearance;
    }

}

// ─── live slot scanning ──────────────────────────────────────────────────────

// Locates the player Agent by walking m_AgentArray and matching EntityType==1.
// During in-game customization/equipment menus the engine can shuffle agent
// indices (or temporarily despawn the player), so we can't rely on Agent[0].
// Falls back to Agent[0] only if no EntityType==1 agent is found.
static TD::Agent* FindPlayerAgent(TD::World* world, int* outFoundIdx)
{
    if (outFoundIdx) *outFoundIdx = -1;
    if (!world || !world->m_AgentArray || world->m_AgentCount <= 0) return nullptr;

    for (int i = 0; i < world->m_AgentCount; ++i)
    {
        auto* a = world->m_AgentArray[i];
        if (!a) continue;
        int type = 0;
        __try { type = *(int*)((__int64)a + 0x3A4); }
        __except (EXCEPTION_EXECUTE_HANDLER) { continue; }
        if (type == 1) { if (outFoundIdx) *outFoundIdx = i; return a; }
    }
    // Fallback — slot 0 even if not type 1 (caller decides if they want it)
    return world->m_AgentArray[0];
}

static TD::AppearanceManager* GetPlayerAppearance(std::string* errOut)
{
    auto* rc = TD::RogueClient::Singleton();
    if (!rc)     { if (errOut) *errOut = "RogueClient null";         return nullptr; }
    auto* client = rc->m_pClient;
    if (!client) { if (errOut) *errOut = "Client null";              return nullptr; }
    auto* world  = client->m_pWorld;
    if (!world)  { if (errOut) *errOut = "World null";               return nullptr; }
    if (!world->m_AgentArray || world->m_AgentCount <= 0)
                 { if (errOut) *errOut = "agent array empty";        return nullptr; }

    int playerIdx = -1;
    TD::Agent* player = FindPlayerAgent(world, &playerIdx);
    if (!player) { if (errOut) *errOut = "no agents in array";       return nullptr; }
    int type = 0;
    __try { type = *(int*)((__int64)player + 0x3A4); }
    __except (EXCEPTION_EXECUTE_HANDLER) { type = 0; }

    if (type != 1)
    {
        if (errOut)
        {
            char buf[160];
            std::snprintf(buf, sizeof(buf),
                          "no player (type=1) found in %d agents (agent[0].type=%d)",
                          world->m_AgentCount, type);
            *errOut = buf;
        }
        return nullptr;
    }

    auto* am = player->m_pAppearance;
    if (!am)     { if (errOut) *errOut = "AppearanceManager null";   return nullptr; }
    return am;
}

void SkinnedMeshManager::SoftRevertOnEngineActivity()
{
    auto* am = GetPlayerAppearance(nullptr);
    if (!am) return;

    for (int slotIdx = 0; slotIdx < 27; ++slotIdx)
    {
        auto& st = s_modState[slotIdx];
        if (!st.active) continue;

        // Read current counts via the POD helper (SEH lives there — this
        // function holds a std::string in s_modState so it can't host __try).
        CountSnapshot snap = ReadCountsGuarded(am);
        if (!snap.ok) continue;
        int curHashCt   = snap.hashCt;
        int curRecordCt = snap.recordCt;

        // Wait kSettleFramesBeforeBaseline frames for the engine to consume
        // the m_DirtyFlag we set in Apply. Once consumption is done, the
        // hashmap and asset-records counts reflect "our mod is installed,
        // engine is at rest." Snapshot those as the baseline.
        if (st.baselineHashCt < 0)
        {
            if (++st.settleFrames < kSettleFramesBeforeBaseline) continue;
            st.baselineHashCt   = curHashCt;
            st.baselineRecordCt = curRecordCt;
            continue;
        }

        // Any change from baseline means the engine ran some pipeline (user
        // equipped, customized colors, opened/closed a menu, etc.). Soft
        // revert: drop our injected bucket and any Item* still bound to our
        // mod path. The engine's own state (whatever the user just did) is
        // already there — by removing ours, only the engine's stays.
        if (curHashCt != st.baselineHashCt || curRecordCt != st.baselineRecordCt)
        {
            // 1. Remove our bucket via engine's hashmap_remove (sub_1650620).
            //    Uses the same code path Character_ApplyClothingId uses, so
            //    hashmap integrity is preserved.
            CallHashmapRemoveGuarded(am, st.modPath.c_str());

            // 2. Drop any Item* in m_AssetRecords that contains our mod path
            //    in any of its asset slots. May find more than one if the
            //    engine pushed multiples; loop until none remain.
            for (int safety = 0; safety < 8; ++safety)
            {
                int idx = FindAssetRecordByPath(am, st.modPath.c_str());
                if (idx < 0) break;
                if (!RemoveAssetRecordAt(am, idx)) break;
            }

            st = {};   // mark inactive, clear all state
        }
    }
}

void SkinnedMeshManager::AutoReapplyOnDrift()
{
    if (!g_autoReapply) return;

    // Throttle to ~0.1s. ImGui::GetTime is just the display-frame clock, but
    // it's monotonic and cheap, so it's fine for this.
    double now = ImGui::GetTime();
    if (now - s_lastReapplyCheck < 0.1) return;
    s_lastReapplyCheck = now;

    for (int slotIdx = 0; slotIdx < 27; ++slotIdx)
    {
        const std::string& want = s_lastApplied[slotIdx];
        if (want.empty()) continue;

        // Look up current path from the live slot scan.
        const LiveSlot* live = nullptr;
        for (const auto& ls : m_slots)
            if (ls.index == slotIdx) { live = &ls; break; }
        if (!live) continue;

        // No drift → don't disturb. The whole point of this pass is to fix up
        // what the engine has reverted, not to keep poking healthy state.
        if (live->currentPath == want) continue;

        // Cooldown: applying a slot triggers a consume pass that briefly
        // perturbs every other slot. Without this gate, a fresh manual
        // Apply on slot B would immediately re-trigger AutoReapply on
        // slot A (transient drift), which would commit again and perturb
        // slot B again — flicker. Skip slots whose last Apply is still
        // inside the cooldown window; if the drift is real and persistent,
        // it'll get picked up after the window expires.
        if (now - s_lastApplyTime[slotIdx] < kReapplyCooldownSec) continue;

        // Drifted: re-apply. ApplyDirectSwap re-installs everything (path,
        // bucket, dirty flag) and re-arms the SoftRevert tracking for the
        // next engine event. Reinforce-step inside Apply also rewrites
        // any other active mods so they survive this consume pass.
        ApplyDirectSwap(slotIdx, want.c_str(), nullptr);
    }
}

void SkinnedMeshManager::ScanLiveSlots()
{
    // The engine's character/customization menu briefly nulls every slot's
    // m_pSlot while it swaps in a preview character. Naïve scan during that
    // window shows "no populated slots" and looks like everything got
    // destroyed. Strategy: only replace m_slots when we get a populated
    // result. If the scan comes back empty, keep the previous (now stale)
    // m_slots for up to ~5 seconds before giving up.
    //
    // Counter is a function-local static so we don't depend on whether
    // any specific class field is visible to the precompiled header.
    static int s_emptyScans = 0;
    constexpr int kMaxEmptyScansBeforeGiveUp = 300;   // ~5s @ 60 fps

    std::string thisScanError;
    TD::AppearanceManager* am = GetPlayerAppearance(&thisScanError);
    if (!am)
    {
        ++s_emptyScans;
        if (s_emptyScans > kMaxEmptyScansBeforeGiveUp)
        {
            m_slots.clear();
            m_scanError = thisScanError;
        }
        else if (!m_slots.empty())
        {
            m_scanError = "(showing last good scan — " + thisScanError + ")";
        }
        return;
    }

    RawSlotInfo raw[27];
    if (!ReadAllSlotsGuarded(am, raw))
    {
        ++s_emptyScans;
        if (s_emptyScans > kMaxEmptyScansBeforeGiveUp)
        {
            m_slots.clear();
            m_scanError = "exception while reading slot table (player despawning)";
        }
        else if (!m_slots.empty())
        {
            m_scanError = "(showing last good scan — read AV)";
        }
        return;
    }

    // Always emit one LiveSlot per index 0..26. Slot type comes from the
    // locked-down SlotGearType table (verified deterministic on this build) —
    // ClassifyPath is no longer consulted, since the slot index alone is
    // authoritative. Empty / inline-empty slots are still emitted so the UI
    // shows the full clothing layout (and the Unknown 13..26 tail) instead
    // of hiding rows where the engine cleared a path.
    std::vector<LiveSlot> newSlots;
    newSlots.reserve(27);
    bool anyPopulated = false;
    for (int i = 0; i < 27; ++i)
    {
        LiveSlot ls;
        ls.index       = i;
        ls.type        = SlotGearType(i);
        ls.currentPath = raw[i].valid ? raw[i].path : "";
        ls.capacity    = raw[i].cap;
        ls.canMutate   = raw[i].isHeap && raw[i].cap > 0;
        if (raw[i].valid) anyPopulated = true;
        newSlots.push_back(std::move(ls));
    }

    if (!anyPopulated)
    {
        // Transient: engine is mid-equip / in customization preview.
        ++s_emptyScans;
        if (s_emptyScans > kMaxEmptyScansBeforeGiveUp)
        {
            m_slots.clear();
            m_scanError = "no populated slots";
        }
        else if (!m_slots.empty())
        {
            m_scanError = "(showing last good scan — engine mid-update)";
        }
        else
        {
            m_scanError = "no populated slots";
        }
        return;
    }

    // Got a fresh populated scan — replace cache, clear transient counter.
    m_slots      = std::move(newSlots);
    m_scanError.clear();
    s_emptyScans = 0;
}

// ─── direct in-place mutation ────────────────────────────────────────────────

// Rewrites our cached mod onto every OTHER active slot so the engine's
// upcoming consume pass (triggered by m_DirtyFlag) sees ALL our paths in
// place — not just the slot we're currently applying. Without this, the
// consume pass refreshes m_Clothes from staging and silently reverts our
// previously-applied mods, producing the "flicker between slots" symptom
// when several mods are active.
//
// Heavy reset (CallApplyClothingIdGuarded + drop-old-Item*) is deliberately
// skipped — those steps are only needed for a slot whose ORIGINAL bag is
// still bound. After a successful Apply, the slot's Item* is already our
// modded one; a straight path + bucket rewrite is enough to keep it.
//
// Returns the number of slots reinforced (for diagnostics).
static int ReinforceOtherActiveMods(TD::AppearanceManager* am, int exceptSlot)
{
    int reinforced = 0;
    for (int i = 0; i < 27; ++i)
    {
        if (i == exceptSlot) continue;
        if (!s_modState[i].active) continue;
        const std::string& want = s_lastApplied[i];
        if (want.empty()) continue;

        BYTE* sstr = am->m_Clothes[i].m_Path.bytes;

        // Compare current to want — skip rewrite if path is already correct.
        char cur[260] = {};
        if (!ReadSnowdropStringAt(sstr, cur, sizeof(cur))) continue;
        if (std::strcmp(cur, want.c_str()) == 0) continue;

        // Rewrite path: fast in-place if existing heap capacity allows,
        // else engine-allocator-correct assign.
        std::size_t newLen = want.size();
        bool wroteOk = false;
        if (sstr[0x0F] != 0)
        {
            char* heapStr = *(char**)sstr;
            if (heapStr)
            {
                std::uint32_t cap = *(std::uint32_t*)(heapStr - 4);
                if (cap > 0 && cap <= 0x1000 && newLen + 1 <= cap)
                    wroteOk = GuardedHeapWrite(heapStr, want.c_str(), newLen, cap);
            }
        }
        if (!wroteOk)
            wroteOk = CallStringAssignGuarded(&am->m_Clothes[i].m_Path, want.c_str());
        if (!wroteOk) continue;

        // Re-insert the AttachBucket so the consume pass binds the model.
        CallModelLoadTriggerGuarded(am, &am->m_Clothes[i].m_Path, (std::uint32_t)i);
        ++reinforced;
    }
    return reinforced;
}

bool SkinnedMeshManager::ApplyDirectSwap(int slotIndex, const char* newPath,
                                        std::string* errOut)
{
    if (slotIndex < 0 || slotIndex >= 27) { if (errOut) *errOut = "slot index out of range"; return false; }
    if (!newPath || !*newPath)            { if (errOut) *errOut = "empty target path";       return false; }

    TD::AppearanceManager* am = GetPlayerAppearance(errOut);
    if (!am) return false;

    auto& slot = am->m_Clothes[slotIndex];

    // Mirrors the engine's Character_SetClothingIdList (sub_162DD60) almost
    // line-for-line, with our path injected between the per-slot reset and
    // the dirty-flag commit so the consume pipeline picks up our model:
    //
    //   sub_162DD60 does:
    //     1. m_ListUpdated = 1                       (set FIRST)
    //     2. if (m_AssetRecords_Count == 0) sync(am) (sub_16083F0)
    //     3. ApplyClothingId per id in list
    //     4. copy new list into m_ClothingIdList
    //     5. m_DirtyFlag = 1                         (set LAST)
    //
    //   We additionally:
    //     • Snapshot the OLD path before reset and drop the old Item* from
    //       m_AssetRecords ourselves. The engine's pipeline drops the old
    //       Item* via the m_ClothingIdList "what changed" channel, but we
    //       can't update that list cleanly without breaking other slots —
    //       so we replicate the side-effect directly.
    //     • Inject our path into m_Clothes[slotIndex].m_Path between steps
    //       3 and 5, then call ModelLoadTrigger (sub_162FDA0) to insert the
    //       attach bucket for it. The consume pass on the next frame loads
    //       the model, the factory (sub_F48FE0) creates the new Item*, and
    //       the slot ends up in the same end-state as a real in-game equip:
    //       exactly one Item* in m_AssetRecords for that slot.

    // 0. Snapshot the OLD path so we can drop the OLD Item* by path match.
    char oldPath[260] = {};
    ReadSnowdropStringAt(slot.m_Path.bytes, oldPath, sizeof(oldPath));

    // 1. m_ListUpdated FIRST — matches sub_162DD60's ordering. The consume
    //    pipeline checks both flags; setting ListUpdated before any state
    //    mutation keeps the engine's invariant ("list is dirty") true the
    //    entire time we're rewriting state.
    am->m_ListUpdated = 1;

    // 2. Sync gate — same condition as sub_162DD60: if no asset records exist
    //    yet, run sync_character_clothing_and_accessories first. Sync clones
    //    the current state into the staging arrays AND sets m_NeedsResync.
    //    On a populated player this no-ops; on a freshly-loaded character it
    //    seeds the staging arrays the consume pass needs.
    bool syncCalled = false;
    {
        CountSnapshot pre = ReadCountsGuarded(am);
        if (pre.ok && pre.recordCt == 0)
            syncCalled = CallSyncGuarded(am);
    }

    // 3. Per-slot reset via engine API. Clears m_pSlot, clears m_Path to "",
    //    and removes attach buckets where m_ClothingId == slotIndex.
    bool slotReset = CallApplyClothingIdGuarded(am, (std::uint32_t)slotIndex);

    // 3a. Belt-and-braces: disqualify any stragglers ApplyClothingId didn't
    //     catch (the +0x28 hashmap quirk on this build can leave residue).
    int disqualified = DisqualifyBucketsForSlot(am, slotIndex);

    // 3b. Drop any old Item* in m_AssetRecords matching the OLD path. The
    //     engine's consume normally does this via m_ClothingIdList semantics;
    //     we replicate the effect so the new Item* the factory creates next
    //     frame ends up alongside no leftover bag.
    int oldItemsRemoved = 0;
    if (oldPath[0])
    {
        for (int safety = 0; safety < 8; ++safety)
        {
            int idx = FindAssetRecordByPath(am, oldPath);
            if (idx < 0) break;
            if (!RemoveAssetRecordAt(am, idx)) break;
            ++oldItemsRemoved;
        }
    }

    // 4. Inject our path. Re-read sstr because the reset above may have
    //    reallocated the heap buffer (its "writes cached path" step).
    BYTE* sstr = slot.m_Path.bytes;
    std::size_t newLen = std::strlen(newPath);

    // Two-path write strategy:
    //   a) Existing heap allocation has enough capacity → fast
    //      VirtualProtect + memcpy (no engine-side state changes).
    //   b) Otherwise → engine's own SnowdropString::assign (sub_116830),
    //      which reallocates with the engine's allocator (allocator-correct
    //      so any later free by the engine is safe).
    bool wroteOk = false;
    bool usedEngineAssign = false;

    if (sstr[0x0F] != 0)
    {
        char* heapStr = *(char**)sstr;
        if (heapStr)
        {
            std::uint32_t capacity = *(std::uint32_t*)(heapStr - 4);
            if (capacity > 0 && capacity <= 0x1000 && newLen + 1 <= capacity)
                wroteOk = GuardedHeapWrite(heapStr, newPath, newLen, capacity);
        }
    }

    if (!wroteOk)
    {
        usedEngineAssign = CallStringAssignGuarded(&slot.m_Path, newPath);
        wroteOk = usedEngineAssign;
    }

    if (!wroteOk)
    {
        if (errOut) *errOut = "path write failed (both fast-path and engine assign)";
        return false;
    }

    // 5. Insert a properly-keyed AttachBucket for our new path via the engine's
    //    own model-load trigger (sub_162FDA0 → sub_1544E60 hashmap_insert).
    //    The bucket carries m_SlotName + m_ClothingId so the consume pipeline
    //    knows which slot the loaded model belongs to.
    bool bucketInserted = CallModelLoadTriggerGuarded(am, &slot.m_Path,
                                                      (std::uint32_t)slotIndex);

    // 5b. Reinforce every other still-active mod before flipping DirtyFlag.
    //     This is the multi-slot fix: the consume pass refreshes m_Clothes
    //     from staging, and if our other modded paths aren't present at that
    //     moment they get reverted to vanilla — producing flicker as each
    //     manual Apply or auto-reapply knocks the previously-modded slots.
    //     Rewriting all active mods here ensures the consume sees the full
    //     outfit in one shot.
    int reinforced = ReinforceOtherActiveMods(am, slotIndex);

    // 5a. If sync didn't run above, set m_NeedsResync manually. Sync sets it;
    //     when we skip sync the consume pass still wants the resync signal so
    //     the visual fully refreshes.
    if (!syncCalled) am->m_NeedsResync = 1;

    // 6. m_DirtyFlag LAST — matches sub_162DD60. The engine's consume pass
    //    on the next frame: reads dirty/listupdated → walks m_Clothes for
    //    populated paths → loads model from our bucket → factory creates
    //    the right Item subclass → pushes it into m_AssetRecords.
    am->m_DirtyFlag = 1;

    // Register / reset per-slot mod tracking (same as before).
    {
        auto& st = s_modState[slotIndex];
        st = {};
        st.active  = true;
        st.modPath = newPath;
        s_lastApplied[slotIndex] = newPath;
        s_lastApplyTime[slotIndex] = ImGui::GetTime();
    }

    // Re-baseline every OTHER active mod's SoftRevert tracking. Applying
    // this slot just changed m_AttachHashmap_Count / m_AssetRecords_Count
    // globally (new bucket + new Item* on the consume pass), so the stale
    // baselines on the other s_modState entries would otherwise read as
    // "engine activity" on the next SoftRevert tick and trigger an
    // unwanted teardown of those mods. Resetting baselineHashCt = -1
    // forces a fresh snapshot after the kSettleFramesBeforeBaseline wait.
    for (int i = 0; i < 27; ++i)
    {
        if (i == slotIndex) continue;
        if (!s_modState[i].active) continue;
        s_modState[i].settleFrames     = 0;
        s_modState[i].baselineHashCt   = -1;
        s_modState[i].baselineRecordCt = -1;
    }

    if (errOut)
    {
        char buf[260];
        std::snprintf(buf, sizeof(buf),
                      "ok (reset:%s sync:%s old_items:%d disq:%d %s %s reinforce:%d)",
                      slotReset        ? "y" : "n",
                      syncCalled       ? "y" : "skip",
                      oldItemsRemoved,
                      disqualified,
                      usedEngineAssign ? "engine-assign" : "fast-memcpy",
                      bucketInserted   ? "bucket+"       : "bucket-FAIL",
                      reinforced);
        *errOut = buf;
    }
    return true;
}

// ── ApplyEquipByName: production descriptor-bound equip ─────────────────────
// Thin wrapper over EquipPipelineProbe::RunEquipTestPatternAPlus(name, true, …).
// The probe is the algorithm — see .claude/docs/06-inventory-equip-pipeline.md
// "The shipping algorithm (verified 2026-05-12)" for the full recipe and the
// "why each step matters" table.
//
// Returns false if mitemName is empty, not in InventoryConfig, or the engine
// call AVs. Caller may then fall back to ApplyDirectSwap for legacy path-only
// behavior on items the cache doesn't know about (custom paths, mods, etc.).
bool SkinnedMeshManager::ApplyEquipByName(int slotIndex, const char* mitemName,
                                          std::string* errOut)
{
    if (slotIndex < 0 || slotIndex >= 27)
    {
        if (errOut) *errOut = "slot index out of range";
        return false;
    }
    if (!mitemName || !*mitemName)
    {
        if (errOut) *errOut = "empty mitem name";
        return false;
    }

    // Pattern A+: clone donor wrapper, retarget +0x00, sub_162DB80,
    // clear AM flags. clearFlagsAfter must always be true in production
    // (false leaves the equip living for ~1 frame before the engine
    // reverts — that's only useful for the A/B probe).
    EquipPipelineProbe::Result r{};
    bool ranEndToEnd = EquipPipelineProbe::RunEquipTestPatternAPlus(
        mitemName,
        /*clearFlagsAfter=*/true,
        &r);

    // The probe's `ranEndToEnd` is true even on engine AVs — it returns
    // false only for setup failures (item not in cache, no player AM).
    // Translate that into our success/failure: the equip is "good" if
    // the engine call returned cleanly AND m_pSlot was populated.
    bool ok = ranEndToEnd && r.callReturned;

    if (errOut)
    {
        // The probe summary is verbose — useful as the diagnostic.
        *errOut = r.summary;
    }

    return ok;
}

// ─── UI ──────────────────────────────────────────────────────────────────────

static const SkinnedMeshManager::ModelSwapEntry*
GetModelList(SkinnedMeshManager::GearType t, int& outCount)
{
    using GT = SkinnedMeshManager::GearType;
    const ModelList* lst = nullptr;
    switch (t)
    {
    case GT::Backpack:   lst = &s_backpackModels;   break;
    case GT::Shirt:      lst = &s_shirtModels;      break;
    case GT::Chestplate: lst = &s_chestplateModels; break;
    case GT::Jacket:     lst = &s_jacketModels;     break;
    case GT::Pants:      lst = &s_pantsModels;      break;
    case GT::Thigh:      lst = &s_thighModels;      break;
    case GT::Feet:       lst = &s_feetModels;       break;
    case GT::Scarf:      lst = &s_scarfModels;      break;
    case GT::Kneepads:   lst = &s_kneepadsModels;   break;
    case GT::Hat:        lst = &s_hatModels;        break;
    case GT::Gloves:     lst = &s_glovesModels;     break;
    case GT::GasMask:    lst = &s_gasMaskModels;    break;
    case GT::CosmeticMask: lst = &s_cosmeticMaskModels; break;
    default:             outCount = 0; return nullptr;
    }
    outCount = lst->count;
    return lst->entries;
}

// Per-slot UI state (one row per LiveSlot).
struct SlotUIState
{
    int  pickedIndex = -1;          // -1 = none / "current"; >=0 = entry in model list
    char custom[256] = {};          // free-text path (overrides pickedIndex if non-empty)
    std::string lastResult;         // result message of the most recent Apply
    bool        lastOk = false;
    int  variantUI = -1;            // last variant value rendered in the UI;
                                    // -1 = "auto" (use template's authored variant).
                                    // Cached so the InputInt control can show
                                    // the current value across draw frames.

    // Live ColorOverlay editor state. The 16 floats mirror the four
    // float4 ColorOverlay values in m_pSlot's 112-byte record. We
    // snapshot the original values the first time the popup opens
    // for this slot so "Undo session" can restore them — the engine's
    // shared category record is mutated in place so we can't get them
    // back any other way until a zone change re-loads the table.
    bool  colorsSnapshotTaken = false;
    float colorsSnapshot[16] = {};
};

static SlotUIState& UIStateForSlot(int slotIndex)
{
    static SlotUIState s_states[27];
    int idx = (slotIndex < 0 || slotIndex >= 27) ? 0 : slotIndex;
    return s_states[idx];
}

// ─── Appearance save slots ───────────────────────────────────────────────────
// Each save is one plain-text file under ".\\ssh_qol\\<name>.skin" relative
// to the game's working dir (same convention as ssh_QOL_Config.ini). Format:
//
//   # comment lines start with '#'
//   version 1
//   [N]
//   asset=<.mitem base name>      (empty = "unchanged" pick)
//   variant=<int, -1 = auto>
//   colors=<16 floats space-separated>   (omitted if slot isn't equipped)
//
// Only the Skin Changer's own UI state is persisted: per-slot dropdown pick,
// per-slot variant override, and the per-slot live ColorOverlay (the 4 RGBA
// values in the "Colors" popup). EquipPipelineProbe's original-outfit
// snapshot is intentionally NOT saved.

namespace {

constexpr const char* kSaveDirRel = "ssh_qol";
constexpr const char* kSaveExt    = ".skin";

void EnsureSaveDir()
{
    CreateDirectoryA(kSaveDirRel, nullptr);   // returns 0 + ALREADY_EXISTS is fine
}

std::string SavePathFor(const char* name)
{
    std::string p = kSaveDirRel;
    p += '\\';
    p += name;
    p += kSaveExt;
    return p;
}

void RTrim(std::string& s)
{
    while (!s.empty() &&
           (s.back() == '\r' || s.back() == '\n' ||
            s.back() == ' '  || s.back() == '\t'))
        s.pop_back();
}

// Sanitize an in-place save name so Windows accepts it as a file name.
void SanitizeSaveName(char* name)
{
    for (char* p = name; *p; ++p)
    {
        switch (*p)
        {
        case '\\': case '/': case ':': case '*': case '?':
        case '"':  case '<': case '>': case '|':
            *p = '_';
            break;
        default: break;
        }
    }
}

int FindIndexForAsset(SkinnedMeshManager::GearType gt, const char* asset)
{
    if (!asset || !*asset) return -1;
    int mc = 0;
    const auto* models = GetModelList(gt, mc);
    if (!models) return -1;
    for (int i = 0; i < mc; ++i)
        if (std::strcmp(models[i].assetPath, asset) == 0) return i;
    return -1;
}

bool WriteSkinSave(const char* name, std::string* errOut)
{
    EnsureSaveDir();
    std::string path = SavePathFor(name);
    FILE* fp = nullptr;
    fopen_s(&fp, path.c_str(), "wb");
    if (!fp) { if (errOut) *errOut = "open for write failed: " + path; return false; }

    std::fprintf(fp, "# ssh's QOL Tools - Skin Changer Save\n");
    std::fprintf(fp, "version 1\n");

    for (int slot = 0; slot < SkinnedMeshManager::kKnownSlotCount; ++slot)
    {
        SlotUIState& ui = UIStateForSlot(slot);
        auto gt = SkinnedMeshManager::SlotGearType(slot);

        int mc = 0;
        const auto* models = GetModelList(gt, mc);
        const char* asset = "";
        if (models && ui.pickedIndex >= 0 && ui.pickedIndex < mc)
            asset = models[ui.pickedIndex].assetPath;

        int variant = EquipPipelineProbe::GetVariantOverride(slot);

        std::fprintf(fp, "[%d]\n", slot);
        std::fprintf(fp, "asset=%s\n", asset);
        std::fprintf(fp, "variant=%d\n", variant);

        float colors[16];
        if (EquipPipelineProbe::ReadLiveColors(slot, colors))
        {
            std::fprintf(fp, "colors=");
            for (int i = 0; i < 16; ++i)
                std::fprintf(fp, "%s%.6f", i == 0 ? "" : " ", colors[i]);
            std::fprintf(fp, "\n");
        }
    }

    std::fclose(fp);
    return true;
}

bool ReadSkinSave(const char* name, std::string* errOut)
{
    std::string path = SavePathFor(name);
    FILE* fp = nullptr;
    fopen_s(&fp, path.c_str(), "rb");
    if (!fp) { if (errOut) *errOut = "open for read failed: " + path; return false; }

    char buf[1024];
    int curSlot = -1;
    while (std::fgets(buf, sizeof(buf), fp))
    {
        std::string line(buf);
        RTrim(line);
        if (line.empty() || line[0] == '#') continue;

        if (line.front() == '[' && line.back() == ']')
        {
            int s = std::atoi(line.c_str() + 1);
            curSlot = (s >= 0 && s < SkinnedMeshManager::kKnownSlotCount) ? s : -1;
            continue;
        }
        if (curSlot < 0) continue;

        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);

        SlotUIState& ui = UIStateForSlot(curSlot);
        if (key == "asset")
        {
            auto gt = SkinnedMeshManager::SlotGearType(curSlot);
            ui.pickedIndex = FindIndexForAsset(gt, val.c_str());
        }
        else if (key == "variant")
        {
            int v = std::atoi(val.c_str());
            if (v < -1) v = -1;
            ui.variantUI = v;
            EquipPipelineProbe::SetVariantOverride(curSlot, v);
        }
        else if (key == "colors")
        {
            float c[16] = {};
            int n = 0;
            const char* p = val.c_str();
            char* end = nullptr;
            while (n < 16 && *p)
            {
                float f = std::strtof(p, &end);
                if (end == p) break;
                c[n++] = f;
                p = end;
                while (*p == ' ' || *p == '\t') ++p;
            }
            // WriteLiveColors silently no-ops on slots that aren't equipped
            // yet — that's fine; the user can re-Load after Apply All Selected
            // brings the slots online.
            if (n == 16)
                EquipPipelineProbe::WriteLiveColors(curSlot, c);
        }
    }

    std::fclose(fp);
    return true;
}

void ListSkinSaves(std::vector<std::string>& out)
{
    out.clear();
    EnsureSaveDir();
    std::string pattern = std::string(kSaveDirRel) + "\\*" + kSaveExt;
    WIN32_FIND_DATAA fd{};
    HANDLE h = FindFirstFileA(pattern.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        std::string fname = fd.cFileName;
        auto dot = fname.rfind('.');
        if (dot != std::string::npos) fname.resize(dot);
        if (!fname.empty()) out.push_back(fname);
    } while (FindNextFileA(h, &fd));
    FindClose(h);
}

bool DeleteSkinSave(const char* name)
{
    std::string p = SavePathFor(name);
    return DeleteFileA(p.c_str()) != 0;
}

} // anonymous namespace

void SkinnedMeshManager::DrawUI()
{
    // Refresh slot table every draw — cheap, 27 entries.
    ScanLiveSlots();

    // ── Header: cache + player status, one line each ──────────────────
    // GetCfg() lazy-resolves the InventoryConfig via the singleton chain
    // (module+0x4688B28 → … → cfg). The display just reflects whether
    // it's been resolved yet — no manual scan button needed.
    TD::InventoryConfig* cfg = ItemDescriptorCache::GetCfg();
    if (cfg)
    {
        ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f),
                           "InventoryConfig: ready");
    }
    else
    {
        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.2f, 1.0f),
                           "InventoryConfig: resolving… (waiting for item system to load)");
    }

    if (!m_scanError.empty())
    {
        bool transient = m_scanError.find("last good scan") != std::string::npos;
        ImVec4 col = transient ? ImVec4(1.0f, 0.85f, 0.2f, 1.0f)
                               : ImVec4(1.0f, 0.40f, 0.40f, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, col);
        ImGui::Text("%s", m_scanError.c_str());
        ImGui::PopStyleColor();
    }

    // ── Appearance Save Slots ────────────────────────────────────────
    // Persists only the Skin Changer's own UI state (per-slot picks +
    // variant overrides + live ColorOverlay values) to .\ssh_qol\<name>.skin
    // — not the underlying character, and not the equip-pipeline snapshot.
    {
        static char  s_saveName[64]              = "default";
        static char  s_loadSelected[64]          = {};
        static std::vector<std::string> s_saveList;
        static bool  s_saveListReady             = false;
        static std::string s_saveResult;
        static bool  s_saveOk                    = false;

        if (!s_saveListReady) { ListSkinSaves(s_saveList); s_saveListReady = true; }

        if (ImGui::CollapsingHeader("Appearance Save Slots",
                                    ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::PushItemWidth(220.0f);
            ImGui::InputText("Save name", s_saveName, sizeof(s_saveName));
            ImGui::PopItemWidth();
            ImGui::SameLine();
            if (ImGui::Button("Save") && s_saveName[0])
            {
                SanitizeSaveName(s_saveName);
                std::string err;
                s_saveOk = WriteSkinSave(s_saveName, &err);
                s_saveResult = s_saveOk
                    ? std::string("saved: ") + s_saveName + kSaveExt
                    : err;
                ListSkinSaves(s_saveList);
            }

            ImGui::PushItemWidth(220.0f);
            const char* combo_preview = s_loadSelected[0] ? s_loadSelected
                                                           : "(pick a save)";
            if (ImGui::BeginCombo("Saved slot", combo_preview))
            {
                if (s_saveList.empty())
                    ImGui::TextDisabled("(no saves yet)");
                for (const auto& n : s_saveList)
                {
                    bool selected = (std::strcmp(s_loadSelected, n.c_str()) == 0);
                    if (ImGui::Selectable(n.c_str(), selected))
                        std::snprintf(s_loadSelected, sizeof(s_loadSelected),
                                      "%s", n.c_str());
                    if (selected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            ImGui::PopItemWidth();
            ImGui::SameLine();
            if (ImGui::Button("Load") && s_loadSelected[0])
            {
                std::string err;
                s_saveOk = ReadSkinSave(s_loadSelected, &err);
                s_saveResult = s_saveOk
                    ? std::string("loaded: ") + s_loadSelected
                      + " — click 'Apply All Selected' to equip, then Load again to apply colors"
                    : err;
            }
            ImGui::SameLine();
            if (ImGui::Button("Delete") && s_loadSelected[0])
            {
                s_saveOk = DeleteSkinSave(s_loadSelected);
                s_saveResult = s_saveOk
                    ? std::string("deleted: ") + s_loadSelected
                    : std::string("delete failed (file in use or missing)");
                if (s_saveOk) s_loadSelected[0] = 0;
                ListSkinSaves(s_saveList);
            }
            ImGui::SameLine();
            if (ImGui::Button("Refresh"))
                ListSkinSaves(s_saveList);

            if (!s_saveResult.empty())
            {
                ImVec4 col = s_saveOk ? ImVec4(0.5f, 1.0f, 0.5f, 1.0f)
                                      : ImVec4(1.0f, 0.5f, 0.5f, 1.0f);
                ImGui::TextColored(col, "%s", s_saveResult.c_str());
            }
        }
    }

    if (m_slots.empty())
    {
        ImGui::TextDisabled("(no populated slots — load a character into the world first)");
        return;
    }

    ImGui::Separator();

    // ── Apply All / Restore Visibility row ────────────────────────────
    // Apply All: fires one engine call covering every dropdown's current
    // selection (plus any earlier injections still active). One animation
    // reset for the whole outfit instead of one per slot.
    //
    // Restore Visibility: same call, but re-uses only existing tracked
    // injections (no new selections). Recovers slots gone invisible after
    // an in-game equip clobbered our wrappers.
    static std::string s_batchResult;
    static bool        s_batchOk = false;

    if (ImGui::Button("Apply All Selected"))
    {
        // Collect parallel arrays of (slot, mitemName) for every dropdown
        // with a valid pick. Static storage for the name strings keeps
        // the const char* pointers we hand to RunEquipBatch valid for
        // the call's lifetime.
        static char  s_names[27][160];
        static const char* s_namePtrs[27];
        static int   s_slots[27];
        int n = 0;

        for (const auto& ls : m_slots)
        {
            if (ls.index < 0 || ls.index >= kKnownSlotCount) continue;
            SlotUIState& ui = UIStateForSlot(ls.index);
            int modelCount = 0;
            const auto* models = GetModelList(ls.type, modelCount);
            if (ui.pickedIndex < 0 || ui.pickedIndex >= modelCount) continue;

            // Same basename derivation as the per-slot Apply button.
            const char* target = models[ui.pickedIndex].assetPath;
            const char* slash  = std::strrchr(target, '/');
            const char* base   = slash ? slash + 1 : target;
            std::size_t k = 0;
            while (base[k] && base[k] != '.' && k + 1 < sizeof(s_names[0]))
            {
                s_names[n][k] = base[k]; ++k;
            }
            s_names[n][k] = '\0';
            s_namePtrs[n] = s_names[n];
            s_slots[n]    = ls.index;
            ++n;
        }

        // Always fire the batch — even when no dropdown was changed,
        // RunEquipBatch will still re-apply existing active injections
        // so a single click reasserts the whole modded outfit.
        char err[256] = {};
        int staged = EquipPipelineProbe::RunEquipBatch(s_slots, s_namePtrs, n,
                                                      err, sizeof(err));
        char buf[320];
        if (staged > 0)
        {
            s_batchOk = true;
            std::snprintf(buf, sizeof(buf), "applied %d slot(s) in one call", staged);
        }
        else if (n == 0 && err[0] == '\0')
        {
            s_batchOk = false;
            std::snprintf(buf, sizeof(buf), "no dropdown selections (and no active injections)");
        }
        else
        {
            s_batchOk = false;
            std::snprintf(buf, sizeof(buf), "batch failed: %s",
                          err[0] ? err : "(no diagnostic)");
        }
        s_batchResult = buf;
    }

    ImGui::SameLine();
    if (ImGui::Button("Snapshot"))
    {
        int n = EquipPipelineProbe::TakeOriginalSnapshotNow();
        s_batchOk = (n > 0);
        char buf[256];
        if (n > 0)
            std::snprintf(buf, sizeof(buf), "snapshot captured (%d wrappers)", n);
        else
            std::snprintf(buf, sizeof(buf), "snapshot failed: %s",
                          EquipPipelineProbe::LastSnapshotReason());
        s_batchResult = buf;
    }

    ImGui::SameLine();
    {
        const bool haveSnapshot = EquipPipelineProbe::HasOriginalSnapshot();
        if (!haveSnapshot)
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.30f, 0.30f, 0.30f, 1.0f));
        if (ImGui::Button("Restore Original"))
        {
            if (!haveSnapshot)
            {
                s_batchOk = false;
                s_batchResult = "no snapshot yet — apply something first";
            }
            else
            {
                int n = EquipPipelineProbe::RestoreOriginalOutfit();
                s_batchOk = (n > 0);
                char buf[160];
                std::snprintf(buf, sizeof(buf),
                              n > 0 ? "restored %d original wrapper(s)"
                                    : "restore failed (engine AV?)", n);
                s_batchResult = buf;
            }
        }
        if (!haveSnapshot) ImGui::PopStyleColor();
    }

    if (!s_batchResult.empty())
    {
        ImVec4 col = s_batchOk ? ImVec4(0.5f, 1.0f, 0.5f, 1.0f)
                               : ImVec4(1.0f, 0.5f, 0.5f, 1.0f);
        ImGui::TextColored(col, "%s", s_batchResult.c_str());
    }

    ImGui::Separator();

    // ── Per-slot rows: compact label + dropdown ───────────────────────
    // Only known body-part slots (0..12) get a row. Higher indices in
    // m_Clothes are weapon/ammo/consumable slots the equip pipeline
    // doesn't touch. No per-row Apply button — the top "Apply All
    // Selected" button equips everything in one engine call.
    for (const auto& ls : m_slots)
    {
        const bool isKnownSlot = (ls.index >= 0 && ls.index < kKnownSlotCount);
        if (!isKnownSlot)
            continue;

        ImGui::PushID(ls.index);

        int          modelCount = 0;
        const auto*  models     = GetModelList(ls.type, modelCount);
        SlotUIState& ui         = UIStateForSlot(ls.index);

        // Fixed-width label so dropdowns line up vertically regardless
        // of gear-type name length.
        ImGui::TextColored(ImVec4(0.6f, 0.85f, 1.0f, 1.0f),
                           "%-10s", GearTypeName(ls.type));
        ImGui::SameLine(110.0f);

        const char* preview = (ui.pickedIndex >= 0 && ui.pickedIndex < modelCount)
                              ? models[ui.pickedIndex].displayName
                              : "(unchanged)";
        ImGui::PushItemWidth(360.0f);
        if (ImGui::BeginCombo("##picker", preview))
        {
            // First entry: explicit "unchanged" so the user can deselect.
            if (ImGui::Selectable("(unchanged)", ui.pickedIndex < 0))
                ui.pickedIndex = -1;
            for (int i = 0; i < modelCount; ++i)
            {
                bool selected = (ui.pickedIndex == i);
                if (ImGui::Selectable(models[i].displayName, selected))
                    ui.pickedIndex = i;
                if (selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::PopItemWidth();

        // Tooltip on hover shows the current live path — keeps the row
        // single-line while still surfacing the diagnostic info.
        if (ImGui::IsItemHovered() && !ls.currentPath.empty())
            ImGui::SetTooltip("slot %d  current: %s", ls.index, ls.currentPath.c_str());

        // ── Color-variant cycle ──────────────────────────────────────
        // Re-sync the UI value from the probe each draw so apply-all /
        // restore-original passes that reset the override stay visible.
        ui.variantUI = EquipPipelineProbe::GetVariantOverride(ls.index);

        ImGui::SameLine();
        ImGui::PushItemWidth(90.0f);
        int prev = ui.variantUI;
        // InputInt with step buttons. Negative values mean "auto"
        // (use the template's authored variant from item+0x3C).
        if (ImGui::InputInt("##variant", &ui.variantUI, 1, 1))
        {
            if (ui.variantUI < -1) ui.variantUI = -1;
            if (ui.variantUI != prev)
            {
                EquipPipelineProbe::SetVariantOverride(ls.index, ui.variantUI);
                EquipPipelineProbe::ReapplyAllInjections();
            }
        }
        ImGui::PopItemWidth();
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Color variant for this slot.\n"
                              "  -1 = use item's authored color\n"
                              "  0+ = cycle through other shipped colors\n"
                              "Engine maps value modulo the category's record count,\n"
                              "so over-shooting just wraps around.");

        // ── Direct RGBA edit on the live record ──────────────────────
        // m_Clothes[slot].m_pSlot points at the engine's per-(category,
        // variant) material record. Its first 64 bytes are four float4
        // ColorOverlay tints. Editing them here writes through the
        // shared engine record — fully unrestricted color but shared
        // with anything else resolving to the same (category, variant).
        ImGui::SameLine();
        const char* popupId = "##colors_popup";
        if (ImGui::SmallButton("Colors"))
        {
            // Reset snapshot flag so the next ReadLiveColors below
            // re-captures the current state as "original" for Undo.
            ui.colorsSnapshotTaken = false;
            ImGui::OpenPopup(popupId);
        }

        if (ImGui::BeginPopup(popupId))
        {
            float colors[16];
            if (EquipPipelineProbe::ReadLiveColors(ls.index, colors))
            {
                if (!ui.colorsSnapshotTaken)
                {
                    std::memcpy(ui.colorsSnapshot, colors, sizeof(colors));
                    ui.colorsSnapshotTaken = true;
                }

                ImGui::TextDisabled("ColorOverlay layers (R, G, B, A)");
                bool changed = false;
                for (int i = 0; i < 4; ++i)
                {
                    char label[16];
                    std::snprintf(label, sizeof(label), "Color %d", i);
                    if (ImGui::ColorEdit4(label, &colors[i * 4]))
                        changed = true;
                }

                if (changed)
                    EquipPipelineProbe::WriteLiveColors(ls.index, colors);

                ImGui::Separator();
                if (ImGui::Button("Undo session"))
                    EquipPipelineProbe::WriteLiveColors(ls.index, ui.colorsSnapshot);
                ImGui::SameLine();
                ImGui::TextDisabled("(restores values from popup open)");
            }
            else
            {
                ImGui::TextDisabled("no live record — equip this slot first");
            }
            ImGui::EndPopup();
        }

        ImGui::PopID();
    }
}

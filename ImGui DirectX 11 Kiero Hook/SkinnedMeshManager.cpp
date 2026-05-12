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

static const SkinnedMeshManager::ModelSwapEntry s_backpackModels_[] =
{
    { "Ninjabike Messenger Bag",         "rogue/graph objects/gear/ca_cm_b_sv_set01.mgraphobject" },
    { "Striker's Battlegear",            "rogue/graph objects/gear/CA_CM_B_T7_R_DLC1.mgraphobject" },
    { "Striker's Battlegear Classified", "rogue/graph objects/gear/ca_cm_b_mm_st.mgraphobject" },
    { "Predator's Mark Classified",      "rogue/graph objects/gear/ca_cm_b_pa_pr.mgraphobject" },
    { "Hunters Faith Classified",        "rogue/graph objects/gear/ca_cm_b_rt_hf.mgraphobject" },
    { "Nomad Classified",                "rogue/graph objects/gear/ca_cm_b_uc_pn.mgraphobject" },
    { "D3-FNC Classified",               "rogue/graph objects/gear/ca_cm_b_pa_d3.mgraphobject" },
    { "Lone Star Classified",            "rogue/graph objects/gear/ca_cm_b_tt_ls.mgraphobject" },
    { "Lone Star",                       "rogue/graph objects/gear/CA_CM_B_Set03_BG.mgraphobject" },
    { "Banshee Classified",              "rogue/graph objects/gear/ca_cm_b_pa_ba.mgraphobject" },
    { "Banshee",                         "rogue/graph objects/gear/ca_cm_b_uw_dar.mgraphobject" },
    { "DeadEYE Classified",              "rogue/graph objects/gear/ca_cm_b_tt_de.mgraphobject" },
    { "Sentry Call Classified",          "rogue/graph objects/gear/ca_cm_b_as_sc.mgraphobject" },
    { "Alphabridge Classified",          "rogue/graph objects/gear/ca_cm_b_rt_ab.mgraphobject" },
    { "Reclaimer Classified",            "rogue/graph objects/gear/ca_cm_b_tt_rc.mgraphobject" },
    { "FireCrest Classified",            "rogue/graph objects/gear/ca_cm_b_rt_fc.mgraphobject" },
    { "FireCrest",                       "rogue/graph objects/gear/CA_CM_B_GS_UW.mgraphobject" },
    { "Spec-ops pack",                   "rogue/graph objects/gear/CA_CM_B_T7_L.mgraphobject" },
    { "Urban assault pack",              "rogue/graph objects/gear/CA_CM_B_T7_E.mgraphobject" },
    { "Security pack",                   "rogue/graph objects/gear/CA_CM_B_T1_R.mgraphobject" },
    { "Safety bag",                      "rogue/graph objects/gear/CA_CM_B_T1_U.mgraphobject" },
};

static const SkinnedMeshManager::ModelSwapEntry s_ChestPlateModels_[] =
{
    { "ca_cm_l2_as_sc",     "rogue/graph objects/gear/ca_cm_l2_as_sc.mgraphobject" },
    { "ca_cm_l2_gs_uw",     "rogue/graph objects/gear/ca_cm_l2_gs_uw.mgraphobject" },
    { "ca_cm_l2_mm_st",     "rogue/graph objects/gear/ca_cm_l2_mm_st.mgraphobject" },
    { "ca_cm_l2_nm_uw",     "rogue/graph objects/gear/ca_cm_l2_nm_uw.mgraphobject" },
    { "ca_cm_l2_pa_ba",     "rogue/graph objects/gear/ca_cm_l2_pa_ba.mgraphobject" },
    { "ca_cm_l2_pa_d3",     "rogue/graph objects/gear/ca_cm_l2_pa_d3.mgraphobject" },
    { "ca_cm_l2_pa_pr",     "rogue/graph objects/gear/ca_cm_l2_pa_pr.mgraphobject" },
    { "ca_cm_l2_pk_uw",     "rogue/graph objects/gear/ca_cm_l2_pk_uw.mgraphobject" },
    { "ca_cm_l2_rt_ab",     "rogue/graph objects/gear/ca_cm_l2_rt_ab.mgraphobject" },
    { "ca_cm_l2_rt_fc",     "rogue/graph objects/gear/ca_cm_l2_rt_fc.mgraphobject" },
    { "ca_cm_l2_rt_hf",     "rogue/graph objects/gear/ca_cm_l2_rt_hf.mgraphobject" },
    { "ca_cm_l2_rt_ta",     "rogue/graph objects/gear/ca_cm_l2_rt_ta.mgraphobject" },
    { "ca_cm_l2_set01_bg",  "rogue/graph objects/gear/ca_cm_l2_set01_bg.mgraphobject" },
    { "ca_cm_l2_set02_bg",  "rogue/graph objects/gear/ca_cm_l2_set02_bg.mgraphobject" },
    { "ca_cm_l2_set03_bg",  "rogue/graph objects/gear/ca_cm_l2_set03_bg.mgraphobject" },
    { "ca_cm_l2_set04_bg",  "rogue/graph objects/gear/ca_cm_l2_set04_bg.mgraphobject" },
    { "ca_cm_l2_ss_uw",     "rogue/graph objects/gear/ca_cm_l2_ss_uw.mgraphobject" },
    { "ca_cm_l2_sv_set01",  "rogue/graph objects/gear/ca_cm_l2_sv_set01.mgraphobject" },
    { "ca_cm_l2_t1_c",      "rogue/graph objects/gear/ca_cm_l2_t1_c.mgraphobject" },
    { "ca_cm_l2_t1_e",      "rogue/graph objects/gear/ca_cm_l2_t1_e.mgraphobject" },
    { "ca_cm_l2_t2_c",      "rogue/graph objects/gear/ca_cm_l2_t2_c.mgraphobject" },
    { "ca_cm_l2_t2_e",      "rogue/graph objects/gear/ca_cm_l2_t2_e.mgraphobject" },
    { "ca_cm_l2_t2_r",      "rogue/graph objects/gear/ca_cm_l2_t2_r.mgraphobject" },
    { "ca_cm_l2_t2_u",      "rogue/graph objects/gear/ca_cm_l2_t2_u.mgraphobject" },
    { "ca_cm_l2_t3_c",      "rogue/graph objects/gear/ca_cm_l2_t3_c.mgraphobject" },
    { "ca_cm_l2_t3_r",      "rogue/graph objects/gear/ca_cm_l2_t3_r.mgraphobject" },
    { "ca_cm_l2_t3_u",      "rogue/graph objects/gear/ca_cm_l2_t3_u.mgraphobject" },
    { "ca_cm_l2_t4_c",      "rogue/graph objects/gear/ca_cm_l2_t4_c.mgraphobject" },
    { "ca_cm_l2_t4_e",      "rogue/graph objects/gear/ca_cm_l2_t4_e.mgraphobject" },
    { "ca_cm_l2_t4_l",      "rogue/graph objects/gear/ca_cm_l2_t4_l.mgraphobject" },
    { "ca_cm_l2_t4_r",      "rogue/graph objects/gear/ca_cm_l2_t4_r.mgraphobject" },
    { "ca_cm_l2_t4_u",      "rogue/graph objects/gear/ca_cm_l2_t4_u.mgraphobject" },
    { "ca_cm_l2_t5_c",      "rogue/graph objects/gear/ca_cm_l2_t5_c.mgraphobject" },
    { "ca_cm_l2_t5_e",      "rogue/graph objects/gear/ca_cm_l2_t5_e.mgraphobject" },
    { "ca_cm_l2_t5_l",      "rogue/graph objects/gear/ca_cm_l2_t5_l.mgraphobject" },
    { "ca_cm_l2_t5_r",      "rogue/graph objects/gear/ca_cm_l2_t5_r.mgraphobject" },
    { "ca_cm_l2_t5_u",      "rogue/graph objects/gear/ca_cm_l2_t5_u.mgraphobject" },
    { "ca_cm_l2_t6_e",      "rogue/graph objects/gear/ca_cm_l2_t6_e.mgraphobject" },
    { "ca_cm_l2_t6_l",      "rogue/graph objects/gear/ca_cm_l2_t6_l.mgraphobject" },
    { "ca_cm_l2_t6_r",      "rogue/graph objects/gear/ca_cm_l2_t6_r.mgraphobject" },
    { "ca_cm_l2_t6_u",      "rogue/graph objects/gear/ca_cm_l2_t6_u.mgraphobject" },
    { "ca_cm_l2_t7_e",      "rogue/graph objects/gear/ca_cm_l2_t7_e.mgraphobject" },
    { "ca_cm_l2_t7_l",      "rogue/graph objects/gear/ca_cm_l2_t7_l.mgraphobject" },
    { "ca_cm_l2_t7_l_dlc1", "rogue/graph objects/gear/ca_cm_l2_t7_l_dlc1.mgraphobject" },
    { "ca_cm_l2_t7_r",      "rogue/graph objects/gear/ca_cm_l2_t7_r.mgraphobject" },
    { "ca_cm_l2_tt_de",     "rogue/graph objects/gear/ca_cm_l2_tt_de.mgraphobject" },
    { "ca_cm_l2_tt_fm",     "rogue/graph objects/gear/ca_cm_l2_tt_fm.mgraphobject" },
    { "ca_cm_l2_tt_ls",     "rogue/graph objects/gear/ca_cm_l2_tt_ls.mgraphobject" },
    { "ca_cm_l2_tt_rc",     "rogue/graph objects/gear/ca_cm_l2_tt_rc.mgraphobject" },
    { "ca_cm_l2_uc_pn",     "rogue/graph objects/gear/ca_cm_l2_uc_pn.mgraphobject" },
    { "ca_cm_l2_uw_dar",    "rogue/graph objects/gear/ca_cm_l2_uw_dar.mgraphobject" },
    { "ca_cm_l2_wd_uw",     "rogue/graph objects/gear/ca_cm_l2_wd_uw.mgraphobject" },
};

static const SkinnedMeshManager::ModelSwapEntry s_HolsterModels_[] =
{
    { "ca_cm_t_as_sc",     "rogue/graph objects/gear/ca_cm_t_as_sc.mgraphobject" },
    { "ca_cm_t_gs_uw",     "rogue/graph objects/gear/ca_cm_t_gs_uw.mgraphobject" },
    { "ca_cm_t_mm_st",     "rogue/graph objects/gear/ca_cm_t_mm_st.mgraphobject" },
    { "ca_cm_t_nm_uw",     "rogue/graph objects/gear/ca_cm_t_nm_uw.mgraphobject" },
    { "ca_cm_t_pa_ba",     "rogue/graph objects/gear/ca_cm_t_pa_ba.mgraphobject" },
    { "ca_cm_t_pa_d3",     "rogue/graph objects/gear/ca_cm_t_pa_d3.mgraphobject" },
    { "ca_cm_t_pa_pr",     "rogue/graph objects/gear/ca_cm_t_pa_pr.mgraphobject" },
    { "ca_cm_t_pk_uw",     "rogue/graph objects/gear/ca_cm_t_pk_uw.mgraphobject" },
    { "ca_cm_t_rt_ab",     "rogue/graph objects/gear/ca_cm_t_rt_ab.mgraphobject" },
    { "ca_cm_t_rt_fc",     "rogue/graph objects/gear/ca_cm_t_rt_fc.mgraphobject" },
    { "ca_cm_t_rt_hf",     "rogue/graph objects/gear/ca_cm_t_rt_hf.mgraphobject" },
    { "ca_cm_t_rt_ta",     "rogue/graph objects/gear/ca_cm_t_rt_ta.mgraphobject" },
    { "ca_cm_t_set01_bg",  "rogue/graph objects/gear/ca_cm_t_set01_bg.mgraphobject" },
    { "ca_cm_t_set02_bg",  "rogue/graph objects/gear/ca_cm_t_set02_bg.mgraphobject" },
    { "ca_cm_t_set03_bg",  "rogue/graph objects/gear/ca_cm_t_set03_bg.mgraphobject" },
    { "ca_cm_t_set04_bg",  "rogue/graph objects/gear/ca_cm_t_set04_bg.mgraphobject" },
    { "ca_cm_t_ss_uw",     "rogue/graph objects/gear/ca_cm_t_ss_uw.mgraphobject" },
    { "ca_cm_t_sv_set01",  "rogue/graph objects/gear/ca_cm_t_sv_set01.mgraphobject" },
    { "ca_cm_t_t0_c",      "rogue/graph objects/gear/ca_cm_t_t0_c.mgraphobject" },
    { "ca_cm_t_t1_c",      "rogue/graph objects/gear/ca_cm_t_t1_c.mgraphobject" },
    { "ca_cm_t_t1_e",      "rogue/graph objects/gear/ca_cm_t_t1_e.mgraphobject" },
    { "ca_cm_t_t2_c",      "rogue/graph objects/gear/ca_cm_t_t2_c.mgraphobject" },
    { "ca_cm_t_t2_e",      "rogue/graph objects/gear/ca_cm_t_t2_e.mgraphobject" },
    { "ca_cm_t_t3_c",      "rogue/graph objects/gear/ca_cm_t_t3_c.mgraphobject" },
    { "ca_cm_t_t3_r",      "rogue/graph objects/gear/ca_cm_t_t3_r.mgraphobject" },
    { "ca_cm_t_t3_u",      "rogue/graph objects/gear/ca_cm_t_t3_u.mgraphobject" },
    { "ca_cm_t_t4_c",      "rogue/graph objects/gear/ca_cm_t_t4_c.mgraphobject" },
    { "ca_cm_t_t4_e",      "rogue/graph objects/gear/ca_cm_t_t4_e.mgraphobject" },
    { "ca_cm_t_t4_l",      "rogue/graph objects/gear/ca_cm_t_t4_l.mgraphobject" },
    { "ca_cm_t_t4_r",      "rogue/graph objects/gear/ca_cm_t_t4_r.mgraphobject" },
    { "ca_cm_t_t4_u",      "rogue/graph objects/gear/ca_cm_t_t4_u.mgraphobject" },
    { "ca_cm_t_t5_c",      "rogue/graph objects/gear/ca_cm_t_t5_c.mgraphobject" },
    { "ca_cm_t_t5_e",      "rogue/graph objects/gear/ca_cm_t_t5_e.mgraphobject" },
    { "ca_cm_t_t5_l",      "rogue/graph objects/gear/ca_cm_t_t5_l.mgraphobject" },
    { "ca_cm_t_t5_r",      "rogue/graph objects/gear/ca_cm_t_t5_r.mgraphobject" },
    { "ca_cm_t_t5_u",      "rogue/graph objects/gear/ca_cm_t_t5_u.mgraphobject" },
    { "ca_cm_t_t6_e",      "rogue/graph objects/gear/ca_cm_t_t6_e.mgraphobject" },
    { "ca_cm_t_t6_l",      "rogue/graph objects/gear/ca_cm_t_t6_l.mgraphobject" },
    { "ca_cm_t_t6_r",      "rogue/graph objects/gear/ca_cm_t_t6_r.mgraphobject" },
    { "ca_cm_t_t6_u",      "rogue/graph objects/gear/ca_cm_t_t6_u.mgraphobject" },
    { "ca_cm_t_t7_e",      "rogue/graph objects/gear/ca_cm_t_t7_e.mgraphobject" },
    { "ca_cm_t_t7_l",      "rogue/graph objects/gear/ca_cm_t_t7_l.mgraphobject" },
    { "ca_cm_t_t7_l_dlc1", "rogue/graph objects/gear/ca_cm_t_t7_l_dlc1.mgraphobject" },
    { "ca_cm_t_t7_r",      "rogue/graph objects/gear/ca_cm_t_t7_r.mgraphobject" },
    { "ca_cm_t_tt_de",     "rogue/graph objects/gear/ca_cm_t_tt_de.mgraphobject" },
    { "ca_cm_t_tt_fm",     "rogue/graph objects/gear/ca_cm_t_tt_fm.mgraphobject" },
    { "ca_cm_t_tt_ls",     "rogue/graph objects/gear/ca_cm_t_tt_ls.mgraphobject" },
    { "ca_cm_t_tt_rc",     "rogue/graph objects/gear/ca_cm_t_tt_rc.mgraphobject" },
    { "ca_cm_t_uc_pn",     "rogue/graph objects/gear/ca_cm_t_uc_pn.mgraphobject" },
    { "ca_cm_t_uw_dar",    "rogue/graph objects/gear/ca_cm_t_uw_dar.mgraphobject" },
    { "ca_cm_t_wd_uw",     "rogue/graph objects/gear/ca_cm_t_wd_uw.mgraphobject" },
};

static const SkinnedMeshManager::ModelSwapEntry s_KneePadModels_[] =
{
    { "ca_cm_k_as_sc",     "rogue/graph objects/gear/ca_cm_k_as_sc.mgraphobject" },
    { "ca_cm_k_gs_uw",     "rogue/graph objects/gear/ca_cm_k_gs_uw.mgraphobject" },
    { "ca_cm_k_mm_st",     "rogue/graph objects/gear/ca_cm_k_mm_st.mgraphobject" },
    { "ca_cm_k_nm_uw",     "rogue/graph objects/gear/ca_cm_k_nm_uw.mgraphobject" },
    { "ca_cm_k_pa_ba",     "rogue/graph objects/gear/ca_cm_k_pa_ba.mgraphobject" },
    { "ca_cm_k_pa_d3",     "rogue/graph objects/gear/ca_cm_k_pa_d3.mgraphobject" },
    { "ca_cm_k_pa_pr",     "rogue/graph objects/gear/ca_cm_k_pa_pr.mgraphobject" },
    { "ca_cm_k_pk_uw",     "rogue/graph objects/gear/ca_cm_k_pk_uw.mgraphobject" },
    { "ca_cm_k_rt_ab",     "rogue/graph objects/gear/ca_cm_k_rt_ab.mgraphobject" },
    { "ca_cm_k_rt_fc",     "rogue/graph objects/gear/ca_cm_k_rt_fc.mgraphobject" },
    { "ca_cm_k_rt_hf",     "rogue/graph objects/gear/ca_cm_k_rt_hf.mgraphobject" },
    { "ca_cm_k_rt_ta",     "rogue/graph objects/gear/ca_cm_k_rt_ta.mgraphobject" },
    { "ca_cm_k_set01_bg",  "rogue/graph objects/gear/ca_cm_k_set01_bg.mgraphobject" },
    { "ca_cm_k_set02_bg",  "rogue/graph objects/gear/ca_cm_k_set02_bg.mgraphobject" },
    { "ca_cm_k_set03_bg",  "rogue/graph objects/gear/ca_cm_k_set03_bg.mgraphobject" },
    { "ca_cm_k_set04_bg",  "rogue/graph objects/gear/ca_cm_k_set04_bg.mgraphobject" },
    { "ca_cm_k_ss_uw",     "rogue/graph objects/gear/ca_cm_k_ss_uw.mgraphobject" },
    { "ca_cm_k_sv_set01",  "rogue/graph objects/gear/ca_cm_k_sv_set01.mgraphobject" },
    { "ca_cm_k_t0_c",      "rogue/graph objects/gear/ca_cm_k_t0_c.mgraphobject" },
    { "ca_cm_k_t1_c",      "rogue/graph objects/gear/ca_cm_k_t1_c.mgraphobject" },
    { "ca_cm_k_t1_e",      "rogue/graph objects/gear/ca_cm_k_t1_e.mgraphobject" },
    { "ca_cm_k_t2_c",      "rogue/graph objects/gear/ca_cm_k_t2_c.mgraphobject" },
    { "ca_cm_k_t2_e",      "rogue/graph objects/gear/ca_cm_k_t2_e.mgraphobject" },
    { "ca_cm_k_t3_c",      "rogue/graph objects/gear/ca_cm_k_t3_c.mgraphobject" },
    { "ca_cm_k_t3_r",      "rogue/graph objects/gear/ca_cm_k_t3_r.mgraphobject" },
    { "ca_cm_k_t4_c",      "rogue/graph objects/gear/ca_cm_k_t4_c.mgraphobject" },
    { "ca_cm_k_t4_l",      "rogue/graph objects/gear/ca_cm_k_t4_l.mgraphobject" },
    { "ca_cm_k_t5_c",      "rogue/graph objects/gear/ca_cm_k_t5_c.mgraphobject" },
    { "ca_cm_k_t5_l",      "rogue/graph objects/gear/ca_cm_k_t5_l.mgraphobject" },
    { "ca_cm_k_t6_l",      "rogue/graph objects/gear/ca_cm_k_t6_l.mgraphobject" },
    { "ca_cm_k_t6_u",      "rogue/graph objects/gear/ca_cm_k_t6_u.mgraphobject" },
    { "ca_cm_k_t7_l",      "rogue/graph objects/gear/ca_cm_k_t7_l.mgraphobject" },
    { "ca_cm_k_t7_l_dlc1", "rogue/graph objects/gear/ca_cm_k_t7_l_dlc1.mgraphobject" },
    { "ca_cm_k_t7_r",      "rogue/graph objects/gear/ca_cm_k_t7_r.mgraphobject" },
    { "ca_cm_k_tt_de",     "rogue/graph objects/gear/ca_cm_k_tt_de.mgraphobject" },
    { "ca_cm_k_tt_fm",     "rogue/graph objects/gear/ca_cm_k_tt_fm.mgraphobject" },
    { "ca_cm_k_tt_ls",     "rogue/graph objects/gear/ca_cm_k_tt_ls.mgraphobject" },
    { "ca_cm_k_tt_rc",     "rogue/graph objects/gear/ca_cm_k_tt_rc.mgraphobject" },
    { "ca_cm_k_uc_pn",     "rogue/graph objects/gear/ca_cm_k_uc_pn.mgraphobject" },
    { "ca_cm_k_ulc_haz",   "rogue/graph objects/gear/ca_cm_k_ulc_haz.mgraphobject" },
    { "ca_cm_k_uw_dar",    "rogue/graph objects/gear/ca_cm_k_uw_dar.mgraphobject" },
    { "ca_cm_k_wd_uw",     "rogue/graph objects/gear/ca_cm_k_wd_uw.mgraphobject" },
};

static const SkinnedMeshManager::ModelSwapEntry s_GloveModels_[] =
{
    { "ca_cm_h_mm_st",      "rogue/graph objects/gear/ca_cm_h_mm_st.mgraphobject" },
    { "ca_cm_h_pa_ba",      "rogue/graph objects/gear/ca_cm_h_pa_ba.mgraphobject" },
    { "ca_cm_h_pa_d3",      "rogue/graph objects/gear/ca_cm_h_pa_d3.mgraphobject" },
    { "ca_cm_h_pa_pr",      "rogue/graph objects/gear/ca_cm_h_pa_pr.mgraphobject" },
    { "ca_cm_h_rt_ab",      "rogue/graph objects/gear/ca_cm_h_rt_ab.mgraphobject" },
    { "ca_cm_h_rt_fc",      "rogue/graph objects/gear/ca_cm_h_rt_fc.mgraphobject" },
    { "ca_cm_h_rt_hf",      "rogue/graph objects/gear/ca_cm_h_rt_hf.mgraphobject" },
    { "ca_cm_h_rt_ta",      "rogue/graph objects/gear/ca_cm_h_rt_ta.mgraphobject" },
    { "ca_cm_h_set01_bg",   "rogue/graph objects/gear/ca_cm_h_set01_bg.mgraphobject" },
    { "ca_cm_h_set02_bg",   "rogue/graph objects/gear/ca_cm_h_set02_bg.mgraphobject" },
    { "ca_cm_h_set03_bg",   "rogue/graph objects/gear/ca_cm_h_set03_bg.mgraphobject" },
    { "ca_cm_h_set04_bg",   "rogue/graph objects/gear/ca_cm_h_set04_bg.mgraphobject" },
    { "ca_cm_h_sv_set01",   "rogue/graph objects/gear/ca_cm_h_sv_set01.mgraphobject" },
    { "ca_cm_h_t0_c",       "rogue/graph objects/gear/ca_cm_h_t0_c.mgraphobject" },
    { "ca_cm_h_t1_c",       "rogue/graph objects/gear/ca_cm_h_t1_c.mgraphobject" },
    { "ca_cm_h_t1_e",       "rogue/graph objects/gear/ca_cm_h_t1_e.mgraphobject" },
    { "ca_cm_h_t2_c",       "rogue/graph objects/gear/ca_cm_h_t2_c.mgraphobject" },
    { "ca_cm_h_t2_e",       "rogue/graph objects/gear/ca_cm_h_t2_e.mgraphobject" },
    { "ca_cm_h_t3_c",       "rogue/graph objects/gear/ca_cm_h_t3_c.mgraphobject" },
    { "ca_cm_h_t3_r",       "rogue/graph objects/gear/ca_cm_h_t3_r.mgraphobject" },
    { "ca_cm_h_t4_c",       "rogue/graph objects/gear/ca_cm_h_t4_c.mgraphobject" },
    { "ca_cm_h_t4_l",       "rogue/graph objects/gear/ca_cm_h_t4_l.mgraphobject" },
    { "ca_cm_h_t5_c",       "rogue/graph objects/gear/ca_cm_h_t5_c.mgraphobject" },
    { "ca_cm_h_t5_l",       "rogue/graph objects/gear/ca_cm_h_t5_l.mgraphobject" },
    { "ca_cm_h_t6_l",       "rogue/graph objects/gear/ca_cm_h_t6_l.mgraphobject" },
    { "ca_cm_h_t6_u",       "rogue/graph objects/gear/ca_cm_h_t6_u.mgraphobject" },
    { "ca_cm_h_t7_l",       "rogue/graph objects/gear/ca_cm_h_t7_l.mgraphobject" },
    { "ca_cm_h_t7_l_dlc1",  "rogue/graph objects/gear/ca_cm_h_t7_l_dlc1.mgraphobject" },
    { "ca_cm_h_t7_r",       "rogue/graph objects/gear/ca_cm_h_t7_r.mgraphobject" },
    { "ca_cm_h_tt_de",      "rogue/graph objects/gear/ca_cm_h_tt_de.mgraphobject" },
    { "ca_cm_h_tt_fm",      "rogue/graph objects/gear/ca_cm_h_tt_fm.mgraphobject" },
    { "ca_cm_h_tt_ls",      "rogue/graph objects/gear/ca_cm_h_tt_ls.mgraphobject" },
    { "ca_cm_h_tt_rc",      "rogue/graph objects/gear/ca_cm_h_tt_rc.mgraphobject" },
    { "ca_cm_h_tx_as_sc",   "rogue/graph objects/gear/ca_cm_h_tx_as_sc.mgraphobject" },
    { "ca_cm_h_tx_gs_uw",   "rogue/graph objects/gear/ca_cm_h_tx_gs_uw.mgraphobject" },
    { "ca_cm_h_tx_mech_01", "rogue/graph objects/gear/ca_cm_h_tx_mech_01.mgraphobject" },
    { "ca_cm_h_tx_mech_02", "rogue/graph objects/gear/ca_cm_h_tx_mech_02.mgraphobject" },
    { "ca_cm_h_tx_nm_uw",   "rogue/graph objects/gear/ca_cm_h_tx_nm_uw.mgraphobject" },
    { "ca_cm_h_tx_ss_uw",   "rogue/graph objects/gear/ca_cm_h_tx_ss_uw.mgraphobject" },
    { "ca_cm_h_tx_ulc_emt", "rogue/graph objects/gear/ca_cm_h_tx_ulc_emt.mgraphobject" },
    { "ca_cm_h_tx_uw_dar",  "rogue/graph objects/gear/ca_cm_h_tx_uw_dar.mgraphobject" },
    { "ca_cm_h_uc_pn",      "rogue/graph objects/gear/ca_cm_h_uc_pn.mgraphobject" },
};

static const SkinnedMeshManager::ModelSwapEntry s_HatModels_[] =
{
    { "ca_cm_h_a",               "rogue/graph objects/gear/ca_cm_h_a.mgraphobject" },
    { "ca_cm_h_b",               "rogue/graph objects/gear/ca_cm_h_b.mgraphobject" },
    { "ca_cm_h_bg_riot",         "rogue/graph objects/gear/ca_cm_h_bg_riot.mgraphobject" },
    { "ca_cm_h_c",               "rogue/graph objects/gear/ca_cm_h_c.mgraphobject" },
    { "ca_cm_h_chase",           "rogue/graph objects/gear/ca_cm_h_chase.mgraphobject" },
    { "ca_cm_h_chase02",         "rogue/graph objects/gear/ca_cm_h_chase02.mgraphobject" },
    { "ca_cm_h_d",               "rogue/graph objects/gear/ca_cm_h_d.mgraphobject" },
    { "ca_cm_h_dlc1",            "rogue/graph objects/gear/ca_cm_h_dlc1.mgraphobject" },
    { "ca_cm_h_e",               "rogue/graph objects/gear/ca_cm_h_e.mgraphobject" },
    { "ca_cm_h_exo_ast",         "rogue/graph objects/gear/ca_cm_h_exo_ast.mgraphobject" },
    { "ca_cm_h_exo_ast2",        "rogue/graph objects/gear/ca_cm_h_exo_ast2.mgraphobject" },
    { "ca_cm_h_f",               "rogue/graph objects/gear/ca_cm_h_f.mgraphobject" },
    { "ca_cm_h_fac_cln",         "rogue/graph objects/gear/ca_cm_h_fac_cln.mgraphobject" },
    { "ca_cm_h_fac_jtf",         "rogue/graph objects/gear/ca_cm_h_fac_jtf.mgraphobject" },
    { "ca_cm_h_fac_lmb",         "rogue/graph objects/gear/ca_cm_h_fac_lmb.mgraphobject" },
    { "ca_cm_h_fac_lmb3",        "rogue/graph objects/gear/ca_cm_h_fac_lmb3.mgraphobject" },
    { "ca_cm_h_fac_rkr",         "rogue/graph objects/gear/ca_cm_h_fac_rkr.mgraphobject" },
    { "ca_cm_h_g",               "rogue/graph objects/gear/ca_cm_h_g.mgraphobject" },
    { "ca_cm_h_gs_fm",           "rogue/graph objects/gear/ca_cm_h_gs_fm.mgraphobject" },
    { "ca_cm_h_gv_al",           "rogue/graph objects/gear/ca_cm_h_gv_al.mgraphobject" },
    { "ca_cm_h_gv_de",           "rogue/graph objects/gear/ca_cm_h_gv_de.mgraphobject" },
    { "ca_cm_h_gv_fc",           "rogue/graph objects/gear/ca_cm_h_gv_fc.mgraphobject" },
    { "ca_cm_h_gv_fl",           "rogue/graph objects/gear/ca_cm_h_gv_fl.mgraphobject" },
    { "ca_cm_h_gv_np",           "rogue/graph objects/gear/ca_cm_h_gv_np.mgraphobject" },
    { "ca_cm_h_gv_rc",           "rogue/graph objects/gear/ca_cm_h_gv_rc.mgraphobject" },
    { "ca_cm_h_gv_sc",           "rogue/graph objects/gear/ca_cm_h_gv_sc.mgraphobject" },
    { "ca_cm_h_gv_st",           "rogue/graph objects/gear/ca_cm_h_gv_st.mgraphobject" },
    { "ca_cm_h_gv_tc",           "rogue/graph objects/gear/ca_cm_h_gv_tc.mgraphobject" },
    { "ca_cm_h_h",               "rogue/graph objects/gear/ca_cm_h_h.mgraphobject" },
    { "ca_cm_h_hol",             "rogue/graph objects/gear/ca_cm_h_hol.mgraphobject" },
    { "ca_cm_h_hun_ft",          "rogue/graph objects/gear/ca_cm_h_hun_ft.mgraphobject" },
    { "ca_cm_h_i",               "rogue/graph objects/gear/ca_cm_h_i.mgraphobject" },
    { "ca_cm_h_j",               "rogue/graph objects/gear/ca_cm_h_j.mgraphobject" },
    { "ca_cm_h_lon_st",          "rogue/graph objects/gear/ca_cm_h_lon_st.mgraphobject" },
    { "ca_cm_h_lsa",             "rogue/graph objects/gear/ca_cm_h_lsa.mgraphobject" },
    { "ca_cm_h_lsb",             "rogue/graph objects/gear/ca_cm_h_lsb.mgraphobject" },
    { "ca_cm_h_mb1_a",           "rogue/graph objects/gear/ca_cm_h_mb1_a.mgraphobject" },
    { "ca_cm_h_mb1_c",           "rogue/graph objects/gear/ca_cm_h_mb1_c.mgraphobject" },
    { "ca_cm_h_mb1_h2",          "rogue/graph objects/gear/ca_cm_h_mb1_h2.mgraphobject" },
    { "ca_cm_h_mb2_a",           "rogue/graph objects/gear/ca_cm_h_mb2_a.mgraphobject" },
    { "ca_cm_h_mb2_b",           "rogue/graph objects/gear/ca_cm_h_mb2_b.mgraphobject" },
    { "ca_cm_h_mb2_c",           "rogue/graph objects/gear/ca_cm_h_mb2_c.mgraphobject" },
    { "ca_cm_h_mb2_d",           "rogue/graph objects/gear/ca_cm_h_mb2_d.mgraphobject" },
    { "ca_cm_h_mb3_a",           "rogue/graph objects/gear/ca_cm_h_mb3_a.mgraphobject" },
    { "ca_cm_h_mb3_b",           "rogue/graph objects/gear/ca_cm_h_mb3_b.mgraphobject" },
    { "ca_cm_h_mb3_c",           "rogue/graph objects/gear/ca_cm_h_mb3_c.mgraphobject" },
    { "ca_cm_h_mb4_a",           "rogue/graph objects/gear/ca_cm_h_mb4_a.mgraphobject" },
    { "ca_cm_h_mb4_b",           "rogue/graph objects/gear/ca_cm_h_mb4_b.mgraphobject" },
    { "ca_cm_h_mb4_c",           "rogue/graph objects/gear/ca_cm_h_mb4_c.mgraphobject" },
    { "ca_cm_h_mb4_d",           "rogue/graph objects/gear/ca_cm_h_mb4_d.mgraphobject" },
    { "ca_cm_h_mb4_e",           "rogue/graph objects/gear/ca_cm_h_mb4_e.mgraphobject" },
    { "ca_cm_h_mb5_a",           "rogue/graph objects/gear/ca_cm_h_mb5_a.mgraphobject" },
    { "ca_cm_h_mb5_c",           "rogue/graph objects/gear/ca_cm_h_mb5_c.mgraphobject" },
    { "ca_cm_h_mb5_d",           "rogue/graph objects/gear/ca_cm_h_mb5_d.mgraphobject" },
    { "ca_cm_h_mb5_e",           "rogue/graph objects/gear/ca_cm_h_mb5_e.mgraphobject" },
    { "ca_cm_h_mbox_bolivia_01", "rogue/graph objects/gear/ca_cm_h_mbox_bolivia_01.mgraphobject" },
    { "ca_cm_h_mbox_miner_01",   "rogue/graph objects/gear/ca_cm_h_mbox_miner_01.mgraphobject" },
    { "ca_cm_h_mbox_trap_01",    "rogue/graph objects/gear/ca_cm_h_mbox_trap_01.mgraphobject" },
    { "ca_cm_h_mc_gn",           "rogue/graph objects/gear/ca_cm_h_mc_gn.mgraphobject" },
    { "ca_cm_h_mc_rt",           "rogue/graph objects/gear/ca_cm_h_mc_rt.mgraphobject" },
    { "ca_cm_h_mil_df",          "rogue/graph objects/gear/ca_cm_h_mil_df.mgraphobject" },
    { "ca_cm_h_mil_dm",          "rogue/graph objects/gear/ca_cm_h_mil_dm.mgraphobject" },
    { "ca_cm_h_mil_pj",          "rogue/graph objects/gear/ca_cm_h_mil_pj.mgraphobject" },
    { "ca_cm_h_mil_sn",          "rogue/graph objects/gear/ca_cm_h_mil_sn.mgraphobject" },
    { "ca_cm_h_mt_rs",           "rogue/graph objects/gear/ca_cm_h_mt_rs.mgraphobject" },
    { "ca_cm_h_peu_fm",          "rogue/graph objects/gear/ca_cm_h_peu_fm.mgraphobject" },
    { "ca_cm_h_peu_mi",          "rogue/graph objects/gear/ca_cm_h_peu_mi.mgraphobject" },
    { "ca_cm_h_peu_ny",          "rogue/graph objects/gear/ca_cm_h_peu_ny.mgraphobject" },
    { "ca_cm_h_peu_pc",          "rogue/graph objects/gear/ca_cm_h_peu_pc.mgraphobject" },
    { "ca_cm_h_pmc2_01",         "rogue/graph objects/gear/ca_cm_h_pmc2_01.mgraphobject" },
    { "ca_cm_h_pmc2_02",         "rogue/graph objects/gear/ca_cm_h_pmc2_02.mgraphobject" },
    { "ca_cm_h_pmc_01",          "rogue/graph objects/gear/ca_cm_h_pmc_01.mgraphobject" },
    { "ca_cm_h_pmc_02",          "rogue/graph objects/gear/ca_cm_h_pmc_02.mgraphobject" },
    { "ca_cm_h_pmc_03",          "rogue/graph objects/gear/ca_cm_h_pmc_03.mgraphobject" },
    { "ca_cm_h_pmc_04",          "rogue/graph objects/gear/ca_cm_h_pmc_04.mgraphobject" },
    { "ca_cm_h_pol_mc",          "rogue/graph objects/gear/ca_cm_h_pol_mc.mgraphobject" },
    { "ca_cm_h_pol_nt",          "rogue/graph objects/gear/ca_cm_h_pol_nt.mgraphobject" },
    { "ca_cm_h_pol_sh",          "rogue/graph objects/gear/ca_cm_h_pol_sh.mgraphobject" },
    { "ca_cm_h_pol_sw",          "rogue/graph objects/gear/ca_cm_h_pol_sw.mgraphobject" },
    { "ca_cm_h_pr_mk",           "rogue/graph objects/gear/ca_cm_h_pr_mk.mgraphobject" },
    { "ca_cm_h_snta",            "rogue/graph objects/gear/ca_cm_h_snta.mgraphobject" },
    { "ca_cm_h_spo_bp",          "rogue/graph objects/gear/ca_cm_h_spo_bp.mgraphobject" },
    { "ca_cm_h_spo_hp",          "rogue/graph objects/gear/ca_cm_h_spo_hp.mgraphobject" },
    { "ca_cm_h_spo_rd",          "rogue/graph objects/gear/ca_cm_h_spo_rd.mgraphobject" },
    { "ca_cm_h_spo_sb",          "rogue/graph objects/gear/ca_cm_h_spo_sb.mgraphobject" },
    { "ca_cm_h_sw_e1",           "rogue/graph objects/gear/ca_cm_h_sw_e1.mgraphobject" },
    { "ca_cm_h_sw_w1",           "rogue/graph objects/gear/ca_cm_h_sw_w1.mgraphobject" },
    { "ca_cm_h_sw_w2",           "rogue/graph objects/gear/ca_cm_h_sw_w2.mgraphobject" },
    { "ca_cm_h_tox",             "rogue/graph objects/gear/ca_cm_h_tox.mgraphobject" },
    { "ca_cm_h_ubi_gr",          "rogue/graph objects/gear/ca_cm_h_ubi_gr.mgraphobject" },
    { "ca_cm_h_ubi_sc",          "rogue/graph objects/gear/ca_cm_h_ubi_sc.mgraphobject" },
    { "ca_cm_h_uc_ge4",          "rogue/graph objects/gear/ca_cm_h_uc_ge4.mgraphobject" },
    { "ca_cm_h_ues_s1",          "rogue/graph objects/gear/ca_cm_h_ues_s1.mgraphobject" },
    { "ca_cm_h_ues_s2",          "rogue/graph objects/gear/ca_cm_h_ues_s2.mgraphobject" },
    { "ca_cm_h_ues_s3",          "rogue/graph objects/gear/ca_cm_h_ues_s3.mgraphobject" },
    { "ca_cm_h_ues_tx",          "rogue/graph objects/gear/ca_cm_h_ues_tx.mgraphobject" },
    { "ca_cm_h_ulc_cent",        "rogue/graph objects/gear/ca_cm_h_ulc_cent.mgraphobject" },
    { "ca_cm_h_ulc_emt",         "rogue/graph objects/gear/ca_cm_h_ulc_emt.mgraphobject" },
    { "ca_cm_h_ulc_fire",        "rogue/graph objects/gear/ca_cm_h_ulc_fire.mgraphobject" },
    { "ca_cm_h_ulc_haz",         "rogue/graph objects/gear/ca_cm_h_ulc_haz.mgraphobject" },
    { "ca_cm_h_ulc_hun",         "rogue/graph objects/gear/ca_cm_h_ulc_hun.mgraphobject" },
    { "ca_cm_h_ulc_jtf",         "rogue/graph objects/gear/ca_cm_h_ulc_jtf.mgraphobject" },
    { "ca_cm_h_ulc_pol",         "rogue/graph objects/gear/ca_cm_h_ulc_pol.mgraphobject" },
    { "ca_cm_h_ulc_rb6",         "rogue/graph objects/gear/ca_cm_h_ulc_rb6.mgraphobject" },
    { "ca_cm_h_ulc_sbow",        "rogue/graph objects/gear/ca_cm_h_ulc_sbow.mgraphobject" },
    { "ca_cm_h_ulc_sur",         "rogue/graph objects/gear/ca_cm_h_ulc_sur.mgraphobject" },
    { "ca_cm_h_usm_de",          "rogue/graph objects/gear/ca_cm_h_usm_de.mgraphobject" },
    { "ca_cm_h_usm_sn",          "rogue/graph objects/gear/ca_cm_h_usm_sn.mgraphobject" },
    { "ca_cm_h_usm_ur",          "rogue/graph objects/gear/ca_cm_h_usm_ur.mgraphobject" },
    { "ca_cm_h_usm_wl",          "rogue/graph objects/gear/ca_cm_h_usm_wl.mgraphobject" },
    { "ca_cm_h_uw_dar",          "rogue/graph objects/gear/ca_cm_h_uw_dar.mgraphobject" },
    { "ca_cm_h_uw_pun",          "rogue/graph objects/gear/ca_cm_h_uw_pun.mgraphobject" },
    { "ca_cm_h_uw_rav",          "rogue/graph objects/gear/ca_cm_h_uw_rav.mgraphobject" },
};

static const SkinnedMeshManager::ModelSwapEntry s_GasMaskModels_[] =
{
    { "ca_hg_gs_uw",    "rogue/graph objects/gear/ca_hg_gs_uw.mgraphobject" },
    { "ca_hg_nm_uw",    "rogue/graph objects/gear/ca_hg_nm_uw.mgraphobject" },
    { "ca_hg_pa_ba",    "rogue/graph objects/gear/ca_hg_pa_ba.mgraphobject" },
    { "ca_hg_pa_d3",    "rogue/graph objects/gear/ca_hg_pa_d3.mgraphobject" },
    { "ca_hg_pa_pr",    "rogue/graph objects/gear/ca_hg_pa_pr.mgraphobject" },
    { "ca_hg_pk_uw",    "rogue/graph objects/gear/ca_hg_pk_uw.mgraphobject" },
    { "ca_hg_rt_fc",    "rogue/graph objects/gear/ca_hg_rt_fc.mgraphobject" },
    { "ca_hg_set01_bg", "rogue/graph objects/gear/ca_hg_set01_bg.mgraphobject" },
    { "ca_hg_set04_bg", "rogue/graph objects/gear/ca_hg_set04_bg.mgraphobject" },
    { "ca_hg_ss_uw",    "rogue/graph objects/gear/ca_hg_ss_uw.mgraphobject" },
    { "ca_hg_uw_dar",   "rogue/graph objects/gear/ca_hg_uw_dar.mgraphobject" },
    { "ca_hg_wd_uw",    "rogue/graph objects/gear/ca_hg_wd_uw.mgraphobject" },
    { "ch_pm_mask_hun", "rogue/graph objects/gear/ch_pm_mask_hun.mgraphobject" },
    { "cp_hg_as_sc",    "rogue/graph objects/gear/cp_hg_as_sc.mgraphobject" },
    { "cp_hg_mm_st",    "rogue/graph objects/gear/cp_hg_mm_st.mgraphobject" },
    { "cp_hg_rt_ab",    "rogue/graph objects/gear/cp_hg_rt_ab.mgraphobject" },
    { "cp_hg_rt_hf",    "rogue/graph objects/gear/cp_hg_rt_hf.mgraphobject" },
    { "cp_hg_rt_ta",    "rogue/graph objects/gear/cp_hg_rt_ta.mgraphobject" },
    { "cp_hg_set02_bg", "rogue/graph objects/gear/cp_hg_set02_bg.mgraphobject" },
    { "cp_hg_set03_bg", "rogue/graph objects/gear/cp_hg_set03_bg.mgraphobject" },
    { "cp_hg_sv_set01", "rogue/graph objects/gear/cp_hg_sv_set01.mgraphobject" },
    { "cp_hg_t1",       "rogue/graph objects/gear/cp_hg_t1.mgraphobject" },
    { "cp_hg_t2",       "rogue/graph objects/gear/cp_hg_t2.mgraphobject" },
    { "cp_hg_t3",       "rogue/graph objects/gear/cp_hg_t3.mgraphobject" },
    { "cp_hg_t4",       "rogue/graph objects/gear/cp_hg_t4.mgraphobject" },
    { "cp_hg_t5_c",     "rogue/graph objects/gear/cp_hg_t5_c.mgraphobject" },
    { "cp_hg_t6",       "rogue/graph objects/gear/cp_hg_t6.mgraphobject" },
    { "cp_hg_t7",       "rogue/graph objects/gear/cp_hg_t7.mgraphobject" },
    { "cp_hg_tt_de",    "rogue/graph objects/gear/cp_hg_tt_de.mgraphobject" },
    { "cp_hg_tt_fm",    "rogue/graph objects/gear/cp_hg_tt_fm.mgraphobject" },
    { "cp_hg_tt_ls",    "rogue/graph objects/gear/cp_hg_tt_ls.mgraphobject" },
    { "cp_hg_tt_rc",    "rogue/graph objects/gear/cp_hg_tt_rc.mgraphobject" },
    { "cp_hg_uc_pn",    "rogue/graph objects/gear/cp_hg_uc_pn.mgraphobject" },
};

static const SkinnedMeshManager::ModelSwapEntry s_ShirtModels_[] =
{
    { "ca_cm_l1_a",        "rogue/graph objects/gear/ca_cm_l1_a.mgraphobject" },
    { "ca_cm_l1_a0",       "rogue/graph objects/gear/ca_cm_l1_a0.mgraphobject" },
    { "ca_cm_l1_b",        "rogue/graph objects/gear/ca_cm_l1_b.mgraphobject" },
    { "ca_cm_l1_bg_riot",  "rogue/graph objects/gear/ca_cm_l1_bg_riot.mgraphobject" },
    { "ca_cm_l1_c",        "rogue/graph objects/gear/ca_cm_l1_c.mgraphobject" },
    { "ca_cm_l1_chase02",  "rogue/graph objects/gear/ca_cm_l1_chase02.mgraphobject" },
    { "ca_cm_l1_d",        "rogue/graph objects/gear/ca_cm_l1_d.mgraphobject" },
    { "ca_cm_l1_e",        "rogue/graph objects/gear/ca_cm_l1_e.mgraphobject" },
    { "ca_cm_l1_exo_ast",  "rogue/graph objects/gear/ca_cm_l1_exo_ast.mgraphobject" },
    { "ca_cm_l1_exo_ast2", "rogue/graph objects/gear/ca_cm_l1_exo_ast2.mgraphobject" },
    { "ca_cm_l1_f",        "rogue/graph objects/gear/ca_cm_l1_f.mgraphobject" },
    { "ca_cm_l1_fac_cln",  "rogue/graph objects/gear/ca_cm_l1_fac_cln.mgraphobject" },
    { "ca_cm_l1_fac_jtf",  "rogue/graph objects/gear/ca_cm_l1_fac_jtf.mgraphobject" },
    { "ca_cm_l1_fac_lmb",  "rogue/graph objects/gear/ca_cm_l1_fac_lmb.mgraphobject" },
    { "ca_cm_l1_fac_lmb3", "rogue/graph objects/gear/ca_cm_l1_fac_lmb3.mgraphobject" },
    { "ca_cm_l1_fac_rkr",  "rogue/graph objects/gear/ca_cm_l1_fac_rkr.mgraphobject" },
    { "ca_cm_l1_g",        "rogue/graph objects/gear/ca_cm_l1_g.mgraphobject" },
    { "ca_cm_l1_gs_fm",    "rogue/graph objects/gear/ca_cm_l1_gs_fm.mgraphobject" },
    { "ca_cm_l1_gv_al",    "rogue/graph objects/gear/ca_cm_l1_gv_al.mgraphobject" },
    { "ca_cm_l1_gv_de",    "rogue/graph objects/gear/ca_cm_l1_gv_de.mgraphobject" },
    { "ca_cm_l1_gv_fc",    "rogue/graph objects/gear/ca_cm_l1_gv_fc.mgraphobject" },
    { "ca_cm_l1_gv_fl",    "rogue/graph objects/gear/ca_cm_l1_gv_fl.mgraphobject" },
    { "ca_cm_l1_gv_np",    "rogue/graph objects/gear/ca_cm_l1_gv_np.mgraphobject" },
    { "ca_cm_l1_gv_rc",    "rogue/graph objects/gear/ca_cm_l1_gv_rc.mgraphobject" },
    { "ca_cm_l1_gv_sc",    "rogue/graph objects/gear/ca_cm_l1_gv_sc.mgraphobject" },
    { "ca_cm_l1_gv_sk",    "rogue/graph objects/gear/ca_cm_l1_gv_sk.mgraphobject" },
    { "ca_cm_l1_gv_st",    "rogue/graph objects/gear/ca_cm_l1_gv_st.mgraphobject" },
    { "ca_cm_l1_gv_tc",    "rogue/graph objects/gear/ca_cm_l1_gv_tc.mgraphobject" },
    { "ca_cm_l1_h",        "rogue/graph objects/gear/ca_cm_l1_h.mgraphobject" },
    { "ca_cm_l1_hol",      "rogue/graph objects/gear/ca_cm_l1_hol.mgraphobject" },
    { "ca_cm_l1_hun_ft",   "rogue/graph objects/gear/ca_cm_l1_hun_ft.mgraphobject" },
    { "ca_cm_l1_i",        "rogue/graph objects/gear/ca_cm_l1_i.mgraphobject" },
    { "ca_cm_l1_j",        "rogue/graph objects/gear/ca_cm_l1_j.mgraphobject" },
    { "ca_cm_l1_k",        "rogue/graph objects/gear/ca_cm_l1_k.mgraphobject" },
    { "ca_cm_l1_lon_st",   "rogue/graph objects/gear/ca_cm_l1_lon_st.mgraphobject" },
    { "ca_cm_l1_lsa",      "rogue/graph objects/gear/ca_cm_l1_lsa.mgraphobject" },
    { "ca_cm_l1_lsb",      "rogue/graph objects/gear/ca_cm_l1_lsb.mgraphobject" },
    { "ca_cm_l1_mb2_a",    "rogue/graph objects/gear/ca_cm_l1_mb2_a.mgraphobject" },
    { "ca_cm_l1_mb2_b",    "rogue/graph objects/gear/ca_cm_l1_mb2_b.mgraphobject" },
    { "ca_cm_l1_mb2_c",    "rogue/graph objects/gear/ca_cm_l1_mb2_c.mgraphobject" },
    { "ca_cm_l1_mb2_d",    "rogue/graph objects/gear/ca_cm_l1_mb2_d.mgraphobject" },
    { "ca_cm_l1_mb5_a",    "rogue/graph objects/gear/ca_cm_l1_mb5_a.mgraphobject" },
    { "ca_cm_l1_mb5_b",    "rogue/graph objects/gear/ca_cm_l1_mb5_b.mgraphobject" },
    { "ca_cm_l1_mb5_c",    "rogue/graph objects/gear/ca_cm_l1_mb5_c.mgraphobject" },
    { "ca_cm_l1_mb5_d",    "rogue/graph objects/gear/ca_cm_l1_mb5_d.mgraphobject" },
    { "ca_cm_l1_mc_gn",    "rogue/graph objects/gear/ca_cm_l1_mc_gn.mgraphobject" },
    { "ca_cm_l1_mc_rt",    "rogue/graph objects/gear/ca_cm_l1_mc_rt.mgraphobject" },
    { "ca_cm_l1_mil_df",   "rogue/graph objects/gear/ca_cm_l1_mil_df.mgraphobject" },
    { "ca_cm_l1_mil_dm",   "rogue/graph objects/gear/ca_cm_l1_mil_dm.mgraphobject" },
    { "ca_cm_l1_mil_pj",   "rogue/graph objects/gear/ca_cm_l1_mil_pj.mgraphobject" },
    { "ca_cm_l1_mil_sn",   "rogue/graph objects/gear/ca_cm_l1_mil_sn.mgraphobject" },
    { "ca_cm_l1_mm",       "rogue/graph objects/gear/ca_cm_l1_mm.mgraphobject" },
    { "ca_cm_l1_mt_rs",    "rogue/graph objects/gear/ca_cm_l1_mt_rs.mgraphobject" },
    { "ca_cm_l1_peu_fm",   "rogue/graph objects/gear/ca_cm_l1_peu_fm.mgraphobject" },
    { "ca_cm_l1_peu_mi",   "rogue/graph objects/gear/ca_cm_l1_peu_mi.mgraphobject" },
    { "ca_cm_l1_peu_ny",   "rogue/graph objects/gear/ca_cm_l1_peu_ny.mgraphobject" },
    { "ca_cm_l1_peu_pc",   "rogue/graph objects/gear/ca_cm_l1_peu_pc.mgraphobject" },
    { "ca_cm_l1_pmc2_01",  "rogue/graph objects/gear/ca_cm_l1_pmc2_01.mgraphobject" },
    { "ca_cm_l1_pmc2_02",  "rogue/graph objects/gear/ca_cm_l1_pmc2_02.mgraphobject" },
    { "ca_cm_l1_pmc_01",   "rogue/graph objects/gear/ca_cm_l1_pmc_01.mgraphobject" },
    { "ca_cm_l1_pmc_02",   "rogue/graph objects/gear/ca_cm_l1_pmc_02.mgraphobject" },
    { "ca_cm_l1_pmc_03",   "rogue/graph objects/gear/ca_cm_l1_pmc_03.mgraphobject" },
    { "ca_cm_l1_pmc_04",   "rogue/graph objects/gear/ca_cm_l1_pmc_04.mgraphobject" },
    { "ca_cm_l1_pol_mc",   "rogue/graph objects/gear/ca_cm_l1_pol_mc.mgraphobject" },
    { "ca_cm_l1_pol_nt",   "rogue/graph objects/gear/ca_cm_l1_pol_nt.mgraphobject" },
    { "ca_cm_l1_pol_sh",   "rogue/graph objects/gear/ca_cm_l1_pol_sh.mgraphobject" },
    { "ca_cm_l1_pol_sw",   "rogue/graph objects/gear/ca_cm_l1_pol_sw.mgraphobject" },
    { "ca_cm_l1_pr_mk",    "rogue/graph objects/gear/ca_cm_l1_pr_mk.mgraphobject" },
    { "ca_cm_l1_spo_bp",   "rogue/graph objects/gear/ca_cm_l1_spo_bp.mgraphobject" },
    { "ca_cm_l1_spo_hp",   "rogue/graph objects/gear/ca_cm_l1_spo_hp.mgraphobject" },
    { "ca_cm_l1_spo_rd",   "rogue/graph objects/gear/ca_cm_l1_spo_rd.mgraphobject" },
    { "ca_cm_l1_spo_sb",   "rogue/graph objects/gear/ca_cm_l1_spo_sb.mgraphobject" },
    { "ca_cm_l1_sss",      "rogue/graph objects/gear/ca_cm_l1_sss.mgraphobject" },
    { "Starting Hoodie",    "rogue/graph objects/gear/ca_cm_l1_start.mgraphobject" },
    { "ca_cm_l1_sw_e1",    "rogue/graph objects/gear/ca_cm_l1_sw_e1.mgraphobject" },
    { "ca_cm_l1_sw_e2",    "rogue/graph objects/gear/ca_cm_l1_sw_e2.mgraphobject" },
    { "ca_cm_l1_sw_w1",    "rogue/graph objects/gear/ca_cm_l1_sw_w1.mgraphobject" },
    { "ca_cm_l1_sw_w2",    "rogue/graph objects/gear/ca_cm_l1_sw_w2.mgraphobject" },
    { "ca_cm_l1_tox",      "rogue/graph objects/gear/ca_cm_l1_tox.mgraphobject" },
    { "ca_cm_l1_ubi_gr",   "rogue/graph objects/gear/ca_cm_l1_ubi_gr.mgraphobject" },
    { "ca_cm_l1_ubi_sc",   "rogue/graph objects/gear/ca_cm_l1_ubi_sc.mgraphobject" },
    { "ca_cm_l1_uc_ge4",   "rogue/graph objects/gear/ca_cm_l1_uc_ge4.mgraphobject" },
    { "ca_cm_l1_uc_phx",   "rogue/graph objects/gear/ca_cm_l1_uc_phx.mgraphobject" },
    { "ca_cm_l1_ues_s1",   "rogue/graph objects/gear/ca_cm_l1_ues_s1.mgraphobject" },
    { "ca_cm_l1_ues_s2",   "rogue/graph objects/gear/ca_cm_l1_ues_s2.mgraphobject" },
    { "ca_cm_l1_ues_s3",   "rogue/graph objects/gear/ca_cm_l1_ues_s3.mgraphobject" },
    { "ca_cm_l1_ues_tx",   "rogue/graph objects/gear/ca_cm_l1_ues_tx.mgraphobject" },
    { "ca_cm_l1_ulc_haz",  "rogue/graph objects/gear/ca_cm_l1_ulc_haz.mgraphobject" },
    { "ca_cm_l1_ulc_hun",  "rogue/graph objects/gear/ca_cm_l1_ulc_hun.mgraphobject" },
    { "ca_cm_l1_ulc_rb6",  "rogue/graph objects/gear/ca_cm_l1_ulc_rb6.mgraphobject" },
    { "ca_cm_l1_ulc_up",   "rogue/graph objects/gear/ca_cm_l1_ulc_up.mgraphobject" },
    { "ca_cm_l1_usm_de",   "rogue/graph objects/gear/ca_cm_l1_usm_de.mgraphobject" },
    { "ca_cm_l1_usm_sn",   "rogue/graph objects/gear/ca_cm_l1_usm_sn.mgraphobject" },
    { "ca_cm_l1_usm_ur",   "rogue/graph objects/gear/ca_cm_l1_usm_ur.mgraphobject" },
    { "ca_cm_l1_usm_wl",   "rogue/graph objects/gear/ca_cm_l1_usm_wl.mgraphobject" },
    { "ca_cm_l1_uw_dar",   "rogue/graph objects/gear/ca_cm_l1_uw_dar.mgraphobject" },
};

static const SkinnedMeshManager::ModelSwapEntry s_FootModels_[] =
{
    { "ca_cm_f_a",              "rogue/graph objects/gear/ca_cm_f_a.mgraphobject" },
    { "ca_cm_f_b",              "rogue/graph objects/gear/ca_cm_f_b.mgraphobject" },
    { "ca_cm_f_bg_riot",        "rogue/graph objects/gear/ca_cm_f_bg_riot.mgraphobject" },
    { "ca_cm_f_c",              "rogue/graph objects/gear/ca_cm_f_c.mgraphobject" },
    { "ca_cm_f_chase",          "rogue/graph objects/gear/ca_cm_f_chase.mgraphobject" },
    { "ca_cm_f_chase02",        "rogue/graph objects/gear/ca_cm_f_chase02.mgraphobject" },
    { "ca_cm_f_d",              "rogue/graph objects/gear/ca_cm_f_d.mgraphobject" },
    { "ca_cm_f_e",              "rogue/graph objects/gear/ca_cm_f_e.mgraphobject" },
    { "ca_cm_f_exo_ast",        "rogue/graph objects/gear/ca_cm_f_exo_ast.mgraphobject" },
    { "ca_cm_f_exo_ast2",       "rogue/graph objects/gear/ca_cm_f_exo_ast2.mgraphobject" },
    { "ca_cm_f_f",              "rogue/graph objects/gear/ca_cm_f_f.mgraphobject" },
    { "ca_cm_f_fac_cln",        "rogue/graph objects/gear/ca_cm_f_fac_cln.mgraphobject" },
    { "ca_cm_f_fac_jtf",        "rogue/graph objects/gear/ca_cm_f_fac_jtf.mgraphobject" },
    { "ca_cm_f_fac_lmb",        "rogue/graph objects/gear/ca_cm_f_fac_lmb.mgraphobject" },
    { "ca_cm_f_fac_lmb3",       "rogue/graph objects/gear/ca_cm_f_fac_lmb3.mgraphobject" },
    { "ca_cm_f_fac_rkr",        "rogue/graph objects/gear/ca_cm_f_fac_rkr.mgraphobject" },
    { "ca_cm_f_g",              "rogue/graph objects/gear/ca_cm_f_g.mgraphobject" },
    { "ca_cm_f_gs_fm",          "rogue/graph objects/gear/ca_cm_f_gs_fm.mgraphobject" },
    { "ca_cm_f_gv_al",          "rogue/graph objects/gear/ca_cm_f_gv_al.mgraphobject" },
    { "ca_cm_f_gv_de",          "rogue/graph objects/gear/ca_cm_f_gv_de.mgraphobject" },
    { "ca_cm_f_gv_fc",          "rogue/graph objects/gear/ca_cm_f_gv_fc.mgraphobject" },
    { "ca_cm_f_gv_fl",          "rogue/graph objects/gear/ca_cm_f_gv_fl.mgraphobject" },
    { "ca_cm_f_gv_np",          "rogue/graph objects/gear/ca_cm_f_gv_np.mgraphobject" },
    { "ca_cm_f_gv_rc",          "rogue/graph objects/gear/ca_cm_f_gv_rc.mgraphobject" },
    { "ca_cm_f_gv_sc",          "rogue/graph objects/gear/ca_cm_f_gv_sc.mgraphobject" },
    { "ca_cm_f_gv_sk",          "rogue/graph objects/gear/ca_cm_f_gv_sk.mgraphobject" },
    { "ca_cm_f_gv_st",          "rogue/graph objects/gear/ca_cm_f_gv_st.mgraphobject" },
    { "ca_cm_f_gv_tc",          "rogue/graph objects/gear/ca_cm_f_gv_tc.mgraphobject" },
    { "ca_cm_f_h",              "rogue/graph objects/gear/ca_cm_f_h.mgraphobject" },
    { "ca_cm_f_hun_ft",         "rogue/graph objects/gear/ca_cm_f_hun_ft.mgraphobject" },
    { "ca_cm_f_i",              "rogue/graph objects/gear/ca_cm_f_i.mgraphobject" },
    { "ca_cm_f_j",              "rogue/graph objects/gear/ca_cm_f_j.mgraphobject" },
    { "ca_cm_f_k",              "rogue/graph objects/gear/ca_cm_f_k.mgraphobject" },
    { "ca_cm_f_l",              "rogue/graph objects/gear/ca_cm_f_l.mgraphobject" },
    { "ca_cm_f_l_dlc1",         "rogue/graph objects/gear/ca_cm_f_l_dlc1.mgraphobject" },
    { "ca_cm_f_lon_st",         "rogue/graph objects/gear/ca_cm_f_lon_st.mgraphobject" },
    { "ca_cm_f_lsa",            "rogue/graph objects/gear/ca_cm_f_lsa.mgraphobject" },
    { "ca_cm_f_lsb",            "rogue/graph objects/gear/ca_cm_f_lsb.mgraphobject" },
    { "ca_cm_f_m",              "rogue/graph objects/gear/ca_cm_f_m.mgraphobject" },
    { "ca_cm_f_mb2_a",          "rogue/graph objects/gear/ca_cm_f_mb2_a.mgraphobject" },
    { "ca_cm_f_mb2_b",          "rogue/graph objects/gear/ca_cm_f_mb2_b.mgraphobject" },
    { "ca_cm_f_mb2_c",          "rogue/graph objects/gear/ca_cm_f_mb2_c.mgraphobject" },
    { "ca_cm_f_mb2_d",          "rogue/graph objects/gear/ca_cm_f_mb2_d.mgraphobject" },
    { "ca_cm_f_mbox_fisher_01", "rogue/graph objects/gear/ca_cm_f_mbox_fisher_01.mgraphobject" },
    { "ca_cm_f_mbox_miner_01",  "rogue/graph objects/gear/ca_cm_f_mbox_miner_01.mgraphobject" },
    { "ca_cm_f_mbox_punk_01",   "rogue/graph objects/gear/ca_cm_f_mbox_punk_01.mgraphobject" },
    { "ca_cm_f_mbox_snow_01",   "rogue/graph objects/gear/ca_cm_f_mbox_snow_01.mgraphobject" },
    { "ca_cm_f_mc_gn",          "rogue/graph objects/gear/ca_cm_f_mc_gn.mgraphobject" },
    { "ca_cm_f_mc_rt",          "rogue/graph objects/gear/ca_cm_f_mc_rt.mgraphobject" },
    { "ca_cm_f_mil_df",         "rogue/graph objects/gear/ca_cm_f_mil_df.mgraphobject" },
    { "ca_cm_f_mil_dm",         "rogue/graph objects/gear/ca_cm_f_mil_dm.mgraphobject" },
    { "ca_cm_f_mil_pj",         "rogue/graph objects/gear/ca_cm_f_mil_pj.mgraphobject" },
    { "ca_cm_f_mil_sn",         "rogue/graph objects/gear/ca_cm_f_mil_sn.mgraphobject" },
    { "ca_cm_f_mm",             "rogue/graph objects/gear/ca_cm_f_mm.mgraphobject" },
    { "ca_cm_f_mt_rs",          "rogue/graph objects/gear/ca_cm_f_mt_rs.mgraphobject" },
    { "ca_cm_f_n",              "rogue/graph objects/gear/ca_cm_f_n.mgraphobject" },
    { "ca_cm_f_peu_mi",         "rogue/graph objects/gear/ca_cm_f_peu_mi.mgraphobject" },
    { "ca_cm_f_peu_ny",         "rogue/graph objects/gear/ca_cm_f_peu_ny.mgraphobject" },
    { "ca_cm_f_pmc2_01",        "rogue/graph objects/gear/ca_cm_f_pmc2_01.mgraphobject" },
    { "ca_cm_f_pmc2_02",        "rogue/graph objects/gear/ca_cm_f_pmc2_02.mgraphobject" },
    { "ca_cm_f_pmc_01",         "rogue/graph objects/gear/ca_cm_f_pmc_01.mgraphobject" },
    { "ca_cm_f_pmc_02",         "rogue/graph objects/gear/ca_cm_f_pmc_02.mgraphobject" },
    { "ca_cm_f_pmc_03",         "rogue/graph objects/gear/ca_cm_f_pmc_03.mgraphobject" },
    { "ca_cm_f_pmc_04",         "rogue/graph objects/gear/ca_cm_f_pmc_04.mgraphobject" },
    { "ca_cm_f_pol_sh",         "rogue/graph objects/gear/ca_cm_f_pol_sh.mgraphobject" },
    { "ca_cm_f_pol_sw",         "rogue/graph objects/gear/ca_cm_f_pol_sw.mgraphobject" },
    { "ca_cm_f_pr_mk",          "rogue/graph objects/gear/ca_cm_f_pr_mk.mgraphobject" },
    { "ca_cm_f_snta",           "rogue/graph objects/gear/ca_cm_f_snta.mgraphobject" },
    { "ca_cm_f_spo_bp",         "rogue/graph objects/gear/ca_cm_f_spo_bp.mgraphobject" },
    { "ca_cm_f_spo_hp",         "rogue/graph objects/gear/ca_cm_f_spo_hp.mgraphobject" },
    { "ca_cm_f_spo_rd",         "rogue/graph objects/gear/ca_cm_f_spo_rd.mgraphobject" },
    { "ca_cm_f_spo_sb",         "rogue/graph objects/gear/ca_cm_f_spo_sb.mgraphobject" },
    { "ca_cm_f_sw_e1",          "rogue/graph objects/gear/ca_cm_f_sw_e1.mgraphobject" },
    { "ca_cm_f_sw_e2",          "rogue/graph objects/gear/ca_cm_f_sw_e2.mgraphobject" },
    { "ca_cm_f_sw_w1",          "rogue/graph objects/gear/ca_cm_f_sw_w1.mgraphobject" },
    { "ca_cm_f_sw_w2",          "rogue/graph objects/gear/ca_cm_f_sw_w2.mgraphobject" },
    { "ca_cm_f_tox",            "rogue/graph objects/gear/ca_cm_f_tox.mgraphobject" },
    { "ca_cm_f_ubi_gr",         "rogue/graph objects/gear/ca_cm_f_ubi_gr.mgraphobject" },
    { "ca_cm_f_ubi_sc",         "rogue/graph objects/gear/ca_cm_f_ubi_sc.mgraphobject" },
    { "ca_cm_f_uc_ge4",         "rogue/graph objects/gear/ca_cm_f_uc_ge4.mgraphobject" },
    { "ca_cm_f_uc_phx",         "rogue/graph objects/gear/ca_cm_f_uc_phx.mgraphobject" },
    { "ca_cm_f_ues_tx",         "rogue/graph objects/gear/ca_cm_f_ues_tx.mgraphobject" },
    { "ca_cm_f_ulc_emt",        "rogue/graph objects/gear/ca_cm_f_ulc_emt.mgraphobject" },
    { "ca_cm_f_ulc_fire",       "rogue/graph objects/gear/ca_cm_f_ulc_fire.mgraphobject" },
    { "ca_cm_f_ulc_haz",        "rogue/graph objects/gear/ca_cm_f_ulc_haz.mgraphobject" },
    { "ca_cm_f_ulc_hun",        "rogue/graph objects/gear/ca_cm_f_ulc_hun.mgraphobject" },
    { "ca_cm_f_ulc_jtf",        "rogue/graph objects/gear/ca_cm_f_ulc_jtf.mgraphobject" },
    { "ca_cm_f_ulc_pol",        "rogue/graph objects/gear/ca_cm_f_ulc_pol.mgraphobject" },
    { "ca_cm_f_ulc_sur",        "rogue/graph objects/gear/ca_cm_f_ulc_sur.mgraphobject" },
    { "ca_cm_f_ulc_up",         "rogue/graph objects/gear/ca_cm_f_ulc_up.mgraphobject" },
    { "ca_cm_f_usm_de",         "rogue/graph objects/gear/ca_cm_f_usm_de.mgraphobject" },
    { "ca_cm_f_usm_sn",         "rogue/graph objects/gear/ca_cm_f_usm_sn.mgraphobject" },
    { "ca_cm_f_usm_ur",         "rogue/graph objects/gear/ca_cm_f_usm_ur.mgraphobject" },
    { "ca_cm_f_usm_wl",         "rogue/graph objects/gear/ca_cm_f_usm_wl.mgraphobject" },
    { "ca_cm_f_uw_dar",         "rogue/graph objects/gear/ca_cm_f_uw_dar.mgraphobject" },
    { "ca_cm_f_uw_poi",         "rogue/graph objects/gear/ca_cm_f_uw_poi.mgraphobject" },
    { "ca_cm_f_uw_pun",         "rogue/graph objects/gear/ca_cm_f_uw_pun.mgraphobject" },
    { "ca_cm_f_uw_rav",         "rogue/graph objects/gear/ca_cm_f_uw_rav.mgraphobject" },
};

static const SkinnedMeshManager::ModelSwapEntry s_ScarfModels_[] =
{
    { "ca_cm_s_a",               "rogue/graph objects/gear/ca_cm_s_a.mgraphobject" },
    { "ca_cm_s_b",               "rogue/graph objects/gear/ca_cm_s_b.mgraphobject" },
    { "ca_cm_s_bg_riot",         "rogue/graph objects/gear/ca_cm_s_bg_riot.mgraphobject" },
    { "ca_cm_s_c",               "rogue/graph objects/gear/ca_cm_s_c.mgraphobject" },
    { "ca_cm_s_chase",           "rogue/graph objects/gear/ca_cm_s_chase.mgraphobject" },
    { "ca_cm_s_d",               "rogue/graph objects/gear/ca_cm_s_d.mgraphobject" },
    { "ca_cm_s_e",               "rogue/graph objects/gear/ca_cm_s_e.mgraphobject" },
    { "ca_cm_s_exo_ast",         "rogue/graph objects/gear/ca_cm_s_exo_ast.mgraphobject" },
    { "ca_cm_s_exo_ast2",        "rogue/graph objects/gear/ca_cm_s_exo_ast2.mgraphobject" },
    { "ca_cm_s_f",               "rogue/graph objects/gear/ca_cm_s_f.mgraphobject" },
    { "ca_cm_s_fac_cln",         "rogue/graph objects/gear/ca_cm_s_fac_cln.mgraphobject" },
    { "ca_cm_s_fac_jtf",         "rogue/graph objects/gear/ca_cm_s_fac_jtf.mgraphobject" },
    { "ca_cm_s_fac_lmb",         "rogue/graph objects/gear/ca_cm_s_fac_lmb.mgraphobject" },
    { "ca_cm_s_fac_lmb3",        "rogue/graph objects/gear/ca_cm_s_fac_lmb3.mgraphobject" },
    { "ca_cm_s_g",               "rogue/graph objects/gear/ca_cm_s_g.mgraphobject" },
    { "ca_cm_s_g_dlc1",          "rogue/graph objects/gear/ca_cm_s_g_dlc1.mgraphobject" },
    { "ca_cm_s_gs_fm",           "rogue/graph objects/gear/ca_cm_s_gs_fm.mgraphobject" },
    { "ca_cm_s_gv_al",           "rogue/graph objects/gear/ca_cm_s_gv_al.mgraphobject" },
    { "ca_cm_s_gv_fc",           "rogue/graph objects/gear/ca_cm_s_gv_fc.mgraphobject" },
    { "ca_cm_s_gv_fl",           "rogue/graph objects/gear/ca_cm_s_gv_fl.mgraphobject" },
    { "ca_cm_s_gv_np",           "rogue/graph objects/gear/ca_cm_s_gv_np.mgraphobject" },
    { "ca_cm_s_gv_rc",           "rogue/graph objects/gear/ca_cm_s_gv_rc.mgraphobject" },
    { "ca_cm_s_gv_sc",           "rogue/graph objects/gear/ca_cm_s_gv_sc.mgraphobject" },
    { "ca_cm_s_gv_st",           "rogue/graph objects/gear/ca_cm_s_gv_st.mgraphobject" },
    { "ca_cm_s_gv_tc",           "rogue/graph objects/gear/ca_cm_s_gv_tc.mgraphobject" },
    { "ca_cm_s_h",               "rogue/graph objects/gear/ca_cm_s_h.mgraphobject" },
    { "ca_cm_s_hun_ft",          "rogue/graph objects/gear/ca_cm_s_hun_ft.mgraphobject" },
    { "ca_cm_s_lon_st",          "rogue/graph objects/gear/ca_cm_s_lon_st.mgraphobject" },
    { "ca_cm_s_mb2_a",           "rogue/graph objects/gear/ca_cm_s_mb2_a.mgraphobject" },
    { "ca_cm_s_mb2_b",           "rogue/graph objects/gear/ca_cm_s_mb2_b.mgraphobject" },
    { "ca_cm_s_mb2_c",           "rogue/graph objects/gear/ca_cm_s_mb2_c.mgraphobject" },
    { "ca_cm_s_mb2_d",           "rogue/graph objects/gear/ca_cm_s_mb2_d.mgraphobject" },
    { "ca_cm_s_mbox_bolivia_01", "rogue/graph objects/gear/ca_cm_s_mbox_bolivia_01.mgraphobject" },
    { "ca_cm_s_mbox_earmf_01",   "rogue/graph objects/gear/ca_cm_s_mbox_earmf_01.mgraphobject" },
    { "ca_cm_s_mbox_headphn_01", "rogue/graph objects/gear/ca_cm_s_mbox_headphn_01.mgraphobject" },
    { "ca_cm_s_mc_gn",           "rogue/graph objects/gear/ca_cm_s_mc_gn.mgraphobject" },
    { "ca_cm_s_mc_rt",           "rogue/graph objects/gear/ca_cm_s_mc_rt.mgraphobject" },
    { "ca_cm_s_mil_df",          "rogue/graph objects/gear/ca_cm_s_mil_df.mgraphobject" },
    { "ca_cm_s_mil_dm",          "rogue/graph objects/gear/ca_cm_s_mil_dm.mgraphobject" },
    { "ca_cm_s_mil_pj",          "rogue/graph objects/gear/ca_cm_s_mil_pj.mgraphobject" },
    { "ca_cm_s_mil_sn",          "rogue/graph objects/gear/ca_cm_s_mil_sn.mgraphobject" },
    { "ca_cm_s_pmc2_01",         "rogue/graph objects/gear/ca_cm_s_pmc2_01.mgraphobject" },
    { "ca_cm_s_pmc2_02",         "rogue/graph objects/gear/ca_cm_s_pmc2_02.mgraphobject" },
    { "ca_cm_s_pmc_01",          "rogue/graph objects/gear/ca_cm_s_pmc_01.mgraphobject" },
    { "ca_cm_s_pmc_02",          "rogue/graph objects/gear/ca_cm_s_pmc_02.mgraphobject" },
    { "ca_cm_s_pmc_03",          "rogue/graph objects/gear/ca_cm_s_pmc_03.mgraphobject" },
    { "ca_cm_s_pol_mc",          "rogue/graph objects/gear/ca_cm_s_pol_mc.mgraphobject" },
    { "ca_cm_s_pol_sw",          "rogue/graph objects/gear/ca_cm_s_pol_sw.mgraphobject" },
    { "ca_cm_s_pr_mk_01",        "rogue/graph objects/gear/ca_cm_s_pr_mk_01.mgraphobject" },
    { "ca_cm_s_snta",            "rogue/graph objects/gear/ca_cm_s_snta.mgraphobject" },
    { "ca_cm_s_usm_de",          "rogue/graph objects/gear/ca_cm_s_usm_de.mgraphobject" },
    { "ca_cm_s_usm_sn",          "rogue/graph objects/gear/ca_cm_s_usm_sn.mgraphobject" },
    { "ca_cm_s_usm_ur",          "rogue/graph objects/gear/ca_cm_s_usm_ur.mgraphobject" },
    { "ca_cm_s_usm_wl",          "rogue/graph objects/gear/ca_cm_s_usm_wl.mgraphobject" },
    { "ca_cm_s_uw_dar",          "rogue/graph objects/gear/ca_cm_s_uw_dar.mgraphobject" },
};

static const SkinnedMeshManager::ModelSwapEntry s_PantsModels_[] =
{
    { "ca_cm_p_a",        "rogue/graph objects/gear/ca_cm_p_a.mgraphobject" },
    { "ca_cm_p_b",        "rogue/graph objects/gear/ca_cm_p_b.mgraphobject" },
    { "ca_cm_p_bg_riot",  "rogue/graph objects/gear/ca_cm_p_bg_riot.mgraphobject" },
    { "ca_cm_p_c",        "rogue/graph objects/gear/ca_cm_p_c.mgraphobject" },
    { "ca_cm_p_chase",    "rogue/graph objects/gear/ca_cm_p_chase.mgraphobject" },
    { "ca_cm_p_chase02",  "rogue/graph objects/gear/ca_cm_p_chase02.mgraphobject" },
    { "ca_cm_p_d",        "rogue/graph objects/gear/ca_cm_p_d.mgraphobject" },
    { "ca_cm_p_d_dlc1",   "rogue/graph objects/gear/ca_cm_p_d_dlc1.mgraphobject" },
    { "ca_cm_p_e",        "rogue/graph objects/gear/ca_cm_p_e.mgraphobject" },
    { "ca_cm_p_exo_ast",  "rogue/graph objects/gear/ca_cm_p_exo_ast.mgraphobject" },
    { "ca_cm_p_exo_ast2", "rogue/graph objects/gear/ca_cm_p_exo_ast2.mgraphobject" },
    { "ca_cm_p_f",        "rogue/graph objects/gear/ca_cm_p_f.mgraphobject" },
    { "ca_cm_p_fac_cln",  "rogue/graph objects/gear/ca_cm_p_fac_cln.mgraphobject" },
    { "ca_cm_p_fac_jtf",  "rogue/graph objects/gear/ca_cm_p_fac_jtf.mgraphobject" },
    { "ca_cm_p_fac_lmb",  "rogue/graph objects/gear/ca_cm_p_fac_lmb.mgraphobject" },
    { "ca_cm_p_fac_lmb3", "rogue/graph objects/gear/ca_cm_p_fac_lmb3.mgraphobject" },
    { "ca_cm_p_fac_rkr",  "rogue/graph objects/gear/ca_cm_p_fac_rkr.mgraphobject" },
    { "ca_cm_p_g",        "rogue/graph objects/gear/ca_cm_p_g.mgraphobject" },
    { "ca_cm_p_gs_fm",    "rogue/graph objects/gear/ca_cm_p_gs_fm.mgraphobject" },
    { "ca_cm_p_gv_al",    "rogue/graph objects/gear/ca_cm_p_gv_al.mgraphobject" },
    { "ca_cm_p_gv_de",    "rogue/graph objects/gear/ca_cm_p_gv_de.mgraphobject" },
    { "ca_cm_p_gv_fc",    "rogue/graph objects/gear/ca_cm_p_gv_fc.mgraphobject" },
    { "ca_cm_p_gv_fl",    "rogue/graph objects/gear/ca_cm_p_gv_fl.mgraphobject" },
    { "ca_cm_p_gv_np",    "rogue/graph objects/gear/ca_cm_p_gv_np.mgraphobject" },
    { "ca_cm_p_gv_rc",    "rogue/graph objects/gear/ca_cm_p_gv_rc.mgraphobject" },
    { "ca_cm_p_gv_sc",    "rogue/graph objects/gear/ca_cm_p_gv_sc.mgraphobject" },
    { "ca_cm_p_gv_sk",    "rogue/graph objects/gear/ca_cm_p_gv_sk.mgraphobject" },
    { "ca_cm_p_gv_st",    "rogue/graph objects/gear/ca_cm_p_gv_st.mgraphobject" },
    { "ca_cm_p_gv_tc",    "rogue/graph objects/gear/ca_cm_p_gv_tc.mgraphobject" },
    { "ca_cm_p_h",        "rogue/graph objects/gear/ca_cm_p_h.mgraphobject" },
    { "ca_cm_p_hun_ft",   "rogue/graph objects/gear/ca_cm_p_hun_ft.mgraphobject" },
    { "ca_cm_p_i",        "rogue/graph objects/gear/ca_cm_p_i.mgraphobject" },
    { "ca_cm_p_j",        "rogue/graph objects/gear/ca_cm_p_j.mgraphobject" },
    { "ca_cm_p_lon_st",   "rogue/graph objects/gear/ca_cm_p_lon_st.mgraphobject" },
    { "ca_cm_p_lsa",      "rogue/graph objects/gear/ca_cm_p_lsa.mgraphobject" },
    { "ca_cm_p_lsb",      "rogue/graph objects/gear/ca_cm_p_lsb.mgraphobject" },
    { "ca_cm_p_mb2_a",    "rogue/graph objects/gear/ca_cm_p_mb2_a.mgraphobject" },
    { "ca_cm_p_mb2_b",    "rogue/graph objects/gear/ca_cm_p_mb2_b.mgraphobject" },
    { "ca_cm_p_mb2_c",    "rogue/graph objects/gear/ca_cm_p_mb2_c.mgraphobject" },
    { "ca_cm_p_mb2_d",    "rogue/graph objects/gear/ca_cm_p_mb2_d.mgraphobject" },
    { "ca_cm_p_mc_gn",    "rogue/graph objects/gear/ca_cm_p_mc_gn.mgraphobject" },
    { "ca_cm_p_mc_rt",    "rogue/graph objects/gear/ca_cm_p_mc_rt.mgraphobject" },
    { "ca_cm_p_mil_df",   "rogue/graph objects/gear/ca_cm_p_mil_df.mgraphobject" },
    { "ca_cm_p_mil_dm",   "rogue/graph objects/gear/ca_cm_p_mil_dm.mgraphobject" },
    { "ca_cm_p_mil_pj",   "rogue/graph objects/gear/ca_cm_p_mil_pj.mgraphobject" },
    { "ca_cm_p_mil_sn",   "rogue/graph objects/gear/ca_cm_p_mil_sn.mgraphobject" },
    { "ca_cm_p_mm",       "rogue/graph objects/gear/ca_cm_p_mm.mgraphobject" },
    { "ca_cm_p_mt_rs",    "rogue/graph objects/gear/ca_cm_p_mt_rs.mgraphobject" },
    { "ca_cm_p_peu_fm",   "rogue/graph objects/gear/ca_cm_p_peu_fm.mgraphobject" },
    { "ca_cm_p_peu_mi",   "rogue/graph objects/gear/ca_cm_p_peu_mi.mgraphobject" },
    { "ca_cm_p_peu_ny",   "rogue/graph objects/gear/ca_cm_p_peu_ny.mgraphobject" },
    { "ca_cm_p_peu_pc",   "rogue/graph objects/gear/ca_cm_p_peu_pc.mgraphobject" },
    { "ca_cm_p_pmc2_01",  "rogue/graph objects/gear/ca_cm_p_pmc2_01.mgraphobject" },
    { "ca_cm_p_pmc2_02",  "rogue/graph objects/gear/ca_cm_p_pmc2_02.mgraphobject" },
    { "ca_cm_p_pmc_01",   "rogue/graph objects/gear/ca_cm_p_pmc_01.mgraphobject" },
    { "ca_cm_p_pmc_02",   "rogue/graph objects/gear/ca_cm_p_pmc_02.mgraphobject" },
    { "ca_cm_p_pmc_03",   "rogue/graph objects/gear/ca_cm_p_pmc_03.mgraphobject" },
    { "ca_cm_p_pmc_04",   "rogue/graph objects/gear/ca_cm_p_pmc_04.mgraphobject" },
    { "ca_cm_p_pol_mc",   "rogue/graph objects/gear/ca_cm_p_pol_mc.mgraphobject" },
    { "ca_cm_p_pol_nt",   "rogue/graph objects/gear/ca_cm_p_pol_nt.mgraphobject" },
    { "ca_cm_p_pol_sh",   "rogue/graph objects/gear/ca_cm_p_pol_sh.mgraphobject" },
    { "ca_cm_p_pol_sw",   "rogue/graph objects/gear/ca_cm_p_pol_sw.mgraphobject" },
    { "ca_cm_p_pr_mk",    "rogue/graph objects/gear/ca_cm_p_pr_mk.mgraphobject" },
    { "ca_cm_p_snta",     "rogue/graph objects/gear/ca_cm_p_snta.mgraphobject" },
    { "ca_cm_p_spo_bp",   "rogue/graph objects/gear/ca_cm_p_spo_bp.mgraphobject" },
    { "ca_cm_p_spo_hp",   "rogue/graph objects/gear/ca_cm_p_spo_hp.mgraphobject" },
    { "ca_cm_p_spo_rd",   "rogue/graph objects/gear/ca_cm_p_spo_rd.mgraphobject" },
    { "ca_cm_p_spo_sb",   "rogue/graph objects/gear/ca_cm_p_spo_sb.mgraphobject" },
    { "ca_cm_p_sss",      "rogue/graph objects/gear/ca_cm_p_sss.mgraphobject" },
    { "ca_cm_p_sw_e1",    "rogue/graph objects/gear/ca_cm_p_sw_e1.mgraphobject" },
    { "ca_cm_p_sw_e2",    "rogue/graph objects/gear/ca_cm_p_sw_e2.mgraphobject" },
    { "ca_cm_p_sw_w1",    "rogue/graph objects/gear/ca_cm_p_sw_w1.mgraphobject" },
    { "ca_cm_p_sw_w2",    "rogue/graph objects/gear/ca_cm_p_sw_w2.mgraphobject" },
    { "ca_cm_p_tox",      "rogue/graph objects/gear/ca_cm_p_tox.mgraphobject" },
    { "ca_cm_p_ubi_gr",   "rogue/graph objects/gear/ca_cm_p_ubi_gr.mgraphobject" },
    { "ca_cm_p_ubi_sc",   "rogue/graph objects/gear/ca_cm_p_ubi_sc.mgraphobject" },
    { "ca_cm_p_uc_ge4",   "rogue/graph objects/gear/ca_cm_p_uc_ge4.mgraphobject" },
    { "ca_cm_p_uc_phx",   "rogue/graph objects/gear/ca_cm_p_uc_phx.mgraphobject" },
    { "ca_cm_p_ues_s1",   "rogue/graph objects/gear/ca_cm_p_ues_s1.mgraphobject" },
    { "ca_cm_p_ues_s2",   "rogue/graph objects/gear/ca_cm_p_ues_s2.mgraphobject" },
    { "ca_cm_p_ues_s3",   "rogue/graph objects/gear/ca_cm_p_ues_s3.mgraphobject" },
    { "ca_cm_p_ues_tx",   "rogue/graph objects/gear/ca_cm_p_ues_tx.mgraphobject" },
    { "ca_cm_p_ulc_emt",  "rogue/graph objects/gear/ca_cm_p_ulc_emt.mgraphobject" },
    { "ca_cm_p_ulc_fire", "rogue/graph objects/gear/ca_cm_p_ulc_fire.mgraphobject" },
    { "ca_cm_p_ulc_haz",  "rogue/graph objects/gear/ca_cm_p_ulc_haz.mgraphobject" },
    { "ca_cm_p_ulc_hun",  "rogue/graph objects/gear/ca_cm_p_ulc_hun.mgraphobject" },
    { "ca_cm_p_ulc_jtf",  "rogue/graph objects/gear/ca_cm_p_ulc_jtf.mgraphobject" },
    { "ca_cm_p_ulc_pol",  "rogue/graph objects/gear/ca_cm_p_ulc_pol.mgraphobject" },
    { "ca_cm_p_ulc_rb6",  "rogue/graph objects/gear/ca_cm_p_ulc_rb6.mgraphobject" },
    { "ca_cm_p_ulc_up",   "rogue/graph objects/gear/ca_cm_p_ulc_up.mgraphobject" },
    { "ca_cm_p_usm_des",  "rogue/graph objects/gear/ca_cm_p_usm_des.mgraphobject" },
    { "ca_cm_p_usm_sn",   "rogue/graph objects/gear/ca_cm_p_usm_sn.mgraphobject" },
    { "ca_cm_p_usm_ur",   "rogue/graph objects/gear/ca_cm_p_usm_ur.mgraphobject" },
    { "ca_cm_p_usm_wl",   "rogue/graph objects/gear/ca_cm_p_usm_wl.mgraphobject" },
    { "ca_cm_p_uw_dar",   "rogue/graph objects/gear/ca_cm_p_uw_dar.mgraphobject" },
    { "ca_cm_p_uw_pun",   "rogue/graph objects/gear/ca_cm_p_uw_pun.mgraphobject" },
};

static const SkinnedMeshManager::ModelSwapEntry s_JacketModels_[] =
{
    { "ca_cm_l3_a",               "rogue/graph objects/gear/ca_cm_l3_a.mgraphobject" },
    { "ca_cm_l3_b",               "rogue/graph objects/gear/ca_cm_l3_b.mgraphobject" },
    { "ca_cm_l3_bg_riot",         "rogue/graph objects/gear/ca_cm_l3_bg_riot.mgraphobject" },
    { "ca_cm_l3_c",               "rogue/graph objects/gear/ca_cm_l3_c.mgraphobject" },
    { "ca_cm_l3_chase",           "rogue/graph objects/gear/ca_cm_l3_chase.mgraphobject" },
    { "ca_cm_l3_chase02",         "rogue/graph objects/gear/ca_cm_l3_chase02.mgraphobject" },
    { "ca_cm_l3_d",               "rogue/graph objects/gear/ca_cm_l3_d.mgraphobject" },
    { "ca_cm_l3_e",               "rogue/graph objects/gear/ca_cm_l3_e.mgraphobject" },
    { "ca_cm_l3_f",               "rogue/graph objects/gear/ca_cm_l3_f.mgraphobject" },
    { "ca_cm_l3_fac_cln",         "rogue/graph objects/gear/ca_cm_l3_fac_cln.mgraphobject" },
    { "ca_cm_l3_fac_jtf",         "rogue/graph objects/gear/ca_cm_l3_fac_jtf.mgraphobject" },
    { "ca_cm_l3_fac_lmb",         "rogue/graph objects/gear/ca_cm_l3_fac_lmb.mgraphobject" },
    { "ca_cm_l3_fac_lmb3",        "rogue/graph objects/gear/ca_cm_l3_fac_lmb3.mgraphobject" },
    { "ca_cm_l3_fac_rkr",         "rogue/graph objects/gear/ca_cm_l3_fac_rkr.mgraphobject" },
    { "ca_cm_l3_g",               "rogue/graph objects/gear/ca_cm_l3_g.mgraphobject" },
    { "ca_cm_l3_h",               "rogue/graph objects/gear/ca_cm_l3_h.mgraphobject" },
    { "ca_cm_l3_i",               "rogue/graph objects/gear/ca_cm_l3_i.mgraphobject" },
    { "ca_cm_l3_j",               "rogue/graph objects/gear/ca_cm_l3_j.mgraphobject" },
    { "ca_cm_l3_k",               "rogue/graph objects/gear/ca_cm_l3_k.mgraphobject" },
    { "ca_cm_l3_l",               "rogue/graph objects/gear/ca_cm_l3_l.mgraphobject" },
    { "ca_cm_l3_lon_st",          "rogue/graph objects/gear/ca_cm_l3_lon_st.mgraphobject" },
    { "ca_cm_l3_lsa",             "rogue/graph objects/gear/ca_cm_l3_lsa.mgraphobject" },
    { "ca_cm_l3_lsb",             "rogue/graph objects/gear/ca_cm_l3_lsb.mgraphobject" },
    { "ca_cm_l3_m",               "rogue/graph objects/gear/ca_cm_l3_m.mgraphobject" },
    { "ca_cm_l3_mb1_a",           "rogue/graph objects/gear/ca_cm_l3_mb1_a.mgraphobject" },
    { "ca_cm_l3_mb1_c",           "rogue/graph objects/gear/ca_cm_l3_mb1_c.mgraphobject" },
    { "ca_cm_l3_mb1_j2",          "rogue/graph objects/gear/ca_cm_l3_mb1_j2.mgraphobject" },
    { "ca_cm_l3_mb2_a",           "rogue/graph objects/gear/ca_cm_l3_mb2_a.mgraphobject" },
    { "ca_cm_l3_mb2_b",           "rogue/graph objects/gear/ca_cm_l3_mb2_b.mgraphobject" },
    { "ca_cm_l3_mb2_c",           "rogue/graph objects/gear/ca_cm_l3_mb2_c.mgraphobject" },
    { "ca_cm_l3_mb2_d",           "rogue/graph objects/gear/ca_cm_l3_mb2_d.mgraphobject" },
    { "ca_cm_l3_mb3_a",           "rogue/graph objects/gear/ca_cm_l3_mb3_a.mgraphobject" },
    { "ca_cm_l3_mb3_b",           "rogue/graph objects/gear/ca_cm_l3_mb3_b.mgraphobject" },
    { "ca_cm_l3_mb3_c",           "rogue/graph objects/gear/ca_cm_l3_mb3_c.mgraphobject" },
    { "ca_cm_l3_mb4_a",           "rogue/graph objects/gear/ca_cm_l3_mb4_a.mgraphobject" },
    { "ca_cm_l3_mb4_b",           "rogue/graph objects/gear/ca_cm_l3_mb4_b.mgraphobject" },
    { "ca_cm_l3_mb4_c",           "rogue/graph objects/gear/ca_cm_l3_mb4_c.mgraphobject" },
    { "ca_cm_l3_mb4_d",           "rogue/graph objects/gear/ca_cm_l3_mb4_d.mgraphobject" },
    { "ca_cm_l3_mb4_e",           "rogue/graph objects/gear/ca_cm_l3_mb4_e.mgraphobject" },
    { "ca_cm_l3_mb5_a",           "rogue/graph objects/gear/ca_cm_l3_mb5_a.mgraphobject" },
    { "ca_cm_l3_mb5_b",           "rogue/graph objects/gear/ca_cm_l3_mb5_b.mgraphobject" },
    { "ca_cm_l3_mb5_c",           "rogue/graph objects/gear/ca_cm_l3_mb5_c.mgraphobject" },
    { "ca_cm_l3_mb5_d",           "rogue/graph objects/gear/ca_cm_l3_mb5_d.mgraphobject" },
    { "ca_cm_l3_mb5_e",           "rogue/graph objects/gear/ca_cm_l3_mb5_e.mgraphobject" },
    { "ca_cm_l3_mbox_ghillie_01", "rogue/graph objects/gear/ca_cm_l3_mbox_ghillie_01.mgraphobject" },
    { "ca_cm_l3_mbox_reflex_01",  "rogue/graph objects/gear/ca_cm_l3_mbox_reflex_01.mgraphobject" },
    { "ca_cm_l3_mbox_varsity_01", "rogue/graph objects/gear/ca_cm_l3_mbox_varsity_01.mgraphobject" },
    { "ca_cm_l3_mc_gn",           "rogue/graph objects/gear/ca_cm_l3_mc_gn.mgraphobject" },
    { "ca_cm_l3_mc_rt",           "rogue/graph objects/gear/ca_cm_l3_mc_rt.mgraphobject" },
    { "ca_cm_l3_mil_df",          "rogue/graph objects/gear/ca_cm_l3_mil_df.mgraphobject" },
    { "ca_cm_l3_mil_dm",          "rogue/graph objects/gear/ca_cm_l3_mil_dm.mgraphobject" },
    { "ca_cm_l3_mil_pj",          "rogue/graph objects/gear/ca_cm_l3_mil_pj.mgraphobject" },
    { "ca_cm_l3_mil_sn",          "rogue/graph objects/gear/ca_cm_l3_mil_sn.mgraphobject" },
    { "ca_cm_l3_mt_rs",           "rogue/graph objects/gear/ca_cm_l3_mt_rs.mgraphobject" },
    { "ca_cm_l3_n",               "rogue/graph objects/gear/ca_cm_l3_n.mgraphobject" },
    { "ca_cm_l3_o",               "rogue/graph objects/gear/ca_cm_l3_o.mgraphobject" },
    { "ca_cm_l3_peu_fm",          "rogue/graph objects/gear/ca_cm_l3_peu_fm.mgraphobject" },
    { "ca_cm_l3_peu_mi",          "rogue/graph objects/gear/ca_cm_l3_peu_mi.mgraphobject" },
    { "ca_cm_l3_peu_ny",          "rogue/graph objects/gear/ca_cm_l3_peu_ny.mgraphobject" },
    { "ca_cm_l3_peu_pc",          "rogue/graph objects/gear/ca_cm_l3_peu_pc.mgraphobject" },
    { "ca_cm_l3_pol_mc",          "rogue/graph objects/gear/ca_cm_l3_pol_mc.mgraphobject" },
    { "ca_cm_l3_pol_nt",          "rogue/graph objects/gear/ca_cm_l3_pol_nt.mgraphobject" },
    { "ca_cm_l3_pol_sh",          "rogue/graph objects/gear/ca_cm_l3_pol_sh.mgraphobject" },
    { "ca_cm_l3_pol_sw",          "rogue/graph objects/gear/ca_cm_l3_pol_sw.mgraphobject" },
    { "ca_cm_l3_snta",            "rogue/graph objects/gear/ca_cm_l3_snta.mgraphobject" },
    { "ca_cm_l3_spo_bp",          "rogue/graph objects/gear/ca_cm_l3_spo_bp.mgraphobject" },
    { "ca_cm_l3_spo_hp",          "rogue/graph objects/gear/ca_cm_l3_spo_hp.mgraphobject" },
    { "ca_cm_l3_spo_rd",          "rogue/graph objects/gear/ca_cm_l3_spo_rd.mgraphobject" },
    { "ca_cm_l3_spo_sb",          "rogue/graph objects/gear/ca_cm_l3_spo_sb.mgraphobject" },
    { "ca_cm_l3_sw_e1",           "rogue/graph objects/gear/ca_cm_l3_sw_e1.mgraphobject" },
    { "ca_cm_l3_sw_e2",           "rogue/graph objects/gear/ca_cm_l3_sw_e2.mgraphobject" },
    { "ca_cm_l3_sw_w1",           "rogue/graph objects/gear/ca_cm_l3_sw_w1.mgraphobject" },
    { "ca_cm_l3_sw_w2",           "rogue/graph objects/gear/ca_cm_l3_sw_w2.mgraphobject" },
    { "ca_cm_l3_t",               "rogue/graph objects/gear/ca_cm_l3_t.mgraphobject" },
    { "ca_cm_l3_t4",              "rogue/graph objects/gear/ca_cm_l3_t4.mgraphobject" },
    { "ca_cm_l3_uc_phx",          "rogue/graph objects/gear/ca_cm_l3_uc_phx.mgraphobject" },
    { "ca_cm_l3_ues_s1",          "rogue/graph objects/gear/ca_cm_l3_ues_s1.mgraphobject" },
    { "ca_cm_l3_ues_s2",          "rogue/graph objects/gear/ca_cm_l3_ues_s2.mgraphobject" },
    { "ca_cm_l3_ues_s3",          "rogue/graph objects/gear/ca_cm_l3_ues_s3.mgraphobject" },
    { "ca_cm_l3_ues_tx",          "rogue/graph objects/gear/ca_cm_l3_ues_tx.mgraphobject" },
    { "ca_cm_l3_ulc_cent",        "rogue/graph objects/gear/ca_cm_l3_ulc_cent.mgraphobject" },
    { "ca_cm_l3_ulc_emt",         "rogue/graph objects/gear/ca_cm_l3_ulc_emt.mgraphobject" },
    { "ca_cm_l3_ulc_fir",         "rogue/graph objects/gear/ca_cm_l3_ulc_fir.mgraphobject" },
    { "ca_cm_l3_ulc_jtf",         "rogue/graph objects/gear/ca_cm_l3_ulc_jtf.mgraphobject" },
    { "ca_cm_l3_ulc_pol",         "rogue/graph objects/gear/ca_cm_l3_ulc_pol.mgraphobject" },
    { "ca_cm_l3_ulc_sbow",        "rogue/graph objects/gear/ca_cm_l3_ulc_sbow.mgraphobject" },
    { "ca_cm_l3_ulc_sur",         "rogue/graph objects/gear/ca_cm_l3_ulc_sur.mgraphobject" },
    { "ca_cm_l3_ulc_up",          "rogue/graph objects/gear/ca_cm_l3_ulc_up.mgraphobject" },
    { "ca_cm_l3_usm_de",          "rogue/graph objects/gear/ca_cm_l3_usm_de.mgraphobject" },
    { "ca_cm_l3_usm_sn",          "rogue/graph objects/gear/ca_cm_l3_usm_sn.mgraphobject" },
    { "ca_cm_l3_usm_ur",          "rogue/graph objects/gear/ca_cm_l3_usm_ur.mgraphobject" },
    { "ca_cm_l3_usm_wl",          "rogue/graph objects/gear/ca_cm_l3_usm_wl.mgraphobject" },
    { "ca_cm_l3_uw_dar",          "rogue/graph objects/gear/ca_cm_l3_uw_dar.mgraphobject" },
    { "ca_cm_l3_uw_pun",          "rogue/graph objects/gear/ca_cm_l3_uw_pun.mgraphobject" },
    { "ca_cm_l3_v",               "rogue/graph objects/gear/ca_cm_l3_v.mgraphobject" },
    { "ca_cm_l3_x",               "rogue/graph objects/gear/ca_cm_l3_x.mgraphobject" },
    { "ca_cm_l3_x_dlc1",          "rogue/graph objects/gear/ca_cm_l3_x_dlc1.mgraphobject" },
};

static const SkinnedMeshManager::ModelSwapEntry s_CosmeticMaskModels_[] =
{
    { "ch_pm_maskonly_tox",               "rogue/graph objects/gear/ch_pm_maskonly_tox.mgraphobject" },
    { "ch_pm_mask_tox_04",               "rogue/graph objects/gear/ch_pm_mask_tox_04.mgraphobject" },
    { "ch_pm_mask_tox_03",               "rogue/graph objects/gear/ch_pm_mask_tox_03.mgraphobject" },
    { "ch_pm_mask_tox_02",               "rogue/graph objects/gear/ch_pm_mask_tox_02.mgraphobject" },
    { "ch_pm_mask_tox_02",               "rogue/graph objects/gear/ch_pm_mask_tox_02.mgraphobject" },
    { "ch_pm_mask_tox_01",               "rogue/graph objects/gear/ch_pm_mask_tox_01.mgraphobject" },
    { "ch_pm_mask_preorder02",               "rogue/graph objects/gear/ch_pm_mask_preorder02.mgraphobject" },
    { "ch_pm_mask_mm_03",               "rogue/graph objects/gear/ch_pm_mask_mm_03.mgraphobject" },
    { "ch_pm_mask_preorder01",               "rogue/graph objects/gear/ch_pm_mask_preorder01.mgraphobject" },
    { "ch_pm_mask_mm_02",               "rogue/graph objects/gear/ch_pm_mask_mm_02.mgraphobject" },
    { "ch_pm_mask_mm_01",               "rogue/graph objects/gear/ch_pm_mask_mm_01.mgraphobject" },
    { "ch_pm_mask_hun_03",               "rogue/graph objects/gear/ch_pm_mask_hun_03.mgraphobject" },
    { "ch_pm_mask_hun_02",               "rogue/graph objects/gear/ch_pm_mask_hun_02.mgraphobject" },
    { "ch_pm_mask_hun",               "rogue/graph objects/gear/ch_pm_mask_hun.mgraphobject" },
    { "ch_pm_mask_ge6_03",               "rogue/graph objects/gear/ch_pm_mask_ge6_03.mgraphobject" },
    { "ch_pm_mask_ge6_02",               "rogue/graph objects/gear/ch_pm_mask_ge6_02.mgraphobject" },
    { "ch_pm_mask_ge6_01",               "rogue/graph objects/gear/ch_pm_mask_ge6_01.mgraphobject" },
    { "ch_pm_mask_ge5_03",               "rogue/graph objects/gear/ch_pm_mask_ge5_03.mgraphobject" },
    { "ch_pm_mask_ge5_02",               "rogue/graph objects/gear/ch_pm_mask_ge5_02.mgraphobject" },
    { "ch_pm_mask_ge5_01",               "rogue/graph objects/gear/ch_pm_mask_ge5_01.mgraphobject" },
    { "ch_pm_mask_ge4_03",               "rogue/graph objects/gear/ch_pm_mask_ge4_03.mgraphobject" },
    { "ch_pm_mask_ge4_02",               "rogue/graph objects/gear/ch_pm_mask_ge4_02.mgraphobject" },
    { "ch_pm_mask_ge4_01",               "rogue/graph objects/gear/ch_pm_mask_ge4_01.mgraphobject" },
    { "ch_pm_mask_ge3_03",               "rogue/graph objects/gear/ch_pm_mask_ge3_03.mgraphobject" },
    { "ch_pm_mask_ge3_02",               "rogue/graph objects/gear/ch_pm_mask_ge3_02.mgraphobject" },
    { "ch_pm_mask_ge3_01",               "rogue/graph objects/gear/ch_pm_mask_ge3_01.mgraphobject" },
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
};

static SlotUIState& UIStateForSlot(int slotIndex)
{
    static SlotUIState s_states[27];
    int idx = (slotIndex < 0 || slotIndex >= 27) ? 0 : slotIndex;
    return s_states[idx];
}

void SkinnedMeshManager::DrawUI()
{
    // Refresh on every draw — cheap, only walks 27 slots.
    ScanLiveSlots();

    ImGui::TextWrapped("Skin Changer — routes through the engine's full equip pipeline "
                       "(SetClothingIdList ordering: ListUpdated → sync → ApplyClothingId "
                       "→ drop old Item* → inject path → ModelLoadTrigger → DirtyFlag). "
                       "The factory creates the new Item* on the next consume frame, so "
                       "the swap should be 1:1 with an in-game equip.");

    // Live diagnostic — shows what the singleton chain is actually returning so
    // we can tell the difference between "engine has no player" and "scan logic
    // bug" when slots vanish. Reads happen in a POD-only helper (DiagInfo) so
    // the SEH guard around the agent-type read doesn't conflict with C++ object
    // unwinding in this function.
    {
        DiagInfo di;
        GatherDiagInfo(&di);
        ImGui::TextDisabled(
            "[diag] agents=%d  player_idx=%d  player=0x%p  type=%d  AM=0x%p",
            di.agentCount, di.playerIdx, (void*)di.player, di.playerType, (void*)di.am);
    }

    // Descriptor-cache probe. The ItemDescriptorCache hook captures the
    // InventoryConfig pointer the first time the engine queries an item by
    // name. Once captured, we can resolve any .mitem name to a real engine
    // descriptor — the foundation for descriptor-based equipping that
    // triggers the side effects (head/hair swap on cosmetic masks, layered-
    // clothing coverage on jackets) the path-only swap currently misses.
    if (ImGui::CollapsingHeader("Item Descriptor Cache (probe)"))
    {
        TD::InventoryConfig* cfg = ItemDescriptorCache::GetCfg();

        // Strategy banner: ACG blocks code patching on this build, so we
        // scan for the InventoryConfig pointer instead of hooking.
        ImGui::TextDisabled("strategy: heap scan (ACG blocks .text patching)");

        if (cfg)
        {
            ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f),
                               "InventoryConfig: %p  (captured)", (void*)cfg);
        }
        else
        {
            ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.2f, 1.0f),
                               "InventoryConfig: not yet captured");
            ImGui::TextDisabled("Click Scan to walk process heap and find it.");
            ImGui::TextDisabled("Takes a few seconds — UI will hang during the scan.");
        }

        if (ImGui::Button("Scan for InventoryConfig"))
        {
            ItemDescriptorCache::TryCapture();
        }
        ImGui::SameLine();
        ImGui::TextDisabled("last result: %s", ItemDescriptorCache::GetLastStatusName());

        // Scan stats — repurposed from the old hook diagnostics.
        ItemDescriptorCache::PageDiag pd = ItemDescriptorCache::GetPageDiag();
        ImGui::TextDisabled("scan stats: regions=%llu  bytes=%llu  candidates=%llu",
                            (unsigned long long)pd.allocationBase,
                            (unsigned long long)pd.state | (((unsigned long long)pd.protect) << 32),
                            (unsigned long long)pd.type);

        static char s_lookupName[256] = "ch_pm_mask_ge3_03";
        static std::string s_lookupResult;

        ImGui::PushItemWidth(360.0f);
        ImGui::InputText("item name##descLookup", s_lookupName, sizeof(s_lookupName));
        ImGui::PopItemWidth();
        ImGui::SameLine();
        if (ImGui::Button("Lookup"))
        {
            TD::ItemDescriptor* desc = ItemDescriptorCache::LookupByName(s_lookupName);
            if (!desc)
            {
                s_lookupResult = cfg ? "not found in cache" : "cfg not captured yet";
            }
            else
            {
                char male[260] = {};
                char female[260] = {};
                ItemDescriptorCache::GetMaleVisualGearPath(desc, male, sizeof(male));
                ItemDescriptorCache::GetFemaleVisualGearPath(desc, female, sizeof(female));
                int slot = ItemDescriptorCache::GetEquipmentSlot(desc);
                int gen  = ItemDescriptorCache::GetAttributeGenType(desc);
                int cat  = ItemDescriptorCache::GetInventoryCategory(desc);

                char buf[1024];
                std::snprintf(buf, sizeof(buf),
                              "desc=%p slot=%d genType=%d invCat=%d\n"
                              "  M: %s\n  F: %s",
                              (void*)desc, slot, gen, cat,
                              male[0]   ? male   : "(empty)",
                              female[0] ? female : "(empty)");
                s_lookupResult = buf;
            }
        }
        if (!s_lookupResult.empty())
            ImGui::TextWrapped("%s", s_lookupResult.c_str());
    }

    // ── Equip-pipeline probe ───────────────────────────────────────────────
    // One-shot empirical test of sub_162DB80 (AppearanceManager_SetEquippedItems)
    // with a single template Item* in the list. The deep RE pass on
    // 2026-05-12 produced a hypothesis that templates from InventoryConfig
    // can't be used directly as list entries (slot-id read at
    // (entry.first_qword)+0x40 lands in the static vtable's inline string
    // data). This probe either confirms the AV at the predicted address
    // OR shows the call returning cleanly. See
    // .claude/docs/06-inventory-equip-pipeline.md "Structural blocker"
    // section. Once this question is answered, this whole block can go.
    if (ImGui::CollapsingHeader("sub_162DB80 Probe (DANGER)"))
    {
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
                           "WARNING: invokes the engine's equip API with a single");
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
                           "template Item*. May corrupt outfit state or crash.");
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
                           "Save / be safe-zone before clicking. SEH catches in-thread");
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
                           "AVs only; engine corruption may surface a frame later.");

        static char        s_probeName[256] = "ch_pm_mask_ge3_03";
        static std::string s_probeResult;
        static bool        s_probeArmed = false;

        ImGui::PushItemWidth(360.0f);
        ImGui::InputText("item name##probeEquip", s_probeName, sizeof(s_probeName));
        ImGui::PopItemWidth();

        ImGui::Checkbox("Armed (must check before Run)", &s_probeArmed);
        ImGui::SameLine();
        // Older ImGui in this project doesn't have BeginDisabled, so we
        // gate the button by checking s_probeArmed at click time. Visual
        // disable comes from a dimmer style when not armed. Cache the
        // armed state UP FRONT — clicking the button flips s_probeArmed
        // and would otherwise unbalance the Push/Pop pair.
        const bool wasArmed = s_probeArmed;
        if (!wasArmed)
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
        if (ImGui::Button("Run probe") && wasArmed)
        {
            EquipPipelineProbe::Result r{};
            EquipPipelineProbe::RunEquipTest(s_probeName, &r);
            s_probeResult = r.summary;
            s_probeArmed  = false;     // re-arm after each click — never auto-repeat
        }
        if (!wasArmed)
            ImGui::PopStyleColor();

        if (!s_probeResult.empty())
            ImGui::TextWrapped("%s", s_probeResult.c_str());
    }

    // ── Equip-pipeline probe v2 (Pattern A: clone-and-retarget) ────────
    // The v1 probe above empirically confirmed that templates cannot be
    // passed directly to sub_162DB80 (AV at the slot-id read). Pattern A
    // sidesteps that by cloning an existing EquipInstance wrapper from
    // PlayerInventory and swapping only its +0x00 (inner Item*) to our
    // target. The engine then sees `(*(QWORD*)entry)+0x40 = template's
    // real slot id`, which is the read that crashed before. Layout
    // verified live 2026-05-12 — see 06-inventory-equip-pipeline.md.
    if (ImGui::CollapsingHeader("Pattern A Probe — Clone + Retarget (DANGER)"))
    {
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.3f, 1.0f),
                           "Clones an existing wrapper from PlayerInventory,");
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.3f, 1.0f),
                           "swaps its inner Item* to your target, calls sub_162DB80.");
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.3f, 1.0f),
                           "Safer than v1 in theory, but still untested live —");
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.3f, 1.0f),
                           "may corrupt outfit state or crash. Re-arm before each Run.");

        static char        s_patAName[256] = "ch_pm_mask_ge3_03";
        static std::string s_patAResult;
        static bool        s_patAArmed = false;

        ImGui::PushItemWidth(360.0f);
        ImGui::InputText("item name##probePatA", s_patAName, sizeof(s_patAName));
        ImGui::PopItemWidth();

        ImGui::Checkbox("Armed (must check before Run)##patAArmed", &s_patAArmed);
        ImGui::SameLine();
        const bool patAWasArmed = s_patAArmed;
        if (!patAWasArmed)
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
        if (ImGui::Button("Run PatternA probe") && patAWasArmed)
        {
            EquipPipelineProbe::Result r{};
            EquipPipelineProbe::RunEquipTestPatternA(s_patAName, &r);
            s_patAResult = r.summary;
            s_patAArmed  = false;
        }
        if (!patAWasArmed)
            ImGui::PopStyleColor();

        if (!s_patAResult.empty())
            ImGui::TextWrapped("%s", s_patAResult.c_str());
    }

    // ── Equip-pipeline probe v3 (Pattern A+: clear-flags-after) ────────
    // Pattern A succeeded for one frame, then the engine reverted our
    // wrapper. Hypothesis: the engine's revert tick fires on a transition
    // of m_DirtyFlag / m_ListUpdated / m_NeedsResync. Pattern A+ clears
    // all three immediately after the sub_162DB80 call and observes
    // whether the equip persists past the next frame.
    //
    // Two modes:
    //   - "Clear flags after"  → A+ behavior
    //   - "Control"            → identical to plain Pattern A
    // Running both lets us A/B test the hypothesis in the same session.
    if (ImGui::CollapsingHeader("Pattern A+ Probe — Clear Flags (DANGER)"))
    {
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.3f, 1.0f),
                           "After sub_162DB80, writes 0 to m_DirtyFlag,");
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.3f, 1.0f),
                           "m_ListUpdated, m_NeedsResync to suppress engine revert.");
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.3f, 1.0f),
                           "WARNING: clearing m_DirtyFlag before consume may also");
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.3f, 1.0f),
                           "prevent the mesh from rendering at all.");

        static char        s_patAPlusName[256] = "ch_pm_mask_ge3_03";
        static std::string s_patAPlusResult;
        static bool        s_patAPlusArmed    = false;
        static bool        s_patAPlusClearOn  = true;     // default to the new behavior

        ImGui::PushItemWidth(360.0f);
        ImGui::InputText("item name##probePatAPlus", s_patAPlusName, sizeof(s_patAPlusName));
        ImGui::PopItemWidth();

        ImGui::Checkbox("Clear flags after call (A+ mode)", &s_patAPlusClearOn);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(
                "ON  = write 0 to m_DirtyFlag / m_ListUpdated / m_NeedsResync\n"
                "      immediately after sub_162DB80 (the A+ test).\n"
                "OFF = leave flags as engine set them — identical to plain\n"
                "      Pattern A (control / sanity check).");

        ImGui::Checkbox("Armed (must check before Run)##patAPlusArmed", &s_patAPlusArmed);
        ImGui::SameLine();
        const bool patAPlusWasArmed = s_patAPlusArmed;
        if (!patAPlusWasArmed)
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
        if (ImGui::Button("Run A+ probe") && patAPlusWasArmed)
        {
            EquipPipelineProbe::Result r{};
            EquipPipelineProbe::RunEquipTestPatternAPlus(s_patAPlusName,
                                                          s_patAPlusClearOn, &r);
            s_patAPlusResult = r.summary;
            s_patAPlusArmed  = false;
        }
        if (!patAPlusWasArmed)
            ImGui::PopStyleColor();

        if (!s_patAPlusResult.empty())
            ImGui::TextWrapped("%s", s_patAPlusResult.c_str());

        // Manual cleanup — forces removal of every tracked Pattern A+
        // injection's AttachHashmap bucket right now. Useful if you've
        // injected a mask, UI-equipped a different one, and the orphan
        // mesh is lingering despite the per-frame maintainer (e.g. if
        // the engine renamed the bucket key in a way the maintainer
        // can't match).
        if (ImGui::Button("Cleanup all Pattern A+ injections"))
        {
            std::string err;
            if (TD::AppearanceManager* am = GetPlayerAppearance(&err))
                EquipPipelineProbe::ClearAllInjections(am);
            s_patAPlusResult = "manual cleanup invoked";
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(
                "Walks every tracked Pattern A+ injection and removes its\n"
                "AttachHashmap bucket via the engine's hashmap_remove\n"
                "(sub_1650620). Clears all tracking state. Safe to spam.");
    }

    // Auto re-apply controls. Drift-only: if the slot's current path already
    // matches what we last applied, this pass leaves it alone — only the
    // slots the engine has reverted get re-Applied.
    ImGui::Checkbox("Auto re-apply every 0.1s on drift", &g_autoReapply);
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip(
            "Polls each slot every ~0.1s. If a slot's path no longer matches\n"
            "the last mod you applied (e.g. the engine just reverted it after\n"
            "you equipped or customized something), re-runs Apply for that slot.\n"
            "Slots that are already matching are left untouched.");
    }
    ImGui::SameLine();
    if (ImGui::Button("Forget all"))
    {
        for (int i = 0; i < 27; ++i) s_lastApplied[i].clear();
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Clears the saved last-applied paths so auto re-apply has nothing to restore.");

    // Show error/transient banner if any, but don't bail out — the slot list
    // below is rendered from m_slots which may be stale-but-valid during the
    // engine's customization-menu preview window.
    if (!m_scanError.empty())
    {
        bool isTransient = m_scanError.find("last good scan") != std::string::npos;
        ImVec4 col = isTransient ? ImVec4(1.0f, 0.85f, 0.2f, 1.0f)
                                 : ImVec4(1.0f, 0.40f, 0.40f, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, col);
        ImGui::Text("%s", m_scanError.c_str());
        ImGui::PopStyleColor();
    }

    if (m_slots.empty())
    {
        ImGui::TextDisabled("(no populated slots — load into the world first)");
        return;
    }

    ImGui::Separator();

    for (const auto& ls : m_slots)
    {
        ImGui::PushID(ls.index);

        // A slot is "writable" if either:
        //   • It already has a heap allocation (canMutate — direct in-place
        //     write or engine reassign).
        //   • It's an Unknown slot 13..26 (no body-part binding, so we don't
        //     surface the Apply UI even though the engine *could* assign a
        //     path there). Cosmetic for now.
        //   • It's a known slot 0..12 with an inline-empty SnowdropString —
        //     the engine's assign helper (sub_116830) converts inline→heap
        //     using the engine's own allocator, so first-time writes into
        //     an empty body-part slot work.
        const bool isKnownSlot = (ls.index >= 0 && ls.index < kKnownSlotCount);
        const bool writable    = ls.canMutate || isKnownSlot;

        // Header: "Slot 7 — Jacket (L3)"
        ImGui::TextColored(ImVec4(0.6f, 0.85f, 1.0f, 1.0f),
                           "Slot %d — %s", ls.index, GearTypeName(ls.type));
        ImGui::SameLine();
        const char* mutTag = ls.canMutate ? "mutable"
                                          : (writable ? "inline-empty (engine-assign)"
                                                      : "INLINE/locked");
        ImGui::TextDisabled("[cap %u, %s]", (unsigned)ls.capacity, mutTag);

        if (ls.currentPath.empty())
        {
            if (ls.canMutate)
                ImGui::TextDisabled("Current: (engine cleared — heap allocation reserved, can still write)");
            else if (writable)
                ImGui::TextDisabled("Current: (empty — first write will allocate via engine's assign)");
            else
                ImGui::TextDisabled("Current: (empty — slot unused on this character)");
        }
        else
        {
            ImGui::TextWrapped("Current: %s", ls.currentPath.c_str());
        }

        if (!writable)
        {
            ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.2f, 1.0f),
                               "  cannot mutate (unknown slot — no body-part binding)");
            ImGui::Separator();
            ImGui::PopID();
            continue;
        }

        // (Descriptor-side-effects warning removed 2026-05-12 — the Apply
        // button now routes through ApplyEquipByName which drives the full
        // engine equip pipeline. Cosmetic mask head/hair swap and L1/L2/L3
        // covered-mesh selection both fire correctly.)

        int        modelCount = 0;
        const auto* models = GetModelList(ls.type, modelCount);

        SlotUIState& ui = UIStateForSlot(ls.index);

        // Dropdown of curated models for this gear type
        const char* preview = (ui.pickedIndex >= 0 && ui.pickedIndex < modelCount)
                              ? models[ui.pickedIndex].displayName
                              : "(pick a model or type a custom path)";
        ImGui::PushItemWidth(360.0f);
        if (ImGui::BeginCombo("##picker", preview))
        {
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

        // Free-text custom path (overrides dropdown when non-empty)
        ImGui::PushItemWidth(360.0f);
        ImGui::InputText("custom##path", ui.custom, sizeof(ui.custom));
        ImGui::PopItemWidth();

        if (ImGui::Button("Apply"))
        {
            const char* target = nullptr;
            if (ui.custom[0] != '\0')
                target = ui.custom;
            else if (ui.pickedIndex >= 0 && ui.pickedIndex < modelCount)
                target = models[ui.pickedIndex].assetPath;

            if (!target || !*target)
            {
                ui.lastOk = false;
                ui.lastResult = "no target selected";
            }
            else
            {
                // ── Try descriptor-bound equip (Pattern A+) first ─────────
                // Derive the .mitem base name from the asset path. The .mitem
                // for `rogue/graph objects/gear/ca_cm_b_uw_dar.mgraphobject`
                // is just `ca_cm_b_uw_dar` (basename minus the extension).
                // Holds for every catalogued vanilla item we've checked.
                // For paths the cache doesn't know (custom paths, modded
                // assets), we fall back to the legacy path-only flow.
                char mitemName[160] = {};
                {
                    const char* slash = std::strrchr(target, '/');
                    const char* base  = slash ? slash + 1 : target;
                    std::size_t i = 0;
                    while (base[i] && base[i] != '.' && i + 1 < sizeof(mitemName))
                    {
                        mitemName[i] = base[i];
                        ++i;
                    }
                    mitemName[i] = '\0';
                }

                std::string err;
                bool ok = false;
                bool triedDescriptor = false;

                // Only attempt descriptor equip if the cache is captured —
                // otherwise LookupByName would return nullptr and we'd
                // fall back unnecessarily.
                if (mitemName[0] && ItemDescriptorCache::GetCfg() &&
                    ItemDescriptorCache::LookupByName(mitemName))
                {
                    triedDescriptor = true;
                    ok = ApplyEquipByName(ls.index, mitemName, &err);
                }

                // Fall back to legacy path-only flow if the item isn't in
                // InventoryConfig OR if the descriptor call AV'd. The path
                // flow still lacks descriptor side effects, but it's the
                // best we can do for items the cache doesn't know.
                if (!ok)
                {
                    std::string err2;
                    bool legacyOk = ApplyDirectSwap(ls.index, target, &err2);
                    if (legacyOk)
                    {
                        ok  = true;
                        err = triedDescriptor
                            ? (std::string("[descriptor failed, used legacy] ") + err2)
                            : (std::string("[legacy path] ") + err2);
                    }
                    else if (err.empty())
                    {
                        err = err2;
                    }
                }

                ui.lastOk = ok;
                ui.lastResult = ok
                    ? (err.empty() ? (std::string("ok — wrote: ") + target)
                                   : (err + " — " + target))
                    : (std::string("failed: ") + err);
            }
        }

        if (!ui.lastResult.empty())
        {
            ImVec4 col = ui.lastOk ? ImVec4(0.5f, 1.0f, 0.5f, 1.0f)
                                   : ImVec4(1.0f, 0.5f, 0.5f, 1.0f);
            ImGui::TextColored(col, "  %s", ui.lastResult.c_str());
        }

        ImGui::Separator();
        ImGui::PopID();
    }
}

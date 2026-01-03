#include "SkinnedMeshManager.h"
#include "Util.h"
#include "IniWriter.h"
#include "IniReader.h"
#include "Main.h"
#include "attributes.h"
#include "configGlobals.h"
#include "KeybindHelper.h"
#include "imgui/imgui.h"
#include "XorStr.hpp"

SkinnedMeshManager::SkinnedMeshManager()
{
}

SkinnedMeshManager::~SkinnedMeshManager()
{

}

static SkinnedMeshManager::ModelSwapEntry backpackModels[] =
{
    { "Ninjabike Messenger Bag", "rogue/graph objects/gear/ca_cm_b_sv_set01.mgraphobject" },
    { "Striker's Battlegear",                 "rogue/graph objects/gear/CA_CM_B_T7_R_DLC1.mgraphobject" },
    { "Striker's Battlegear Classified",      "rogue/graph objects/gear/ca_cm_b_mm_st.mgraphobject" },
    { "Predator's Mark Classified",         "rogue/graph objects/gear/ca_cm_b_pa_pr.mgraphobject" },
    { "Hunters Faith Classified",           "rogue/graph objects/gear/ca_cm_b_rt_hf.mgraphobject" },
    { "Nomad Classified",           "rogue/graph objects/gear/ca_cm_b_uc_pn.mgraphobject" },
    { "D3-FNC Classified",           "rogue/graph objects/gear/ca_cm_b_pa_d3.mgraphobject" },
    { "Lone Star Classified",           "rogue/graph objects/gear/ca_cm_b_tt_ls.mgraphobject" },
    { "Lone Star",                      "rogue/graph objects/gear/CA_CM_B_Set03_BG.mgraphobject" },
    { "Banshee Classified",      "rogue/graph objects/gear/ca_cm_b_pa_ba.mgraphobject" },
    { "Banshee",                 "rogue/graph objects/gear/ca_cm_b_uw_dar.mgraphobject" },
    { "DeadEYE Classified",      "rogue/graph objects/gear/ca_cm_b_tt_de.mgraphobject" },
    { "Sentry Call Classified",  "rogue/graph objects/gear/ca_cm_b_as_sc.mgraphobject" },
    { "Alphabridge Classified",  "rogue/graph objects/gear/ca_cm_b_rt_ab.mgraphobject" },
    { "Reclaimer Classified",    "rogue/graph objects/gear/ca_cm_b_tt_rc.mgraphobject" },
    { "FireCrest Classified",           "rogue/graph objects/gear/ca_cm_b_rt_fc.mgraphobject" },
    { "FireCrest",                      "rogue/graph objects/gear/CA_CM_B_GS_UW.mgraphobject" },
    { "Spec-ops pack",                      "rogue/graph objects/gear/CA_CM_B_T7_L.mgraphobject" },
    { "Urban assault pack",                      "rogue/graph objects/gear/CA_CM_B_T7_E.mgraphobject" },
    { "Security pack",                      "rogue/graph objects/gear/CA_CM_B_T1_R.mgraphobject" },
    { "Safety bag",                      "rogue/graph objects/gear/CA_CM_B_T1_U.mgraphobject" }
    //{ "",                      "" },
};


void SkinnedMeshManager::BackpackUI()
{
    static int fromIndex = 0;
    static int toIndex = 1;

    ImGui::Text("Backpack Skin");

    ImGui::PushItemWidth(260);

    if (ImGui::BeginCombo("##BackpackFrom", backpackModels[fromIndex].displayName))
    {
        for (int i = 0; i < IM_ARRAYSIZE(backpackModels); i++)
        {
            bool selected = (fromIndex == i);
            if (ImGui::Selectable(backpackModels[i].displayName, selected))
                fromIndex = i;

            if (selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    ImGui::SameLine();
    ImGui::Text(">>");
    ImGui::SameLine();

    if (ImGui::BeginCombo("##BackpackTo", backpackModels[toIndex].displayName))
    {
        for (int i = 0; i < IM_ARRAYSIZE(backpackModels); i++)
        {
            bool selected = (toIndex == i);
            if (ImGui::Selectable(backpackModels[i].displayName, selected))
                toIndex = i;

            if (selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    ImGui::PopItemWidth();

    if (fromIndex == toIndex)
    {
        ImGui::SameLine();
        ImGui::TextDisabled("(same item)");
    }
    else
    {
        if (ImGui::Button("Apply Swap"))
        {
            const auto& from = backpackModels[fromIndex];
            const auto& to = backpackModels[toIndex];
        }
    }
}

void SkinnedMeshManager::DrawUI()
{
    BackpackUI();
}





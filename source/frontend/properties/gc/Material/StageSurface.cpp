#include "Common.hpp"
#include <frontend/properties/gc/TevSolver.hpp> // optimizeNode
#include <frontend/widgets/Image.hpp>           // ImagePreview
#include <imcxx/Widgets.hpp>                    // imcxx::Combo
#include <librii/hx/KonstSel.hpp>               // elevateKonstSel
#include <plugins/gc/Export/Scene.hpp>

namespace libcube::UI {

using namespace riistudio::util;

struct StageSurface final {
  static inline const char* name() { return "Stage"_j; }
  static inline const char* icon = (const char*)ICON_FA_NETWORK_WIRED;

  // Mark this surface to be more than an IDL tag.
  int tag_stateful;

  riistudio::frontend::ImagePreview mImg; // In mat sampler
  std::string mLastImg;
};

#define STATIC_STRVIEW(s)                                                      \
  { s, sizeof(s) - 1 }

std::string_view colorOpt = STATIC_STRVIEW("Register 3 Color\0"
                                           "Register 3 Alpha\0"
                                           "Register 0 Color\0"
                                           "Register 0 Alpha\0"
                                           "Register 1 Color\0"
                                           "Register 1 Alpha\0"
                                           "Register 2 Color\0"
                                           "Register 2 Alpha\0"
                                           "Texture Color\0"
                                           "Texture Alpha\0"
                                           "Raster Color\0"
                                           "Raster Alpha\0"
                                           "1.0\0"
                                           "0.5\0"
                                           "Constant Color Selection\0"
                                           "0.0\0"
                                           "\0");

std::string_view alphaOpt = STATIC_STRVIEW("Register 3 Alpha\0"
                                           "Register 0 Alpha\0"
                                           "Register 1 Alpha\0"
                                           "Register 2 Alpha\0"
                                           "Texture Alpha\0"
                                           "Raster Alpha\0"
                                           "Constant Alpha Selection\0"
                                           "0.0\0"
                                           "\0");

#undef STATIC_STRVIEW

librii::gx::TevBias drawTevBias(librii::gx::TevBias bias) {
  return imcxx::Combo("Bias"_j, bias,
                      "No bias\0"
                      "Add middle gray\0"
                      "Subtract middle gray\0"_j);
}
librii::gx::TevScale drawTevScale(librii::gx::TevScale scale) {
  return imcxx::Combo("Scale"_j, scale,
                      "100% brightness\0"
                      "200% brightness\0"
                      "400% brightness\0"
                      "50% brightness\0"_j);
}
librii::gx::TevReg drawOutRegister(librii::gx::TevReg out) {
  return imcxx::Combo("Calculation Result Output Destination"_j, out,
                      "Register 3\0"
                      "Register 0\0"
                      "Register 1\0"
                      "Register 2\0"_j);
}
gx::TevColorArg drawTevOp(const TevExpression& expression, const char* title,
                          gx::TevColorArg op, u32 op_mask) {
  ConditionalHighlight g(expression.isUsed(op_mask));
  return imcxx::Combo(title, op, riistudio::translateString(colorOpt));
}
gx::TevAlphaArg drawTevOp(const TevExpression& expression, const char* title,
                          gx::TevAlphaArg op, u32 op_mask) {
  ConditionalHighlight g(expression.isUsed(op_mask));
  return imcxx::Combo(title, op, riistudio::translateString(alphaOpt));
};

template <typename T> T DrawKonstSel(T x) {
  auto ksel = librii::hx::elevateKonstSel(x);

  int k_type = ksel.k_constant ? 0 : 1;
  const int last_k_type = k_type;
  ImGui::Combo("Konst Selection"_j, &k_type,
               "Constant\0"
               "Uniform\0"_j);
  if (k_type == 0) { // constant
    float k_frac = static_cast<float>(ksel.k_numerator) / 8.0f;
    ImGui::SliderFloat("Constant Value"_j, &k_frac, 0.0f, 1.0f);
    ksel.k_numerator = static_cast<int>(roundf(k_frac * 8.0f));
    ksel.k_numerator = std::max(ksel.k_numerator, 1);
  } else { // uniform
    if (k_type != last_k_type) {
      ksel.k_reg = 0;
      ksel.k_sub = 0;
    }
    ImGui::PushItemWidth(ImGui::GetWindowContentRegionWidth() / 3 - 2);
    ImGui::Combo(".##Constant Register ID", &ksel.k_reg,
                 "Constant Color 0\0"
                 "Constant Color 1\0"
                 "Constant Color 2\0"
                 "Constant Color 3\0"_j);
    ImGui::SameLine();
    if (std::is_same_v<T, gx::TevKColorSel>) {
      ImGui::Combo("Const Register##Subscript", &ksel.k_sub,
                   "RGB\0"
                   "RRR\0"
                   "GGG\0"
                   "BBB\0"
                   "AAA\0"_j);
    } else {
      --ksel.k_sub;
      ImGui::Combo("Const Register##Subscript", &ksel.k_sub,
                   "R\0"
                   "G\0"
                   "B\0"
                   "A\0"_j);
      ++ksel.k_sub;
    }
    ImGui::PopItemWidth();
  }
  ksel.k_constant = k_type == 0;

  return librii::hx::lowerKonstSel<T>(ksel);
}

template <typename T> T drawSubStage(T stage) {
  TevExpression expression(stage);

  ImGui::SetWindowFontScale(1.3f);
  ImGui::Text("%s", expression.getString());
  ImGui::SetWindowFontScale(1.0f);

  stage.constantSelection = DrawKonstSel(stage.constantSelection);

  stage.a = drawTevOp(expression, "Operand A"_j, stage.a, A);
  stage.b = drawTevOp(expression, "Operand B"_j, stage.b, B);
  stage.c = drawTevOp(expression, "Operand C"_j, stage.c, C);
  stage.d = drawTevOp(expression, "Operand D"_j, stage.d, D);

  stage.bias = drawTevBias(stage.bias);
  stage.scale = drawTevScale(stage.scale);

  ImGui::Checkbox("Clamp calculation to 0-255"_j, &stage.clamp);
  stage.out = drawOutRegister(stage.out);

  return stage;
}

void drawProperty(kpi::PropertyDelegate<IGCMaterial>& delegate,
                  StageSurface& tev) {
  auto& matData = delegate.getActive().getMaterialData();
  const libcube::Scene* pScn = dynamic_cast<const libcube::Scene*>(
      dynamic_cast<const kpi::IObject*>(&delegate.getActive())
          ->childOf->childOf);

  auto drawStage = [&](librii::gx::TevStage& stage, int i) {
#define STAGE_PROP(a, b) AUTO_PROP(mStages[i].a, b)
    if (ImGui::CollapsingHeader("Stage Setting"_j,
                                ImGuiTreeNodeFlags_DefaultOpen)) {
      // RasColor
      int rasId = [](gx::ColorSelChanApi sel) -> int {
        switch (sel) {
        case gx::ColorSelChanApi::color0:
        case gx::ColorSelChanApi::alpha0:
        case gx::ColorSelChanApi::color0a0:
          return 0;
        case gx::ColorSelChanApi::color1:
        case gx::ColorSelChanApi::alpha1:
        case gx::ColorSelChanApi::color1a1:
          return 1;
        default:
        case gx::ColorSelChanApi::zero:
        case gx::ColorSelChanApi::null:
          return 2;
        case gx::ColorSelChanApi::ind_alpha:
          return 3;
        case gx::ColorSelChanApi::normalized_ind_alpha:
          return 4;
        }
      }(stage.rasOrder);
      const int rasIdOld = rasId;
      ImGui::Combo("Channel ID"_j, &rasId,
                   "Channel 0\0"
                   "Channel 1\0"
                   "None\0"
                   "Indirect Alpha\0"
                   "Normalized Indirect Alpha\0"_j);

      if (rasId != rasIdOld) {
        gx::ColorSelChanApi ras = [](int sel) -> gx::ColorSelChanApi {
          switch (sel) {
          case 0:
            return gx::ColorSelChanApi::color0a0;
          case 1:
            return gx::ColorSelChanApi::color1a1;
          case 2:
          default:
            return gx::ColorSelChanApi::zero; // TODO: Prefer null?
          case 3:
            return gx::ColorSelChanApi::ind_alpha;
          case 4:
            return gx::ColorSelChanApi::normalized_ind_alpha;
          }
        }(rasId);

        STAGE_PROP(rasOrder, ras);
      }
      {
        ImGui::PushItemWidth(50);
        int ras_swap = stage.rasSwap;
        ImGui::SameLine();
        auto sid = std::string("Swap ID"_j) + "##Ras";
        ImGui::SliderInt(sid.c_str(), &ras_swap, 0, 3);
        STAGE_PROP(rasSwap, static_cast<u8>(ras_swap));
        ImGui::PopItemWidth();
      }
      if (stage.texCoord != stage.texMap) {
        ImGui::TextUnformatted("TODO: TexCoord != TexMap: Not valid"_j);
      } else {
        // TODO: Better selection here
        int texid = stage.texMap;
        texid = SamplerCombo(texid, matData.samplers,
                             delegate.getActive().getTextureSource(*pScn),
                             delegate.mEd, true);
        if (texid != stage.texMap) {
          for (auto* e : delegate.mAffected) {
            auto& stage = e->getMaterialData().mStages[i];
            stage.texCoord = stage.texMap = texid;
          }
          delegate.commit("Property update");
        }

        ImGui::PushItemWidth(50);
        int tex_swap = stage.texMapSwap;
        ImGui::SameLine();
        auto sid = std::string("Swap ID"_j) + "##Tex";
        ImGui::SliderInt(sid.c_str(), &tex_swap, 0, 3);
        STAGE_PROP(texMapSwap, static_cast<u8>(tex_swap));
        ImGui::PopItemWidth();
      }
      if (stage.texMap >= matData.texGens.size()) {
        ImGui::TextUnformatted("No valid image."_j);
      } else {
        const riistudio::lib3d::Texture* curImg = nullptr;

        const auto mImgs = delegate.getActive().getTextureSource(*pScn);
        for (auto& it : mImgs) {
          if (it.getName() == matData.samplers[stage.texMap].mTexture) {
            curImg = &it;
          }
        }
        if (curImg) {
          if (matData.samplers[stage.texCoord].mTexture != tev.mLastImg) {
            tev.mImg.setFromImage(*curImg);
            tev.mLastImg = curImg->getName();
          }
          tev.mImg.draw(128.0f * (static_cast<f32>(curImg->getWidth()) /
                                  static_cast<f32>(curImg->getHeight())),
                        128.0f);
        }
      }
    }

    if (ImGui::CollapsingHeader("Color Stage"_j,
                                ImGuiTreeNodeFlags_DefaultOpen)) {
      // TODO: Only add for now..
      if (stage.colorStage.formula == librii::gx::TevColorOp::add) {
        librii::gx::TevStage::ColorStage substage =
            drawSubStage(stage.colorStage);
        STAGE_PROP(colorStage.constantSelection, substage.constantSelection);
        STAGE_PROP(colorStage.a, substage.a);
        STAGE_PROP(colorStage.b, substage.b);
        STAGE_PROP(colorStage.c, substage.c);
        STAGE_PROP(colorStage.d, substage.d);
        STAGE_PROP(colorStage.clamp, substage.clamp);
        STAGE_PROP(colorStage.bias, substage.bias);
        STAGE_PROP(colorStage.scale, substage.scale);
        STAGE_PROP(colorStage.out, substage.out);
      }
    }
    if (ImGui::CollapsingHeader("Alpha Stage"_j,
                                ImGuiTreeNodeFlags_DefaultOpen)) {
      IDScope alphag("Alpha");
      if (stage.alphaStage.formula == librii::gx::TevAlphaOp::add) {
        librii::gx::TevStage::AlphaStage substage =
            drawSubStage(stage.alphaStage);

        STAGE_PROP(alphaStage.constantSelection, substage.constantSelection);
        STAGE_PROP(alphaStage.a, substage.a);
        STAGE_PROP(alphaStage.b, substage.b);
        STAGE_PROP(alphaStage.c, substage.c);
        STAGE_PROP(alphaStage.d, substage.d);
        STAGE_PROP(alphaStage.clamp, substage.clamp);
        STAGE_PROP(alphaStage.bias, substage.bias);
        STAGE_PROP(alphaStage.scale, substage.scale);
        STAGE_PROP(alphaStage.out, substage.out);
      }
    }
  };

  std::array<bool, 16> opened{true};

  auto& stages = matData.mStages;

  if (ImGui::Button("Add a Stage"_j)) {
    for (auto& mat : delegate.mAffected)
      mat->getMaterialData().mStages.push_back({});
  }

  if (ImGui::BeginTabBar("Stages"_j,
                         ImGuiTabBarFlags_AutoSelectNewTabs |
                             ImGuiTabBarFlags_FittingPolicyResizeDown)) {
    for (std::size_t i = 0; i < stages.size(); ++i) {
      auto& stage = stages[i];

      opened[i] = true;

      if (opened[i] &&
          ImGui::BeginTabItem(
              (std::string("Stage "_j) + std::to_string(i)).c_str(), &opened[i],
              ImGuiTabItemFlags_NoPushId)) {
        drawStage(stage, i);

        ImGui::EndTabItem();
      }
    }
    ImGui::EndTabBar();
  }

  bool changed = false;

  for (std::size_t i = 0; i < stages.size(); ++i) {
    if (opened[i])
      continue;

    changed = true;

    // Only one may be deleted at a time
    stages.erase(i);

    if (stages.empty()) {
      stages.push_back({});
    }

    break;
  }

  if (changed)
    delegate.commit("Stage added/removed");
}

kpi::RegisterPropertyView<IGCMaterial, StageSurface> StageSurfaceInstaller;

} // namespace libcube::UI

#include <plugins/gc/Export/Material.hpp>
#include <vendor/imgui/imgui.h>
#include <vendor/imgui/imgui_internal.h>
#include <kpi/PropertyView.hpp>
#include <vendor/fa5/IconsFontAwesome5.h>
#include <frontend/editor/kit/Image.hpp>
namespace libcube::UI {

struct CullMode
{
	bool front, back;

	CullMode()
		: front(true), back(false)
	{}
	CullMode(bool f, bool b)
		: front(f), back(b)
	{}
	CullMode(libcube::gx::CullMode c)
	{
		set(c);
	}

	void set(libcube::gx::CullMode c) noexcept
	{
		switch (c)
		{
		case libcube::gx::CullMode::All:
			front = back = false;
			break;
		case libcube::gx::CullMode::None:
			front = back = true;
			break;
		case libcube::gx::CullMode::Front:
			front = false;
			back = true;
			break;
		case libcube::gx::CullMode::Back:
			front = true;
			back = false;
			break;
		}
	}
	libcube::gx::CullMode get() const noexcept
	{
		if (front && back)
			return libcube::gx::CullMode::None;

		if (!front && !back)
			return libcube::gx::CullMode::All;

		return front ? libcube::gx::CullMode::Back : libcube::gx::CullMode::Front;
	}

	void draw()
	{
		ImGui::Text("Show sides of faces:");
		ImGui::Checkbox("Front", &front);
		ImGui::Checkbox("Back", &back);
	}
};


struct ConditionalActive {
	ConditionalActive(bool pred) : bPred(pred) {
		if (!bPred) {
			ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
		}
	}
	~ConditionalActive() {
		if (!bPred) {
			ImGui::PopItemFlag();
			ImGui::PopStyleVar();
		}
	}
	bool bPred = false;
};

struct DisplaySurface final {
    const char* name = "Surface Visibility";
    const char* icon = ICON_FA_GHOST;
};
struct ColorSurface final {
	const char* name = "Colors";
	const char* icon = ICON_FA_PAINT_BRUSH;
};
struct SamplerSurface final {
	const char* name = "Samplers";
	const char* icon = ICON_FA_IMAGES;
};
struct SwapTableSurface final {
	const char* name = "Swap Tables";
	const char* icon = ICON_FA_SWATCHBOOK;
};
struct StageSurface final {
	const char* name = "Stage";
	const char* icon = ICON_FA_NETWORK_WIRED;
};
struct FogSurface final {
	const char* name = "Fog";
	const char* icon = ICON_FA_GHOST;
};
struct PixelSurface final {
	const char* name = "Pixel";
	const char* icon = ICON_FA_GHOST;
};


void drawProperty(kpi::PropertyDelegate<IGCMaterial>& delegate, DisplaySurface) {
    const auto before = delegate.getActive().getMaterialData().cullMode;
    
    CullMode cm(before);
    cm.draw();

    //  delegate.property(before, cm.get(),
    //      [](const auto& x) { return x.getMaterialData().cullMode; },
    //      [](auto& x, auto y) { x.getMaterialData().cullMode = y; });
    KPI_PROPERTY(delegate, before, cm.get(), getMaterialData().cullMode);
}

#define AUTO_PROP(before, after) \
	KPI_PROPERTY(delegate, delegate.getActive().getMaterialData().##before, after, getMaterialData().##before)
void drawProperty(kpi::PropertyDelegate<IGCMaterial>& delegate, ColorSurface) {
	libcube::gx::ColorF32 clr;
	auto& matData = delegate.getActive().getMaterialData();

	if (ImGui::CollapsingHeader("TEV Color Registers", ImGuiTreeNodeFlags_DefaultOpen)) {

		// TODO: Is CPREV default state accessible?
		for (std::size_t i = 0; i < 4; ++i)
		{
			clr = matData.tevColors[i];
			ImGui::ColorEdit4((std::string("Color Register ") + std::to_string(i)).c_str(), clr);
			clr.clamp(-1024.0f / 255.0f, 1023.0f / 255.0f);
			AUTO_PROP(tevColors[i], static_cast<libcube::gx::ColorS10>(clr));
		}
	}

	if (ImGui::CollapsingHeader("TEV Constant Colors", ImGuiTreeNodeFlags_DefaultOpen)) {
		for (std::size_t i = 0; i < 4; ++i)
		{
			clr = matData.tevKonstColors[i];
			ImGui::ColorEdit4((std::string("Constant Register ") + std::to_string(i)).c_str(), clr);
			clr.clamp(0.0f, 1.0f);
			AUTO_PROP(tevKonstColors[i], static_cast<libcube::gx::Color>(clr));
		}
	}
}

void drawProperty(kpi::PropertyDelegate<IGCMaterial>& delegate, SamplerSurface)
{
	auto& matData = delegate.getActive().getMaterialData();

	// Hack: The view needs to be stateful..
	static riistudio::frontend::ImagePreview mImg; // In mat sampler
	static std::string mLastImg;
	if (ImGui::BeginTabBar("Textures")) {
		for (std::size_t i = 0; i < matData.texGens.nElements; ++i) {
			auto& tg = matData.texGens[i];
			auto& tm = matData.texMatrices[i]; // TODO: Proper lookup
			auto& samp = matData.samplers[i];

			const auto* mImgs = delegate.getActive().getTextureSource();
			if (ImGui::BeginTabItem((std::string("Texture ") + std::to_string(i)).c_str())) {
				if (ImGui::CollapsingHeader("Image", ImGuiTreeNodeFlags_DefaultOpen)) {
					if (ImGui::BeginCombo("Name", samp->mTexture.c_str())) {
						for (const auto& tex : *mImgs) {
							bool selected = tex->getName() == samp->mTexture;
							if (ImGui::Selectable(tex->getName().c_str(), selected)) {
								AUTO_PROP(samplers[i]->mTexture, tex->getName());
							}
							if (selected) ImGui::SetItemDefaultFocus();
						}

						ImGui::EndCombo();
					}

					const riistudio::lib3d::Texture* curImg = nullptr;

					for (std::size_t j = 0; j < mImgs->size(); ++j) {
						auto& it = mImgs->at<riistudio::lib3d::Texture>(j);
						if (it.getName() == samp->mTexture) {
							curImg = &it;
						}
					}

					if (curImg == nullptr) {
						ImGui::Text("No valid image.");
					}
					else {
						if (mLastImg != curImg->getName()) {
							mImg.setFromImage(*curImg);
							mLastImg = curImg->getName();
						}
						mImg.draw(128.0f * (static_cast<f32>(curImg->getWidth()) / static_cast<f32>(curImg->getHeight())), 128.0f);
					}
				}
				if (ImGui::CollapsingHeader("Mapping", ImGuiTreeNodeFlags_DefaultOpen)) {
					if (ImGui::BeginTabBar("Mapping")) {
						if (ImGui::BeginTabItem("Standard")) {
							ImGui::EndTabItem();
						}
						if (ImGui::BeginTabItem("Advanced")) {
							if (ImGui::CollapsingHeader("Texture Coordinate Generator", ImGuiTreeNodeFlags_DefaultOpen)) {
								int basefunc = 0;
								int mtxtype = 0;
								int lightid = 0;

								switch (tg.func) {
								case libcube::gx::TexGenType::Matrix2x4:
									basefunc = 0;
									mtxtype = 0;
									break;
								case libcube::gx::TexGenType::Matrix3x4:
									basefunc = 0;
									mtxtype = 1;
									break;
								case libcube::gx::TexGenType::Bump0:
								case libcube::gx::TexGenType::Bump1:
								case libcube::gx::TexGenType::Bump2:
								case libcube::gx::TexGenType::Bump3:
								case libcube::gx::TexGenType::Bump4:
								case libcube::gx::TexGenType::Bump5:
								case libcube::gx::TexGenType::Bump6:
								case libcube::gx::TexGenType::Bump7:
									basefunc = 1;
									lightid = static_cast<int>(tg.func) - static_cast<int>(libcube::gx::TexGenType::Bump0);
									break;
								case libcube::gx::TexGenType::SRTG:
									basefunc = 2;
									break;
								}

								ImGui::Combo("Function", &basefunc, "Standard Texture Matrix\0Bump Mapping: Use vertex lighting calculation result.\0SRTG: Map R(ed) and G(reen) components of a color channel to U/V coordinates");
								{
									ConditionalActive g(basefunc == 0);
									ImGui::Combo("Matrix Size", &mtxtype, "UV Matrix: 2x4\0UVW Matrix: 3x4");
									bool identitymatrix = tg.matrix == libcube::gx::TexMatrix::Identity;
									int texmatrixid = 0;
									const int rawtgmatrix = static_cast<int>(tg.matrix);
									if (rawtgmatrix >= static_cast<int>(libcube::gx::TexMatrix::TexMatrix0) && rawtgmatrix <= static_cast<int>(libcube::gx::TexMatrix::TexMatrix7)) {
										texmatrixid = (rawtgmatrix - static_cast<int>(libcube::gx::TexMatrix::TexMatrix0)) / 3;
									}
									ImGui::Checkbox("Identity Matrix", &identitymatrix);
									ImGui::SameLine();
									{
										ConditionalActive g2(!identitymatrix);
										ImGui::SliderInt("Matrix ID", &texmatrixid, 0, 7);
									}
									libcube::gx::TexMatrix newtexmatrix = identitymatrix ? libcube::gx::TexMatrix::Identity :
										static_cast<libcube::gx::TexMatrix>(static_cast<int>(libcube::gx::TexMatrix::TexMatrix0) + texmatrixid * 3);
									AUTO_PROP(texGens[i].matrix, newtexmatrix);
								}
								{
									ConditionalActive g(basefunc == 1);
									ImGui::SliderInt("Hardware light ID", &lightid, 0, 7);
								}

								libcube::gx::TexGenType newfunc = libcube::gx::TexGenType::Matrix2x4;
								switch (basefunc) {
								case 0:
									newfunc = mtxtype ? libcube::gx::TexGenType::Matrix3x4 : libcube::gx::TexGenType::Matrix2x4;
									break;
								case 1:
									newfunc = static_cast<libcube::gx::TexGenType>(static_cast<int>(libcube::gx::TexGenType::Bump0) + lightid);
									break;
								case 2:
									newfunc = libcube::gx::TexGenType::SRTG;
									break;
								}
								AUTO_PROP(texGens[i].func, newfunc);

								int src = static_cast<int>(tg.sourceParam);
								ImGui::Combo("Source data", &src, "Position\0Normal\0Binormal\0Tangent\0UV 0\0UV 1\0UV 2\0UV 3\0UV 4\0UV 5\0UV 6\0UV 7\0Bump UV0\0Bump UV1\0Bump UV2\0Bump UV3\0Bump UV4\0Bump UV5\0Bump UV6\0Color Channel 0\0Color Channel 1");
								AUTO_PROP(texGens[i].sourceParam, static_cast<libcube::gx::TexGenSrc>(src));
							}
							if (ImGui::CollapsingHeader("Texture Coordinate Generator", ImGuiTreeNodeFlags_DefaultOpen)) {
								// TODO: Effect matrix
								int xfmodel = static_cast<int>(tm->transformModel);
								ImGui::Combo("Transform Model", &xfmodel, " Default\0 Maya\0 3DS Max\0 Softimage XSI");
								AUTO_PROP(texMatrices[i]->transformModel, static_cast<libcube::GCMaterialData::CommonTransformModel>(xfmodel));
								// TODO: Not all backends support all modes..
								int mapMethod = 0;
								switch (tm->method) {
								default:
								case libcube::GCMaterialData::CommonMappingMethod::Standard:
									mapMethod = 0;
									break;
								case libcube::GCMaterialData::CommonMappingMethod::EnvironmentMapping:
									mapMethod = 1;
									break;
								case libcube::GCMaterialData::CommonMappingMethod::ViewProjectionMapping:
									mapMethod = 2;
									break;
								case libcube::GCMaterialData::CommonMappingMethod::ProjectionMapping:
									mapMethod = 3;
									break;
								case libcube::GCMaterialData::CommonMappingMethod::EnvironmentLightMapping:
									mapMethod = 4;
									break;
								case libcube::GCMaterialData::CommonMappingMethod::EnvironmentSpecularMapping:
									mapMethod = 5;
									break;
								case libcube::GCMaterialData::CommonMappingMethod::ManualEnvironmentMapping:
									mapMethod = 6;
									break;
								}
								ImGui::Combo("Mapping method", &mapMethod, "Standard Mapping\0Environment Mapping\0View Projection Mapping\0Manual Projection Mapping\0Environment Light Mapping\0Environment Specular Mapping\0Manual Environment Mapping");
								libcube::GCMaterialData::CommonMappingMethod newMapMethod = libcube::GCMaterialData::CommonMappingMethod::Standard;
								using cmm = libcube::GCMaterialData::CommonMappingMethod;
								switch (mapMethod) {
								default:
								case 0:
									newMapMethod = cmm::Standard;
									break;
								case 1:
									newMapMethod = cmm::EnvironmentMapping;
									break;
								case 2:
									newMapMethod = cmm::ViewProjectionMapping;
									break;
								case 3:
									newMapMethod = cmm::ProjectionMapping;
									break;
								case 4:
									newMapMethod = cmm::EnvironmentLightMapping;
									break;
								case 5:
									newMapMethod = cmm::EnvironmentSpecularMapping;
									break;
								case 6:
									newMapMethod = cmm::ManualEnvironmentMapping;
									break;
								}
								AUTO_PROP(texMatrices[i]->method, newMapMethod);

								int mod = static_cast<int>(tm->option);
								ImGui::Combo("Option", &mod, "Standard\0J3D Basic: Don't remap into texture space (Keep -1..1 not 0...1)\0J3D Old: Keep translation column.");
								AUTO_PROP(texMatrices[i]->option, static_cast<libcube::GCMaterialData::CommonMappingOption>(mod));
							}
							ImGui::EndTabItem();
						}
						ImGui::EndTabBar();
					}
					if (ImGui::CollapsingHeader("Transformation", ImGuiTreeNodeFlags_DefaultOpen)) {
						auto s = tm->scale;
						auto r = tm->rotate;
						auto t = tm->translate;
						ImGui::DragFloat2("Scale", &s.x);
						ImGui::DragFloat("Rotate", &r);
						ImGui::DragFloat2("Translate", &t.x);
						AUTO_PROP(texMatrices[i]->scale, s);
						AUTO_PROP(texMatrices[i]->rotate, r);
						AUTO_PROP(texMatrices[i]->translate, t);
					}
				}
				if (ImGui::CollapsingHeader("Tiling", ImGuiTreeNodeFlags_DefaultOpen)) {
					int sTile = static_cast<int>(samp->mWrapU);
					ImGui::Combo("U tiling", &sTile, "Clamp\0Repeat\0Mirror");
					int tTile = static_cast<int>(samp->mWrapV);
					ImGui::Combo("V tiling", &tTile, "Clamp\0Repeat\0Mirror");
					AUTO_PROP(samplers[i]->mWrapU, static_cast<libcube::gx::TextureWrapMode>(sTile));
					AUTO_PROP(samplers[i]->mWrapV, static_cast<libcube::gx::TextureWrapMode>(tTile));
				}
				if (ImGui::CollapsingHeader("Filtering", ImGuiTreeNodeFlags_DefaultOpen)) {
					int magBase = static_cast<int>(samp->mMagFilter);

					int minBase = 0;
					int minMipBase = 0;
					bool mip = false;

					switch (samp->mMinFilter) {
					case libcube::gx::TextureFilter::near_mip_near:
						mip = true;
					case libcube::gx::TextureFilter::near:
						minBase = minMipBase = static_cast<int>(libcube::gx::TextureFilter::near);
						break;
					case libcube::gx::TextureFilter::lin_mip_lin:
						mip = true;
					case libcube::gx::TextureFilter::linear:
						minBase = minMipBase = static_cast<int>(libcube::gx::TextureFilter::linear);
						break;
					case libcube::gx::TextureFilter::near_mip_lin:
						mip = true;
						minBase = static_cast<int>(libcube::gx::TextureFilter::near);
						minMipBase = static_cast<int>(libcube::gx::TextureFilter::linear);
						break;
					case libcube::gx::TextureFilter::lin_mip_near:
						mip = true;
						minBase = static_cast<int>(libcube::gx::TextureFilter::linear);
						minMipBase = static_cast<int>(libcube::gx::TextureFilter::near);
						break;
					}

					const char* linNear = "Nearest (no interpolation/pixelated)\0Linear (interpolated/blurry)";

					ImGui::Combo("Interpolation when scaled up", &magBase, linNear);
					AUTO_PROP(samplers[i]->mMagFilter, static_cast<libcube::gx::TextureFilter>(magBase));
					ImGui::Combo("Interpolation when scaled down", &minBase, linNear);

					ImGui::Checkbox("Use mipmap", &mip);

					{
						struct ConditionalActive g(mip);

						if (ImGui::CollapsingHeader("Mipmapping", ImGuiTreeNodeFlags_DefaultOpen)) {
							ImGui::Combo("Interpolation type", &minMipBase, linNear);

							bool mipBiasClamp = samp->bBiasClamp;
							ImGui::Checkbox("Bias clamp", &mipBiasClamp);
							AUTO_PROP(samplers[i]->bBiasClamp, mipBiasClamp);

							bool edgelod = samp->bEdgeLod;
							ImGui::Checkbox("Edge LOD", &edgelod);
							AUTO_PROP(samplers[i]->bEdgeLod, edgelod);

							float lodbias = samp->mLodBias;
							ImGui::SliderFloat("LOD bias", &lodbias, -4.0f, 3.99f);
							AUTO_PROP(samplers[i]->mLodBias, lodbias);

							int maxaniso = 0;
							switch (samp->mMaxAniso) {
							case libcube::gx::AnisotropyLevel::x1:
								maxaniso = 1;
								break;
							case libcube::gx::AnisotropyLevel::x2:
								maxaniso = 2;
								break;
							case libcube::gx::AnisotropyLevel::x4:
								maxaniso = 4;
								break;
							}
							ImGui::SliderInt("Anisotropic filtering level", &maxaniso, 1, 4);
							libcube::gx::AnisotropyLevel alvl = libcube::gx::AnisotropyLevel::x1;
							switch (maxaniso) {
							case 1:
								alvl = libcube::gx::AnisotropyLevel::x1;
								break;
							case 2:
								alvl = libcube::gx::AnisotropyLevel::x2;
								break;
							case 3:
							case 4:
								alvl = libcube::gx::AnisotropyLevel::x4;
								break;
							}
							AUTO_PROP(samplers[i]->mMaxAniso, alvl);
						}
					}

					libcube::gx::TextureFilter computedMin = libcube::gx::TextureFilter::near;
					if (!mip) {
						computedMin = static_cast<libcube::gx::TextureFilter>(minBase);
					}
					else {
						bool baseLin = static_cast<libcube::gx::TextureFilter>(minBase) == libcube::gx::TextureFilter::linear;
						if (static_cast<libcube::gx::TextureFilter>(minMipBase) == libcube::gx::TextureFilter::linear) {
							computedMin = baseLin ? libcube::gx::TextureFilter::lin_mip_lin : libcube::gx::TextureFilter::near_mip_lin;
						}
						else {
							computedMin = baseLin ? libcube::gx::TextureFilter::lin_mip_near : libcube::gx::TextureFilter::near_mip_near;
						}
					}

					AUTO_PROP(samplers[i]->mMinFilter, computedMin);
				}
				ImGui::EndTabItem();
			}
		}
		ImGui::EndTabBar();
	}
}
// TODO -- filler
void drawProperty(kpi::PropertyDelegate<IGCMaterial>& delegate, SwapTableSurface) {
	ImGui::BeginColumns("swap", 4);
	int sel = 0;
	for (int j = 0; j < 4; ++j) {
		ImGui::Combo("R", &sel, "R\0G\0\0B\0A");
		ImGui::NextColumn();
		ImGui::Combo("G", &sel, "R\0G\0\0B\0A");
		ImGui::NextColumn();
		ImGui::Combo("B", &sel, "R\0G\0\0B\0A");
		ImGui::NextColumn();
		ImGui::Combo("A", &sel, "R\0G\0\0B\0A");
		ImGui::NextColumn();
	}
	ImGui::EndColumns();
}
void drawProperty(kpi::PropertyDelegate<IGCMaterial>& delegate, StageSurface) {
	auto& matData = delegate.getActive().getMaterialData();

	// Hack: The view needs to be stateful..
	static riistudio::frontend::ImagePreview mImg; // In mat sampler
	static std::string mLastImg;
	if (ImGui::BeginTabBar("Stages")) {
		for (std::size_t i = 0; i < matData.shader.mStages.size(); ++i) {
			auto& stage = matData.shader.mStages[i];

			if (ImGui::BeginTabItem((std::string("Stage ") + std::to_string(i)).c_str())) {
				if (ImGui::CollapsingHeader("Stage Setting", ImGuiTreeNodeFlags_DefaultOpen)) {
					// RasColor
					// TODO: Better selection here
					int texid = stage.texMap;
					ImGui::InputInt("TexId", &texid);
					AUTO_PROP(shader.mStages[i].texMap, (u8)texid);
					AUTO_PROP(shader.mStages[i].texCoord, (u8)texid);

					if (stage.texCoord != stage.texMap) {
						ImGui::Text("TODO: TexCoord != TexMap: Not valid");
					}
					if (stage.texCoord >= matData.texGens.size()) {
						ImGui::Text("No valid image.");
					}
					else {
						const riistudio::lib3d::Texture* curImg = nullptr;

						const auto* mImgs = delegate.getActive().getTextureSource();
						for (std::size_t j = 0; j < mImgs->size(); ++j) {
							auto& it = mImgs->at<riistudio::lib3d::Texture>(j);
							if (it.getName() == matData.samplers[stage.texMap]->mTexture) {
								curImg = &it;
							}
						}
						if (matData.samplers[stage.texCoord]->mTexture != mLastImg) {
							mImg.setFromImage(*curImg);
							mLastImg = curImg->getName();
						}
						mImg.draw(128.0f * (static_cast<f32>(curImg->getWidth()) / static_cast<f32>(curImg->getHeight())), 128.0f);
					}
				}
				if (ImGui::CollapsingHeader("Color Stage", ImGuiTreeNodeFlags_DefaultOpen)) {
					// TODO: Only add for now..
					if (stage.colorStage.formula == libcube::gx::TevColorOp::add) {
						ImGui::PushItemWidth(200);
						ImGui::Text("[");
						ImGui::SameLine();
						int a = static_cast<int>(stage.colorStage.a);
						int b = static_cast<int>(stage.colorStage.b);
						int c = static_cast<int>(stage.colorStage.c);
						int d = static_cast<int>(stage.colorStage.d);
						bool clamp = stage.colorStage.clamp;
						int dst = static_cast<int>(stage.colorStage.out);
						const char* colorOpt = "Register 3 Color\0Register 3 Alpha\0Register 0 Color\0Register 0 Alpha\0Register 1 Color\0Register 1 Alpha\0Register 2 Color\0Register 2 Alpha\0Texture Color\0Texture Alpha\0Raster Color\0Raster Alpha\0 1.0\0 0.5\0 Constant Color Selection\0 0.0";
						ImGui::Combo("##D", &d, colorOpt);
						ImGui::SameLine();
						ImGui::Text("{(1 - ");
						{
							ConditionalActive g(false);
							ImGui::SameLine();
							ImGui::Combo("##C_", &c, colorOpt);
						}
						ImGui::SameLine();
						ImGui::Text(") * ");
						ImGui::SameLine();
						ImGui::Combo("##A", &a, colorOpt);

						ImGui::SameLine();
						ImGui::Text(" + ");
						ImGui::SameLine();
						ImGui::Combo("##C", &c, colorOpt);
						ImGui::SameLine();
						ImGui::Text(" * ");
						ImGui::SameLine();
						ImGui::Combo("##B", &b, colorOpt);
						ImGui::SameLine();
						ImGui::Text(" } ");

						AUTO_PROP(shader.mStages[i].colorStage.a, static_cast<libcube::gx::TevColorArg>(a));
						AUTO_PROP(shader.mStages[i].colorStage.b, static_cast<libcube::gx::TevColorArg>(b));
						AUTO_PROP(shader.mStages[i].colorStage.c, static_cast<libcube::gx::TevColorArg>(c));
						AUTO_PROP(shader.mStages[i].colorStage.d, static_cast<libcube::gx::TevColorArg>(d));
						//ImGui::SameLine();
						//ImGui::Combo("##Bias", &bias, "+ 0.0\0+")

						ImGui::Checkbox("Clamp calculation to 0-255", &clamp);
						ImGui::Combo("Calculation Result Output Destionation", &dst, "Register 3\0Register 0\0Register 1\0Register 2");
						ImGui::PopItemWidth();
					}
					if (stage.alphaStage.formula == libcube::gx::TevAlphaOp::add) {
						ImGui::PushItemWidth(200);
						ImGui::PushID("Alpha");
						ImGui::Text("[");
						ImGui::SameLine();
						int a = static_cast<int>(stage.alphaStage.a);
						int b = static_cast<int>(stage.alphaStage.b);
						int c = static_cast<int>(stage.alphaStage.c);
						int d = static_cast<int>(stage.alphaStage.d);
						bool clamp = stage.alphaStage.clamp;
						int dst = static_cast<int>(stage.alphaStage.out);
						const char* alphaOpt = "Register 3 Alpha\0Register 0 Alpha\0Register 1 Alpha\0Register 2 Alpha\0Texture Alpha\0Raster Alpha\0Constant Alpha Selection\0 0.0\0";
						ImGui::Combo("##AD", &d, alphaOpt);
						ImGui::SameLine();
						ImGui::Text("{(1 - ");
						{
							ConditionalActive g(false);
							ImGui::SameLine();
							ImGui::Combo("##C_", &c, alphaOpt);
						}
						ImGui::SameLine();
						ImGui::Text(") * ");
						ImGui::SameLine();
						ImGui::Combo("##AA", &a, alphaOpt);

						ImGui::SameLine();
						ImGui::Text(" + ");
						ImGui::SameLine();
						ImGui::Combo("##AC", &c, alphaOpt);
						ImGui::SameLine();
						ImGui::Text(" * ");
						ImGui::SameLine();
						ImGui::Combo("##AB", &b, alphaOpt);
						ImGui::SameLine();
						ImGui::Text(" } ");

						AUTO_PROP(shader.mStages[i].alphaStage.a, static_cast<libcube::gx::TevAlphaArg>(a));
						AUTO_PROP(shader.mStages[i].alphaStage.b, static_cast<libcube::gx::TevAlphaArg>(b));
						AUTO_PROP(shader.mStages[i].alphaStage.c, static_cast<libcube::gx::TevAlphaArg>(c));
						AUTO_PROP(shader.mStages[i].alphaStage.d, static_cast<libcube::gx::TevAlphaArg>(d));
						//ImGui::SameLine();
						//ImGui::Combo("##Bias", &bias, "+ 0.0\0+")

						ImGui::Checkbox("AClamp calculation to 0-255", &clamp);
						ImGui::Combo("ACalculation Result Output Destionation", &dst, "Register 3\0Register 0\0Register 1\0Register 2");
						ImGui::PopID();
						ImGui::PopItemWidth();
					}
				}
				ImGui::EndTabItem();
			}
		}
		ImGui::EndTabBar();
	}

}

void drawProperty(kpi::PropertyDelegate<IGCMaterial>& delegate, FogSurface) {
	auto& matData = delegate.getActive().getMaterialData();
}

void drawProperty(kpi::PropertyDelegate<IGCMaterial>& delegate, PixelSurface) {
	auto& matData = delegate.getActive().getMaterialData();

	if (ImGui::CollapsingHeader("Alpha Comparison", ImGuiTreeNodeFlags_DefaultOpen)) {
		const char* compStr = "Always do not pass.\0<\0==\0<=\0>\0!=\0>=\0Always pass.";
		ImGui::PushItemWidth(100);

		{
			ImGui::Text("( Pixel Alpha");
			ImGui::SameLine();
			int leftAlpha = static_cast<int>(matData.alphaCompare.compLeft);
			ImGui::Combo("##l", &leftAlpha, compStr);
			AUTO_PROP(alphaCompare.compLeft, static_cast<libcube::gx::Comparison>(leftAlpha));

			int leftRef = static_cast<int>(delegate.getActive().getMaterialData().alphaCompare.refLeft);
			ImGui::SameLine();
			ImGui::SliderInt("##lr", &leftRef, 0, 255);
			AUTO_PROP(alphaCompare.refLeft, (u8)leftRef);
			ImGui::SameLine();
			ImGui::Text(")");
		}
		{
			int op = static_cast<int>(matData.alphaCompare.op);
			ImGui::Combo("##o", &op, "&&\0||\0!=\0==\0");
			AUTO_PROP(alphaCompare.op, static_cast<libcube::gx::AlphaOp>(op));
		}
		{
			ImGui::Text("( Pixel Alpha");
			ImGui::SameLine();
			int rightAlpha = static_cast<int>(matData.alphaCompare.compRight);
			ImGui::Combo("##r", &rightAlpha, compStr);
			AUTO_PROP(alphaCompare.compRight, static_cast<libcube::gx::Comparison>(rightAlpha));

			int rightRef = static_cast<int>(matData.alphaCompare.refRight);
			ImGui::SameLine();
			ImGui::SliderInt("##rr", &rightRef, 0, 255);
			AUTO_PROP(alphaCompare.refRight, (u8)rightRef);

			ImGui::SameLine();
			ImGui::Text(")");
		}
		ImGui::PopItemWidth();
	}
	if (ImGui::CollapsingHeader("Z Buffer", ImGuiTreeNodeFlags_DefaultOpen)) {
		bool zcmp = matData.zMode.compare;
		ImGui::Checkbox("Compare Z Values", &zcmp);
		AUTO_PROP(zMode.compare, zcmp);

		{
			ConditionalActive g(zcmp);

			ImGui::Indent(30.0f);

			bool zearly = matData.earlyZComparison;
			ImGui::Checkbox("Compare Before Texture Processing", &zearly);
			AUTO_PROP(earlyZComparison, zearly);

			bool zwrite = matData.zMode.update;
			ImGui::Checkbox("Write to Z Buffer", &zwrite);
			AUTO_PROP(zMode.update, zwrite);

			int zcond = static_cast<int>(matData.zMode.function);
			ImGui::Combo("Condition", &zcond, "Never draw.\0Pixel Z < EFB Z\0Pixel Z == EFB Z\0Pixel Z <= EFB Z\0Pixel Z > EFB Z\0Pixel Z != EFB Z\0Pixel Z >= EFB Z\0 Always draw.");
			AUTO_PROP(zMode.function, static_cast<libcube::gx::Comparison>(zcond));

			ImGui::Unindent(30.0f);
		}

	}
	if (ImGui::CollapsingHeader("Blending", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::PushItemWidth(200);
		int btype = static_cast<int>(matData.blendMode.type);
		ImGui::Combo("Type", &btype, "Do not blend.\0Blending\0Logical Operations\0Subtract from Frame Buffer");
		AUTO_PROP(blendMode.type, static_cast<libcube::gx::BlendModeType>(btype));

		{

			ConditionalActive g(btype == static_cast<int>(libcube::gx::BlendModeType::blend));
			ImGui::Text("Blend calculation");

			const char* blendOpts = " 0\0 1\0 EFB Color\0 1 - EFB Color\0 Pixel Alpha\0 1 - Pixel Alpha\0 EFB Alpha\0 1 - EFB Alpha";
			const char* blendOptsDst = " 0\0 1\0 Pixel Color\0 1 - Pixel Color\0 Pixel Alpha\0 1 - Pixel Alpha\0 EFB Alpha\0 1 - EFB Alpha";
			ImGui::Text("( Pixel Color * ");

			int srcFact = static_cast<int>(matData.blendMode.source);
			ImGui::SameLine();
			ImGui::Combo("##Src", &srcFact, blendOpts);
			AUTO_PROP(blendMode.source, static_cast<libcube::gx::BlendModeFactor>(srcFact));

			ImGui::SameLine();
			ImGui::Text(") + ( EFB Color * ");

			int dstFact = static_cast<int>(matData.blendMode.dest);
			ImGui::SameLine();
			ImGui::Combo("##Dst", &dstFact, blendOptsDst);
			AUTO_PROP(blendMode.dest, static_cast<libcube::gx::BlendModeFactor>(dstFact));

			ImGui::SameLine();
			ImGui::Text(")");
		}
		{
			ConditionalActive g(btype == static_cast<int>(libcube::gx::BlendModeType::logic));
			ImGui::Text("Logical Operations");
		}
		ImGui::PopItemWidth();
	}
}

void installDisplaySurface() {
	kpi::PropertyViewManager& manager = kpi::PropertyViewManager::getInstance();
	manager.addPropertyView<libcube::IGCMaterial, DisplaySurface>();
	manager.addPropertyView<libcube::IGCMaterial, ColorSurface>();
	manager.addPropertyView<libcube::IGCMaterial, SamplerSurface>();
	manager.addPropertyView<libcube::IGCMaterial, SwapTableSurface>();
	manager.addPropertyView<libcube::IGCMaterial, StageSurface>();
	manager.addPropertyView<libcube::IGCMaterial, FogSurface>();
	manager.addPropertyView<libcube::IGCMaterial, PixelSurface>();

}

}

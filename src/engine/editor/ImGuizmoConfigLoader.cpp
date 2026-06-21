#include "ImGuizmoConfigLoader.h"

#ifdef _DEBUG

#include <core/io/json/JsonReader.h>

#include "ImGuizmo.h"

namespace Unnamed {
	static constexpr std::string_view kChannel = "GCL";

	ImGuizmoConfigLoader::ImGuizmoConfigLoader(
		std::string configPath
	) {
		JsonReader reader(configPath);
		auto&      style  = ImGuizmo::GetStyle();
		auto&      colors = style.Colors;

		// Style
		{
			const auto rs                  = reader["style"];
			style.TranslationLineThickness =
				rs["translationLineThickness"].GetFloat();
			style.TranslationLineArrowSize =
				rs["translationLineArrowSize"].GetFloat();
			style.RotationLineThickness =
				rs["rotationLineThickness"].GetFloat();
			style.RotationOuterLineThickness =
				rs["rotationOuterLineThickness"].GetFloat();
			style.ScaleLineThickness =
				rs["scaleLineThickness"].GetFloat();
			style.ScaleLineCircleSize =
				rs["scaleLineCircleSize"].GetFloat();
			style.HatchedAxisLineThickness =
				rs["hatchedAxisLineThickness"].GetFloat();
			style.CenterCircleSize =
				rs["centerCircleSize"].GetFloat();
		}

		// Color
		{
			using namespace ImGuizmo;
			auto       rc = reader["color"];
			const auto dx = rc["directionX"].GetArray();
			colors[DIRECTION_X] = ToImVec4(dx.GetVec4());
			const auto dy = rc["directionY"].GetArray();
			colors[DIRECTION_Y] = ToImVec4(dy.GetVec4());
			const auto dz = rc["directionZ"].GetArray();
			colors[DIRECTION_Z] = ToImVec4(dz.GetVec4());
			const auto px = rc["planeX"].GetArray();
			colors[PLANE_X] = ToImVec4(px.GetVec4());
			const auto py = rc["planeY"].GetArray();
			colors[PLANE_Y] = ToImVec4(py.GetVec4());
			const auto pz = rc["planeZ"].GetArray();
			colors[PLANE_Z] = ToImVec4(pz.GetVec4());
			const auto sel = rc["selection"].GetArray();
			colors[SELECTION] = ToImVec4(sel.GetVec4());
			const auto inact = rc["inactive"].GetArray();
			colors[INACTIVE] = ToImVec4(inact.GetVec4());
			const auto tline = rc["translationLine"].GetArray();
			colors[TRANSLATION_LINE] = ToImVec4(tline.GetVec4());
			const auto sline = rc["scaleLine"].GetArray();
			colors[SCALE_LINE] = ToImVec4(sline.GetVec4());
			const auto rborder = rc["rotationUsingBorder"].GetArray();
			colors[ROTATION_USING_BORDER] = ToImVec4(rborder.GetVec4());
			const auto rfill = rc["rotationUsingFill"].GetArray();
			colors[ROTATION_USING_FILL] = ToImVec4(rfill.GetVec4());
			const auto hatch = rc["hatchedAxisLines"].GetArray();
			colors[HATCHED_AXIS_LINES] = ToImVec4(hatch.GetVec4());
			const auto text = rc["text"].GetArray();
			colors[TEXT] = ToImVec4(text.GetVec4());
			const auto textShadow = rc["textShadow"].GetArray();
			colors[TEXT_SHADOW] = ToImVec4(textShadow.GetVec4());
		}

		// どっちが正方向かわからんくなるので禁止
		ImGuizmo::AllowAxisFlip(false);
	}
}
#endif

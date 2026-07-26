#pragma once
#include <cstdint>
#include <optional>
#include <vector>

namespace Unnamed::Render {
	struct RgTextureDesc;
	class RgResourceRegistry;

	enum class RG_ACCESS : uint8_t {
		RENDER_TARGET,
		UAV_WRITE,
		SRV_READ_PS,
		SRV_READ_CS,

		DEPTH_WRITE,
		DEPTH_READ,
	};

	/// @brief RgUseは、RenderGraph resource handleと要求accessを1件のpass useとして保持します
	struct RgUse {
		uint32_t  textureId = 0;
		RG_ACCESS access    = RG_ACCESS::SRV_READ_PS;
	};

	/// @brief Colorは、render target clear命令へ渡すRGBA値を保持します
	struct Color {
		float r, g, b, a = 0.0f;
	};

	/// @brief RgClearCmdは、render target handleとclear colorをpass開始時のclear命令として保持します
	struct RgClearCmd {
		uint32_t textureId = 0;
		Color    color;
	};

	/// @brief RgDepthClearCmdは、depth target handleとdepth・stencil値をclear命令として保持します
	struct RgDepthClearCmd {
		uint32_t textureId = 0;
		float    depth     = 1.0f;
		uint8_t  stencil   = 0;
	};

	/// @brief RenderGraphBuilderは、RenderGraphの入力記述から実行用データ構造を構築します
	class RenderGraphBuilder {
	public:
		explicit RenderGraphBuilder(RgResourceRegistry& registry);

		static constexpr uint32_t kBackBufferId = 0xFFFF'FFFEu;

		[[nodiscard]] uint32_t CreateTexture(const RgTextureDesc& desc) const;

		void WriteUav(uint32_t texId);

		void ReadSrvPs(uint32_t texId);
		void ReadSrvCs(uint32_t texId);

		void WriteBackBufferRt();

		void WriteRt(uint32_t texId);
		void ClearColor(uint32_t texId, float r, float g, float b, float a);

		void WriteDepth(uint32_t texId);
		void ClearDepth(
			uint32_t texId, float depth = 1.0f, uint8_t stencil = 0
		);

		const std::vector<RgUse>& GetUses();

		[[nodiscard]]
		const std::vector<RgClearCmd>& GetClearColors() const;

		[[nodiscard]]
		const std::vector<RgDepthClearCmd>& GetClearDepths() const;

		[[nodiscard]] const std::vector<uint32_t>&   GetColorRts() const;
		[[nodiscard]] const std::optional<uint32_t>& GetDepthRt() const;

		void SetColorRt(uint32_t texId);
		void SetDepthRt(uint32_t texId);

	private:
		void MarkUse(uint32_t texId, RG_ACCESS access);

		RgResourceRegistry&          mRegistry;
		std::vector<RgUse>           mUses;
		std::vector<RgClearCmd>      mClearCommands;
		std::vector<RgDepthClearCmd> mClearDepthCommands;
		std::vector<uint32_t>        mColorRts;
		std::optional<uint32_t>      mDepthRt;
	};
}

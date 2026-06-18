#pragma once
#include "UiNode.h"
#include "UiTypes.h"

namespace Unnamed::UI::Retained {
	struct UiNode;

	class UiDocument {
	public:
		/// @brief ノードを作成してドキュメントに追加します
		/// @tparam T ノードデータの型
		/// @param id ノードID
		/// @param type ノードの種類
		/// @param localRect ノードのローカル矩形
		/// @param data ノードデータ
		/// @return 作成されたノードのハンドル
		template <typename T>
		UiNodeHandle CreateNode(
			const UiNodeId id, const UI_NODE_TYPE type, const UiRect localRect,
			T&&            data
		) {
			UiNode node    = {};
			node.id        = id;
			node.type      = type;
			node.localRect = localRect;
			node.data      = std::forward<T>(data);

			const uint32_t index = static_cast<uint32_t>(mNodes.size());

			mNodes.emplace_back(std::move(node));

			return UiNodeHandle{
				.index      = index,
				.generation = mNodes[index].generation
			};
		}

		/// @brief 親ノードに子ノードを追加します
		/// @param parentHandle 親ノードのハンドル
		/// @param childHandle 追加する子ノードのハンドル
		void AppendChild(UiNodeHandle parentHandle, UiNodeHandle childHandle);

		/// @brief ノードを取得します
		/// @param handle 取得するノードのハンドル
		/// @return ノードの参照。
		[[nodiscard]] UiNode& GetNode(UiNodeHandle handle);

		/// @brief ノードを取得します（const版）
		/// @param handle 取得するノードのハンドル
		/// @return ノードの参照。
		[[nodiscard]] const UiNode& GetNode(UiNodeHandle handle) const;

	private:
		/// @brief ノードハンドルが有効かどうかを判定します
		/// @param handle 判定するノードハンドル
		/// @return ハンドルが有効であればtrue、そうでなければfalse
		[[nodiscard]] bool IsValid(UiNodeHandle handle) const;

		std::vector<UiNode> mNodes;
	};
}

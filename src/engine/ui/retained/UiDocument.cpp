#include "pch.h"
#include "UiDocument.h"

namespace Unnamed::UI::Retained {
	void UiDocument::AppendChild(
		const UiNodeHandle parentHandle, const UiNodeHandle childHandle
	) {
		UASSERT(IsValid(parentHandle));
		UASSERT(IsValid(childHandle));
		UASSERT(parentHandle != childHandle);

		UiNode& parent = GetNode(parentHandle);
		UiNode& child  = GetNode(childHandle);

		UASSERT(!child.parent.IsValid());

		child.parent = parentHandle;

		if (!parent.firstChild.IsValid()) {
			parent.firstChild = childHandle;
			parent.lastChild  = childHandle;
			return;
		}

		UiNode& lastChild     = GetNode(parent.lastChild);
		lastChild.nextSibling = childHandle;
		parent.lastChild      = childHandle;
	}

	UiNode& UiDocument::GetNode(const UiNodeHandle handle) {
		UASSERT(IsValid(handle));
		return mNodes[handle.index];
	}

	const UiNode& UiDocument::GetNode(const UiNodeHandle handle) const {
		UASSERT(IsValid(handle));
		return mNodes[handle.index];
	}

	bool UiDocument::IsValid(const UiNodeHandle handle) const {
		if (!handle.IsValid()) {
			return false;
		}

		if (handle.index >= mNodes.size()) {
			return false;
		}

		return mNodes[handle.index].generation == handle.generation;
	}
}

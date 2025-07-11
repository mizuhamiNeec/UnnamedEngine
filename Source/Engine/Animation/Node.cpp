#include <Animation/Node.h>

#include <assimp/matrix4x4.h>
#include <assimp/scene.h>

Node ReadNode(const aiNode* node) {
	Node        result;
	aiMatrix4x4 localMat = node->mTransformation;
	localMat.Transpose(); // 行ベクトルに転置

	for (int i = 0; i < 4; ++i) {
		for (int j = 0; j < 4; ++j) {
			result.localMat.m[i][j] = localMat[i][j];
		}
	}

	// 変換情報を分解
	aiVector3D translation, scaling;
	aiQuaternion rotation;
	node->mTransformation.Decompose(scaling, rotation, translation);

	result.transform.translate = Vec3(translation.x, translation.y, translation.z);
	result.transform.rotate = Quaternion(rotation.x, rotation.y, rotation.z, rotation.w);
	result.transform.scale = Vec3(scaling.x, scaling.y, scaling.z);

	result.name = node->mName.C_Str();          // Node名を格納
	result.children.resize(node->mNumChildren); // 子供の数だけ確保
	for (
		uint32_t childIndex = 0;
		childIndex < node->mNumChildren;
		++childIndex
	) {
		result.children[childIndex] = ReadNode(node->mChildren[childIndex]);
	}

	return result;
}

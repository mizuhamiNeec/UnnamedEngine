#include "UiCanvasRuntime.h"

#include <cfloat>
#include <cmath>

#include "core/assets/AssetManager.h"
#include "core/assets/types/MeshAssetData.h"

#include "engine/physics/core/Physics.h"
#include "engine/render/frame/RenderFrameInputs.h"
#include "engine/scene/Scene.h"
#include "engine/unnamed/framework/components/TransformComponent.h"
#include "engine/unnamed/framework/components/mesh/SkeletalMeshRendererComponent.h"
#include "engine/unnamed/framework/components/mesh/StaticMeshRendererComponent.h"
#include "engine/unnamed/framework/entity/Entity.h"
#include "engine/unnamed/physics/CollisionDetection.h"
#include "engine/unnamed/primitive/Primitives.h"

namespace Unnamed {
	namespace {
		[[nodiscard]] Vec4 ToVec4(const Gui::Color& color) {
			return {color.r, color.g, color.b, color.a};
		}
	}

	namespace UiCanvasRuntime {
		Render::ScreenSpriteInput BuildScreenSprite(
			const Gui::UiDrawCommandRect& rect, const int32_t sortKey
		) {
			Render::ScreenSpriteInput sprite = {};
			sprite.texture.source            = Render::SPRITE_TEXTURE_SOURCE::ASSET;
			sprite.texture.textureAssetId    = kInvalidAssetID;
			sprite.positionPx                = Vec2(rect.rect.x, rect.rect.y);
			sprite.sizePx                    = Vec2(rect.rect.width, rect.rect.height);
			sprite.anchor                    = Vec2(0.0f, 0.0f);
			sprite.rotationRad               = 0.0f;
			sprite.color                     = ToVec4(rect.fillColor);
			sprite.sortKey                   = sortKey;
			sprite.uvFlipY                   = true;
			return sprite;
		}

		Render::ScreenSpriteInput BuildScreenSprite(
			const Gui::UiDrawCommandImage& image, const int32_t sortKey
		) {
			Render::ScreenSpriteInput sprite = {};
			sprite.texture.source            = Render::SPRITE_TEXTURE_SOURCE::ASSET;
			sprite.texture.textureAssetId    = kInvalidAssetID;
			sprite.positionPx                = Vec2(
				image.rect.x + image.anchor.x * image.rect.width,
				image.rect.y + image.anchor.y * image.rect.height
			);
			sprite.sizePx                    = Vec2(image.rect.width, image.rect.height);
			sprite.anchor                    = image.anchor;
			sprite.rotationRad               = image.rotationRad;
			sprite.color                     = ToVec4(image.color);
			sprite.sortKey                   = sortKey;
			sprite.uvMin                     = image.uvMin;
			sprite.uvMax                     = image.uvMax;
			sprite.uvFlipY                   = true;
			return sprite;
		}

		bool BuildRayFromMouse(
			const Render::RenderCameraInput& camera,
			const Vec2&                      mousePos,
			const Vec2&                      viewportSize,
			Ray&                             outRay
		) {
			if (
				!camera.valid ||
				viewportSize.x <= 0.0f ||
				viewportSize.y <= 0.0f
			) {
				return false;
			}
			if (
				mousePos.x < 0.0f ||
				mousePos.y < 0.0f ||
				mousePos.x > viewportSize.x ||
				mousePos.y > viewportSize.y
			) {
				return false;
			}

			const float ndcX = (mousePos.x / viewportSize.x) * 2.0f - 1.0f;
			const float ndcY = 1.0f - (mousePos.y / viewportSize.y) * 2.0f;

			const Mat4 invViewProj = (camera.view * camera.proj).Inverse();
			const Vec4 nearClip(ndcX, ndcY, 0.0f, 1.0f);
			const Vec4 farClip(ndcX, ndcY, 1.0f, 1.0f);

			const Vec4  nearWorld4 = invViewProj * nearClip;
			const Vec4  farWorld4  = invViewProj * farClip;
			const float nearW      =
				std::abs(nearWorld4.w) > 1e-6f ? nearWorld4.w : 1.0f;
			const float farW = std::abs(farWorld4.w) > 1e-6f ?
				                   farWorld4.w :
				                   1.0f;

			const Vec3 nearWorld(
				nearWorld4.x / nearW,
				nearWorld4.y / nearW,
				nearWorld4.z / nearW
			);
			const Vec3 farWorld(
				farWorld4.x / farW,
				farWorld4.y / farW,
				farWorld4.z / farW
			);

			Vec3 direction = farWorld - nearWorld;
			if (direction.SqrLength() < 1e-8f) {
				return false;
			}
			direction = direction.Normalized();

			const Vec3 invDir(
				std::abs(direction.x) > 1e-6f ? 1.0f / direction.x : FLT_MAX,
				std::abs(direction.y) > 1e-6f ? 1.0f / direction.y : FLT_MAX,
				std::abs(direction.z) > 1e-6f ? 1.0f / direction.z : FLT_MAX
			);

			outRay = {
				.origin = nearWorld,
				.dir    = direction,
				.invDir = invDir,
				.tMin   = 0.0f,
				.tMax   = FLT_MAX,
			};
			return true;
		}

		bool RayIntersectsUiPlane(
			const Ray&  ray,
			const Vec3& center,
			const Vec3& axisRight,
			const Vec3& axisUp,
			const Vec2& worldSize,
			float&      outDistance,
			Vec2&       outLocalWorld
		) {
			Vec3 right = axisRight;
			Vec3 up    = axisUp;
			if (right.SqrLength() < 1e-8f || up.SqrLength() < 1e-8f) {
				return false;
			}

			// 平面法線を再構成して、UI面の交差位置を求めます。
			right       = right.Normalized();
			up          = up.Normalized();
			Vec3 normal = right.Cross(up);
			if (normal.SqrLength() < 1e-8f) {
				return false;
			}
			normal = normal.Normalized();

			const float denom = ray.dir.Dot(normal);
			if (std::abs(denom) <= 1e-6f) {
				return false;
			}

			const float t = (center - ray.origin).Dot(normal) / denom;
			if (t < 0.0f || t > ray.tMax) {
				return false;
			}

			const Vec3  hitPoint = ray.origin + ray.dir * t;
			const Vec3  localVec = hitPoint - center;
			const float localX   = localVec.Dot(right);
			const float localY   = localVec.Dot(up);
			const float halfW    = worldSize.x * 0.5f;
			const float halfH    = worldSize.y * 0.5f;
			if (
				localX < -halfW || localX > halfW ||
				localY < -halfH || localY > halfH
			) {
				return false;
			}

			outDistance   = t;
			outLocalWorld = Vec2(localX, localY);
			return true;
		}

		Vec2 WorldToUiPixels(
			const Vec2& localWorld,
			const Vec2& worldSize,
			const Vec2& pixelSize
		) {
			const float u = (localWorld.x / worldSize.x) + 0.5f;
			const float v = 0.5f - (localWorld.y / worldSize.y);
			return {u * pixelSize.x, v * pixelSize.y};
		}

		bool RayHitsSceneGeometryBefore(
			const Scene&   scene,
			AssetManager&  assetManager,
			const Ray&     ray,
			const float    maxDistance,
			const uint64_t ignoredEntityGuid
		) {
			if (maxDistance <= 0.0f) {
				return false;
			}

			Ray limitedRay  = ray;
			limitedRay.tMax = maxDistance;

			float nearestHit = FLT_MAX;
			for (const auto& entityPtr : scene.GetEntities()) {
				Entity* entity = entityPtr.get();
				if (
					!entity ||
					!entity->IsActive() ||
					!entity->IsVisible() ||
					entity->IsEditorOnly() ||
					entity->GetGuid() == ignoredEntityGuid
				) {
					continue;
				}

				auto* transform = entity->GetComponent<TransformComponent>();
				if (!transform) {
					continue;
				}

				AssetID meshAssetId = kInvalidAssetID;
				if (
					auto* staticMesh = entity->GetComponent<
						StaticMeshRendererComponent>();
					staticMesh && staticMesh->IsActive()
				) {
					meshAssetId = staticMesh->ResolveMeshAsset(assetManager);
				} else if (
					auto* skeletalMesh = entity->GetComponent<
						SkeletalMeshRendererComponent>();
					skeletalMesh && skeletalMesh->IsActive()
				) {
					meshAssetId = skeletalMesh->ResolveMeshAsset(assetManager);
				}

				if (meshAssetId == kInvalidAssetID) {
					continue;
				}

				const auto* meshAsset = assetManager.Get<MeshAssetData>(
					meshAssetId
				);
				if (!meshAsset || meshAsset->indices.size() < 3) {
					continue;
				}

				AABB localAabb          = {};
				localAabb.min           = meshAsset->localBoundsMin;
				localAabb.max           = meshAsset->localBoundsMax;
				const Mat4 world        = transform->RenderWorldMat();
				const AABB worldAabb    = TransformAABB(localAabb, world);
				float      aabbDistance = limitedRay.tMax;
				if (!Physics::RayVsAABB(limitedRay, worldAabb, aabbDistance)) {
					continue;
				}

				for (size_t i = 0; i + 2 < meshAsset->indices.size(); i += 3) {
					const uint32_t i0 = meshAsset->indices[i + 0];
					const uint32_t i1 = meshAsset->indices[i + 1];
					const uint32_t i2 = meshAsset->indices[i + 2];
					if (
						i0 >= meshAsset->vertices.size() ||
						i1 >= meshAsset->vertices.size() ||
						i2 >= meshAsset->vertices.size()
					) {
						continue;
					}

					Triangle tri = {};
					tri.v0       = world.TransformPoint(
						meshAsset->vertices[i0].position
					);
					tri.v1 = world.TransformPoint(
						meshAsset->vertices[i1].position
					);
					tri.v2 = world.TransformPoint(
						meshAsset->vertices[i2].position
					);

					float triangleDistance = limitedRay.tMax;
					Vec3  normal           = Vec3::zero;
					if (
						Physics::TriangleVsRay(
							tri,
							limitedRay,
							triangleDistance,
							normal
						) &&
						triangleDistance > 0.0f &&
						triangleDistance < nearestHit
					) {
						nearestHit = triangleDistance;
					}
				}
			}

			return nearestHit < maxDistance - 1e-4f;
		}
	}
}

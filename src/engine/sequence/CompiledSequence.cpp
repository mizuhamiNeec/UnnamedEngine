#include "CompiledSequence.h"

#include <algorithm>

namespace Unnamed {
	CompiledSequence::CompiledSequence(
		std::shared_ptr<const SequenceAssetData> asset
	) :
		mAsset(std::move(asset)) {
		if (!mAsset) {
			return;
		}

		mLengthFrames = std::max<int64_t>(0, mAsset->lengthFrames);
		mTracks.reserve(mAsset->tracks.size());
		for (const SequenceTrackAssetData& track : mAsset->tracks) {
			CompiledSequenceTrack compiledTrack = {};
			compiledTrack.source                = &track;
			compiledTrack.sections.reserve(track.sections.size());

			for (const SequenceSectionAssetData& section : track.sections) {
				compiledTrack.sections.emplace_back(
					CompiledSequenceSection{
						.source = &section,
					}
				);
				mLengthFrames = std::max(mLengthFrames, section.endFrame);
			}

			std::ranges::sort(
				compiledTrack.sections,
				[](
				const CompiledSequenceSection& lhs,
				const CompiledSequenceSection& rhs
			) {
					if (!lhs.source || !rhs.source) {
						return lhs.source != nullptr;
					}
					return lhs.source->startFrame < rhs.source->startFrame;
				}
			);

			mTracks.emplace_back(std::move(compiledTrack));
		}
	}

	const std::shared_ptr<const SequenceAssetData>&
	CompiledSequence::GetAsset() const {
		return mAsset;
	}

	const std::vector<CompiledSequenceTrack>&
	CompiledSequence::GetTracks() const {
		return mTracks;
	}

	int64_t CompiledSequence::GetLengthFrames() const {
		return mLengthFrames;
	}
}

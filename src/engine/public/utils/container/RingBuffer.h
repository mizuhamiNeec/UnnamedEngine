#pragma once
#include <array>
#include <mutex>

template <typename T, size_t Capacity>
class RingBuffer {
public:
	RingBuffer() : mHead(0), mTail(0), mSize(0) {
	}

	bool Push(const T& value) {
		std::lock_guard lock(mMutex);
		if (mSize == Capacity) {
			mTail = (mTail + 1) % Capacity;
		} else {
			++mSize;
		}
		mBuffer[mHead] = value;
		mHead          = (mHead + 1) % Capacity;
		return true;
	}

	bool Pop(T& out) {
		std::lock_guard lock(mMutex);
		if (mSize == 0) {
			return false;
		}
		out   = mBuffer[mTail];
		mTail = (mTail + 1) % Capacity;
		--mSize;
		return true;
	}

	[[nodiscard]] size_t Size() const { return mSize; }
	[[nodiscard]] bool   Empty() const { return mSize == 0; }
	[[nodiscard]] bool   Full() const { return mSize == Capacity; }

	[[nodiscard]] size_t LastWrittenIndex() const {
		std::lock_guard lock(mMutex);
		return (mHead + Capacity - 1) % Capacity;
	}

	class Iterator {
	public:
		Iterator(const RingBuffer* buffer, size_t index, size_t count):
			mBuffer(buffer), mIndex(index), mCount(count) {
		}

		const T& operator*() const {
			return mBuffer->mBuffer[mIndex];
		}

		Iterator& operator++() {
			mIndex = (mIndex + 1) % Capacity;
			--mCount;
			return *this;
		}

		bool operator!=(const Iterator& other) const {
			return mCount != other.mCount;
		}

	private:
		const RingBuffer* mBuffer;
		size_t            mIndex;
		size_t            mCount;
	};

	[[nodiscard]] Iterator begin() const {
		std::lock_guard lock(mMutex);
		return Iterator(this, mTail, mSize);
	}

	[[nodiscard]] Iterator end() const {
		std::lock_guard lock(mMutex);
		return Iterator(this, mHead, 0);
	}

private:
	std::array<T, Capacity> mBuffer;
	size_t                  mHead;
	size_t                  mTail;
	size_t                  mSize;
	mutable std::mutex      mMutex;
};

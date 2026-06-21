#pragma once
#include <array>
#include <mutex>
#include <optional>

namespace Unnamed {
	/// @brief スレッドセーフなリングバッファクラス
	/// @tparam T 格納する要素の型
	/// @tparam Capacity リングバッファの容量
	/// @details 固定サイズの循環バッファで、容量に達すると古いデータを上書きします。 めちゃ便利!!
	template <typename T, size_t Capacity>
	class RingBuffer {
	public:
		/// @brief デフォルトコンストラクタ
		RingBuffer() : mHead(0),
		               mTail(0),
		               mSize(0) {
		}

		/// @brief バッファに要素を追加する
		/// @param value 追加する要素
		/// @return 常にtrueを返す
		/// @details バッファが満杯の場合、最も古い要素を上書きします
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

		/// @brief バッファから要素を取り出す
		/// @param out 取り出した要素を格納する参照
		/// @return 成功した場合true、バッファが空の場合false
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

		/// @brief バッファをクリアする
		void Clear() {
			std::lock_guard lock(mMutex);
			mHead = 0;
			mTail = 0;
			mSize = 0;
		}

		/// @brief 現在のバッファサイズを取得する
		/// @return バッファ内の要素数
		[[nodiscard]] size_t Size() const {
			std::lock_guard lock(mMutex);
			return mSize;
		}

		/// @brief バッファが空かどうかを判定する
		/// @return 空の場合true
		[[nodiscard]] bool Empty() const {
			std::lock_guard lock(mMutex);
			return mSize == 0;
		}

		/// @brief バッファが満杯かどうかを判定する
		/// @return 満杯の場合true
		[[nodiscard]] bool Full() const {
			std::lock_guard lock(mMutex);
			return mSize == Capacity;
		}

		/// @brief 最後に書き込まれた要素のインデックスを取得する
		/// @return 最後に書き込まれたインデックス
		[[nodiscard]] std::optional<size_t> LastWrittenIndex() const {
			std::lock_guard lock(mMutex);
			if (mSize == 0) {
				return std::nullopt;
			}
			return (mHead + Capacity - 1) % Capacity;
		}

		/// @brief 論理インデックス(0..Size-1, 古い→新しい順)で要素参照
		/// @details operator[] は範囲外アクセス時の挙動は未定義です（std::vector と同様）
		[[nodiscard]] const T& operator[](const size_t logicalIndex) const {
			std::lock_guard lock(mMutex);
			const size_t    physicalIndex = (mTail + logicalIndex) % Capacity;
			return mBuffer[physicalIndex];
		}

		/// @brief 論理インデックス(0..Size-1, 古い→新しい順)で要素参照
		/// @details operator[] は範囲外アクセス時の挙動は未定義です（std::vector と同様）
		[[nodiscard]] T& operator[](const size_t logicalIndex) {
			std::lock_guard lock(mMutex);
			const size_t    physicalIndex = (mTail + logicalIndex) % Capacity;
			return mBuffer[physicalIndex];
		}

		/// @brief 範囲チェック付き参照（範囲外なら std::nullopt）
		[[nodiscard]] std::optional<std::reference_wrapper<const T>> At(
			const size_t logicalIndex
		) const {
			std::lock_guard lock(mMutex);
			if (logicalIndex >= mSize) {
				return std::nullopt;
			}
			const size_t physicalIndex = (mTail + logicalIndex) % Capacity;
			return std::cref(mBuffer[physicalIndex]);
		}

		/// @brief 範囲チェック付き参照（範囲外なら std::nullopt）
		[[nodiscard]] std::optional<std::reference_wrapper<T>> At(
			const size_t logicalIndex
		) {
			std::lock_guard lock(mMutex);
			if (logicalIndex >= mSize) {
				return std::nullopt;
			}
			const size_t physicalIndex = (mTail + logicalIndex) % Capacity;
			return std::ref(mBuffer[physicalIndex]);
		}

		/// @brief リングバッファのイテレータクラス
		class Iterator {
		public:
			/// @brief イテレータのコンストラクタ
			/// @param buffer 対象のリングバッファ
			/// @param index 開始インデックス
			/// @param count 反復回数
			Iterator(
				const RingBuffer* buffer, const size_t index, const size_t count
			)
				: mBuffer(buffer),
				  mIndex(index),
				  mCount(count) {
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

			[[nodiscard]] size_t Index() const {
				return mIndex;
			}

		private:
			const RingBuffer* mBuffer;
			size_t            mIndex;
			size_t            mCount;
		};

		/// @brief 範囲for用の開始イテレータを取得
		/// @return 開始イテレータ
		[[nodiscard]] Iterator begin() const {
			std::lock_guard lock(mMutex);
			return Iterator(this, mTail, mSize);
		}

		///@brief 範囲for用の終了イテレータを取得
		///@return 終了イテレータ
		///@return 終了イテレータ
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
}

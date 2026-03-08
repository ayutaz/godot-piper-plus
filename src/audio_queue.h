#ifndef AUDIO_QUEUE_H
#define AUDIO_QUEUE_H

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>

// Single-Producer Single-Consumer lock-free queue for audio chunks.
// Producer: synthesis worker thread (pushes per-sentence audio chunks)
// Consumer: main thread _process() (pops and pushes to AudioStreamGeneratorPlayback)
template <size_t Capacity = 16>
class AudioChunkQueue {
public:
	// Push a chunk (producer only). Returns false if queue is full.
	bool push(std::vector<int16_t> &&chunk) {
		const size_t head = head_.load(std::memory_order_relaxed);
		const size_t next = (head + 1) % Capacity;
		if (next == tail_.load(std::memory_order_acquire)) {
			return false;
		}
		buffer_[head] = std::move(chunk);
		head_.store(next, std::memory_order_release);
		return true;
	}

	// Pop a chunk (consumer only). Returns false if queue is empty.
	bool pop(std::vector<int16_t> &chunk) {
		const size_t tail = tail_.load(std::memory_order_relaxed);
		if (tail == head_.load(std::memory_order_acquire)) {
			return false;
		}
		chunk = std::move(buffer_[tail]);
		tail_.store((tail + 1) % Capacity, std::memory_order_release);
		return true;
	}

	bool empty() const {
		return head_.load(std::memory_order_acquire) ==
			   tail_.load(std::memory_order_acquire);
	}

	// Reset queue. Only safe when no concurrent push/pop is happening.
	void clear() {
		size_t tail = tail_.load(std::memory_order_relaxed);
		const size_t head = head_.load(std::memory_order_relaxed);
		while (tail != head) {
			buffer_[tail].clear();
			buffer_[tail].shrink_to_fit();
			tail = (tail + 1) % Capacity;
		}
		head_.store(0, std::memory_order_relaxed);
		tail_.store(0, std::memory_order_relaxed);
	}

private:
	std::vector<int16_t> buffer_[Capacity];
	std::atomic<size_t> head_{0};
	std::atomic<size_t> tail_{0};
};

#endif // AUDIO_QUEUE_H

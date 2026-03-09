#include <gtest/gtest.h>
#include <thread>
#include <atomic>
#include "audio_queue.h"

class AudioQueueTest : public ::testing::Test {
protected:
	AudioChunkQueue<16> queue;
};

// 1. PushAndPop
TEST_F(AudioQueueTest, PushAndPop) {
	std::vector<int16_t> chunk = {100, 200, 300};
	EXPECT_TRUE(queue.push(std::move(chunk)));

	std::vector<int16_t> out;
	EXPECT_TRUE(queue.pop(out));
	ASSERT_EQ(out.size(), 3);
	EXPECT_EQ(out[0], 100);
	EXPECT_EQ(out[1], 200);
	EXPECT_EQ(out[2], 300);
}

// 2. EmptyQueue
TEST_F(AudioQueueTest, EmptyQueue) {
	EXPECT_TRUE(queue.empty());
	std::vector<int16_t> out;
	EXPECT_FALSE(queue.pop(out));
}

// 3. EmptyAfterPop
TEST_F(AudioQueueTest, EmptyAfterPop) {
	std::vector<int16_t> chunk = {1, 2, 3};
	queue.push(std::move(chunk));
	EXPECT_FALSE(queue.empty());

	std::vector<int16_t> out;
	queue.pop(out);
	EXPECT_TRUE(queue.empty());
}

// 4. FIFOOrder
TEST_F(AudioQueueTest, FIFOOrder) {
	for (int i = 0; i < 5; i++) {
		std::vector<int16_t> chunk = {static_cast<int16_t>(i * 10)};
		EXPECT_TRUE(queue.push(std::move(chunk)));
	}

	for (int i = 0; i < 5; i++) {
		std::vector<int16_t> out;
		EXPECT_TRUE(queue.pop(out));
		ASSERT_EQ(out.size(), 1);
		EXPECT_EQ(out[0], static_cast<int16_t>(i * 10));
	}
}

// 5. FullQueue - Capacity=16 means 15 usable slots
TEST_F(AudioQueueTest, FullQueue) {
	// Fill the queue (15 slots usable in a ring buffer of size 16)
	for (int i = 0; i < 15; i++) {
		std::vector<int16_t> chunk = {static_cast<int16_t>(i)};
		EXPECT_TRUE(queue.push(std::move(chunk)));
	}

	// Queue should be full now
	std::vector<int16_t> overflow = {999};
	EXPECT_FALSE(queue.push(std::move(overflow)));
}

// 6. Clear
TEST_F(AudioQueueTest, Clear) {
	for (int i = 0; i < 5; i++) {
		std::vector<int16_t> chunk = {static_cast<int16_t>(i)};
		queue.push(std::move(chunk));
	}

	queue.clear();
	EXPECT_TRUE(queue.empty());

	// Should be able to push again after clear
	std::vector<int16_t> chunk = {42};
	EXPECT_TRUE(queue.push(std::move(chunk)));

	std::vector<int16_t> out;
	EXPECT_TRUE(queue.pop(out));
	EXPECT_EQ(out[0], 42);
}

// 7. ProducerConsumer - 1 producer, 1 consumer thread
TEST_F(AudioQueueTest, ProducerConsumer) {
	const int numItems = 1000;
	std::atomic<int> consumed{0};
	std::atomic<bool> producerDone{false};

	// Producer thread
	std::thread producer([&]() {
		for (int i = 0; i < numItems; i++) {
			std::vector<int16_t> chunk = {static_cast<int16_t>(i % 32768)};
			while (!queue.push(std::move(chunk))) {
				// Queue full, retry
				std::this_thread::yield();
				chunk = {static_cast<int16_t>(i % 32768)};
			}
		}
		producerDone.store(true);
	});

	// Consumer thread
	std::thread consumer([&]() {
		while (!producerDone.load() || !queue.empty()) {
			std::vector<int16_t> out;
			if (queue.pop(out)) {
				consumed.fetch_add(1);
			} else {
				std::this_thread::yield();
			}
		}
	});

	producer.join();
	consumer.join();

	EXPECT_EQ(consumed.load(), numItems);
}

// 8. SmallCapacity - test with capacity 4
TEST(AudioQueueSmall, SmallCapacity) {
	AudioChunkQueue<4> smallQueue;

	// Can push 3 items (Capacity-1)
	for (int i = 0; i < 3; i++) {
		std::vector<int16_t> chunk = {static_cast<int16_t>(i)};
		EXPECT_TRUE(smallQueue.push(std::move(chunk)));
	}

	// 4th push should fail
	std::vector<int16_t> overflow = {99};
	EXPECT_FALSE(smallQueue.push(std::move(overflow)));
}

// 9. MoveSemantics - verify data is moved, not copied
TEST_F(AudioQueueTest, MoveSemantics) {
	std::vector<int16_t> chunk(1000, 42);
	auto* originalData = chunk.data();

	queue.push(std::move(chunk));
	// After move, original chunk should be empty
	EXPECT_TRUE(chunk.empty());

	std::vector<int16_t> out;
	queue.pop(out);
	ASSERT_EQ(out.size(), 1000);
	EXPECT_EQ(out[0], 42);
}

// 10. WrapAround - push and pop more than Capacity items to test index wrapping
TEST_F(AudioQueueTest, WrapAround) {
	for (int round = 0; round < 5; round++) {
		// Push 10 items
		for (int i = 0; i < 10; i++) {
			std::vector<int16_t> chunk = {static_cast<int16_t>(round * 100 + i)};
			EXPECT_TRUE(queue.push(std::move(chunk)));
		}
		// Pop all
		for (int i = 0; i < 10; i++) {
			std::vector<int16_t> out;
			EXPECT_TRUE(queue.pop(out));
			EXPECT_EQ(out[0], static_cast<int16_t>(round * 100 + i));
		}
	}
}

// 11. MemoryOrder - consumers should observe fully published chunk contents
TEST_F(AudioQueueTest, MemoryOrder) {
	const int numItems = 256;
	std::atomic<int> consumed{0};
	std::atomic<bool> producerDone{false};
	std::atomic<bool> corrupted{false};

	std::thread producer([&]() {
		for (int i = 0; i < numItems; ++i) {
			std::vector<int16_t> chunk = {
				static_cast<int16_t>(i),
				static_cast<int16_t>(i + 1),
				static_cast<int16_t>(i + 2),
			};
			while (!queue.push(std::move(chunk))) {
				std::this_thread::yield();
				chunk = {
					static_cast<int16_t>(i),
					static_cast<int16_t>(i + 1),
					static_cast<int16_t>(i + 2),
				};
			}
		}
		producerDone.store(true);
	});

	std::thread consumer([&]() {
		while (!producerDone.load() || !queue.empty()) {
			std::vector<int16_t> out;
			if (queue.pop(out)) {
				if (out.size() != 3 || out[1] != out[0] + 1 || out[2] != out[1] + 1) {
					corrupted.store(true);
				}
				consumed.fetch_add(1);
			} else {
				std::this_thread::yield();
			}
		}
	});

	producer.join();
	consumer.join();

	EXPECT_FALSE(corrupted.load());
	EXPECT_EQ(consumed.load(), numItems);
}

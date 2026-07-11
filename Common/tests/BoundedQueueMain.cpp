// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Autumnal Software

#include "BoundedQueue.h"

#include <chrono>
#include <iostream>
#include <thread>

using namespace std::chrono_literals;

int main()
{
    BoundedQueue<int> queue(4);

    std::thread producer(
        [&queue]()
        {
            for (int value = 1; value <= 12; ++value)
            {
                std::cout << "Producer: pushing " << value << '\n';

                if (!queue.push(value))
                {
                    std::cout << "Producer: queue stopped\n";
                    return;
                }

                std::cout << "Producer: pushed " << value
                          << ", queue size = " << queue.size()
                          << '\n';

                std::this_thread::sleep_for(100ms);
            }

            std::cout << "Producer: finished\n";
            queue.stop();
        });

    std::thread consumer(
        [&queue]()
        {
            while (true)
            {
                auto item = queue.pop();

                if (!item)
                {
                    std::cout << "Consumer: queue stopped and empty\n";
                    break;
                }

                std::cout << "Consumer: processing " << *item
                          << ", queue size = " << queue.size()
                          << '\n';

                std::this_thread::sleep_for(400ms);
            }

            std::cout << "Consumer: finished\n";
        });

    producer.join();
    consumer.join();

    return 0;
}

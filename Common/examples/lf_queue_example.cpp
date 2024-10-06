#include "thread_utils.h"
#include "lf_queue.h"

using namespace Common;

struct MyStruct {
    int d[3];
};

auto consumerFunction(LFQueue<MyStruct>* lfQueue) {
    using namespace std::literals::chrono_literals;
    std::this_thread::sleep_for(5s);

    while (lfQueue->size())
    {
        auto el = lfQueue->getNextRead();
        lfQueue->updateReadIndex();

        std::cout << "consumeFunction read elem:" << el->d[0] << "," << el->d[1] << "," << el->d[2] << " lfq-size:" << lfQueue->size() << std::endl;
        std::this_thread::sleep_for(1s);
    }
    
    std::cout << "consumeFunction exiting." << std::endl;
}

int main() {
    LFQueue<MyStruct> lfQueue(20);

    auto ct = createAndStartThread(-1, "", consumerFunction, &lfQueue); 

    for(auto i = 0; i < 50; ++i) {
        const MyStruct d{i, i * 10, i * 100};
        *(lfQueue.getNextWriteTo()) = d;
        lfQueue.updateWriteIndex();
        std::cout << "main constructed elem:" << d.d[0] << "," << d.d[1] << "," << d.d[2] << " lfq-size:" << lfQueue.size() << std::endl;
        
        using namespace std::literals::chrono_literals;
        std::this_thread::sleep_for(1s);
    }

    ct->join();
    std::cout << "main exiting." << std::endl;
    return 0;
}
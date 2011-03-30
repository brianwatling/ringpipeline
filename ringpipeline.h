#pragma once

#include <boost/static_assert.hpp>
#include <assert.h>

template<typename T, size_t powerOf2ForSize = 10>
class RingPipeline
{
public:
    BOOST_STATIC_ASSERT(powerOf2ForSize < sizeof(size_t) * 8);

    enum
    {
        buffer_size = 1 << powerOf2ForSize,
    };

    typedef T type;

    RingPipeline()
    : head(0), tail(0)
    {}

    T* beginPushWait()
    {
        while(full()) { continue; }; //wait until space is available
        return &buffer[head & (buffer_size - 1)].object;
    }

    T* beginPush()
    {
        assert(!full());
        return &buffer[head & (buffer_size - 1)].object;
    }

    void endPush()
    {
        __sync_synchronize();//sync writes
        head += 1;
    }

    T* beginPop()
    {
        assert(!empty());
        return &buffer[tail & (buffer_size - 1)].object;
    }

    void endPop()
    {
        tail += 1;
    }

    bool empty() const
    {
        return size() == 0;
    }

    bool full() const
    {
        return (head - tail) >= buffer_size;
    }

    size_t capacity() const
    {
        return buffer_size;
    }

    size_t size() const
    {
        return head - tail;
    }

    T* at(size_t spot)
    {
        assert(spot < size());
        return &buffer[(tail + spot) & (buffer_size - 1)].object;
    }

    T* getBySequence(size_t sequence)
    {
        assert(sequence >= tail && sequence < head);
        return &buffer[sequence & (buffer_size - 1)].object;
    }

    size_t getSequence() const
    {
        return head;
    }

private:

    struct Holder
    {
        T object;
    } __attribute__ ((aligned (64)));

    BOOST_STATIC_ASSERT(sizeof(Holder) % 64 == 0);

    Holder buffer[buffer_size];
    volatile size_t head;
    volatile size_t tail;
};

template<typename PipelineType>
class PipelineEntry
{
public:
    PipelineEntry(PipelineType& pipeline, PipelineEntry* prevItem = NULL)
    : pipeline(pipeline), prevItem(prevItem), sequence(0), last(true)
    {
        if(prevItem) {
            prevItem->last = false;
        }
    }

    typedef typename PipelineType::type type;

    type* begin()
    {
        if(prevItem) {
            while(prevItem->sequence <= sequence) { continue; }; //wait until the previous entry has dealt with the item
        } else {
            while(pipeline.getSequence() <= sequence) { continue; };//wait until the pipeline has something
        }
        return pipeline.getBySequence(sequence);
    }

    void end()
    {
        __sync_synchronize();//sync any updates this thread may have done
        sequence += 1;
        if(last) {
            //pipeline.beginPop();//no need to beginPop(), since we don't care about the value
            pipeline.endPop();
        }
    }

private:
    PipelineType& pipeline;
    PipelineEntry* prevItem;
    volatile size_t sequence;
    bool last;
};


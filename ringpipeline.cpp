#include <iostream>
#include <string>
#include <boost/lexical_cast.hpp>
#include <stdexcept>
#include <stdlib.h>
#include <boost/thread.hpp>
#include <boost/bind.hpp>

#include "ringpipeline.h"

#define CHECK(x) \
    do { if(!(x)) throw std::runtime_error(boost::lexical_cast<std::string>(__LINE__) + " failed check: " #x); } while(0)

typedef RingPipeline<int, 15> PipelineType;
typedef PipelineEntry<PipelineType> PipelineEntryType;

int end = 0;

void runReader(PipelineEntryType* reader)
{
    int last = 0;
    while(last < end)
    {
        const int* spot = reader->begin();
        CHECK(*spot == (last + 1));
        last = *spot;
        reader->end();
    }
}

void runWriter(PipelineType* writer, int count)
{
    for(int i = 1; i <= count; ++i)
    {
        int* spot = writer->beginPushWait();
        *spot = i;
        writer->endPush();
    }
}

int main(int argc, char* argv[])
{
    end = atoi(argv[1]);

    {
        PipelineType ringBuffer;
        CHECK(ringBuffer.empty());
        int* in = ringBuffer.beginPush();
        *in = 1;
        CHECK(ringBuffer.empty());
        CHECK(ringBuffer.size() == 0);
        ringBuffer.endPush();
        CHECK(ringBuffer.size() == 1);
        CHECK(!ringBuffer.empty());
        CHECK(*ringBuffer.at(0) == 1);
        int* out = ringBuffer.beginPop();
        CHECK(!ringBuffer.empty());
        CHECK(*out == 1);
        ringBuffer.endPop();
        CHECK(ringBuffer.empty());
        for(size_t i = 0; i < ringBuffer.capacity(); ++i) {
            int* in = ringBuffer.beginPush();
            *in = i;
            ringBuffer.endPush();
            CHECK(ringBuffer.size() == i + 1);
        }
        CHECK(ringBuffer.full());
    }

    {
        PipelineType ringPipeline;
        PipelineEntryType entryOne(ringPipeline, NULL);
        PipelineEntryType entryTwo(ringPipeline, &entryOne);
        PipelineEntryType entryThree(ringPipeline, &entryTwo);

        int* in = ringPipeline.beginPush();
        *in = 1;
        ringPipeline.endPush();
        in = entryOne.begin();
        CHECK(*in == 1);
        entryOne.end();
        in = entryTwo.begin();
        CHECK(*in == 1);
        entryTwo.end();
        in = entryThree.begin();
        CHECK(*in == 1);
        entryThree.end();
        CHECK(ringPipeline.empty());

        for(int i = 0; i < 5; ++i) {
            int* in = ringPipeline.beginPush();
            *in = i;
            ringPipeline.endPush();
        }
        CHECK(ringPipeline.size() == 5);
        for(int i = 0; i < 5; ++i) {
            int* in = entryOne.begin();
            CHECK(*in == i);
            entryOne.end();
            in = entryTwo.begin();
            CHECK(*in == i);
            entryTwo.end();
            in = entryThree.begin();
            CHECK(*in == i);
            entryThree.end();
            CHECK(ringPipeline.size() == (4 - i));
        }
        CHECK(ringPipeline.empty());
    }

    {
        PipelineType ringPipeline;
        PipelineEntryType entryOne(ringPipeline, NULL);
        PipelineEntryType entryTwo(ringPipeline, &entryOne);
        PipelineEntryType entryThree(ringPipeline, &entryTwo);

        boost::thread readerOneThread(boost::bind(&runReader, &entryThree));
        boost::thread readerTwoThread(boost::bind(&runReader, &entryTwo));
        boost::thread readerThreeThread(boost::bind(&runReader, &entryOne));

        boost::thread writerThread(boost::bind(&runWriter, &ringPipeline, end));

        writerThread.join();
        readerOneThread.join();
        readerTwoThread.join();
        readerThreeThread.join();
    }

    return 0;
}


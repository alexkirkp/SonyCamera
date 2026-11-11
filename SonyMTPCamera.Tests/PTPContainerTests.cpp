#include "pch.h"
#include "PTPContainer.h"
#include "TestHarness.h"

namespace
{
    // Builds a wire buffer: header claiming `declared` total bytes followed by `payload` bytes.
    std::vector<BYTE> Wire(unsigned long declared, size_t payload)
    {
        std::vector<BYTE> buf(sizeof(PTPHeader) + payload, 0x5A);
        pPTPHeader h = (pPTPHeader)buf.data();

        h->containerLength = declared;
        h->containerType = CONTAINER_TYPE_DATA;
        h->opcode = 0x1234;
        h->sequenceNumber = 7;

        return buf;
    }
}

TEST(PTPContainer_ParsesMatchingLength)
{
    std::vector<BYTE> buf = Wire(12 + 8, 8);
    PTPContainer c((unsigned long)buf.size(), buf.data());

    CHECK_EQ(c.getDataLen(), 8UL);
    CHECK_EQ((int)c.getOpCode(), 0x1234);
}

TEST(PTPContainer_ClampsDeclaredLengthToTransferred)
{
    // Header claims 1000 bytes but only 4 payload bytes were transferred.
    std::vector<BYTE> buf = Wire(1000, 4);
    PTPContainer c((unsigned long)buf.size(), buf.data());

    CHECK_EQ(c.getDataLen(), 4UL);
}

TEST(PTPContainer_ShortDeclaredLengthDoesNotWrap)
{
    // Declared length below the header size would previously wrap to ~4GB.
    std::vector<BYTE> buf = Wire(3, 4);
    PTPContainer c((unsigned long)buf.size(), buf.data());

    CHECK_EQ(c.getDataLen(), 0UL);
}

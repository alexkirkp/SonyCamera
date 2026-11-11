#include "pch.h"
#include "PropertyValue.h"
#include "TestHarness.h"

TEST(PropertyValue_SizesAndTypes)
{
    CHECK_EQ(PropertyValue((INT8)-1).GetDataSize(), 1u);
    CHECK_EQ(PropertyValue((UINT8)1).GetDataSize(), 1u);
    CHECK_EQ(PropertyValue((INT16)-1).GetDataSize(), 2u);
    CHECK_EQ(PropertyValue((UINT16)1).GetDataSize(), 2u);
    CHECK_EQ(PropertyValue((INT32)-1).GetDataSize(), 4u);
    CHECK_EQ(PropertyValue((UINT32)1).GetDataSize(), 4u);
    CHECK_EQ(PropertyValue(std::wstring(L"abc")).GetDataSize(), 6u);

    CHECK(PropertyValue((UINT16)1).GetType() == DataType::UINT16);
    CHECK(PropertyValue().GetType() == DataType::UNKNOWN);
}

TEST(PropertyValue_WriteProducesLittleEndianBytes)
{
    BYTE buf[8] = {};
    CHECK_EQ(PropertyValue((UINT32)0x01020304).Write(buf, sizeof(buf)), 4u);
    CHECK_EQ(buf[0], 0x04);
    CHECK_EQ(buf[3], 0x01);

    CHECK_EQ(PropertyValue((INT16)-2).Write(buf, sizeof(buf)), 2u);
    CHECK_EQ(buf[0], 0xFE);
    CHECK_EQ(buf[1], 0xFF);
}

TEST(PropertyValue_WriteLeavesAShortBufferAloneButReportsTheSize)
{
    BYTE buf[2] = { 0x55, 0x55 };

    CHECK_EQ(PropertyValue((UINT32)0xFFFFFFFF).Write(buf, sizeof(buf)), 4u);
    CHECK_EQ(buf[0], 0x55);
    CHECK_EQ(buf[1], 0x55);
}

TEST(PropertyValue_CopiesKeepTheirValue)
{
    PropertyValue number((UINT32)42);
    PropertyValue copy(number);
    PropertyValue assigned;
    assigned = number;

    CHECK_EQ(copy.GetUINT32(), 42u);
    CHECK_EQ(assigned.GetUINT32(), 42u);
    CHECK(assigned.GetType() == DataType::UINT32);
}

TEST(PropertyValue_CopiesKeepTheirString)
{
    PropertyValue text(std::wstring(L"hello"));
    PropertyValue copy(text);
    PropertyValue assigned;
    assigned = text;

    CHECK(copy.GetString() == L"hello");
    CHECK(assigned.GetString() == L"hello");
    CHECK_EQ(copy.GetDataSize(), 10u);
}

TEST(PropertyValue_ToStringShowsTypeAndHex)
{
    CHECK(PropertyValue((UINT16)0xABCD).ToString() == L"(UINT16)xabcd");
    CHECK(PropertyValue((UINT32)0x10002).ToString() == L"(UINT32)x00010002");
    CHECK(PropertyValue(std::wstring(L"x")).ToString() == L"(STR)'x'");
}

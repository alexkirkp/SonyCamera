#include "pch.h"
#include "MessageReader.h"
#include "MessageWriter.h"
#include "TestHarness.h"

// MessageReader's accessors are protected; expose them for the tests.
class TestReader : public MessageReader
{
public:
    TestReader(Message* m) : MessageReader(m) {}
    TestReader(Message* m, bool take) : MessageReader(m, take) {}

    using MessageReader::GetBYTE;
    using MessageReader::GetWORD;
    using MessageReader::GetDWORD;
    using MessageReader::GetString;
    using MessageReader::GetBYTEArray;
    using MessageReader::GetWORDArray;
    using MessageReader::GetDWORDArray;
    using MessageReader::GetStringArray;
    using MessageReader::GetMessageSize;
    using MessageReader::GetRawMessage;
};

TEST(Message_ScalarsRoundTripAndAreLittleEndian)
{
    MessageWriter w(0x1234);
    w.WriteBYTE(0xAB);
    w.WriteWORD(0xCDEF);
    w.WriteDWORD(0x01020304);

    Message* m = w.GetMessageObj();
    CHECK_EQ(m->GetCommand(), 0x1234);
    CHECK_EQ(m->GetDataLen(), 7u);

    // Wire format is little-endian
    BYTE* d = m->GetData();
    CHECK_EQ(d[1], 0xEF);
    CHECK_EQ(d[2], 0xCD);
    CHECK_EQ(d[3], 0x04);
    CHECK_EQ(d[6], 0x01);

    TestReader r(m);
    DWORD off = 0;
    CHECK_EQ(r.GetBYTE(off), 0xAB);
    CHECK_EQ(r.GetWORD(off), 0xCDEF);
    CHECK_EQ(r.GetDWORD(off), 0x01020304u);
    CHECK_EQ(off, 7u);

    delete m;
}

TEST(Message_StringsRoundTrip)
{
    MessageWriter w(1);
    w.WriteString(L"");
    w.WriteString(L"Sony ILCE-6700");

    Message* m = w.GetMessageObj();
    TestReader r(m);
    DWORD off = 0;

    CHECK(r.GetString(off).empty());
    CHECK(r.GetString(off) == L"Sony ILCE-6700");
    CHECK_EQ(off, m->GetDataLen());

    delete m;
}

TEST(Message_NullTerminatedStringDropsTheTerminator)
{
    // PTP strings arrive as a character count (including the NUL) then UTF-16 characters
    BYTE raw[] = { 3, 'h', 0, 'i', 0, 0, 0 };
    Message m(1);
    m.SetData(raw, sizeof(raw));

    TestReader r(&m);
    DWORD off = 0;

    CHECK(r.GetString(off) == L"hi");
    CHECK_EQ(off, 7u);
}

TEST(Message_ArraysRoundTrip)
{
    MessageWriter w(2);
    w.WriteBYTEArray({ 1, 2, 3 });
    w.WriteWORDArray({ 0x1111, 0x2222 });
    w.WriteDWORDArray({ 0xDEADBEEF, 0, 7 });
    w.WriteStringArray({ L"a", L"bc" });
    w.WriteDWORDArray({});

    Message* m = w.GetMessageObj();
    TestReader r(m);
    DWORD off = 0;

    CHECK((r.GetBYTEArray(off) == std::list<BYTE>{ 1, 2, 3 }));
    CHECK((r.GetWORDArray(off) == std::list<WORD>{ 0x1111, 0x2222 }));
    CHECK((r.GetDWORDArray(off) == std::list<DWORD>{ 0xDEADBEEF, 0, 7 }));
    CHECK((r.GetStringArray(off) == std::list<std::wstring>{ L"a", L"bc" }));
    CHECK(r.GetDWORDArray(off).empty());
    CHECK_EQ(off, m->GetDataLen());

    delete m;
}

TEST(Message_WriterGrowsPastItsInitialAllocation)
{
    MessageWriter w(3);

    for (DWORD i = 0; i < 1000; i++)
    {
        w.WriteDWORD(i);
    }

    Message* m = w.GetMessageObj();
    CHECK_EQ(m->GetDataLen(), 4000u);

    TestReader r(m);
    DWORD off = 0;
    bool allMatch = true;

    for (DWORD i = 0; i < 1000; i++)
    {
        allMatch = allMatch && r.GetDWORD(off) == i;
    }

    CHECK(allMatch);
    delete m;
}

TEST(Message_ReadingPastTheEndReturnsZeroAndLeavesTheOffsetAlone)
{
    BYTE raw[] = { 1, 2, 3 };
    Message m(1);
    m.SetData(raw, sizeof(raw));
    TestReader r(&m);

    DWORD off = 0;
    CHECK_EQ(r.GetDWORD(off), 0u);     // 4 bytes wanted, 3 present
    CHECK_EQ(off, 0u);

    off = 2;
    CHECK_EQ(r.GetWORD(off), 0);       // 2 bytes wanted, 1 present
    CHECK_EQ(off, 2u);

    off = 3;
    CHECK_EQ(r.GetBYTE(off), 0);
    CHECK_EQ(off, 3u);

    // An offset near DWORD's maximum must not wrap around and read from the start
    off = 0xFFFFFFFE;
    CHECK_EQ(r.GetDWORD(off), 0u);
    CHECK_EQ(off, 0xFFFFFFFEu);
}

TEST(Message_StringLongerThanTheDataIsRejected)
{
    // Claims 10 characters but only carries 2
    BYTE raw[] = { 10, 'a', 0, 'b', 0 };
    Message m(1);
    m.SetData(raw, sizeof(raw));
    TestReader r(&m);
    DWORD off = 0;

    CHECK(r.GetString(off).empty());
    CHECK_EQ(off, 0u);
}

TEST(Message_ReaderCopiesUnlessToldToTakeTheContents)
{
    BYTE raw[] = { 9, 8, 7, 6 };
    Message src(5);
    src.SetData(raw, sizeof(raw));

    {
        TestReader copy(&src);
        CHECK_EQ(src.GetDataLen(), 4u);       // untouched
        CHECK_EQ(copy.GetMessageSize(), 4u);
        CHECK(copy.GetRawMessage() != src.GetData());
    }

    BYTE* original = src.GetData();
    TestReader taken(&src, true);
    CHECK_EQ(src.GetDataLen(), 0u);           // emptied
    CHECK_EQ(taken.GetMessageSize(), 4u);
    CHECK(taken.GetRawMessage() == original); // same buffer, not copied
}

TEST(Message_ReaderWithNoMessageIsSafe)
{
    TestReader r(nullptr);
    DWORD off = 0;

    CHECK_EQ(r.GetMessageSize(), 0u);
    CHECK(r.GetRawMessage() == nullptr);
    CHECK_EQ(r.GetDWORD(off), 0u);
    CHECK(r.GetString(off).empty());
    CHECK_EQ(off, 0u);
}

TEST(Message_ParamsAndSuccessCode)
{
    Message m(COMMAND_RESULT_SUCCESS);
    m.AddParam(1);
    m.AddParam(0xFFFFFFFF);

    CHECK(m.IsSuccess());
    CHECK((m.GetParams() == std::list<DWORD>{ 1, 0xFFFFFFFF }));

    Message other(0x2002);
    CHECK(!other.IsSuccess());
}

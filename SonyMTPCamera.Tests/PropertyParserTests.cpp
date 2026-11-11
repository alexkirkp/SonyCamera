#include "pch.h"
#include "CameraSettings.h"
#include "MessageReader.h"
#include "MessageWriter.h"
#include "TestHarness.h"

// Wire layout of a settings dump: two counts, then per property
//   WORD id, WORD dataType, BYTE access, BYTE spare, default, current, BYTE formMode, form data
static const WORD kType_UINT8 = 0x0002;
static const WORD kType_UINT16 = 0x0004;
static const WORD kType_STR = 0xFFFF;

static void WriteHeader(MessageWriter& w, DWORD count)
{
    w.WriteDWORD(count);
    w.WriteDWORD(count);
}

static void WriteU16EnumProperty(MessageWriter& w, WORD id, std::initializer_list<WORD> values)
{
    w.WriteWORD(id);
    w.WriteWORD(kType_UINT16);
    w.WriteBYTE(1);     // access
    w.WriteBYTE(0);     // spare
    w.WriteWORD(7);     // default
    w.WriteWORD(8);     // current
    w.WriteBYTE(2);     // enumeration
    w.WriteWORD((WORD)values.size());

    for (WORD v : values)
    {
        w.WriteWORD(v);
    }
}

// Builds settings from the first `keep` bytes of the message w produced
static int ParsedCount(MessageWriter& w, DWORD keep)
{
    Message* full = w.GetMessageObj();
    Message cut(full->GetCommand());
    cut.SetData(full->GetData(), keep < full->GetDataLen() ? keep : full->GetDataLen());
    CameraSettings settings(&cut);
    int n = settings.size();

    delete full;

    return n;
}

TEST(PropertyParser_ReadsWellFormedProperties)
{
    MessageWriter w(1);
    WriteHeader(w, 2);
    WriteU16EnumProperty(w, 0x5001, { 1, 2, 3 });
    WriteU16EnumProperty(w, 0x5002, { 4 });

    CHECK_EQ(ParsedCount(w, 0xFFFFFF), 2);
}

TEST(PropertyParser_TruncatedDumpKeepsWhatParsedAndDoesNotCrash)
{
    MessageWriter w(1);
    WriteHeader(w, 2);
    WriteU16EnumProperty(w, 0x5001, { 1, 2, 3 });
    WriteU16EnumProperty(w, 0x5002, { 4, 5, 6, 7 });

    Message* full = w.GetMessageObj();
    DWORD len = full->GetDataLen();
    delete full;

    // Cut the buffer at every possible length: none may read past the end
    for (DWORD keep = 0; keep < len; keep++)
    {
        int n = ParsedCount(w, keep);

        CHECK(n <= 1);
    }
}

TEST(PropertyParser_UnsupportedDataTypeIsSkippedNotFatal)
{
    MessageWriter w(1);
    WriteHeader(w, 2);
    WriteU16EnumProperty(w, 0x5001, { 1 });
    w.WriteWORD(0x5002);
    w.WriteWORD(kType_STR);
    w.WriteBYTE(1);
    w.WriteBYTE(0);
    w.WriteBYTE(0);
    w.WriteBYTE(0);

    // Previously a bare `throw;` -> std::terminate
    CHECK_EQ(ParsedCount(w, 0xFFFFFF), 1);
}

TEST(PropertyParser_ZeroFilledBufferIsHarmless)
{
    MessageWriter w(1);
    WriteHeader(w, 1000000);

    for (int i = 0; i < 64; i++)
    {
        w.WriteBYTE(0);
    }

    CHECK_EQ(ParsedCount(w, 0xFFFFFF), 0);
}

TEST(PropertyParser_HugePropertyCountIsBoundedByTheData)
{
    MessageWriter w(1);
    WriteHeader(w, 0xFFFFFFFF);
    WriteU16EnumProperty(w, 0x5001, { 1 });

    CHECK_EQ(ParsedCount(w, 0xFFFFFF), 1);
}

TEST(PropertyParser_EnumCountLargerThanTheDataIsRejected)
{
    MessageWriter w(1);
    WriteHeader(w, 1);
    w.WriteWORD(0x5001);
    w.WriteWORD(kType_UINT8);
    w.WriteBYTE(1);
    w.WriteBYTE(0);
    w.WriteBYTE(1);     // default
    w.WriteBYTE(1);     // current
    w.WriteBYTE(2);     // enumeration
    w.WriteWORD(60000); // claims 60000 values, carries none

    CHECK_EQ(ParsedCount(w, 0xFFFFFF), 0);
}

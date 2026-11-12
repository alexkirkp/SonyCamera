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
    w.WriteWORD(0x0020);    // not a PTP data type
    w.WriteBYTE(1);
    w.WriteBYTE(0);
    w.WriteBYTE(0);
    w.WriteBYTE(0);

    // Previously a bare `throw;` -> std::terminate
    CHECK_EQ(ParsedCount(w, 0xFFFFFF), 1);
}

static void WriteStringProperty(MessageWriter& w, WORD id, const wchar_t* text)
{
    w.WriteWORD(id);
    w.WriteWORD(kType_STR);
    w.WriteBYTE(0);
    w.WriteBYTE(0);
    w.WriteBYTE(0);     // default: empty string

    BYTE chars = (BYTE)(wcslen(text) + 1);
    w.WriteBYTE(chars); // current: count includes the terminator
    for (BYTE i = 0; i < chars; i++)
    {
        w.WriteWORD((WORD)text[i]);
    }

    w.WriteBYTE(0);     // no form
}

TEST(PropertyParser_StringPropertyIsReadAndParsingContinues)
{
    MessageWriter w(1);
    WriteHeader(w, 2);
    WriteStringProperty(w, 0xd2c5, L"ILCE");
    WriteU16EnumProperty(w, 0x5001, { 1 });

    Message* msg = w.GetMessageObj();
    CameraSettings settings(msg);

    CHECK_EQ(settings.size(), 2);
    CHECK(settings.GetPropertyValue((Property)0xd2c5)->GetString() == L"ILCE");

    delete msg;
}

TEST(PropertyParser_ArrayAnd64BitValuesAreSkippedAndParsingContinues)
{
    MessageWriter w(1);
    WriteHeader(w, 3);

    // UINT16 array: count then elements, for default and current
    w.WriteWORD(0xd2c6);
    w.WriteWORD(0x4004);
    w.WriteBYTE(0);
    w.WriteBYTE(0);
    w.WriteDWORD(0);
    w.WriteDWORD(3);
    w.WriteWORD(1);
    w.WriteWORD(2);
    w.WriteWORD(3);
    w.WriteBYTE(0);

    // UINT64 with a range form
    w.WriteWORD(0xd2c7);
    w.WriteWORD(0x0008);
    w.WriteBYTE(0);
    w.WriteBYTE(0);
    for (int i = 0; i < 2 + 3; i++)
    {
        if (i == 2)
        {
            w.WriteBYTE(1); // range: min, max, step follow
        }
        w.WriteDWORD(0);
        w.WriteDWORD(0);
    }

    WriteU16EnumProperty(w, 0x5001, { 1, 2 });

    CHECK_EQ(ParsedCount(w, 0xFFFFFF), 3);
}

TEST(PropertyParser_ArrayCountLargerThanTheDataIsRejected)
{
    MessageWriter w(1);
    WriteHeader(w, 1);
    w.WriteWORD(0xd2c6);
    w.WriteWORD(0x4006);
    w.WriteBYTE(0);
    w.WriteBYTE(0);
    w.WriteDWORD(0x40000000); // count * 4 would wrap a DWORD

    CHECK_EQ(ParsedCount(w, 0xFFFFFF), 0);
}

// Version 300 layout: a settable list, then a readable list
static void WriteU16TwoListEnumProperty(MessageWriter& w, WORD id, std::initializer_list<WORD> set, std::initializer_list<WORD> get)
{
    w.WriteWORD(id);
    w.WriteWORD(kType_UINT16);
    w.WriteBYTE(1);
    w.WriteBYTE(1);
    w.WriteWORD(2);
    w.WriteWORD(2);
    w.WriteBYTE(2);

    w.WriteWORD((WORD)set.size());
    for (WORD v : set)
    {
        w.WriteWORD(v);
    }

    w.WriteWORD((WORD)get.size());
    for (WORD v : get)
    {
        w.WriteWORD(v);
    }
}

TEST(PropertyParser_TwoEnumListsKeepTheSettableList)
{
    MessageWriter w(1);
    WriteHeader(w, 3);
    WriteU16TwoListEnumProperty(w, 0xd0df, { 2, 3 }, { 2, 3, 4 });
    WriteU16TwoListEnumProperty(w, 0xd0db, { }, { 1, 2 });
    WriteU16TwoListEnumProperty(w, 0x5001, { 1 }, { 1 });

    Message* msg = w.GetMessageObj();
    CameraSettings settings(msg, true);

    CHECK_EQ(settings.size(), 3);
    CHECK_EQ((int)settings.GetProperty((Property)0xd0df)->GetInfo()->GetEnumeration().size(), 2);
    // An empty settable list falls back to the readable one
    CHECK_EQ((int)settings.GetProperty((Property)0xd0db)->GetInfo()->GetEnumeration().size(), 2);

    delete msg;
}

TEST(PropertyParser_TwoEnumListsTruncatedAnywhereDoesNotCrash)
{
    MessageWriter w(1);
    WriteHeader(w, 2);
    WriteU16TwoListEnumProperty(w, 0xd0df, { 2, 3 }, { 2, 3 });
    WriteU16TwoListEnumProperty(w, 0x5001, { 1 }, { 1, 2 });

    Message* full = w.GetMessageObj();
    DWORD len = full->GetDataLen();

    for (DWORD keep = 0; keep < len; keep++)
    {
        Message cut(full->GetCommand());
        cut.SetData(full->GetData(), keep);
        CameraSettings settings(&cut, true);

        CHECK(settings.size() <= 1);
    }

    delete full;
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

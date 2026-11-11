#include "pch.h"
#include "MessageReader.h"
#include "Logger.h"

MessageReader::MessageReader(Message* message)
{
#ifdef DEBUG
    LOGTRACE(L"MessageReader::MessageReader(Message* = x%08p) [this = x%08p]", message, this);
#endif
    if (message)
    {
        m_message = new Message(*message);
    }
}

MessageReader::MessageReader(Message* message, bool takeContents)
{
    if (message)
    {
        m_message = takeContents ? new Message(std::move(*message)) : new Message(*message);
    }
}

MessageReader::~MessageReader()
{
#ifdef DEBUG
    LOGTRACE(L"MessageReader::~MessageReader() [this = x%08p]", this);
#endif
     if (m_message)
    {
        delete m_message;
        m_message = nullptr;
    }

}

DWORD
MessageReader::GetMessageSize()
{
    return m_message ? m_message->GetDataLen() : 0;
}

BYTE*
MessageReader::GetRawMessage()
{
    return m_message ? m_message->GetData() : nullptr;
}

DWORD
MessageReader::GetDWORD(DWORD &offset)
{
    DWORD result = 0;

    if (m_message)
    {
        if (offset < m_message->GetDataLen() && m_message->GetDataLen() - offset >= 4)
        {
            BYTE* data = m_message->GetData();

            result = (DWORD)(data[offset] + ((DWORD)data[offset + 1] << 8) + ((DWORD)data[offset + 2] << 16) + ((DWORD)data[offset + 3] << 24));

            offset += 4;
        }
        else
        {
            LOGERROR(L"Unable to read DWORD value as it would extend beyond end of data");
        }
    }
    else
    {
        LOGERROR(L"Unable to read DWORD as no message object");
    }

    return result;
}

WORD
MessageReader::GetWORD(DWORD &offset)
{
    WORD result = 0;

    if (m_message)
    {
        if (offset < m_message->GetDataLen() && m_message->GetDataLen() - offset >= 2)
        {
            BYTE* data = m_message->GetData();

            result = (WORD)(data[offset] + ((WORD)data[offset + 1] << 8));

            offset += 2;
        }
        else
        {
            LOGERROR(L"Unable to read WORD value as it would extend beyond end of data");
        }
    }
    else
    {
        LOGERROR(L"Unable to read WORD as no message object");
    }

    return result;
}

BYTE
MessageReader::GetBYTE(DWORD &offset)
{
    BYTE result = 0;

    if (m_message)
    {
        if (offset < m_message->GetDataLen())
        {
            result = m_message->GetData()[offset++];
        }
        else
        {
            LOGERROR(L"Unable to read BYTE value as it would extend beyond end of data");
        }
    }
    else
    {
        LOGERROR(L"Unable to read BYTE as no message object");
    }

    return result;
}

std::wstring
MessageReader::GetString(DWORD &offset)
{
    std::wstring result;

    if (m_message)
    {
        if (offset < m_message->GetDataLen())
        {
            BYTE* data = m_message->GetData();
            size_t strlen = (size_t)data[offset];

            // Length byte, then strlen UTF-16 characters
            if ((size_t)(m_message->GetDataLen() - offset - 1) >= strlen * sizeof(WCHAR))
            {
                offset++;

                if (strlen > 0)
                {
                    bool nullTerminated = (*(WCHAR*)(data + offset + (strlen - 1) * sizeof(WCHAR)) == L'\0');

                    result = std::wstring(((WCHAR*)(data + offset)), strlen - (nullTerminated ? 1 : 0));
                    offset += static_cast<DWORD>(strlen * sizeof(WCHAR));
                }
            }
            else
            {
                LOGERROR(L"Unable to read string value as it would extend beyond end of data");
            }
        }
    }
    else
    {
        LOGERROR(L"Unable to read String as no message object");
    }

    return result;
}

// The element count comes from the camera, so never trust it further than the data that is
// actually there: clamp it to how many elements of at least minElementSize bytes can still fit.
static DWORD
ClampCount(DWORD count, DWORD offset, DWORD dataLen, DWORD minElementSize)
{
    DWORD remaining = offset < dataLen ? dataLen - offset : 0;
    DWORD fits = remaining / minElementSize;

    if (count > fits)
    {
        LOGERROR(L"Array claims %u elements but only room for %u remain in the data", count, fits);

        return fits;
    }

    return count;
}

std::list<BYTE>
MessageReader::GetBYTEArray(DWORD& offset)
{
    std::list<BYTE> result;
    DWORD count = GetDWORD(offset);
    count = ClampCount(count, offset, GetMessageSize(), 1);

    for (DWORD i = 0; i < count; i++)
    {
        result.push_back(GetBYTE(offset));
    }

    return result;
}

std::list<WORD>
MessageReader::GetWORDArray(DWORD& offset)
{
    DWORD count = GetDWORD(offset);
    count = ClampCount(count, offset, GetMessageSize(), 2);
    std::list<WORD> result;

    for (DWORD i = 0; i < count; i++)
    {
        result.push_back(GetWORD(offset));
    }

    return result;
}

std::list<DWORD>
MessageReader::GetDWORDArray(DWORD& offset)
{
    DWORD count = GetDWORD(offset);
    count = ClampCount(count, offset, GetMessageSize(), 4);
    std::list<DWORD> result;

    for (DWORD i = 0; i < count; i++)
    {
        result.push_back(GetDWORD(offset));
    }

    return result;
}

std::list<std::wstring>
MessageReader::GetStringArray(DWORD& offset)
{
    // Every string takes at least its length byte
    DWORD count = GetDWORD(offset);
    count = ClampCount(count, offset, GetMessageSize(), 1);
    std::list<std::wstring> result;

    for (DWORD i = 0; i < count; i++)
    {
        DWORD before = offset;

        result.push_back(GetString(offset));

        if (offset == before)
        {
            // A string that couldn't be read doesn't advance; the rest won't read either
            break;
        }
    }

    return result;
}
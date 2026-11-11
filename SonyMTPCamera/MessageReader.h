#pragma once
#include "pch.h"
#include <set>
#include <list>

#include "Message.h"

class MessageReader
{
public:
    // Copies the message (the caller keeps using its own)
    MessageReader(Message *message);
    // Takes the message's buffer without copying it; the caller's message is left empty
    MessageReader(Message *message, bool takeContents);
    virtual ~MessageReader();

protected:
    DWORD GetMessageSize();
    BYTE* GetRawMessage();

    BYTE GetBYTE(DWORD &addr);
    WORD GetWORD(DWORD &addr);
    DWORD GetDWORD(DWORD &addr);
    std::wstring GetString(DWORD &addr);

    std::list<BYTE> GetBYTEArray(DWORD& addr);
    std::list<WORD> GetWORDArray(DWORD& addr);
    std::list<DWORD> GetDWORDArray(DWORD& addr);
    std::list<std::wstring> GetStringArray(DWORD& addr);

private:
    Message* m_message = nullptr;
};
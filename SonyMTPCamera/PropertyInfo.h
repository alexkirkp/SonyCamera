#pragma once
#include "pch.h"
#include <list>
#include "Property.h"
#include "PropertyValue.h"

class PropertyInfo
{
public:
    PropertyInfo();
    PropertyInfo(const PropertyInfo* rhs);
    ~PropertyInfo();

    PropertyInfo* operator=(const PropertyInfo* rhs);

    void SetId(Property id);
    Property GetId() const;
    void SetType(DataType type);
    DataType GetType() const;
    void SetAccess(Accessibility access);
    Accessibility GetAccess() const;
    void SetSonySpare(BYTE sonySpare);
    BYTE GetSonySpare() const;
    // Button-style control (shutter, focus): written with the "set control" command, not as a value
    bool IsButton() const;
    // The descriptor's enabled byte says a write would be ignored: 0 unavailable, 2 display-only
    bool IsWriteIgnored() const;
    // Added by the DLL because the camera didn't enumerate it; its value is a placeholder, not
    // something the camera reported
    void SetStandIn(bool standIn);
    bool IsStandIn() const;
    void SetDefault(PropertyValue* def);
    PropertyValue* GetDefault() const;
    void SetFormMode(FormMode mode);
    FormMode GetFormMode() const;
    void SetRange(PropertyValue* lo, PropertyValue* hi, PropertyValue* step);
    PropertyValue* GetRangeLo() const;
    PropertyValue* GetRangeHi() const;
    PropertyValue* GetRangeStep() const;
    void SetEnumeration(std::list<PropertyValue*> values);
    std::list<PropertyValue*> GetEnumeration() const;

    std::wstring ToString() const;

private:
    void Copy(const PropertyInfo* rhs);

    Property m_id = Property::Undefined;
    DataType m_type = DataType::UNKNOWN;
    Accessibility m_access = Accessibility::READ_ONLY;
    FormMode m_form = FormMode::NONE;
    BYTE m_sonySpare = 0;
    bool m_standIn = false;

    PropertyValue* m_default = nullptr;
    PropertyValue* m_rangeLo = nullptr;
    PropertyValue* m_rangeHi = nullptr;
    PropertyValue* m_rangeStep = nullptr;
    std::list<PropertyValue*> m_enum;
};


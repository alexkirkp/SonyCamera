#pragma once
#include "pch.h"
#include <unordered_map>
#include "CameraProperty.h"
#include "MessageReader.h"
#include "PropertyValue.h"

typedef std::unordered_map<Property, CameraProperty*> CAMERAPROP;
typedef std::pair<Property, CameraProperty*> CAMERAPROPERTY;

class CameraSettings : public MessageReader
{
public:
    CameraSettings();
    // twoEnumLists: the dump was read with ext info version 300 (see CameraProperty::Slurp)
    CameraSettings(Message *message, bool twoEnumLists = false);
    CameraSettings(const CameraSettings& rhs);
    void Copy(const CameraSettings& rhs);
    virtual ~CameraSettings();
    CameraSettings* operator=(CameraSettings* rhs);

    void AddProperty(CameraProperty* property);
    CameraProperty* GetProperty(Property id) const;
    PropertyValue* GetPropertyValue(Property id) const;

    CAMERAPROP::const_iterator cbegin();
    CAMERAPROP::const_iterator cend();
    CAMERAPROP::const_iterator cfind(Property id) const;

    int size() const;

    //private
    CAMERAPROP m_properties;
};
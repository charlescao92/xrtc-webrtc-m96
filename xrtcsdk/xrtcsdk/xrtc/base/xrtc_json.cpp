#include "xrtc/base/xrtc_json.h"

#include <json/json.h>

namespace xrtc {

    JsonValue::JsonValue(bool b) {
        type_ = Type::Bool;
        bool_value_ = b;
    }

    JsonValue::JsonValue(double n) {
        type_ = Type::Double;
        double_value_ = n;
    }

    JsonValue::JsonValue(int n) {
        type_ = Type::Int;
        ull_value_ = n;
    }

    JsonValue::JsonValue(unsigned int n) {
        type_ = Type::Int;
        ull_value_ = n;
    }

    JsonValue::JsonValue(long n) {
        type_ = Type::Int;
        ull_value_ = n;
    }

    JsonValue::JsonValue(unsigned long n) {
        type_ = Type::Int;
        ull_value_ = n;
    }

    JsonValue::JsonValue(long long n) {
        type_ = Type::Int;
        ull_value_ = n;
    }

    JsonValue::JsonValue(unsigned long long n) {
        type_ = Type::Int;
        ull_value_ = n;
    }

    JsonValue::JsonValue(const std::string& s) {
        type_ = Type::String;
        string_value_ = s;
    }

    JsonValue::JsonValue(const char* s) {
        type_ = Type::String;
        string_value_ = s;
    }

    JsonValue::JsonValue(const JsonArray& a) {
        type_ = Type::Array;
        if (array_value_ == nullptr) {
            array_value_ = new JsonArray();
        }
        *array_value_ = a;
    }

    JsonValue::JsonValue(const JsonObject& o) {
        type_ = Type::Object;
        if (object_value_ == nullptr) {
            object_value_ = new JsonObject();
        }
        *object_value_ = o;
    }

    JsonValue::~JsonValue() {
        if (array_value_ != nullptr) {
            delete array_value_;
            array_value_ = nullptr;
        }

        if (object_value_ != nullptr) {
            delete object_value_;
            object_value_ = nullptr;
        }
    }

    JsonValue::JsonValue(const JsonValue& other) {
        type_ = other.type_;
        bool_value_ = other.bool_value_;
        double_value_ = other.double_value_;
        string_value_ = other.string_value_;
        ull_value_ = other.ull_value_;

        if (other.array_value_ != nullptr) {
            if (array_value_ == nullptr) {
                array_value_ = new JsonArray();
            }
            *array_value_ = *other.array_value_;
        }

        if (other.object_value_) {
            if (object_value_ == nullptr) {
                object_value_ = new JsonObject();
            }
            *object_value_ = *other.object_value_;
        }
    }

    JsonValue& JsonValue::operator=(const JsonValue& other) {
        type_ = other.type_;
        bool_value_ = other.bool_value_;
        double_value_ = other.double_value_;
        string_value_ = other.string_value_;
        ull_value_ = other.ull_value_;

        if (other.array_value_ != nullptr) {
            if (array_value_ == nullptr) {
                array_value_ = new JsonArray();
            }
            *array_value_ = *other.array_value_;
        }

        if (other.object_value_) {
            if (object_value_ == nullptr) {
                object_value_ = new JsonObject();
            }
            *object_value_ = *other.object_value_;
        }
        return *this;
    }

    JsonValue::Type JsonValue::type() const {
        return type_;
    }

    bool JsonValue::ToBool(bool default_value) const {
        if (IsBool()) {
            return bool_value_;
        }
        return default_value;
    }

    unsigned long long JsonValue::ToInt(unsigned long long default_value) const {
        if (IsInt()) {
            return ull_value_;
        }
        return default_value;
    }

    double JsonValue::ToDouble(double default_value) const {
        if (IsDouble()) {
            return double_value_;
        }
        return default_value;
    }

    std::string JsonValue::ToString() const {
        if (IsString()) {
            return string_value_;
        }
        return std::string();
    }

    std::string JsonValue::ToString(const std::string& default_value) const {
        if (IsString()) {
            return string_value_;
        }
        return default_value;
    }

    JsonArray JsonValue::ToArray() const {
        if (IsArray() && array_value_ != nullptr) {
            return *array_value_;
        }
        return JsonArray();
    }

    JsonArray JsonValue::ToArray(const JsonArray& default_value) const {
        if (IsArray() && array_value_ != nullptr) {
            return *array_value_;
        }
        return default_value;
    }

    JsonObject JsonValue::ToObject() const {
        if (IsObject() && object_value_ != nullptr) {
            return *object_value_;
        }
        return JsonObject();
    }

    JsonObject JsonValue::ToObject(const JsonObject& default_value) const {
        if (IsObject() && object_value_ != nullptr) {
            return *object_value_;
        }
        return default_value;
    }

    std::string JsonValue::ToJson() const {
        auto lambda_func_json = [](auto& self, const JsonValue& vxrtc_value) -> Json::Value {
            Json::Value value;
            if (vxrtc_value.type() == JsonValue::Bool) {
                value = vxrtc_value.ToBool();
            }
            else if (vxrtc_value.type() == JsonValue::Double) {
                value = vxrtc_value.ToDouble();
            }
            else if (vxrtc_value.type() == JsonValue::String) {
                value = vxrtc_value.ToString();
            }
            else if (vxrtc_value.type() == JsonValue::Int) {
                value = (Json::UInt64)vxrtc_value.ToInt();
            }
            else if (vxrtc_value.type() == JsonValue::Array) {
                JsonArray arr = vxrtc_value.ToArray();
                for (int i = 0; i < arr.Size(); i++) {
                    JsonValue v = arr[i];
                    value[i] = self(self, v);
                }
            }
            else if (vxrtc_value.type() == JsonValue::Object) {
                JsonObject obj = vxrtc_value.ToObject();
                std::vector<std::string> keys = obj.Keys();
                for (auto itor = keys.begin(); itor != keys.end(); itor++) {
                    std::string key = *itor;
                    JsonValue v = obj[key];
                    value[key] = self(self, v);
                }
            }

            return value;
        };

        std::string json = "";
        try {
            Json::Value value = lambda_func_json(lambda_func_json, *this);
            if (value.empty()) {
                return "";
            }
            json = Json::FastWriter().write(value);
        }
        catch (...) {

        }
        return json;
    }

    bool JsonValue::FromJson(const std::string& json) {
        try {
            Json::Value value;
            if (Json::Reader().parse(json, value)) {
                int size = value.size();
                Json::ValueType t = value.type();
                if (t == Json::ValueType::objectValue) {
                    type_ = Type::Object;

                    std::map<std::string, JsonValue> obj_values;
                    Json::Value::Members members = value.getMemberNames();

                    for (auto itor = members.begin(); itor != members.end(); itor++) {
                        std::string key = *itor;
                        Json::Value item = value[key];
                        Json::ValueType item_type = item.type();

                        if (item_type == Json::ValueType::intValue) {
                            long long v = item.asInt64();
                            obj_values[key] = JsonValue(v);
                        }
                        else if (item_type == Json::ValueType::uintValue) {
                            unsigned long long v = item.asUInt64();
                            obj_values[key] = JsonValue(v);
                        }
                        else if (item_type == Json::ValueType::realValue) {
                            double d = item.asDouble();
                            obj_values[key] = JsonValue(d);
                        }
                        else if (item_type == Json::ValueType::stringValue) {
                            std::string s = item.asString();
                            obj_values[key] = JsonValue(s);
                        }
                        else if (item_type == Json::ValueType::booleanValue) {
                            bool b = item.asBool();
                            obj_values[key] = JsonValue(b);
                        }
                        else if (item_type == Json::ValueType::arrayValue) {
                            JsonValue arr;
                            std::string arrayJsonStr = Json::FastWriter().write(item);
                            if (arr.FromJson(arrayJsonStr)) {
                                obj_values[key] = arr;
                            }
                        }
                        else if (item_type == Json::ValueType::objectValue) {
                            JsonValue obj;
                            std::string objJsonStr = Json::FastWriter().write(item);
                            if (obj.FromJson(objJsonStr)) {
                                obj_values[key] = obj;
                            }
                        }
                    }

                    JsonObject obj = JsonObject(obj_values);
                    if (object_value_ == nullptr) {
                        object_value_ = new JsonObject();
                    }

                    *object_value_ = obj;
                }
                else if (t == Json::ValueType::arrayValue) {
                    type_ = Type::Array;
                    std::vector<JsonValue> values;
                    for (int i = 0; i < value.size(); i++) {
                        Json::Value v = value[i];
                        std::string vstr = Json::FastWriter().write(v);
                        JsonValue vxrtc_value;
                        vxrtc_value.FromJson(vstr);
                        values.push_back(vxrtc_value);
                    }

                    JsonArray arr = JsonArray(values);
                    if (array_value_ == nullptr) {
                        array_value_ = new JsonArray();
                    }
                    *array_value_ = arr;
                }
            }
        }
        catch (...) {
            return false;
        }

        return true;
    }

    JsonObject::JsonObject() {

    }

    JsonObject::JsonObject(std::map<std::string, JsonValue> values) {
        values_ = values;
    }

    JsonObject::~JsonObject() {
    }

    JsonObject::JsonObject(const JsonObject& other) {
        values_ = other.values_;
    }

    JsonObject& JsonObject::operator=(const JsonObject& other) {
        values_ = other.values_;
        return *this;
    }

    std::vector<std::string> JsonObject::Keys() const {
        std::vector<std::string> key;
        for (auto itor = values_.begin(); itor != values_.end(); itor++) {
            key.push_back(itor->first);
        }
        return key;
    }

    bool JsonObject::Has(std::string key) const {
        if (values_.find(key) == values_.end()) {
            return false;
        }
        return true;
    }

    int JsonObject::Size() const {
        return values_.size();
    }

    JsonValue JsonObject::operator[](std::string& key) const {
        auto itor = values_.find(key);
        if (itor == values_.end()) {
            return JsonValue();
        }
        return itor->second;
    }

    JsonValue& JsonObject::operator[](std::string& key) {
        if (values_.find(key) == values_.end()) {
            values_[key] = JsonValue();
        }

        return values_[key];
    }

    JsonValue& JsonObject::operator[](const char* key) {
        //return operator[](std::string(key));
        std::string key2 = std::string(key);
        if (values_.find(key2) == values_.end()) {
            values_[key2] = JsonValue();
        }
        return values_[key2];
    }

    void JsonObject::Remove(const std::string& key) {
        values_.erase(key);
    }

    JsonArray::JsonArray() {

    }

    JsonArray::JsonArray(std::vector<JsonValue> values) {
        values_ = values;
    }

    JsonArray::~JsonArray() {

    }

    JsonArray::JsonArray(const JsonArray& other) {
        values_ = other.values_;
    }
    JsonArray& JsonArray::operator=(const JsonArray& other) {
        values_ = other.values_;
        return *this;
    }

    int JsonArray::Size() const {
        return values_.size();
    }

    void JsonArray::Append(const JsonValue& value) {
        values_.push_back(value);
    }

    void JsonArray::RemoveAt(int i) {
        if (values_.size() <= i) {
            return;
        }

        auto itor = values_.begin();
        for (int j = 0; j < i; j++) {
            itor++;
        }

        values_.erase(itor);
    }

    JsonValue JsonArray::operator[](int i) const {
        if (values_.size() <= i) {
            return JsonValue();
        }

        return values_[i];
    }

} // namespace xrtc
#ifndef XRTC_BASE_XRTC_JSON_H_
#define XRTC_BASE_XRTC_JSON_H_

#include <vector>
#include <map>
#include <string>

namespace xrtc {

    class JsonArray;
    class JsonObject;

    class JsonValue {
    public:
        enum Type {
            Null = 0x0,
            Bool = 0x1,
            Double = 0x2,
            String = 0x3,
            Array = 0x4,
            Object = 0x5,
            Int = 0x06,
            Undefined = 0x80
        };

        JsonValue(Type t = Null) { type_ = t; }
        JsonValue(bool b);
        JsonValue(double n);

        JsonValue(int n);
        JsonValue(unsigned int n);
        JsonValue(long n);
        JsonValue(unsigned long n);
        JsonValue(long long n);
        JsonValue(unsigned long long n);

        JsonValue(const char*);
        JsonValue(const std::string& s);
        JsonValue(const JsonArray& a);
        JsonValue(const JsonObject& o);

        ~JsonValue();

        JsonValue(const JsonValue& other);
        JsonValue& operator=(const JsonValue& other);

        Type type() const;

        inline bool IsNull() const { return type() == Null; }
        inline bool IsBool() const { return type() == Bool; }
        inline bool IsDouble() const { return type() == Double; }
        inline bool IsString() const { return type() == String; }
        inline bool IsArray() const { return type() == Array; }
        inline bool IsObject() const { return type() == Object; }
        inline bool IsUndefined() const { return type() == Undefined; }
        inline bool IsInt() const { return type() == Int; }

        bool ToBool(bool defaultValue = false) const;
        unsigned long long ToInt(unsigned long long defaultValue = 0) const;
        double ToDouble(double defaultValue = 0) const;
        std::string ToString() const;
        std::string ToString(const std::string& default_value) const;
        JsonArray ToArray() const;
        JsonArray ToArray(const JsonArray& default_value) const;
        JsonObject ToObject() const;
        JsonObject ToObject(const JsonObject& default_value) const;

        std::string ToJson() const;
        bool FromJson(const std::string& json);

    private:
        Type type_ = Null;
        bool bool_value_ = false;
        double double_value_ = 0.0f;
        unsigned long long ull_value_ = 0;

        std::string string_value_ = "";
        JsonArray* array_value_ = nullptr;
        JsonObject* object_value_ = nullptr;
    };

    class JsonObject {
    public:
        JsonObject();
        JsonObject(std::map<std::string, JsonValue>);

        ~JsonObject();

        JsonObject(const JsonObject& other);
        JsonObject& operator=(const JsonObject& other);

        std::vector<std::string> Keys() const;
        int Size() const;

        JsonValue operator[](std::string& key) const;
        JsonValue operator[](const char* key) const;

        JsonValue& operator[](std::string& key);
        JsonValue& operator[](const char* key);

        bool Has(std::string) const;
        void Remove(const std::string& key);

    private:
        std::map<std::string, JsonValue> values_;
    };

    class JsonArray {
    public:
        JsonArray();
        JsonArray(std::vector<JsonValue>);

        ~JsonArray();
        JsonArray(const JsonArray& other);
        JsonArray& operator=(const JsonArray& other);

        int Size() const;
        inline int Count() const { return Size(); }
        void Append(const JsonValue& value);
        void RemoveAt(int i);

        JsonValue operator[](int i) const;

    private:
        std::vector<JsonValue> values_;
    };

} // namespace xrtc

#endif // XRTC_BASE_XRTC_JSON_H_
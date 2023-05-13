#include <rtc_base/logging.h>
#include <rtc_base/byte_order.h>
#include <rtc_base/crc32.h>
#include <rtc_base/message_digest.h>
#include <rtc_base/socket_address.h>

#include "ice/stun.h"

namespace xrtc {

const char EMPTY_TRANSACION_ID[] = "000000000000";
const size_t STUN_FINGERPRINT_XOR_VALUE = 0x5354554e;

std::string stun_method_to_string(int type) {
    switch (type) {
        case STUN_BINDING_REQUEST:
            return "BINDING REQUEST";
        case STUN_BINDING_RESPONSE:
            return "BINDING RESPONSE";
        case STUN_BINDING_ERROR_RESPONSE:
            return "BINDING ERROR_RESPONSE";
        default:
            return "Unknown<" + std::to_string(type) + ">";
    }
}

StunMessage::StunMessage() :
    type_(0),
    length_(0),
    transaction_id_(EMPTY_TRANSACION_ID)
{
}

StunMessage::~StunMessage() = default;

bool StunMessage::validate_fingerprint(const char *data, size_t len) {
    // 检查长度
    size_t fingerprint_attr_size = k_stun_attribute_header_size + StunUInt32Attribute::SIZE; // 8 = 4 + 4
    if (len % 4 != 0 || len < k_stun_header_size + fingerprint_attr_size) {
        return false;
    }

    // 检查magic cookie
    const char* magic_cookie = data + k_stun_transaction_id_offset -
        k_stun_magic_cookie_length;
    if (rtc::GetBE32(magic_cookie) != k_stun_magic_cookie) {
        return false;
    }
    
    // 检查attr type和length
    const char* fingerprint_attr_data = data + len - fingerprint_attr_size;
    if (rtc::GetBE16(fingerprint_attr_data) != STUN_ATTR_FINGERPRINT ||
            rtc::GetBE16(fingerprint_attr_data + sizeof(uint16_t)) !=
            StunUInt32Attribute::SIZE) 
    {
        return false;
    }
    
    // 检查fingerprint的值
    uint32_t fingerprint = rtc::GetBE32(fingerprint_attr_data + k_stun_attribute_header_size);

    return (fingerprint ^ STUN_FINGERPRINT_XOR_VALUE) ==
        rtc::ComputeCrc32(data, len - fingerprint_attr_size);
}

StunMessage::IntegrityStatus StunMessage::validate_message_integrity(const std::string& password) {
    password_ = password;

    if (get_byte_string(STUN_ATTR_MESSAGE_INTEGRITY)) {
        if (validate_message_integrity_of_type(STUN_ATTR_MESSAGE_INTEGRITY,
                    k_stun_message_integrity_size,
                    buffer_.c_str(), buffer_.length(),
                    password))
        {
            integrity_ = IntegrityStatus::k_integrity_ok;
        } else {
            integrity_ = IntegrityStatus::k_integrity_bad;
        }
    } else {
        integrity_ = IntegrityStatus::k_no_integrity;
    }

    return integrity_;
}

bool StunMessage::validate_message_integrity_of_type(uint16_t mi_attr_type,
        size_t mi_attr_size, const char* data, size_t size,
        const std::string& password) 
{
    if (size % 4 != 0 || size < k_stun_header_size) {
        return false;
    }
    
    // data[2] 前两个字节是message type
    uint16_t length = rtc::GetBE16(&data[2]);
    if (length + k_stun_header_size != size) {
        return false;
    }

    // 查找MI属性的位置
    size_t current_pos = k_stun_header_size;
    bool has_message_integrity = false;
    while (current_pos + k_stun_attribute_header_size <= size) {
        uint16_t attr_type;
        uint16_t attr_length;
        attr_type = rtc::GetBE16(&data[current_pos]); 
        attr_length = rtc::GetBE16(&data[current_pos + sizeof(attr_type)]);
        if (attr_type == mi_attr_type) {
            has_message_integrity = true;
            break;
        }

        // 偏移位置后继续查找MI属性位置
        current_pos += (k_stun_attribute_header_size + attr_length);
        if (attr_length % 4 != 0) {
            current_pos += (4 - (attr_length % 4));
        }                     
    }

    if (!has_message_integrity) {
        return false;
    }

    size_t mi_pos = current_pos;
    std::unique_ptr<char[]> temp_data(new char[mi_pos]);
    memcpy(temp_data.get(), data, mi_pos);
    // 判断MI之后是否还有其他属性，存在则需要调整STUN头部字段message length 的值
    if (size > current_pos + k_stun_attribute_header_size + mi_attr_size) {
        size_t extra_pos = mi_pos + k_stun_attribute_header_size + mi_attr_size;
        size_t extra_size = size - extra_pos;
        size_t adjust_new_len = size - extra_size - k_stun_header_size;
        rtc::SetBE16(temp_data.get() + 2, adjust_new_len);
    }   

    // 计算哈希值
    char hmac[k_stun_message_integrity_size];
    size_t ret = rtc::ComputeHmac(rtc::DIGEST_SHA_1, password.c_str(),
            password.length(), temp_data.get(), current_pos, hmac,
            sizeof(hmac));
    
    if (ret != k_stun_message_integrity_size) {
        return false;
    }

    return memcmp(data + mi_pos + k_stun_attribute_header_size, hmac, mi_attr_size) == 0;   
}

bool StunMessage::read(rtc::ByteBufferReader* buf) {
    if (!buf) {
        return false;
    }

    buffer_.assign(buf->Data(), buf->Length());

    // rtc::ByteBufferReader获取message类型，注意该指针读取值后是会偏移的。
    if (!buf->ReadUInt16(&type_)) {
        return false;
    }

    // 过滤掉rtp/rtcp 10(2)
    if (type_ & 0x0800) {
        return false;
    }

    if (!buf->ReadUInt16(&length_)) {
        return false;
    }

    std::string magic_cookie;
    if (!buf->ReadString(&magic_cookie, k_stun_magic_cookie_length)) {
        return false;
    }

    std::string transaction_id;
    if (!buf->ReadString(&transaction_id, k_stun_transaction_id_length)) {
        return false;
    }

    uint32_t magic_cookie_int;
    memcpy(&magic_cookie_int, magic_cookie.data(), sizeof(magic_cookie_int));
    if (rtc::NetworkToHost32(magic_cookie_int) != k_stun_magic_cookie) {
        // magic_cookie是老版本的
        transaction_id.insert(0, magic_cookie);
    }

    transaction_id_ = transaction_id;

    // buf剩余的长度，是否跟Message Length相等
    if (buf->Length() != length_) {
        return false;
    }

    attrs_.resize(0);

    // 循环读取stun各种属性
    while (buf->Length() > 0) {
        uint16_t attr_type;
        uint16_t attr_length;
        if (!buf->ReadUInt16(&attr_type)) {
            return false;
        }

        if (!buf->ReadUInt16(&attr_length)) {
            return false;
        }

        std::unique_ptr<StunAttribute> attr(create_attribute(attr_type, attr_length));
        if (!attr) {   // 如果读取属性失败，则跳过，读取下一个 
            if (attr_length % 4 != 0) {
                attr_length += (4 - (attr_length % 4)); // 获取真实的长度，需要加上因为4字节对齐要补充的值
            } 
            if (!buf->Consume(attr_length)) {
                return false;
            } 

        } else {
            if (!attr->read(buf)) {
                return false;
            }

            attrs_.push_back(std::move(attr));
        }       
    }

    return true;
}

bool StunMessage::write(rtc::ByteBufferWriter* buf) const {
    if (!buf) {
        return false;
    }

    // 写入头部信息
    buf->WriteUInt16(type_);
    buf->WriteUInt16(length_);
    buf->WriteUInt32(k_stun_magic_cookie);
    buf->WriteString(transaction_id_);
    
    // 写入所有属性
    for (const auto& attr : attrs_) {
        buf->WriteUInt16(attr->type());
        buf->WriteUInt16(attr->length());
        if (!attr->write(buf)) {
            return false;
        }
    }

    return true;
}

StunAttributeValueType StunMessage::get_attribute_value_type(int type) {
    switch (type) {
        case STUN_ATTR_USERNAME:
            return STUN_VALUE_BYTE_STRING;
        case STUN_ATTR_MESSAGE_INTEGRITY:
            return STUN_VALUE_BYTE_STRING;
        case STUN_ATTR_PRIORITY:
            return STUN_VALUE_UINT32;
        default:
            return STUN_VALUE_UNKNOWN;
    }
}

const StunUInt32Attribute* StunMessage::get_uint32(uint16_t type) {
    return static_cast<const StunUInt32Attribute*>(get_attribute(type));
}

const StunByteStringAttribute* StunMessage::get_byte_string(uint16_t type) {
    return static_cast<const StunByteStringAttribute*>(get_attribute(type));
}

const StunErrorCodeAttribute* StunMessage::get_error_code() {
    return static_cast<const StunErrorCodeAttribute*>(
            get_attribute(STUN_ATTR_ERROR_CODE));
}
    
int StunMessage::get_error_code_value() {
    auto error_attr = get_error_code();
    return error_attr ? error_attr->code() : STUN_ERROR_GLOBAL_FAILURE;
}

void StunMessage::add_attribute(std::unique_ptr<StunAttribute> attr) {
    size_t attr_len = attr->length();
    if (attr_len % 4 != 0) {
        attr_len += (4 - (attr_len % 4));
    }

    length_ += (attr_len + k_stun_attribute_header_size);

    attrs_.push_back(std::move(attr));
}

bool StunMessage::add_message_integrity(const std::string& password) {
    return add_message_integrity_of_type(STUN_ATTR_MESSAGE_INTEGRITY,
            k_stun_message_integrity_size, password.c_str(),
            password.size());
}

bool StunMessage::add_message_integrity_of_type(uint16_t attr_type,
        uint16_t attr_size, const char* key, size_t key_len)
{
    auto mi_attr_ptr = std::make_unique<StunByteStringAttribute>(attr_type,
            std::string(attr_size, '0'));
    auto mi_attr = mi_attr_ptr.get();
    add_attribute(std::move(mi_attr_ptr));

    rtc::ByteBufferWriter buf;
    if (!write(&buf)) {
        return false;
    }

    // 计算哈希值
    size_t msg_len_for_hmac = buf.Length() - k_stun_attribute_header_size - mi_attr->length();
    char hmac[k_stun_message_integrity_size];
    size_t ret = rtc::ComputeHmac(rtc::DIGEST_SHA_1, key, key_len,
            buf.Data(), msg_len_for_hmac, hmac, sizeof(hmac));
    if (ret != sizeof(hmac)) {
        RTC_LOG(LS_WARNING) << "compute hmac error";
        return false;
    }
    
    mi_attr->copy_bytes(hmac, k_stun_message_integrity_size);
    password_.assign(key, key_len);
    integrity_ = IntegrityStatus::k_integrity_ok;

    return true;
}

bool StunMessage::add_fingerprint() {
    auto fingerprint_attr_ptr = std::make_unique<StunUInt32Attribute>(
            STUN_ATTR_FINGERPRINT, 0);
    auto fingerprint_attr = fingerprint_attr_ptr.get();
    add_attribute(std::move(fingerprint_attr_ptr));

    rtc::ByteBufferWriter buf;
    if (!write(&buf)) {
        return false;
    }

    size_t msg_len_for_crc32 = buf.Length() - k_stun_attribute_header_size -
        fingerprint_attr->length();
    uint32_t crc32 = rtc::ComputeCrc32(buf.Data(), msg_len_for_crc32);
    fingerprint_attr->set_value(crc32 ^ STUN_FINGERPRINT_XOR_VALUE);
    
    return true;       
}

StunAttribute* StunMessage::create_attribute(uint16_t type, uint16_t length) {
    StunAttributeValueType value_type = get_attribute_value_type(type);
    if (STUN_VALUE_UNKNOWN != value_type) {
        return StunAttribute::create(value_type, type, length, this);
    }

    return nullptr;
}

const StunAttribute* StunMessage::get_attribute(uint16_t type) {
    for (const auto& attr : attrs_) {
        if (attr->type() == type) {
            return attr.get();
        }
    }

    return nullptr;
}

StunAttribute::StunAttribute(uint16_t type, uint16_t length) :
    type_(type), length_(length) {
}

StunAttribute::~StunAttribute() = default;

StunAttribute* StunAttribute::create(StunAttributeValueType value_type,
        uint16_t type, uint16_t length, void* /*owner*/)
{
    switch (value_type) {
        case STUN_VALUE_BYTE_STRING:
            return new StunByteStringAttribute(type, length);
        case STUN_VALUE_UINT32:
            return new StunUInt32Attribute(type);
        default:
            return nullptr;
    }
}

std::unique_ptr<StunErrorCodeAttribute> StunAttribute::create_error_code() {
    return std::make_unique<StunErrorCodeAttribute>(
            STUN_ATTR_ERROR_CODE, StunErrorCodeAttribute::MIN_SIZE);
}

void StunAttribute::consume_padding(rtc::ByteBufferReader* buf) {
    int remain = length() % 4;
    if (remain > 0) {
        buf->Consume(4 - remain);
    }
}

void StunAttribute::write_padding(rtc::ByteBufferWriter* buf) {
    int remain = length() % 4;
    if (remain > 0) {
        char zeroes[4] = {0};
        buf->WriteBytes(zeroes, 4 - remain);
    }
}

// Address
StunAddressAttribute::StunAddressAttribute(uint16_t type, 
        const rtc::SocketAddress& addr) :
    StunAttribute(type, 0)
{
    set_address(addr);
}

void StunAddressAttribute::set_address(const rtc::SocketAddress& addr) {
    address_ = addr;
    
    switch (family()) {
        case STUN_ADDRESS_IPV4:
            set_length(SIZE_IPV4);
            break;
        case STUN_ADDRESS_IPV6:
            set_length(SIZE_IPV6);
            break;
        default:
            set_length(SIZE_UNDEF);
            break;
    }
}

StunAddressFamily StunAddressAttribute::family() {
    switch (address_.family()) {
        case AF_INET:
            return STUN_ADDRESS_IPV4;
        case AF_INET6:
            return STUN_ADDRESS_IPV6;
        default:
            return STUN_ADDRESS_UNDEFINE;
    }
}

bool StunAddressAttribute::read(rtc::ByteBufferReader* /*buf*/) {
    return true;
}

bool StunAddressAttribute::write(rtc::ByteBufferWriter* buf) {
    StunAddressFamily stun_family = family();
    if (STUN_ADDRESS_UNDEFINE == stun_family) {
        RTC_LOG(LS_WARNING) << "write address attribute error: unknown family";
        return false;
    }

    buf->WriteUInt8(0);
    buf->WriteUInt8(stun_family);
    buf->WriteUInt16(address_.port());

    switch (address_.family()) {
        case AF_INET: {
            in_addr v4addr = address_.ipaddr().ipv4_address();
            buf->WriteBytes((const char*)&v4addr, sizeof(v4addr));
            break;
        }
        case AF_INET6: {
            in6_addr v6addr = address_.ipaddr().ipv6_address();
            buf->WriteBytes((const char*)&v6addr, sizeof(v6addr));
            break;
        }
        default:
            return false;
    }

    return true;
}

// Xor Address
StunXorAddressAttribute::StunXorAddressAttribute(uint16_t type, 
        const rtc::SocketAddress& addr) :
    StunAddressAttribute(type, addr)
{
}

bool StunXorAddressAttribute::write(rtc::ByteBufferWriter* buf) {
    StunAddressFamily stun_family = family();
    if (STUN_ADDRESS_UNDEFINE == stun_family) {
        RTC_LOG(LS_WARNING) << "write address attribute error: unknown family";
        return false;
    }
    
    rtc::IPAddress xored_ip = get_xored_ip();
    if (AF_UNSPEC == xored_ip.family()) {
        return false;
    }

    buf->WriteUInt8(0);
    buf->WriteUInt8(stun_family);
    buf->WriteUInt16(address_.port() ^ (k_stun_magic_cookie >> 16));

    switch (address_.family()) {
        case AF_INET: {
            in_addr v4addr = xored_ip.ipv4_address();
            buf->WriteBytes((const char*)&v4addr, sizeof(v4addr));
            break;
        }
        case AF_INET6: {
            in6_addr v6addr = xored_ip.ipv6_address();
            buf->WriteBytes((const char*)&v6addr, sizeof(v6addr));
            break;
        }
        default:
            return false;
    }

    return true;
}

rtc::IPAddress StunXorAddressAttribute::get_xored_ip() {
    rtc::IPAddress ip = address_.ipaddr();
    switch (address_.family()) {
        case AF_INET: {
            in_addr v4addr = ip.ipv4_address();
            v4addr.s_addr = (v4addr.s_addr ^ rtc::HostToNetwork32(k_stun_magic_cookie));
            return rtc::IPAddress(v4addr);
        }
        case AF_INET6:
            break;
        default:
            break;
    }
    return rtc::IPAddress();
}

// UInt32
StunUInt32Attribute::StunUInt32Attribute(uint16_t type) :
    StunAttribute(type, SIZE), bits_(0) {}

StunUInt32Attribute::StunUInt32Attribute(uint16_t type, uint32_t value) :
    StunAttribute(type, SIZE), bits_(value) {}

bool StunUInt32Attribute::read(rtc::ByteBufferReader* buf) {
    if (length() != SIZE || !buf->ReadUInt32(&bits_)) {
        return false;
    }
    return true;
}

bool StunUInt32Attribute::write(rtc::ByteBufferWriter* buf) {
    buf->WriteUInt32(bits_);
    return true;
}

// UInt64
StunUInt64Attribute::StunUInt64Attribute(uint16_t type) :
    StunAttribute(type, SIZE), bits_(0) {}

StunUInt64Attribute::StunUInt64Attribute(uint16_t type, uint64_t value) :
    StunAttribute(type, SIZE), bits_(value) {}

bool StunUInt64Attribute::read(rtc::ByteBufferReader* buf) {
    if (length() != SIZE || !buf->ReadUInt64(&bits_)) {
        return false;
    }
    return true;
}

bool StunUInt64Attribute::write(rtc::ByteBufferWriter* buf) {
    buf->WriteUInt64(bits_);
    return true;
}

// ByteString
StunByteStringAttribute::StunByteStringAttribute(uint16_t type, uint16_t length) :
    StunAttribute(type, length) {}

StunByteStringAttribute::StunByteStringAttribute(uint16_t type, const std::string& str) :
    StunAttribute(type, 0)
{
    copy_bytes(str.c_str(), str.size());
}

void StunByteStringAttribute::copy_bytes(const char* bytes, size_t len) {
    char* new_bytes = new char[len];
    memcpy(new_bytes, bytes, len);
    set_bytes(new_bytes);
    set_length(len);
}

void StunByteStringAttribute::set_bytes(char* bytes) {
    if (bytes_) {
        delete[] bytes_;
    }
    bytes_ = bytes;
}
    
StunByteStringAttribute::~StunByteStringAttribute() {
    if (bytes_) {
        delete[] bytes_;
        bytes_ = nullptr;
    }
}
    
bool StunByteStringAttribute::read(rtc::ByteBufferReader* buf) {
    bytes_ = new char[length()];
    if (!buf->ReadBytes(bytes_, length())) {
        return false;
    }

    consume_padding(buf);

    return true;    
}

bool StunByteStringAttribute::write(rtc::ByteBufferWriter* buf) {
    buf->WriteBytes(bytes_, length());
    write_padding(buf);
    return true;
}

// ErrorCode 
const uint16_t StunErrorCodeAttribute::MIN_SIZE = 4;

StunErrorCodeAttribute::StunErrorCodeAttribute(uint16_t type, uint16_t length) :
    StunAttribute(type, length), class_(0), number_(0) {}

// 200 300 400 500 类型就是2 3 4 5
// 501 502 503 504 就是服务器错误，序号是1 2 3 4
void StunErrorCodeAttribute::set_code(int code) {
    class_ = code / 100;
    number_ = code % 100;
}

int StunErrorCodeAttribute::code() const {
    return class_ * 100 + number_;
}

void StunErrorCodeAttribute::set_reason(const std::string& reason) {
    reason_ = reason;
    set_length(MIN_SIZE + reason.size());
}

bool StunErrorCodeAttribute::read(rtc::ByteBufferReader* buf) {
    uint32_t val;
    if (length() < MIN_SIZE || !buf->ReadUInt32(&val)) {
        return false;
    }

    if ((val >> 11) != 0) {
        RTC_LOG(LS_ERROR) << "error-code bits not zero";
    }

    class_ = ((val >> 8) & 0x7);
    number_ = (val & 0xff);

    if (!buf->ReadString(&reason_, length() - 4)) {
        return false;
    }
        
    consume_padding(buf);

    return true;
}

bool StunErrorCodeAttribute::write(rtc::ByteBufferWriter* buf) {
    buf->WriteUInt32(class_ << 8 | number_); // number占用了后8位
    buf->WriteString(reason_);
    write_padding(buf);
    return true;
}

int get_stun_success_response(int req_type) {
    return is_stun_request_type(req_type) ? (req_type | 0x100) : -1;
}

int get_stun_error_response(int req_type) {
    return is_stun_request_type(req_type) ? (req_type | 0x110) : -1;
}

bool is_stun_request_type(int req_type) {
    return (req_type & k_stun_type_mask) == 0x000;
}

} // end namespace xrtc

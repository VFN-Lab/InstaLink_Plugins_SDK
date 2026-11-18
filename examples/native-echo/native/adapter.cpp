#include "instalink_extension_api.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {

char* DupString(const std::string& value) {
    auto* buffer = static_cast<char*>(std::malloc(value.size() + 1));
    if (buffer == nullptr) {
        return nullptr;
    }
    std::memcpy(buffer, value.c_str(), value.size() + 1);
    return buffer;
}

std::string EscapeJson(const std::string& value) {
    std::string escaped;
    escaped.reserve(value.size());
    for (char ch : value) {
        switch (ch) {
            case '\\':
                escaped += "\\\\";
                break;
            case '"':
                escaped += "\\\"";
                break;
            case '\n':
                escaped += "\\n";
                break;
            case '\r':
                escaped += "\\r";
                break;
            case '\t':
                escaped += "\\t";
                break;
            default:
                escaped += ch;
                break;
        }
    }
    return escaped;
}

std::string DescribePayload() {
    return R"({"ok":true,"adapter":"native_echo_adapter","api_version":1,"actions":["inspect","echo","reverse","uppercase"],"notes":"Replace this demo adapter with Sherpa-ONNX or any custom C++ engine using the same ABI."})";
}

}  // namespace

extern "C" void instalink_extension_on_load(const char* /*runtime_root*/) {}

extern "C" void instalink_extension_on_unload() {}

extern "C" const char* instalink_extension_describe() {
    return DupString(DescribePayload());
}

extern "C" const char* instalink_extension_invoke(const char* action, const char* payload_json) {
    const std::string actionValue = action == nullptr ? "" : action;
    const std::string payloadValue = payload_json == nullptr ? "{}" : payload_json;

    if (actionValue == "inspect") {
        return DupString(DescribePayload());
    }

    if (actionValue == "echo") {
        return DupString(
            std::string("{\"ok\":true,\"action\":\"echo\",\"payload_json\":\"") +
            EscapeJson(payloadValue) +
            "\"}"
        );
    }

    if (actionValue == "reverse") {
        std::string reversed = payloadValue;
        std::reverse(reversed.begin(), reversed.end());
        return DupString(
            std::string("{\"ok\":true,\"action\":\"reverse\",\"result\":\"") +
            EscapeJson(reversed) +
            "\"}"
        );
    }

    if (actionValue == "uppercase") {
        std::string uppercased = payloadValue;
        std::transform(
            uppercased.begin(),
            uppercased.end(),
            uppercased.begin(),
            [](unsigned char ch) { return static_cast<char>(std::toupper(ch)); }
        );
        return DupString(
            std::string("{\"ok\":true,\"action\":\"uppercase\",\"result\":\"") +
            EscapeJson(uppercased) +
            "\"}"
        );
    }

    return DupString(
        std::string("{\"ok\":false,\"action\":\"") +
        EscapeJson(actionValue) +
        "\",\"error\":\"Unsupported native action.\"}"
    );
}

extern "C" void instalink_extension_free_string(const char* value) {
    std::free(const_cast<char*>(value));
}

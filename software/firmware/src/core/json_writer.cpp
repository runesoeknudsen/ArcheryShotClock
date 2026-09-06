#include "json_writer.h"

#include <cstdio>

namespace Core {

JsonWriter::JsonWriter(char* buffer, uint16_t capacity, uint16_t reserve)
    : buffer_(buffer), capacity_(capacity), reserve_(reserve), length_(0), overflow_(false), depth_(0) {
  for (uint8_t index = 0; index < MAX_DEPTH; index++) {
    first_[index] = true;
    closers_[index] = '}';
  }
  if (capacity_ > 0) buffer_[0] = '\0';
}

void JsonWriter::put(char character) {
  if (length_ + reserve_ >= capacity_) {
    overflow_ = true;
    return;
  }
  buffer_[length_++] = character;
}

bool JsonWriter::fits(uint16_t bytes) {
  if (length_ + bytes + reserve_ < capacity_) return true;
  overflow_ = true;
  return false;
}

uint16_t JsonWriter::escapedLength(const char* text) const {
  uint16_t total = 0;
  for (const char* cursor = text; cursor && *cursor; cursor++) {
    const unsigned char character = static_cast<unsigned char>(*cursor);
    if (*cursor == '"' || *cursor == '\\' || *cursor == '\n' || *cursor == '\r' || *cursor == '\t') {
      total += 2;
    } else if (character < 0x20) {
      total += 6;
    } else {
      total += 1;
    }
  }
  return total;
}

uint16_t JsonWriter::keyOverhead(const char* key) const {
  uint16_t length = 0;
  for (const char* cursor = key; cursor && *cursor; cursor++) length++;
  // separator, two quotes, colon
  return static_cast<uint16_t>(length + 4);
}

void JsonWriter::append(const char* text) {
  for (const char* cursor = text; cursor && *cursor; cursor++) put(*cursor);
}

void JsonWriter::appendEscaped(const char* text) {
  for (const char* cursor = text; cursor && *cursor; cursor++) {
    const char character = *cursor;
    if (character == '"' || character == '\\') {
      put('\\');
      put(character);
    } else if (character == '\n') {
      append("\\n");
    } else if (character == '\r') {
      append("\\r");
    } else if (character == '\t') {
      append("\\t");
    } else if (static_cast<unsigned char>(character) < 0x20) {
      char escape[7];
      std::snprintf(escape, sizeof(escape), "\\u%04x", static_cast<unsigned>(static_cast<unsigned char>(character)));
      append(escape);
    } else {
      put(character);
    }
  }
}

void JsonWriter::separator() {
  if (depth_ == 0) return;
  if (!first_[depth_ - 1]) put(',');
  first_[depth_ - 1] = false;
}

void JsonWriter::beginObject() {
  separator();
  put('{');
  if (depth_ < MAX_DEPTH) {
    first_[depth_] = true;
    closers_[depth_] = '}';
    depth_++;
  } else {
    overflow_ = true;
  }
}

void JsonWriter::beginObject(const char* key) {
  separator();
  put('"');
  append(key);
  append("\":{");
  if (depth_ < MAX_DEPTH) {
    first_[depth_] = true;
    closers_[depth_] = '}';
    depth_++;
  } else {
    overflow_ = true;
  }
}

void JsonWriter::endObject() {
  if (depth_ > 0) depth_--;
  put('}');
}

void JsonWriter::beginArray(const char* key) {
  if (!fits(keyOverhead(key) + 1)) return;
  separator();
  put('"');
  append(key);
  append("\":[");
  if (depth_ < MAX_DEPTH) {
    first_[depth_] = true;
    closers_[depth_] = ']';
    depth_++;
  } else {
    overflow_ = true;
  }
}

void JsonWriter::endArray() {
  if (depth_ > 0) depth_--;
  put(']');
}

void JsonWriter::number(const char* key, int32_t value) {
  char digits[12];
  std::snprintf(digits, sizeof(digits), "%ld", static_cast<long>(value));
  if (!fits(keyOverhead(key) + escapedLength(digits))) return;
  separator();
  put('"');
  append(key);
  append("\":");
  append(digits);
}

void JsonWriter::unsigned32(const char* key, uint32_t value) {
  char digits[12];
  std::snprintf(digits, sizeof(digits), "%lu", static_cast<unsigned long>(value));
  if (!fits(keyOverhead(key) + escapedLength(digits))) return;
  separator();
  put('"');
  append(key);
  append("\":");
  append(digits);
}

void JsonWriter::text(const char* key, const char* value) {
  if (!fits(keyOverhead(key) + escapedLength(value ? value : "") + 2)) return;
  separator();
  put('"');
  append(key);
  append("\":\"");
  appendEscaped(value ? value : "");
  put('"');
}

void JsonWriter::boolean(const char* key, bool value) {
  if (!fits(keyOverhead(key) + 5)) return;
  separator();
  put('"');
  append(key);
  append("\":");
  append(value ? "true" : "false");
}

void JsonWriter::raw(const char* key, const char* rawValue) {
  if (!fits(keyOverhead(key) + escapedLength(rawValue))) return;
  separator();
  put('"');
  append(key);
  append("\":");
  append(rawValue);
}

void JsonWriter::pair(const char* key, uint32_t first, uint32_t second) {
  beginArray(key);
  arrayNumber(first);
  arrayNumber(second);
  endArray();
}

void JsonWriter::arrayNumber(uint32_t value) {
  char digits[12];
  std::snprintf(digits, sizeof(digits), "%lu", static_cast<unsigned long>(value));
  if (!fits(static_cast<uint16_t>(escapedLength(digits) + 1))) return;
  separator();
  append(digits);
}

void JsonWriter::arrayText(const char* value) {
  if (!fits(static_cast<uint16_t>(escapedLength(value ? value : "") + 3))) return;
  separator();
  put('"');
  appendEscaped(value ? value : "");
  put('"');
}

void JsonWriter::arrayBoolean(bool value) {
  if (!fits(6)) return;
  separator();
  append(value ? "true" : "false");
}

uint16_t JsonWriter::finish() {
  // Closing runs against the reserve, so a document that overflowed still
  // ends as valid JSON instead of stopping mid-key.
  while (depth_ > 0) {
    depth_--;
    if (length_ + 1 < capacity_) buffer_[length_++] = closers_[depth_];
  }
  if (length_ < capacity_) buffer_[length_] = '\0';
  return length_;
}

}  // namespace Core

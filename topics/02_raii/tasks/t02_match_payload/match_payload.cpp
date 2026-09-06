#include "match_payload.h"

namespace course {
MatchPayload::MatchPayload() {
}

MatchPayload::MatchPayload(const std::string& text) {
    size_ = text.length();
    if (size_ > 0) {
        data_ = new char[size_];
        std::copy(text.begin(), text.end(), data_);
    }
}

MatchPayload::MatchPayload(const MatchPayload& other) {
    size_ = other.size_;
    if (size_ > 0) {
        data_ = new char[size_];
        std::copy_n(other.data_, size_, data_);
    }
}

MatchPayload& MatchPayload::operator=(const MatchPayload& other) {
    if (this == &other) {
        return *this;
    }
    delete [] data_;
    size_ = other.size_;
    if (size_ > 0) {
        data_ = new char[size_];
        std::copy_n(other.data_, size_, data_);
    }

    return *this;
}

MatchPayload::MatchPayload(MatchPayload&& other) noexcept {
    data_ = other.data_;
    size_ = other.size_;
    other.data_ = nullptr;
    other.size_ = 0;
}

MatchPayload& MatchPayload::operator=(MatchPayload&& other) noexcept {
    if (this == &other) {
        return *this;
    }
    delete[] data_;
    data_ = other.data_;
    size_ = other.size_;
    other.data_ = nullptr;
    other.size_ = 0;
    return *this;
}

MatchPayload::~MatchPayload() {
    delete[] data_;
}


const char* MatchPayload::Data() const {
    return data_;
}

std::size_t MatchPayload::Size() const {
    return size_;
}

bool MatchPayload::Empty() const {
    return size_ == 0;
}

std::string MatchPayload::ToString() const {
    if (size_ == 0) {
        return std::string();
    }
    return std::string(data_, size_);
}
} // namespace course
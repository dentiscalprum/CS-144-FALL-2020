#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity)
 : capacity_(capacity), data_(capacity),
   start_(0), end_(0), size_(0), end_input_(false),
   total_written_(0), total_read_(0)
{}

size_t ByteStream::write(const string &data)
{
    if(end_input_) return 0;

    size_t len = min(data.size(), remaining_capacity());
    if(len == 0) return 0;

    size_t already_write = 0;
    if(start_ <= end_) {
        size_t len_write = min(capacity_ - end_, len);
        memcpy(&*(data_.begin()+end_), data.c_str(), len_write);

        end_ += len_write;
        already_write += len_write;
        if(end_ == capacity_) end_ = 0;
        if(already_write == len) {
            total_written_ += already_write;
            size_ += already_write;
            return already_write;
        }
    }

    size_t len_write = min(end_-start_, len-already_write);
    memcpy(&*(data_.begin()+end_), data.c_str() + already_write, len_write);

    end_ += len_write;
    already_write += len_write;
    total_written_ += already_write;
    size_ += already_write;

    return already_write;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const
{
    string str;
    size_t len_peek = min(len, size_);
    size_t already_read = 0;
    size_t cur_start = start_;
    if(cur_start >= end_) {
        size_t len_cpy = min(len_peek, capacity_ - cur_start);
        str += string(&*(data_.begin()+cur_start), len_cpy);
        already_read += len_cpy;
        if(already_read == len_peek) return str;
        cur_start = 0;
    }

    size_t len_cpy = min(len_peek-already_read, end_ - cur_start);
    str += string(&*(data_.begin()+cur_start), len_cpy);
    return str;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len)
{
    size_t pop_len = min(len, size_);
    start_ = (start_ + pop_len) % capacity_;
    total_read_ += pop_len;
    size_ -= pop_len;
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    auto read_str = peek_output(len);
    pop_output(len);
    return read_str;
}

void ByteStream::end_input() {
    end_input_ = true;
}

bool ByteStream::input_ended() const { return end_input_; }

size_t ByteStream::buffer_size() const { return size_; }

bool ByteStream::buffer_empty() const { return size_ == 0; }

bool ByteStream::eof() const { return end_input_ && size_ == 0; }

size_t ByteStream::bytes_written() const { return total_written_; }

size_t ByteStream::bytes_read() const { return total_read_; }

size_t ByteStream::remaining_capacity() const { return capacity_ - size_; }

#include "stream_reassembler.hh"
// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity)
 : _output(capacity), _capacity(capacity),
   nextIndex_(0), index2data_(),
   unassembled_bytes_(0), eof_(false)
{}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof)
{
    size_t len_data = data.size();
    if(index > nextIndex_) {
        tempStoreData(data, index, eof);
        return;
    }

    if(index <= nextIndex_ && index + len_data <= nextIndex_) {
        if(eof) {
            eof_ = eof;
            _output.end_input();
        }
        return;
    }

    // index <= nextIndex_ && index + len_data > nextIndex_
    size_t start_index_in_data = nextIndex_ - index;
    size_t write_byte = _output.write(data.substr(start_index_in_data));
    nextIndex_ += write_byte;
    if(start_index_in_data + write_byte < len_data) {
        tempStoreData(data.substr(start_index_in_data + write_byte),
                        index + start_index_in_data + write_byte,
                        eof);
        return;
    }
    if(eof) {
        eof_ = eof;
        _output.end_input();
    }
    for(auto iter = index2data_.begin(); iter != index2data_.end();) {
        if(iter->first > nextIndex_) {
            break;
        }

        len_data = iter->second.first.size();
        if(iter->first + len_data <= nextIndex_) {
            index2data_.erase(iter++);
            unassembled_bytes_ -= len_data;
            continue;
        }

        auto data_tmp = iter->second.first;
        bool eof_tmp = iter->second.second;
        size_t index_tmp = iter->first;

        index2data_.erase(iter++);
        unassembled_bytes_ -= data_tmp.size();

        start_index_in_data = nextIndex_ - index_tmp;
        write_byte = _output.write(data_tmp.substr(start_index_in_data));
        nextIndex_ += write_byte;
        if(start_index_in_data + write_byte < len_data) {
            tempStoreData(data_tmp.substr(start_index_in_data + write_byte),
                            index_tmp + start_index_in_data + write_byte,
                            eof_tmp);
            return;
        }
        if(eof_tmp) {
            eof_ = eof_tmp;
            _output.end_input();
        }
    }
}


void StreamReassembler::tempStoreData(const string &data, const size_t index, bool eof) {
    size_t index_end = index + data.size();

    if(index2data_.count(index)) {
        if(data.size() > index2data_[index].first.size()) {
            unassembled_bytes_ += (data.size() - index2data_[index].first.size());
            index2data_[index] = make_pair(data, eof);
        }
    } else {
        index2data_[index] = make_pair(data, eof);
        unassembled_bytes_ += data.size();
    }

    auto lower_iter = index2data_.lower_bound(index);
    while(true) {
        auto lower_decre = lower_iter;
        lower_decre--;
        if(lower_iter == index2data_.begin() || lower_decre->first + lower_decre->second.first.size() <= index) {
            break;
        }
        lower_iter = lower_decre;
    }

    string data_insert;
    index_end = lower_iter->first;
    size_t index_begin = lower_iter->first;
    while(true) {
        size_t index_end_backup = index_end;
        while(lower_iter != index2data_.end() && lower_iter->first <= index_end) {
            size_t index_append_start = lower_iter->first;
            auto data_append = lower_iter->second.first;
            if(lower_iter->second.second) eof = true;
            index2data_.erase(lower_iter++);

            size_t len_data = data_append.size();
            unassembled_bytes_ -= len_data;
            // if(index_end >= index_append_start + len_data) {
            //     continue;
            // }
            if(index_append_start + len_data > index_end) {
                size_t start_index_in_data = index_end - index_append_start;
                data_insert += data_append.substr(start_index_in_data);
                index_end = max(index_end, index_append_start + len_data);
            }
        }

        if(index_end_backup == index_end) {
            unassembled_bytes_ += data_insert.size();
            index2data_[index_begin] = make_pair(std::move(data_insert), eof);
            break;
        }
        lower_iter = index2data_.lower_bound(index);
    }
}

size_t StreamReassembler::unassembled_bytes() const { return unassembled_bytes_; }

bool StreamReassembler::empty() const { return index2data_.empty(); }

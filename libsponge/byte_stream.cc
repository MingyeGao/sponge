#include <assert.h>
#include <cstring>
#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

// ByteStream::ByteStream(const size_t capacity) { DUMMY_CODE(capacity); }

size_t ByteStream::write(const string &data) {
    int size_to_write = this->remaining_capacity() > data.size()?data.size():this->remaining_capacity();
    if(!size_to_write) {
        return 0;
    }

    int remained_size_to_buffer_end = 0;
    if(is_offset_overwrite) {
        remained_size_to_buffer_end = read_offset - write_offset;
    }else{
        remained_size_to_buffer_end = buffer.size() - write_offset;
    }

    int size_write_to_end = remained_size_to_buffer_end < static_cast<int>(data.size())?remained_size_to_buffer_end:data.size();
    if(size_write_to_end) {
        std::memcpy(&buffer[write_offset], data.c_str(), size_write_to_end);
        write_offset += size_write_to_end;
    }
    
    int size_write_from_start = size_to_write - size_write_to_end;
    if(size_write_from_start) {
        std::memcpy(&buffer[0], &data.c_str()[size_write_to_end], size_write_from_start);
        write_offset = size_write_from_start;
    }

    if(write_offset >= static_cast<int>(buffer.size())){
        assert(!is_offset_overwrite);
        write_offset -= buffer.size();
        is_offset_overwrite = true;

    }
    
    size += size_to_write;
    total_written += size_to_write;

    return size_to_write;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    std::string result("");
    int peek_len = static_cast<int>(len)>size?size:len;

    int remained_size_to_the_end = buffer.size() - read_offset;
    int peek_len_to_end = peek_len>remained_size_to_the_end?remained_size_to_the_end:peek_len;
    if(peek_len_to_end){
        result += std::string(buffer.begin() + read_offset, buffer.begin() + read_offset + peek_len_to_end);
    }

    int peek_len_from_start = peek_len - peek_len_to_end;
    if(peek_len_from_start){
        result += std::string(buffer.begin(), buffer.begin() + peek_len_from_start);
    }

    return result;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) { 
    int pop_len = static_cast<int>(len)>size?size:len;
    read_offset += pop_len;
    if(read_offset >= static_cast<int>(buffer.size())){
        assert(is_offset_overwrite);
        read_offset -= buffer.size();
        is_offset_overwrite = false;
    }
    size -= pop_len;
    total_read += pop_len;
 }

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    std::string result = this->peek_output(len);
    this->pop_output(len);

    return result;
}

void ByteStream::end_input() {input_is_end = true;}

bool ByteStream::input_ended() const { return input_is_end; }

size_t ByteStream::buffer_size() const { return size; }

bool ByteStream::buffer_empty() const { return size == 0; }

bool ByteStream::eof() const { return input_is_end && read_offset == write_offset; }

size_t ByteStream::bytes_written() const { return total_written; }

size_t ByteStream::bytes_read() const { return total_read; }

size_t ByteStream::remaining_capacity() const { return buffer.size() - size; }

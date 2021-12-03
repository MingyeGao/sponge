#include <assert.h>
#include <cstring>
#include <iostream>
#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity):_output(capacity), _capacity(capacity), 
    buffer(ResembleBuffer(capacity)), unassemble_bytes(0) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    if(_output.eof()){
        return;
    }

    int end_index = index + data.size() - 1;
    if(eof){
        buffer.set_eof_index(end_index);
    }


    if(is_data_in_range(static_cast<int>(index), end_index)){
        int stream_base_index = unassembled_base_index();
        int stream_upper_index = unassembled_upper_index();
        int actual_start_index = stream_base_index>static_cast<int>(index)?stream_base_index:static_cast<int>(index);
        int actual_end_index = stream_upper_index<end_index?stream_upper_index:end_index;

        std::string actual_data_to_write = std::string(data.begin() + (actual_start_index - index), data.begin() + (actual_end_index - index + 1));
        int write_num = buffer.write_data(actual_data_to_write, actual_start_index);
        unassemble_bytes += write_num;
    }

    if(buffer.can_read()){
        int available_bytes = buffer.available_bytes_for_read();
        std::string available_data = buffer.read(available_bytes);
        _output.write(available_data);
        unassemble_bytes -= available_bytes;
    }

    if(buffer.is_eof_reached()){
        _output.end_input();
    }

}

size_t StreamReassembler::unassembled_bytes() const { 
    return unassemble_bytes;
}

bool StreamReassembler::empty() const { return unassemble_bytes == 0; }

int StreamReassembler::unassembled_base_index() {
    return buffer.base_index();
}

int StreamReassembler::unassembled_upper_index() {
    return buffer.base_index() + _output.remaining_capacity() - 1; 
}

bool StreamReassembler::is_data_in_range(int first_data_index, int last_data_index) {
    if(first_data_index > unassembled_upper_index() || last_data_index < unassembled_base_index()){
        return false;
    }
    return true;
}


//! Segment Methods
bool Segment::covers(std::shared_ptr<Segment> s) {
    return first_data_index_in_stream <= s->first_data_index_in_stream && last_data_index_in_stream >= s->last_data_index_in_stream;
}

bool Segment::overlaps(std::shared_ptr<Segment> s) {
    return (first_data_index_in_stream >= s->first_data_index_in_stream && first_data_index_in_stream <= s->last_data_index_in_stream) || 
    (last_data_index_in_stream <= s->last_data_index_in_stream && last_data_index_in_stream >= s->first_data_index_in_stream);
}

bool Segment::left_adjacent(std::shared_ptr<Segment> s) {
    return last_data_index_in_stream == (s->first_data_index_in_stream - 1);
}

bool Segment::right_adjacent(std::shared_ptr<Segment> s) {
    return first_data_index_in_stream == (s->last_data_index_in_stream + 1);
}

bool Segment::lefter_than(std::shared_ptr<Segment> s) {
    return last_data_index_in_stream < s->first_data_index_in_stream;
}

bool Segment::rightter_than(std::shared_ptr<Segment> s) {
    return first_data_index_in_stream > s->last_data_index_in_stream;
}

std::shared_ptr<Segment> Segment::get_lefter_part_than(std::shared_ptr<Segment> s) {
    if(first_data_index_in_stream >= s->first_data_index_in_stream){
        return nullptr;
    }
    return std::make_shared<Segment>(first_data_index_in_stream, s->first_data_index_in_stream - 1);
}

std::shared_ptr<Segment> Segment::get_rightter_part_than(std::shared_ptr<Segment>s) {
    if(last_data_index_in_stream <= s->last_data_index_in_stream) {
        return nullptr;
    }
    return std::make_shared<Segment>(s->last_data_index_in_stream + 1, last_data_index_in_stream);
}

int Segment::size() {
    return (last_data_index_in_stream - first_data_index_in_stream + 1);
}

// SegmentList Methods
std::shared_ptr<Segment> SegmentList::first_element() {
    return head->next;
}

// ResembleBuffer Methods
// Caller should make sure [segment_start_index, segment_end_index] doesn't overlap
// existing data.
int ResembleBuffer::write_data_within_index(const std::string &data, const uint64_t data_start_index, int segment_start_index, int segment_end_index){
    std::string data_to_write = std::string(data.begin() + (segment_start_index - data_start_index), data.begin() + (segment_end_index - data_start_index + 1));
    buffer.write(data_to_write, segment_start_index);
    return segment_end_index - segment_start_index + 1;
}

int ResembleBuffer::write_data(const std::string &data, const uint64_t start_index){
    if(data.size() == 0){
        return 0;
    }
    std::shared_ptr<Segment> data_segment = std::make_shared<Segment>(start_index, start_index + data.size() - 1);
    std::shared_ptr<Segment> current_segment_in_list = segment_list.first_element();
    std::shared_ptr<Segment> prev_segment_in_list = segment_list.head;
    std::shared_ptr<Segment> next_segment_in_list = nullptr;

    int total_write_num = 0;
    
    while(current_segment_in_list) {
        if(data_segment == nullptr) {
            break;
        }
        if(current_segment_in_list->covers(data_segment)){
            return total_write_num;
        }

        std::shared_ptr<Segment> left_part, right_part;
        if(current_segment_in_list->overlaps(data_segment)){
            left_part = data_segment->get_lefter_part_than(current_segment_in_list);
            right_part = data_segment->get_rightter_part_than(current_segment_in_list);
        }else if(data_segment->lefter_than(current_segment_in_list)){
            left_part = data_segment;
        }else{
            right_part = data_segment;
        }


        if(left_part) {
            write_data_within_index(data, start_index, left_part->first_data_index_in_stream, left_part->last_data_index_in_stream);
            segment_list.insert_before(current_segment_in_list, left_part);
            total_write_num += left_part->size();

            if(left_part->prev != segment_list.head && left_part->right_adjacent(left_part->prev)){
                segment_list.combines_element_and_left(left_part);
            }

            if(current_segment_in_list->right_adjacent(current_segment_in_list->prev)){
                current_segment_in_list = segment_list.combines_element_and_left(current_segment_in_list);
            }
        }

        data_segment = right_part;
        next_segment_in_list = current_segment_in_list->next;
        prev_segment_in_list = current_segment_in_list;
        current_segment_in_list = next_segment_in_list;
        
    }

    if(data_segment){
        write_data_within_index(data, start_index, data_segment->first_data_index_in_stream, data_segment->last_data_index_in_stream);
        segment_list.insert_after(prev_segment_in_list, data_segment);
        prev_segment_in_list->next = data_segment;
        data_segment->prev = prev_segment_in_list;
        if(data_segment->prev != segment_list.head && data_segment->right_adjacent(data_segment->prev)){
            segment_list.combines_element_and_left(data_segment);
        }
        total_write_num += data_segment->size();
    }

    return total_write_num;
}

int ResembleBuffer::available_bytes_for_read(){
    return segment_list.first_element()->size();
}

bool ResembleBuffer::can_read(){
    return segment_list.first_element() && segment_list.first_element()->first_data_index_in_stream == buffer.base_index;
}

int ResembleBuffer::upper_index() {
    return buffer.upper_index();
}

int ResembleBuffer::base_index() {
    return buffer.base_index;
}

std::string ResembleBuffer::read(int byte_num){
    std::string result = buffer.read(byte_num);
    segment_list.drop_first_element();
    return result;
}

void ResembleBuffer::set_eof_index(int eof_index){
    buffer.set_eof_index(eof_index);
}

bool ResembleBuffer::is_data_within_buffer_range(int data_start_index, int data_end_index){
    if(data_start_index > buffer.upper_index()){
        return false;
    }
    if(data_end_index < buffer.base_index){
        return false;
    }
    return true;
}

bool ResembleBuffer::is_eof_reached(){
    return buffer.is_eof_set && base_index() > buffer.stream_eof_index; 
}

// SegmentList Methods
std::shared_ptr<Segment> SegmentList::combines_element_and_left(std::shared_ptr<Segment> s){
    assert(s->prev != head);
    assert(s->first_data_index_in_stream == (s->prev->last_data_index_in_stream + 1));
    std::shared_ptr<Segment> s_prev = s->prev;
    std::shared_ptr<Segment> ns = std::make_shared<Segment>(s_prev->first_data_index_in_stream, s->last_data_index_in_stream);
    s_prev->prev->next = ns;
    ns->prev = s_prev->prev;
    ns->next = s->next;
    if(s->next){
        s->next->prev = ns;
    }
    
    return ns;
}

void SegmentList::insert_after(std::shared_ptr<Segment> target, std::shared_ptr<Segment> new_segment) {
    std::shared_ptr<Segment> target_next = target->next;
    target->next = new_segment;
    new_segment->prev = target;
    if(target_next){
        target_next->prev = new_segment;
    }
    new_segment->next = target_next;
}
  
void SegmentList::insert_before(std::shared_ptr<Segment> target, std::shared_ptr<Segment> new_segment){
    std::shared_ptr<Segment> target_prev = target->prev;
    target_prev->next = new_segment;
    new_segment->prev = target_prev;
    new_segment->next = target;
    target->prev = new_segment;
}

void SegmentList::drop_first_element(){
    head->next = head->next->next;
    if(head->next){
        head->next->prev = head;
    }
}


// RotateBuffer Methods

// |----------|------------------------|
//          base
int RotateBuffer::upper_index() {
    int upper_index = base_index + buffer.size() - 1;
    if(is_eof_set && stream_eof_index < upper_index){
        upper_index = stream_eof_index;
    }
    return upper_index;
}

std::string RotateBuffer::read(int bytes_num){
    std::string result = std::string("");
    if(bytes_num > static_cast<int>(buffer.size())){
        bytes_num = static_cast<int>(buffer.size());
    }

    int bytes_num_read_to_back = static_cast<int>(buffer.size()) - base_position;
    if(bytes_num_read_to_back > bytes_num){
        bytes_num_read_to_back = bytes_num;
    }
    int bytes_num_read_from_behind = bytes_num - bytes_num_read_to_back;
    if(bytes_num_read_to_back){
        result += std::string(buffer.begin() + base_position, buffer.begin() + base_position + bytes_num_read_to_back);
        base_position += bytes_num_read_to_back;
        base_index += bytes_num_read_to_back;
        if(base_position == static_cast<int>(buffer.size())){
            base_position = 0;
        }
    }

    if(bytes_num_read_from_behind){
        result += std::string(buffer.begin(), buffer.begin() + bytes_num_read_from_behind);
        base_position = bytes_num_read_from_behind;
        base_index += bytes_num_read_from_behind;
        if(base_position == static_cast<int>(buffer.size())){
            base_position = 0;
        }
    } 

    return result;
}


// Caller should make sure that data doesn't overlap exsiting data,
// and data could be written to buffer, no overflow
int RotateBuffer::write(const std::string data, int start_index) {
    
    int write_start_position = position_at_index(start_index);
    int data_size = data.size();
    int bytes_write_to_back = buffer.size() - write_start_position;
    if(bytes_write_to_back > data_size){
        bytes_write_to_back = data_size;
    }
    int bytes_write_from_behind = data_size - bytes_write_to_back;

    // write_to_back
    if(bytes_write_to_back){
        std::memcpy(&buffer[write_start_position], data.c_str(), bytes_write_to_back);
    }

    // write_from_behind
    if(bytes_write_from_behind) {
        std::memcpy(&buffer[0], &data.c_str()[bytes_write_to_back], bytes_write_from_behind);
    }

    return data_size;
}

// Caller should make sure base_index <= index <= upper_index
int RotateBuffer::position_at_index(int index){
    assert(index >= base_index && index <= upper_index());
    int position = base_position + (index - base_index);
    if(position >= static_cast<int>(buffer.size())){
        position -= buffer.size();
    }
    return position;
}

void RotateBuffer::set_eof_index(int eof_index){
    if(is_eof_set){
        return;
    }
    is_eof_set = true;
    stream_eof_index = eof_index;
}
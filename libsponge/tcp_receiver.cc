#include "tcp_receiver.hh"
#include <iostream>

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    if(FIN){
        return;
    }
    // Wait for the first segment with SYN
    if(!SYN && !seg.header().syn){
        return;
    }
    if(!SYN && seg.header().syn){
        SYN = true;
        ISN = seg.header().seqno;
    }
    WrappingInt32 seg_first_seq_no = seg.header().seqno;
    WrappingInt32 seg_payload_first_seq_no = seg.header().seqno;
    if(seg.header().syn){
        seg_payload_first_seq_no = seg_payload_first_seq_no + 1;
    }

    WrappingInt32 seg_last_seq_no = seg.header().seqno + seg.length_in_sequence_space() - 1;
    WrappingInt32 seg_payload_last_seq_no = seg_payload_first_seq_no + seg.length_in_sequence_space() -1;
    if(seg.header().syn){
        seg_payload_last_seq_no = seg_payload_last_seq_no - 1;
    }
    if(seg.header().fin){
        seg_payload_last_seq_no = seg_payload_last_seq_no - 1;
    }

    uint64_t seg_first_absolute_seq_no = unwrap(seg_first_seq_no, ISN, ack_manager.absolute_ack_no);
    uint64_t seg_last_absolute_seq_no = unwrap(seg_last_seq_no, ISN, ack_manager.absolute_ack_no);
    uint64_t seg_payload_first_absolute_seq_no = unwrap(seg_payload_first_seq_no, ISN, ack_manager.absolute_ack_no);
    uint64_t seg_payload_last_absolute_seq_no = unwrap(seg_payload_last_seq_no, ISN, ack_manager.absolute_ack_no);
    if(seg.header().fin){
        eof_index_in_stream = seg_payload_last_absolute_seq_no - 1;
    }

    ack_manager.insert(
        std::make_shared<Segment>(seg_first_absolute_seq_no, seg_last_absolute_seq_no), 
        _reassembler.stream_out().remaining_capacity()
    );
    _reassembler.push_substring(std::string(seg.payload().str()), seg_payload_first_absolute_seq_no - 1, seg.header().fin);
    if((ack_manager.absolute_ack_no - 1) > eof_index_in_stream){
        FIN = true;
    }

}

optional<WrappingInt32> TCPReceiver::ackno() const { 
    if(!SYN){
        return {};
    }
    return ack_manager.ackno(ISN);
}

size_t TCPReceiver::window_size() const { 
    return _reassembler.stream_out().remaining_capacity(); 
}


WrappingInt32 ACKManager::ackno(WrappingInt32 isn) const {
    return wrap(absolute_ack_no, isn);
}

void ACKManager::insert(std::shared_ptr<Segment> s, size_t window_size){
    uint64_t upper_absolute_seq_no = absolute_ack_no + window_size - 1;
    if(s->last_data_index_in_stream < absolute_ack_no || s->first_data_index_in_stream > upper_absolute_seq_no){
        return;
    }

    uint64_t actual_first_data_absolute_seq_no = s->first_data_index_in_stream < absolute_ack_no?absolute_ack_no:s->first_data_index_in_stream;
    uint64_t actual_last_data_absolute_seq_no = s->last_data_index_in_stream > upper_absolute_seq_no?upper_absolute_seq_no:s->last_data_index_in_stream;
    s = std::make_shared<Segment>(actual_first_data_absolute_seq_no, actual_last_data_absolute_seq_no);

    auto current_node = segment_list.first_element();
    std::shared_ptr<Segment> prev_node = segment_list.head;
    std::shared_ptr<Segment> next_node;
    while(current_node) {
        if(s == nullptr) {
            break;
        }
        if(current_node->covers(s)){
            return;
        }

        std::shared_ptr<Segment> left_part, right_part;
        if(current_node->overlaps(s)){
            left_part = s->get_lefter_part_than(current_node);
            right_part = s->get_rightter_part_than(current_node);
        }else if(s->lefter_than(current_node)){
            left_part = s;
        }else{
            right_part = s;
        }


        if(left_part) {
            segment_list.insert_before(current_node, left_part);

            if(left_part->prev != segment_list.head && left_part->right_adjacent(left_part->prev)){
                segment_list.combines_element_and_left(left_part);
            }

            if(current_node->right_adjacent(current_node->prev)){
                current_node = segment_list.combines_element_and_left(current_node);
            }
        }

        s = right_part;
        next_node = current_node->next;
        prev_node = current_node;
        current_node = next_node;
        
    }

    if(s){
        segment_list.insert_after(prev_node, s);
        prev_node->next = s;
        s->prev = prev_node;
        if(s->prev != segment_list.head && s->right_adjacent(s->prev)){
            segment_list.combines_element_and_left(s);
        }
    }

    if(segment_list.first_element()->first_data_index_in_stream == absolute_ack_no){
        absolute_ack_no   = segment_list.first_element()->last_data_index_in_stream + 1;
        segment_list.drop_first_element();
    }
}
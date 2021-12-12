#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>
#include <iostream>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    ,_current_retransmission_timeout{retx_timeout}
    , _stream(capacity)
    ,_timer(Timer())
    ,retransmission_time(0){}

uint64_t TCPSender::bytes_in_flight() const { 
    return next_seqno_absolute() - ack_seqno;
}

void TCPSender::fill_window() {

    uint16_t read_num;
    uint16_t sender_window_size = ack_seqno + _window_size - next_seqno_absolute();
    if(next_seqno_absolute() > (ack_seqno + _window_size)){
        //sender_window_size = 0;
        return;
    }

    while(true){

    
        if(sender_window_size > 0){
            read_num = sender_window_size;
        }else if(receiver_zero_window_size) {
            read_num = 1;
            sender_window_size = 1;
        }else{
            return;
        }
        if(read_num > TCPConfig::MAX_PAYLOAD_SIZE){
            read_num = TCPConfig::MAX_PAYLOAD_SIZE;
        }
        std::string data_to_send = _stream.read(read_num);
       
        if(data_to_send.length() == 0){
            if(_syn_sent && !_stream.eof()){
                return;
            }else if(_fin_sent){
                return;
            }
        }
        TCPSegment new_segment = TCPSegment();
        
        new_segment.header().seqno = wrap(_next_seqno, _isn);

        if(_next_seqno == 0){
            new_segment.header().syn = true;
            _syn_sent = true;
            _next_seqno++;
            sender_window_size--;
        }

        new_segment.payload() = Buffer(std::string(data_to_send));
        _next_seqno += data_to_send.length();
        // In case _window_size=0, sender_window_size=0
        if(sender_window_size > 0){
            sender_window_size -= data_to_send.length();
        }
        
        if(_stream.eof() && (sender_window_size >= 1)){
            new_segment.header().fin = true;
            _fin_sent = true;
            _next_seqno++;
            sender_window_size--;
        }


        if(!receiver_zero_window_size && !_timer.started){
            _timer.start();
        }
        

        if(new_segment.length_in_sequence_space()){
            _segments_out.push(new_segment);
            _outstanding_segment.push(new_segment);
        }

        if(receiver_zero_window_size){
            return;
        }

    }
}
    

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) { 
    auto tmp = unwrap(ackno, _isn, ack_seqno);
    if(tmp > next_seqno_absolute()){
        return;
    }
    if(tmp < ack_seqno){
        return;
    }

    ack_seqno = tmp;
    _window_size = window_size; 
    if(_window_size == 0){
        receiver_zero_window_size = true;
    }else{
        receiver_zero_window_size = false;
    }
    bool new_segment_acked = false;
    while(!_outstanding_segment.empty()){
        TCPSegment earlist_segment = _outstanding_segment.front();
        uint64_t segment_start_absolute_seqno = unwrap(earlist_segment.header().seqno, _isn, ack_seqno);
        uint64_t segment_end_absolute_seqno = segment_start_absolute_seqno + earlist_segment.length_in_sequence_space() - 1;
        uint64_t absolute_ackno = unwrap(ackno, _isn, ack_seqno);
        if(absolute_ackno <= segment_end_absolute_seqno){
            break;
        }else{
            _outstanding_segment.pop();
            new_segment_acked = true;
        }
    }

    if(_outstanding_segment.empty()){
        _current_retransmission_timeout = _initial_retransmission_timeout;
        retransmission_time = 0;
        _timer.stop();
    }else if(new_segment_acked){
        _current_retransmission_timeout = _initial_retransmission_timeout;
        _timer.stop();
        _timer.start();
        retransmission_time = 0;
    }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    _timer.tick(ms_since_last_tick);

    if(_timer.expires(_current_retransmission_timeout)){
        if(_outstanding_segment.empty()){
            return;
        }
        auto oldest_segment = _outstanding_segment.front();
        _segments_out.push(oldest_segment);
        retransmission_time++;
        
        // When filling window, treat a '0' window size as equal to '1' but don't back off RTO
        if(next_seqno_absolute() <= (ack_seqno + _window_size)){
            _current_retransmission_timeout *= 2;
        }

        _timer.stop();
        _timer.start();
    } 
    
 }

unsigned int TCPSender::consecutive_retransmissions() const { 
    return retransmission_time;
}

void TCPSender::send_empty_segment() {
    TCPSegment new_segment = TCPSegment();
    new_segment.header().seqno = next_seqno();
    new_segment.payload() = Buffer(std::string(""));
    _segments_out.push(new_segment);
}

bool Timer::expires(const int retx_timeout) {
    return static_cast<int>(_ms_since_start) >= retx_timeout;
}

void Timer::tick(const size_t ms_since_last_tick) {
    _ms_since_start += ms_since_last_tick;
}
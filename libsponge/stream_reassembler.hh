#ifndef SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH
#define SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH

#include "byte_stream.hh"

#include <cstdint>
#include <string>
#include <vector>

class Segment {
public:
  int first_data_index_in_stream;
  int last_data_index_in_stream;
  Segment *prev;
  Segment *next;
  Segment(int first_data_index, int last_data_index):first_data_index_in_stream(first_data_index), last_data_index_in_stream(last_data_index),
    prev(nullptr), next(nullptr){}
  bool covers(Segment *s);
  bool overlaps(Segment *s);
  bool left_adjacent(Segment *s);
  bool right_adjacent(Segment *s);
  bool lefter_than(Segment *s);
  bool rightter_than(Segment *s);
  Segment* get_lefter_part_than(Segment *s);
  Segment* get_rightter_part_than(Segment *s);
  int size();
};

class SegmentList {
public:
  Segment *head;
  Segment* combines_element_and_left(Segment *s);
  Segment *first_element();
  void insert_after(Segment *target, Segment *new_segment);
  void insert_before(Segment *target, Segment *new_segment);
  SegmentList(): head(new Segment(0, 0)){}
  void drop_first_element();
};

class RotateBuffer {
public:
  std::vector<char> buffer;
  int base_position;
  int base_index;
  bool is_eof_set;
  int stream_eof_index;
  int upper_position();
  int upper_index();
  int write(const std::string data, int start_index);
  int position_at_index(int index);
  std::string read(int bytes_num);
  RotateBuffer(int capacity): buffer(std::vector<char>(capacity)), base_position(0), base_index(0), 
    is_eof_set(false), stream_eof_index(-1){}
  void set_eof_index(int eof_index);
};

class ResembleBuffer {
public:
  RotateBuffer buffer;
  SegmentList segment_list;
  bool can_read();
  int write_data(const std::string &data, const uint64_t index);
  int write_data_within_index(const std::string &data, const uint64_t data_start_index, int segment_start_index, int segment_end_index);
  int unassembled_bytes();
  int base_index();
  int upper_index();
  int available_bytes_for_read();
  ResembleBuffer(int capacity): buffer(RotateBuffer(capacity)), segment_list(SegmentList()){}
  std::string read(int byte_num);
  void set_eof_index(int eof_index);
  bool is_data_within_buffer_range(int data_start_index, int data_end_index);
  bool is_eof_reached();
};

//! \brief A class that assembles a series of excerpts from a byte stream (possibly out of order,
//! possibly overlapping) into an in-order byte stream.
class StreamReassembler {
  private:
    // Your code here -- add private members as necessary.

    ByteStream _output;  //!< The reassembled in-order byte stream
    size_t _capacity;    //!< The maximum number of bytes
    ResembleBuffer buffer; // buffer for unread and unassembled data
    size_t unassemble_bytes;

  public:
    //! \brief Construct a `StreamReassembler` that will store up to `capacity` bytes.
    //! \note This capacity limits both the bytes that have been reassembled,
    //! and those that have not yet been reassembled.
    StreamReassembler(const size_t capacity);

    //! \brief Receive a substring and write any newly contiguous bytes into the stream.
    //!
    //! The StreamReassembler will stay within the memory limits of the `capacity`.
    //! Bytes that would exceed the capacity are silently discarded.
    //!
    //! \param data the substring
    //! \param index indicates the index (place in sequence) of the first byte in `data`
    //! \param eof the last byte of `data` will be the last byte in the entire stream
    void push_substring(const std::string &data, const uint64_t index, const bool eof);
    // int first_unread_data_index_in_stream();
    // int first_unread_data_position_in_buffer();
    // int first_unacceptable_data_index_in_stream();
    // int first_unacceptable_data_positon_in_buffer();

    //! \name Access the reassembled byte stream
    //!@{
    const ByteStream &stream_out() const { return _output; }
    ByteStream &stream_out() { return _output; }
    //!@}

    //! The number of bytes in the substrings stored but not yet reassembled
    //!
    //! \note If the byte at a particular index has been pushed more than once, it
    //! should only be counted once for the purpose of this function.
    size_t unassembled_bytes() const;

    //! \brief Is the internal state empty (other than the output stream)?
    //! \returns `true` if no substrings are waiting to be assembled
    bool empty() const;

    int unassembled_upper_index();
    int unassembled_base_index();
    bool is_data_in_range(int first_data_index, int last_data_index);
};

#endif  // SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH

#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.


using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const {
    return  _timecount - _lastTimeRecieved;
}

void TCPConnection::segment_received(const TCPSegment &seg) {
    _lastTimeRecieved = _timecount;
    if ( seg.header().rst ){
        _receiver.stream_out().set_error();
        _sender.stream_in().set_error();
        _activeFlag = false;
        return;
    }
    _receiver.segment_received(seg);
    if ( seg.header().ack ){
        auto ackno = seg.header().ackno;
        auto windowSize = seg.header().win;
        _sender.ack_received( ackno, windowSize );
    }
    if ( seg.length_in_sequence_space() > 0 ) {
        TCPSegment newSeg;
        newSeg.header().win = _receiver.window_size();
        newSeg.header().ackno = WrappingInt32( seg.header().seqno.raw_value() + 1 );
        _sender.segments_out().push( newSeg );
    }else{
        if ( seg.header().ackno.raw_value() 
            && seg.header().seqno.raw_value() == seg.header().ackno.raw_value() - 1) {
            TCPSegment newSeg;
            newSeg.header().win = _receiver.window_size();
            _sender.segments_out().push(newSeg);
        }
    }

}

bool TCPConnection::active() const { return _activeFlag; }

size_t TCPConnection::write(const string &data) {
    _sender.stream_in().write(data);
    _sender.fill_window();
    return _sender.stream_in().bytes_written();
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    _timecount += ms_since_last_tick;
}

void TCPConnection::end_input_stream() { _sender.stream_in().end_input();}

void TCPConnection::connect() {
    _sender.fill_window();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
            TCPSegment newSeg;
            newSeg.header().rst = true;
            _segments_out.push(newSeg);
            _receiver.stream_out().set_error();
            _sender.stream_in().set_error();
            _activeFlag = false;

        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}

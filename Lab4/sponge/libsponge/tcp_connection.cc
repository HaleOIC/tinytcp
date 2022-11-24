#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.


using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return  _timecount - _lastTimeRecieved;}

void TCPConnection::segment_received(const TCPSegment &seg) {

    // set the last time recieved and used for the above function.
    _lastTimeRecieved = _timecount;

    // the TCPSegment is within a rst flag,so we need to set the TCP
    // receiver and sender to the error state.
    if ( seg.header().rst ){
        _receiver.stream_out().set_error();
        _sender.stream_in().set_error();
        _activeFlag = false;
        return;
    }
    // give the segment to the _receiver
    _receiver.segment_received(seg);

    // give the header information to the _sender.
    if ( seg.header().ack ) _sender.ack_received( seg.header().ackno, seg.header().win );

    if ( !_sender.stream_in().eof() && _receiver.stream_out().input_ended() ) _linger_after_streams_finish = false;


    // handle other TCP connection statuses
    // <! establishment is a flag that remarks the if it has the connection.
    if ( seg.header().syn && !_establishment ) {
        _establishment = true;
        _sender.fill_window(); // generate the segment with SYN
        shift_with_ackno();    // fill in the ackno and ack flag it may rewirte in the after 
        _activeFlag = true;
    }

    // give back the message after handle the TCPsegment
    if ( seg.length_in_sequence_space() == 0 && _receiver.ackno().has_value()
        && seg.header().seqno == _receiver.ackno().value() - 1 ){
        _sender.send_empty_segment();

    }

    if ( _receiver.unassembled_bytes() == 0 &&  _receiver.stream_out().input_ended() ) {
        // satisfy the #1
        if ( _sender.bytes_in_flight() == 0 
            && _sender.segments_out().empty() 
            && _sender.stream_in().input_ended() 
            && _sender.stream_in().bytes_written() == _sender.next_seqno_absolute() - 2 ) {
            // satisfy #2
            if ( !_linger_after_streams_finish || time_since_last_segment_received() >= 10 * _cfg.rt_timeout ){
                _activeFlag = false;
            }
        }

    }

    shift_with_ackno();

}

bool TCPConnection::active() const { return _activeFlag; }

size_t TCPConnection::write(const string &data) {
    auto bytes = _sender.stream_in().write(data);
    _sender.fill_window();
    shift_with_ackno();
    return bytes;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    _timecount += ms_since_last_tick;
    _sender.tick(ms_since_last_tick);
    // over the standard retransmission times.
    if ( _sender.consecutive_retransmissions() > _cfg.MAX_RETX_ATTEMPTS ) set_rst();
    shift_with_ackno();

}

void TCPConnection::end_input_stream() {
    // as long as the _sender input is ended, send the fin flag segment
    _sender.stream_in().end_input();
    _sender.fill_window();
    shift_with_ackno();
}

void TCPConnection::connect() {
    // send out a syn flag and send it out 
    _sender.fill_window();
    // set the active flag to true
    _activeFlag = true;
    

}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
            set_rst();
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}


/**
 * private function about implementation of the TCP connection
 * which is added by the editor.
 * not the origin.
 */ 

void TCPConnection::set_rst(){
    TCPSegment newSeg;
    newSeg.header().rst = true;
    _segments_out.push(newSeg);
    shift_with_ackno();
    _receiver.stream_out().set_error();
    _sender.stream_in().set_error();
    _activeFlag = false;
}

void TCPConnection::shift_with_ackno(){
    // shift the TCPSegment in the outqueue with new ackno and ack flag.
    if ( !_receiver.ackno().has_value() ) return;
    auto ackno = _receiver.ackno().value();
    queue<TCPSegment> newQue;
    while ( !_sender.segments_out().empty() ){
        auto seg = _sender.segments_out().front();
        seg.header().ack = true;
        seg.header().ackno = ackno;
        seg.header().win = _receiver.window_size();
        newQue.push( seg );
        _sender.segments_out().pop();
    }
    while ( !newQue.empty() ) {
        _sender.segments_out().push( newQue.front() );
        newQue.pop();
    }
}

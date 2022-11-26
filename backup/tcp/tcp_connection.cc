#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.


using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return  _ms_since_last_segment_received;}

void TCPConnection::segment_received(const TCPSegment &seg) {

    if ( !_activeFlag ) return;

    // set the last time recieved and used for the above function.
    _ms_since_last_segment_received = 0;

    // the TCPSegment is within a rst flag,so we need to set the TCP
    // receiver and sender to the error state.
    if ( seg.header().rst ){
        set_rst();
        return;
    }
    // give the segment to the _receiver
    _receiver.segment_received(seg);

    // give the header information to the _sender.
   bool ackJudge = seg.header().ack;
   if ( ackJudge ) _sender.ack_received( seg.header().ackno, seg.header().win );


    // handle other TCP connection statuses
    // <! establishment is a flag that remarks the if it has the connection.
    if ( seg.header().syn && !_establishment ) {
        if ( ackJudge ) _establishment = true;
        else _sender.fill_window(); // generate the segment with SYN
    } else if ( !_establishment && ackJudge ) _establishment = true;



    if ( seg.length_in_sequence_space() > 0 || !_sender.segments_out().empty() ) {
        _sender.fill_window();
        if ( _sender.segments_out().empty() ) _sender.send_empty_segment();
    }

    // give back the message after handle the TCPsegment
    if ( seg.length_in_sequence_space() == 0 && _receiver.ackno().has_value()
        && seg.header().seqno == _receiver.ackno().value() - 1 ){
        _sender.send_empty_segment();
    }

    send_segments();
    test_shut_down_connection();

}

bool TCPConnection::active() const { return _activeFlag; }

size_t TCPConnection::write(const string &data) {

    auto bytes = _sender.stream_in().write(data);
    _sender.fill_window();
    send_segments();
    test_shut_down_connection();
    return bytes;

}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {

    _ms_since_last_segment_received += ms_since_last_tick;
    _sender.tick(ms_since_last_tick);
    // over the standard retransmission times.
    if ( _sender.consecutive_retransmissions() > _cfg.MAX_RETX_ATTEMPTS ) {
        // _sender.segments_out().pop();
        set_rst();
        return;
    }
    send_segments();
    test_shut_down_connection();
}

void TCPConnection::end_input_stream() {
    // as long as the _sender input is ended, send the fin flag segment
    _sender.stream_in().end_input();
    _sender.fill_window();
    send_segments();
    test_shut_down_connection();
}

void TCPConnection::connect() {
    if ( _sender.next_seqno_absolute() != 0 ) return;
    // send out a syn flag and send it out 
    _sender.fill_window();
    // set the active flag to true
    _activeFlag = true;
    send_segments();
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
    // set some other related status.
    _receiver.stream_out().set_error();
    _sender.stream_in().set_error();
    _activeFlag = false;
    _linger_after_streams_finish = false;
    _rstFlag = true;

    
    // send the segment with the rst flag is on.
    if ( _establishment ) send_segments();
}

void TCPConnection::shift_with_ackno(){
    // shift the TCPSegment in the outqueue with new ackno and ack flag.
    TCPSegment tmp = _sender.segments_out().front();
    _sender.segments_out().pop();
    if ( _receiver.ackno().has_value() ) {
        tmp.header().ack = true;
        tmp.header().ackno = _receiver.ackno().value();
    }
    tmp.header().rst = _rstFlag;
    tmp.header().win = _receiver.window_size();

    _segments_out.push( tmp );
}

void TCPConnection::test_shut_down_connection(){

    if (_receiver.stream_out().input_ended() && !_sender.stream_in().eof() && _sender.next_seqno_absolute() > 0) 
        _linger_after_streams_finish = false;
    else if ( _receiver.stream_out().eof() && _sender.stream_in().eof() 
                && unassembled_bytes() == 0 && bytes_in_flight() == 0 
                && _sender.next_seqno_absolute() == _sender.stream_in().bytes_written() + 2 ) {
        if ( !_linger_after_streams_finish )
            _activeFlag = false;
        else if ( time_since_last_segment_received() >= 10 * _cfg.rt_timeout )
            _activeFlag = false;
    }

}

void TCPConnection::send_segments(){
    if ( _sender.segments_out().empty() ) _sender.send_empty_segment();
    if ( _rstFlag ) {
        shift_with_ackno();
        return;
    }
    while ( !_sender.segments_out().empty() ) shift_with_ackno();
}
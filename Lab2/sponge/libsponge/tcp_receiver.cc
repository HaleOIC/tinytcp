#include "tcp_receiver.hh"
#include <iostream>

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.


using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
	string msg = (seg.header()).summary();
	size_t posSYN = msg.find( "flags=" );
	bool SYNflag = ( msg[posSYN + 6] == 'S' );
	size_t posSeqNo = msg.find( "seqno=" );
	
}

optional<WrappingInt32> TCPReceiver::ackno() const { return {}; }

size_t TCPReceiver::window_size() const { return {}; }

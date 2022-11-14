#include "stream_reassembler.hh"

#include <string>
// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`


using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), _capacity(capacity) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    // confirm the maxIndex at this moment
    size_t maxIndex = _capacity - _output.buffer_size() + _wannaIndex;

    // solve the eof 
    if ( eof ) _eofIndex = index + data.size();
    /**
     * Aim: make cut of the data
     * if index is smaller than the wanna index, the part before the wanna index is not useful.
     * if index is bigger than the max index, the part after the max index is not useful.
     * if some part of the data is occupied in the innerMap make some cut of it.
     */

    size_t len = data.size();
    string newData = data;
    size_t newIndex = index;
    if ( index < _wannaIndex ) {
        size_t pos = _wannaIndex - index;
        newData = newData.substr( pos, len - pos );
        newIndex = _wannaIndex;
    }
    if ( newIndex + newData.size() > maxIndex ){
        size_t length = maxIndex - newIndex;
        newData = newData.substr( 0, length );
    }
    if ( _innerMap.size() == 0 ) {
        _innerMap.insert({ newIndex, newData });
        return;
    }
    auto iter1 = _innerMap.lower_bound( newIndex );
    if ( iter1 != _innerMap.begin() ){
        /**
         * it means the map has the item whose tail is begin the newdata.
         */
        auto iter = --iter1;
        if ( iter -> first < newIndex && iter->first + (iter->second).size() > newIndex ){
            size_t pos = (iter->second).size() + (iter->first) - newIndex;
            newIndex = iter->first + (iter->second).size();
            newData = newData.substr( pos, newData.size() - pos );
        }
    }
    auto iter2 = --(_innerMap.lower_bound( newIndex + newData.size() ));
    if ( iter2 -> first < newIndex + newData.size() && iter2 -> first + (iter2 -> second).size() > newIndex + newData.size() ) {
        size_t pos = newIndex + newData.size() - (iter2 -> first);
        newData = newData.substr( 0, newData.size() - pos );
    }else ++iter2;
    --iter2;
    if ( distance(iter2,iter1) <= 0 ) _innerMap.erase( iter1, iter2 );
    _innerMap.insert({ newIndex, newData });


    /**
     * Aim: make the value of the buffer into the _output
     */
    auto iter = _innerMap.begin();
    while ( iter != _innerMap.end() ){
        if ( iter -> first == _wannaIndex ) {
            _output.write( iter->second );
            _wannaIndex += (iter->second).size();
            _innerMap.erase(iter);
            iter = _innerMap.begin();
        }else break;
    }
    if ( newIndex > _eofIndex ) _output.end_input();

}


size_t StreamReassembler::unassembled_bytes() const { return _unorderedSize; }

bool StreamReassembler::empty() const { return !_unorderedSize; }

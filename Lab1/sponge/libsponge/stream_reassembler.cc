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

    if ( eof ) _eofIndex = index + data.size();
    if ( eof ) _eofFlag = true;
    // make some cut of the newIndata 
    string newData = data;
    size_t newIndex = index;
    if ( newData.size() + newIndex <= _wannaIndex ) {
        newData = "";
        newIndex = _wannaIndex;
    }else if ( index < _wannaIndex ) {
        size_t pos = _wannaIndex - index;
        newData = newData.substr( pos, newData.size() - pos );
        newIndex = _wannaIndex;
    }
    /**No matter how big the newData is , the total size is fixed.
     * so whether you put it into the _output,or put it into the _innerMap,
     * it will always occupy the space. 
     * So a easy way to think is to put it into the _innerMap,
     * and then make the satisfying string into the _output.
     * at the same time, make the count and delete.
     */
    put_string_innerMap(newData, newIndex);
    make_transition( );

    // solve the eof 
    if ( empty() && _eofFlag && _wannaIndex >= _eofIndex) _output.end_input(); 
}


size_t StreamReassembler::unassembled_bytes() const { return _unorderedSize; }

bool StreamReassembler::empty() const { return !_unorderedSize; }


void StreamReassembler::put_string_innerMap( const std::string& tar, const size_t& Index ){
    string data = tar;
    size_t index = Index;
    if ( data.size() == 0 ) return;

    /**
     * case 1. when the map has no elements
     * make the cut of the maxLength and put it into the map.
     */
    if ( _innerMap.empty() ) {
        size_t maxLength = _capacity - _output.buffer_size();
        data = data.substr( 0, maxLength );
        _innerMap.insert( {index, data} );
        _unorderedSize += data.size();
        return;
    }

    //case 2. when the map has elements
    /** 
     * (1) judge the segment position in the map.
     * 
     * (2) judge whether the besides element that the iterator points  
     *     has the overlaping parts. if yes, cut the overlaping from
     *     the data, else continue;
     * 
     * (3) judge if besides have the overwritten cases, delete them
     *     from the map.
     * 
     * (4) update the _unorderedSize and put the data into the map.
     *  
     * tips: take care of the iterator invalidation as the elements 
     * in the map has been changed.
     */

    auto iterLow = _innerMap.lower_bound( index );
    if ( iterLow != _innerMap.begin() ){
        auto iter = iterLow; --iter;
        size_t existIndex = iter -> first;
        string existStr = iter -> second;
        if ( existIndex + existStr.size() > index ){
            size_t len = existIndex + existStr.size() - index;
            len = min( len, data.size() );
            data = data.substr( len, data.size() - len );
            index = index + len;
        }
    }

    auto iterhigh = _innerMap.lower_bound( index + data.size() );
    if ( iterhigh == _innerMap.begin() ) {
            iterhigh = _innerMap.end();
            iterLow = iterhigh;
    }else{
        --iterhigh;
        auto iter = iterhigh;
        size_t existIndex = iter -> first;
        string existStr = iter -> second;
        if ( existIndex + existStr.size() > index + data.size() ){
            if ( iterhigh == _innerMap.begin() ){
                size_t len = index + data.size() - existIndex;
                len = min( len, data.size() );
                data = data.substr( 0, data.size() - len );
                iterhigh = _innerMap.end();
                iterLow = iterhigh;
            }else{
                --iterhigh;
                size_t len = index + data.size() - existIndex;
                len = min( len, data.size() );
                data = data.substr( 0, data.size() - len );
            }
        }
    }
    // there is something wrong here.
    if ( !( iterhigh == iterLow && iterhigh == _innerMap.end() ) ){
        if ( iterLow -> first < iterhigh -> first ) {
            ++iterhigh;
            size_t tot = 0;
            for ( auto iter = iterLow; iter != iterhigh; ++ iter )
                tot += (iter->second).size();
            _innerMap.erase( iterLow, iterhigh );
            _unorderedSize -= tot;
        }
    }
    if ( data.size() > 0 ) {
        _innerMap.insert({ index, data });
        _unorderedSize += data.size();
    }

    /**
     * if over the capacity, just throw away the bytes at the end.
     */
    auto iter = _innerMap.end();
    --iter;
    while ( true ){
        // if ( _innerMap.empty() ) break;
        if ( _capacity >= _output.buffer_size() + _unorderedSize ) break;
        if ( _capacity >= _output.buffer_size() + _unorderedSize - (iter->second).size()){
            size_t len = _output.buffer_size() + _unorderedSize - _capacity;
            string newData = (iter->second).substr(0, (iter->second).size() - len);
            if ( newData.size() == 0 ) _innerMap.erase( iter );
            else _innerMap[ iter->first ] = newData;
            _unorderedSize -= len;
            break;
        }else{
            _innerMap.erase( iter );
            iter = _innerMap.end();
            if ( iter == _innerMap.begin() ) break;
            --iter;
        }
    }
}



void StreamReassembler::make_transition(){
    while ( true ){
        if (_innerMap.size() == 0 ) break;
        auto iter = _innerMap.find( _wannaIndex );
        if ( iter != _innerMap.end() ){
            _output.write( iter -> second );
            _wannaIndex += (iter -> second).size();
            _unorderedSize -= (iter->second).size();
            _innerMap.erase( iter );
        } else break;
    }
}




// the thought in the 11.14
    // if ( newIndex + newData.size() > maxIndex ){
    //     size_t length = maxIndex - newIndex;
    //     newData = newData.substr( 0, length );
    // }
    // if ( _innerMap.size() == 0 ) {
    //     _innerMap.insert({ newIndex, newData });
    //     return;
    // }
    // auto iter1 = _innerMap.lower_bound( newIndex );
    // if ( iter1 != _innerMap.begin() ){
    //     /**
    //      * it means the map has the item whose tail is begin the newdata.
    //      */
    //     auto iter = --iter1;
    //     if ( iter -> first < newIndex && iter->first + (iter->second).size() > newIndex ){
    //         size_t pos = (iter->second).size() + (iter->first) - newIndex;
    //         newIndex = iter->first + (iter->second).size();
    //         newData = newData.substr( pos, newData.size() - pos );
    //     }
    // }
    // auto iter2 = --(_innerMap.lower_bound( newIndex + newData.size() ));
    // if ( iter2 -> first < newIndex + newData.size() && iter2 -> first + (iter2 -> second).size() > newIndex + newData.size() ) {
    //     size_t pos = newIndex + newData.size() - (iter2 -> first);
    //     newData = newData.substr( 0, newData.size() - pos );
    // }else ++iter2;
    // --iter2;
    // if ( distance(iter2,iter1) <= 0 ) _innerMap.erase( iter1, iter2 );
    // _innerMap.insert({ newIndex, newData });


    // /**
    //  * Aim: make the value of the buffer into the _output
    //  */
    // auto iter = _innerMap.begin();
    // while ( iter != _innerMap.end() ){
    //     if ( iter -> first == _wannaIndex ) {
    //         _output.write( iter->second );
    //         _wannaIndex += (iter->second).size();
    //         _innerMap.erase(iter);
    //         iter = _innerMap.begin();
    //     }else break;
    // }
    // if ( newIndex > _eofIndex ) _output.end_input();

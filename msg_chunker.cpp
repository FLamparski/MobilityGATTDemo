#include <Arduino.h>
#include "msg_chunker.h"

void for_each_chunk(std::string str, int chunk_size, std::function<void(std::string)> callback) {
    int len = str.length();
    int pos = 0;
    while (pos < len) {
        int cl = pos < len - chunk_size ? chunk_size : len - pos;
        auto chunk = str.substr(pos, cl);
        pos += cl;
        callback(chunk);
    }
}

#include <string>
#include <functional>

#ifndef _MSG_CHUNKER_H
#define _MSG_CHUNKER_H

void for_each_chunk(std::string str, int chunk_size, std::function<void(std::string)> callback);

#endif

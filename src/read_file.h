#pragma once
#include <fstream>
#include <iostream>
#include <sstream>
#include <cstring>

//Caller must ensure that file_contents_holder is delete[]`d!
inline bool read_binary_file(const char* file_path, char* &file_contents_holder, size_t& file_size)
{
    std::ifstream file_reader(file_path, std::ios::ate | std::ios::binary);

    if(!file_reader.is_open())
        return false;

    //get file size then seek back to beginning
    file_size = (size_t)file_reader.tellg();
    file_reader.seekg(0);
    
    //allocate and read file contents
    file_contents_holder = new char[file_size];
    file_reader.read(file_contents_holder, file_size);

    file_reader.close();

    return true;
}
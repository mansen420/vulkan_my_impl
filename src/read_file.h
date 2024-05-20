#pragma once
#include <fstream>
#include <iostream>
#include <sstream>
#include <cstring>
#include <vector>
//Reads binary file into C-style string
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
//Reads binary file into contents
//Be warned that previous contents of the vector will be lost
inline bool read_binary_file(const char* file_path, std::vector<char>& contents)
{
    std::ifstream file_reader(file_path, std::ios::ate | std::ios::binary);

    if(!file_reader.is_open())
        return false;

    //get file size then seek back to beginning
    size_t file_size = (size_t)file_reader.tellg();
    file_reader.seekg(0);
    
    contents.resize(file_size);
    //read file contents
    file_reader.read(contents.data(), file_size);

    file_reader.close();

    return true;
}
//Will read first match from search_paths
inline bool read_binary_file(std::vector<const char*> search_paths, const char* filename, std::vector<char>& contents)
{
    bool result;
    for(const auto& directory : search_paths)
    {
        std::string path(directory);
        path.append(filename);
        result = read_binary_file(path.c_str(), contents);
        if (result)
            break;
    }
    return result;
}
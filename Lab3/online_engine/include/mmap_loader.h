#pragma once

#include <iostream>
#include <stdexcept>
#include <string>

// --- Windows 平台的内存映射实现 ---
#ifdef _WIN32

#ifndef NOMINMAX
#define NOMINMAX
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>

template <typename T>
class MMapLoader {
private:
    HANDLE hFile;
    HANDLE hMap;
    size_t file_size;
    size_t element_count;
    T* mapped_data;

public:
    MMapLoader(const std::string& filepath) : hFile(INVALID_HANDLE_VALUE), hMap(NULL), file_size(0), element_count(0), mapped_data(nullptr) {
        // 1. 打开文件
        hFile = CreateFileA(filepath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE) {
            throw std::runtime_error("Failed to open file (Windows): " + filepath);
        }

        // 2. 获取文件大小
        LARGE_INTEGER size;
        if (!GetFileSizeEx(hFile, &size)) {
            CloseHandle(hFile);
            throw std::runtime_error("Failed to get file status (Windows): " + filepath);
        }
        file_size = size.QuadPart;

        if (file_size == 0) {
            CloseHandle(hFile);
            throw std::runtime_error("File is empty: " + filepath);
        }

        // 3. 创建文件映射对象
        hMap = CreateFileMappingA(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
        if (hMap == NULL) {
            CloseHandle(hFile);
            throw std::runtime_error("Failed to create file mapping (Windows)");
        }

        // 4. 将文件视图映射到进程的虚拟地址空间
        mapped_data = static_cast<T*>(MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0));
        if (mapped_data == NULL) {
            CloseHandle(hMap);
            CloseHandle(hFile);
            throw std::runtime_error("Failed to map view (Windows)");
        }

        element_count = file_size / sizeof(T);
        std::cout << "Successfully mapped file (Windows API): " << filepath << ", element count: " << element_count << std::endl;
    }

    ~MMapLoader() {
        if (mapped_data != nullptr) {
            UnmapViewOfFile(mapped_data);
        }
        if (hMap != NULL) {
            CloseHandle(hMap);
        }
        if (hFile != INVALID_HANDLE_VALUE) {
            CloseHandle(hFile);
        }
    }

    const T* data() const { return mapped_data; }
    size_t size() const { return element_count; }
};

// --- Linux / macOS (POSIX) 平台的内存映射实现 ---
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

template <typename T>
class MMapLoader {
private:
    int fd;
    size_t file_size;
    size_t element_count;
    T* mapped_data;

public:
    MMapLoader(const std::string& filepath) : fd(-1), file_size(0), element_count(0), mapped_data(nullptr) {
        fd = open(filepath.c_str(), O_RDONLY);
        if (fd == -1) {
            throw std::runtime_error("Failed to open file (POSIX): " + filepath);
        }

        struct stat sb;
        if (fstat(fd, &sb) == -1) {
            close(fd);
            throw std::runtime_error("Failed to get file status (POSIX): " + filepath);
        }
        file_size = sb.st_size;

        if (file_size == 0) {
            close(fd);
            throw std::runtime_error("File is empty: " + filepath);
        }

        mapped_data = static_cast<T*>(mmap(nullptr, file_size, PROT_READ, MAP_SHARED, fd, 0));
        if (mapped_data == MAP_FAILED) {
            close(fd);
            throw std::runtime_error("mmap failed (POSIX)");
        }

        element_count = file_size / sizeof(T);
        std::cout << "Successfully mapped file (POSIX API): " << filepath << ", element count: " << element_count << std::endl;
    }

    ~MMapLoader() {
        if (mapped_data != MAP_FAILED && mapped_data != nullptr) {
            munmap(mapped_data, file_size);
        }
        if (fd != -1) {
            close(fd);
        }
    }

    const T* data() const { return mapped_data; }
    size_t size() const { return element_count; }
};
#endif
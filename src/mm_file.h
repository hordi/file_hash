// Copyright 2022 Yurii Hordiienko
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include <memory>
#include <cstddef>

/*! \class MMapFileReader
*   \brief This class covers Memory Mapped technology to read file. Read-only mode.
*/
class mm_file
{
public:
    enum { DEFAULT_BUFFER_SIZE = 256 * 1024 * 1024, END_OF_FILE = -1 };

    /*! \class Cursor
    *   \brief The class represents separated mmap navigation mechanism.
    */
    class Cursor
    {
    public:
        ~Cursor();

        /** \return Get current file position (offset from the beginning). */
        size_t pos() const noexcept { return _filePos; }

        /** Set current file position and prepare memory block to read.
        * \param pos New file position.
        * \param ready_for_read_len length of the memory block ready to read.
        * \param prepare_for_read_len Size of memory to be ready to read. If equal 0 - use maximum buffer size.
        * \return pointer to memory for read or nullptr
        */
        const int8_t* seek(size_t pos, size_t& ready_for_read_len, size_t prepare_for_read_len) noexcept {
            if (auto ptr = _parent->seek(*this, pos, prepare_for_read_len)) {
                ready_for_read_len = _blockSize - (_filePos - _blockPos);
                return ptr;
            }
            return nullptr;
        }

        /** Reset original buffer size if internal block has been increased > default size
        * Invalidate all previous memory pointers
        */
        bool resetBufferSize() noexcept;

    private:
        friend class mm_file;

        Cursor(size_t buf_size, mm_file& parent) noexcept
            :_parent(&parent),
            _view(nullptr),
            _filePos(0),
            _blockPos(0),
            _blockSize(buf_size),
            _max_buf_size(buf_size)
        {}

        Cursor(const Cursor&);
        Cursor& operator=(const Cursor&);

        mm_file* _parent;
        void* _view;                /**< Pointer to active memory block */
        size_t _filePos;            /**< Current File position */
        size_t _blockPos;           /**< File-position of the block */
        size_t _blockSize;          /**< Current memory cache block size */
        const size_t _max_buf_size; /**< Maximum cache memory block size. May be automatically increased for case if have no EOL in the current block*/
    };

    /**
    * \param path Path of file to open.
    */
    mm_file(const char* path = nullptr);
    ~mm_file() {
        close();
    }

    /** Open file
    * \param path A file full path string.
    */
    bool open(const char* path);
    /** \return The file state. */
    bool isOpen() const noexcept;

    /** Close the file. */
    void close() noexcept;

    /** \return Get file size. */
    size_t size() const noexcept { return _fileSize; }

    /**
    * \param max_buf_size Size of cache memory block(128MB by default). Do not use less than 4MB(slow).
    * \return Cursor pointer.
    */
    std::unique_ptr<Cursor> map(size_t pos = 0, size_t prepare_for_read_len = 0, size_t max_buf_size = DEFAULT_BUFFER_SIZE);

private:
    mm_file(const mm_file&) = delete;
    mm_file& operator=(const mm_file&) = delete;

    /** Set current file position and prepare memory block to read.
    * \param pos New file position.
    * \param prepare_for_read_len Size of memory to be ready to read. If equal 0 - use maximum buffer size.
    * \return pointer to memory for read or nullptr
    */
    const int8_t* seek(Cursor&, size_t pos, size_t prepare_for_read_len = 0) noexcept;

    /**
     * \return number of bytes ready to read or END_OF_FILE if no more data available (end of file)
     */
    long long read(Cursor&, const int8_t** pStr, size_t len) const noexcept;

#ifdef _MSC_VER
    void* _file; /**< File handler (winapi HANDLE type) */
    void* _mem;  /**< mmap handler (winapi HANDLE type) */
#else
    int _file;   /**< File handler (linux int-type) */
#endif
    size_t _fileSize; /**< Size of the file */
};

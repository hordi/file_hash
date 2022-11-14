#include "mm_file.h"

#ifdef _MSC_VER
#  include <windows.h>
#else
#  include <sys/mman.h>
#  include <sys/stat.h>
#  include <unistd.h>
#  include <fcntl.h>
#  include <string.h>
#endif

#ifdef _MSC_VER
#  define INVALID_FILE_HANDLE INVALID_HANDLE_VALUE
#else
#  define INVALID_FILE_HANDLE -1
#endif

namespace
{
    struct sys_info
    {
        sys_info() noexcept
        {
#ifdef _MSC_VER
            SYSTEM_INFO rec;
            GetSystemInfo(&rec);

            _offsetAlign = rec.dwAllocationGranularity;
#else
            int offset = sysconf(_SC_PAGE_SIZE);
            if (offset > 0)
                _offsetAlign = (size_t)offset;
            else
                _offsetAlign = 0; //error
#endif
        }

        size_t align(size_t& val) const noexcept
        {
            size_t dx = (val % _offsetAlign);
            val -= dx;
            return dx;
        }

        size_t offsetAlign() const noexcept { return _offsetAlign; }

    private:
        size_t _offsetAlign; /**< System Offset Allocation Align size*/
    };
}

static const sys_info _si;

mm_file::Cursor::~Cursor()
{
    if (_view)
#ifdef _MSC_VER
        UnmapViewOfFile(_view);
#else
        munmap(_view, _blockSize);
#endif
}

bool mm_file::Cursor::resetBufferSize() noexcept
{
    if (_blockSize <= _max_buf_size)
        return true;

#ifdef _MSC_VER
    UnmapViewOfFile(_view);
#else
    munmap(_view, _blockSize);
#endif
    _view = nullptr;

    size_t cur_pos = _filePos;

    _blockSize = _filePos = 0;

    size_t actual_len;
    return seek(cur_pos, actual_len, 0) != nullptr;
}

mm_file::mm_file(const char* path) :
    _file(INVALID_FILE_HANDLE),
#ifdef _MSC_VER
    _mem(INVALID_HANDLE_VALUE),
#else
    //_mem(nullptr),
#endif
    _fileSize(0)
{
    open(path);
}

bool mm_file::open(const char* path)
{
    close();

    if (!path)
        return false;

#ifdef _MSC_VER

    _file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);

    if (INVALID_HANDLE_VALUE == _file)
        return false;

    LARGE_INTEGER sz;

    if (GetFileSizeEx(_file, &sz))
    {
        if (sz.QuadPart) //file not empty
        {
            _fileSize = static_cast<size_t>(sz.QuadPart);
            _mem = CreateFileMapping(_file, NULL, PAGE_READONLY, 0, 0, NULL);
        }
    }

    if (!_mem)
    {
        CloseHandle(_file);
        _file = INVALID_HANDLE_VALUE;

        return false;
    }

#else

    _file = ::open(path, O_RDONLY | O_LARGEFILE);

    if (_file < 0)
        return false;

    struct stat st;
    if (fstat(_file, &st) || !st.st_size)
    {
        ::close(_file);
        _file = INVALID_FILE_HANDLE;

        return false;
    }

    _fileSize = st.st_size;

#endif

    return true;
}

void mm_file::close() noexcept
{
    if (INVALID_FILE_HANDLE != _file)
    {
#ifdef _MSC_VER
        CloseHandle(_file);
        if (_mem && INVALID_HANDLE_VALUE != _mem)
        {
            CloseHandle(_mem);
            _mem = INVALID_HANDLE_VALUE;
        }
#else
        ::close(_file);
#endif

        _file = INVALID_FILE_HANDLE;
        _fileSize = 0;
    }
}

std::unique_ptr<mm_file::Cursor> mm_file::map(size_t pos, size_t prepare_for_read_len, size_t buf_size)
{
    size_t right_pos = pos + prepare_for_read_len;
    if (!_fileSize || right_pos > _fileSize)
        return nullptr;

    buf_size = (buf_size / _si.offsetAlign()) * _si.offsetAlign();

    if (buf_size > _fileSize)
        buf_size = _fileSize;

    std::unique_ptr<Cursor> m(new Cursor(buf_size, *this));

    m->_filePos = pos;
    m->_blockPos = pos;
    auto dx = _si.align(m->_blockPos); //blockPos is aligned from now

    m->_blockSize = prepare_for_read_len + dx;

    if (m->_blockSize < m->_max_buf_size) //cache maximum block size
    {
        m->_blockSize = m->_max_buf_size;

        size_t sz = _fileSize - m->_blockPos;
        if (sz < m->_blockSize) //last file part < block size, seek left to max buf size
        {
            m->_blockPos = _fileSize - m->_blockSize;
            dx = _si.align(m->_blockPos);

            m->_blockSize = _fileSize - m->_blockPos;
        }
    }

#ifdef _MSC_VER
    DWORD high = ((m->_blockPos >> 32) & 0xFFFFFFFF);
    DWORD low = (m->_blockPos & 0xFFFFFFFF);

    static_assert(sizeof(size_t) == sizeof(uint64_t), "x64 only required");

    m->_view = MapViewOfFile(_mem, FILE_MAP_READ, high, low, m->_blockSize);
    if (m->_view)
        return m;
#else
    m->_view = mmap(nullptr, m->_blockSize, PROT_READ, MAP_PRIVATE, _file, m->_blockPos);
    if (m->_view != MAP_FAILED)
        return m;
    m->_view = nullptr; //for proper dtor call
#endif //_MSC_VER

    return std::unique_ptr<Cursor>();
}

const int8_t* mm_file::seek(Cursor& r, size_t pos, size_t prepare_for_read_len) noexcept
{
    size_t right_pos = pos + prepare_for_read_len;
    if (!_fileSize || right_pos > _fileSize)
        return nullptr;

    if (r._view)
    {
        if (pos >= r._blockPos && right_pos <= (r._blockPos + r._blockSize))
        {
            r._filePos = pos;
            return (const int8_t*)r._view + (r._filePos - r._blockPos);
        }

#ifdef _MSC_VER
        UnmapViewOfFile(r._view);
#else
        munmap(r._view, r._blockSize);
#endif
        r._view = nullptr;
    }

    r._blockPos = r._filePos = pos;
    auto dx = _si.align(r._blockPos); //blockPos is aligned from now

    r._blockSize = prepare_for_read_len + dx;

    if (r._blockSize < r._max_buf_size) //cache maximum block size
    {
        r._blockSize = r._max_buf_size;

        size_t sz = _fileSize - r._blockPos;
        if (sz < r._blockSize) //last file part < block size, seek left to max buf size
        {
            r._blockPos = _fileSize - r._blockSize;
            dx = _si.align(r._blockPos);

            r._blockSize = _fileSize - r._blockPos;
        }
    }

#ifdef _MSC_VER
    DWORD offsetHigh = ((r._blockPos >> 32) & 0xFFFFFFFF);
    DWORD offsetLow = (r._blockPos & 0xFFFFFFFF);

    static_assert(sizeof(size_t) == sizeof(uint64_t), "x64 only required");

    r._view = MapViewOfFile(_mem, FILE_MAP_READ, offsetHigh, offsetLow, r._blockSize);
    if (r._view)
        return (const int8_t*)r._view + (r._filePos - r._blockPos);
#else
    r._view = mmap(nullptr, r._blockSize, PROT_READ, MAP_PRIVATE, _file, r._blockPos);
    if (r._view != MAP_FAILED)
        return (const int8_t*)r._view + (r._filePos - r._blockPos);
    r._view = nullptr; //for proper dtor call
#endif //_MSC_VER

    r._blockPos = r._filePos = r._blockSize = 0;
    return nullptr;
}

long long mm_file::read(Cursor& r, const int8_t** pStr, size_t len) const noexcept
{
    size_t max_len = _fileSize - r._filePos;
    if (len > max_len)
        len = max_len;

    if (len)
    {
        size_t actual_len;
        const int8_t* p = r.seek(r._filePos, actual_len, len);
        *pStr = p;
        if (p) {
            r._filePos += actual_len;
            return static_cast<long long>(actual_len);
        }
        return END_OF_FILE;
    }
    return 0;
}

bool mm_file::isOpen() const noexcept {
    return (_file != INVALID_FILE_HANDLE);
}

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

#include "mm_file.h"
#include "aquahash.h"

void print_info()
{
    puts("Usage:   file_hash <file_1 file_n>\n\n"
        "make 128-bits HEX hash-sum of file(s)\n"
        "version 1.0.0"
    );

    exit(0);
}

static void to_hex(char* res, __m128i m) noexcept
{
    const char* hh = "0123456789abcdef";

    const uint8_t* digest = (const uint8_t*)&m;
    for (size_t i = 0; i != 16; ++i) {
        auto c = digest[i];
        *(uint16_t*)((char*)res + i * 2) = hh[(c & 0xf0) >> 4] | (hh[c & 0x0f] << 8);
    }
}

int main(int argv, char** argc)
{
    if (argv == 1)
        print_info();

    char digest[33];
    digest[32] = '\0';

    AquaHash ah;
    mm_file f;

    for (int i = 1; i != argv; ++i)
    {
        const char* fname = argc[i];

        if (!f.open(fname)) {
            printf("can't open file[%s]\n", fname);
            return 1;
        }

        size_t file_size = f.size();

        auto cursor = f.map(0, 0);
        if (!cursor.get()) {
            printf("can't create memory-view\n");
            return 1;
        }

        ah.Initialize();

        for (size_t pos = 0, bytes_available; file_size;)
        {
            size_t block_size = (file_size > mm_file::DEFAULT_BUFFER_SIZE)
                ? mm_file::DEFAULT_BUFFER_SIZE
                : file_size;

            const int8_t* data = cursor->seek(pos, bytes_available, block_size);
            pos += bytes_available;
            file_size -= bytes_available;

            if (!data) break;

            ah.Update((const uint8_t*)data, bytes_available);
        }

        f.close();

        to_hex(digest, ah.Finalize());

        printf("%s *%s\n", digest, fname);
    }

    return 0;
}